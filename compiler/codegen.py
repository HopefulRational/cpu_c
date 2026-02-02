from parser import *
from lexer import *

class CodeGen:
    def __init__(self):
        self.code = []
        self.label_counter = 0
        self.locals = {} # name -> offset from BP (R5). Locals are negative offsets.
        self.local_offset = 0 # Current offset for next local
        self.functions = {} # name -> label
        self.globals = {} # name -> address
        self.next_global_addr = 0x4000 # Start of global data area

    def emit(self, instr):
        self.code.append(instr)

    def new_label(self, prefix="L"):
        self.label_counter += 1
        return f"{prefix}_{self.label_counter}"

    def generate_program(self, program):
        # Allocate Globals First
        for var in program.globals:
            self.globals[var.name] = self.next_global_addr
            self.next_global_addr += 2

        # Generate _start routine
        self.emit("JMP _start")
        
        # Functions
        for func in program.functions:
            self.generate_function(func)
            
        # _start
        self.emit("_start:")
        self.emit("LOAD SP, 0xFFFF") # Init Stack Pointer
        
        # Global Variables Initialization
        for var in program.globals:
            if var.initializer:
                self.generate_expression(var.initializer)
                self.emit("POP R0") # Value
                self.emit(f"LOAD R1, {self.globals[var.name]}") # Addr
                self.emit("STI R0, R1")
        
        # Call main
        ret_label = self.new_label("EXIT")
        self.emit(f"LOAD R6, {ret_label}")
        self.emit("PUSH R6")
        self.emit("JMP main")
        self.emit(f"{ret_label}:")
        self.emit("HALT")
            
        return "\n".join(self.code)

    def generate_function(self, func):
        self.emit(f"{func.name}:")
        self.functions[func.name] = func.name
        
        # Prologue
        # PUSH BP (R5)
        self.emit("PUSH R5")
        # MOV BP, SP
        self.emit("MOV R5, SP")
        
        # Reset locals for function
        self.locals = {}
        self.local_offset = 0
        
        # Parse params (offsets positive: R5 + 4 + idx*2)
        # Stack: [Args...] [RetAddr] [OldBP]
        # Arg 0 is at R5 + 4
        # Arg 1 is at R5 + 6
        
        offset = 4
        for p_type, p_name in func.params:
             self.locals[p_name] = offset
             offset += 2
             
        # Body
        for stmt in func.body:
            self.generate_statement(stmt)
            
        # Epilogue (implicit return if void)
        # Use a common return label for the function?
        # But here we just fall through.
        self.emit(f"{func.name}_ret:")
        self.emit("MOV SP, R5")
        self.emit("POP R5")
        self.emit("POP R6")
        self.emit("MOV IP, R6")

    def generate_statement(self, stmt):
        if isinstance(stmt, VarDecl):
            if stmt.initializer:
                # PUSH value. This effectively allocates space for the variable.
                self.generate_expression(stmt.initializer)
            else:
                # Allocate uninitialized space on stack
                self.emit("LOAD R1, 2")
                self.emit("SUB SP, R1")
            
            self.local_offset -= 2
            self.locals[stmt.name] = self.local_offset
                
        elif isinstance(stmt, AssignStmt):
            self.generate_expression(stmt.expr)
            self.emit("POP R0")
            
            # Check local
            offset = self.locals.get(stmt.name)
            if offset is not None:
                # Addr = R5 + offset
                self.emit(f"LOAD R1, {abs(offset)}") # Load abs offset
                self.emit("MOV R2, R5")
                if offset < 0:
                    self.emit("SUB R2, R1")
                else:
                    self.emit("ADD R2, R1")
                self.emit("STI R0, R2")
            else:
                # Check Global
                addr = self.globals.get(stmt.name)
                if addr is not None:
                    self.emit(f"LOAD R1, {addr}")
                    self.emit("STI R0, R1")
                else:
                    raise Exception(f"Unknown variable {stmt.name}")

        elif isinstance(stmt, PointerAssignStmt):
            # *ptr = val
            self.generate_expression(stmt.value_expr) # PUSH val
            self.generate_expression(stmt.ptr_expr)   # PUSH ptr (address)
            
            self.emit("POP R1") # Address
            self.emit("POP R0") # Value
            self.emit("STI R0, R1")

        elif isinstance(stmt, AsmStmt):
            self.emit(stmt.asm_code)

        elif isinstance(stmt, IfStmt):
            end_label = self.new_label("END_IF")
            else_label = self.new_label("ELSE") if stmt.else_block else end_label
            
            self.generate_expression(stmt.condition)
            self.emit("POP R0")
            self.emit("LOAD R1, 0")
            self.emit("SUB R0, R1")
            self.emit(f"JZ {else_label}")
            
            for s in stmt.then_block:
                self.generate_statement(s)
                
            if stmt.else_block:
                self.emit(f"JMP {end_label}")
                self.emit(f"{else_label}:")
                for s in stmt.else_block:
                    self.generate_statement(s)
            
            self.emit(f"{end_label}:")

        elif isinstance(stmt, WhileStmt):
            loop_label = self.new_label("LOOP")
            end_label = self.new_label("END_WHILE")
            
            self.emit(f"{loop_label}:")
            self.generate_expression(stmt.condition)
            self.emit("POP R0")
            self.emit("LOAD R1, 0")
            self.emit("SUB R0, R1")
            self.emit(f"JZ {end_label}")
            
            for s in stmt.body:
                self.generate_statement(s)
            
            self.emit(f"JMP {loop_label}")
            self.emit(f"{end_label}:")
            
        elif isinstance(stmt, ReturnStmt):
             if stmt.expr:
                 self.generate_expression(stmt.expr)
                 self.emit("POP R0") # Return value in R0
             
             # Find current function name?
             # We need to jump to function epilogue.
             # We can store current function name in self.current_function
             func_name = list(self.functions.keys())[-1] # Hacky but works if linear gen
             self.emit(f"JMP {func_name}_ret")

        # Expression Statement (e.g. FuncCall)
        elif isinstance(stmt, (BinOp, UnaryOp, FuncCall, NumLiteral, VarAccess)):
             self.generate_expression(stmt)
             self.emit("POP R0") # Discard result

    def generate_expression(self, expr):
        if isinstance(expr, NumLiteral):
            self.emit(f"LOAD R0, {expr.value}")
            self.emit("PUSH R0")
            
        elif isinstance(expr, VarAccess):
            # Check Local
            offset = self.locals.get(expr.name)
            if offset is not None:
                # Addr = R5 + offset
                self.emit(f"LOAD R1, {abs(offset)}")
                self.emit("MOV R2, R5")
                if offset < 0:
                    self.emit("SUB R2, R1")
                else:
                    self.emit("ADD R2, R1")
                self.emit("LDI R0, R2")
            else:
                # Check Global
                addr = self.globals.get(expr.name)
                if addr is not None:
                     self.emit(f"LOAD R1, {addr}")
                     self.emit("LDI R0, R1")
                else:
                     raise Exception(f"Unknown variable {expr.name}")
            
            self.emit("PUSH R0")

        elif isinstance(expr, FuncCall):
            # Push Args in Reverse Order
            for arg in reversed(expr.args):
                self.generate_expression(arg)
            
            # Return Address
            ret_label = self.new_label("RET")
            self.emit(f"LOAD R6, {ret_label}")
            self.emit("PUSH R6")
            
            # Jump
            self.emit(f"JMP {expr.name}")
            self.emit(f"{ret_label}:")
            
            # Cleanup Args
            pop_size = len(expr.args) * 2
            if pop_size > 0:
                self.emit(f"LOAD R1, {pop_size}")
                self.emit("LOAD R2, SP")
                self.emit("ADD R2, R1")
                self.emit("MOV SP, R2")
            
            # Result is in R0. Push it.
            self.emit("PUSH R0")

        elif isinstance(expr, BinOp):
            self.generate_expression(expr.left)
            self.generate_expression(expr.right)
            self.emit("POP R1") # Right
            self.emit("POP R0") # Left
            
            if expr.op == TOKEN_PLUS:
                self.emit("ADD R0, R1")
            elif expr.op == TOKEN_MINUS:
                self.emit("SUB R0, R1")
            elif expr.op == TOKEN_STAR:
                raise Exception("MUL not supported yet")
            
            elif expr.op == TOKEN_EQ:
                self.emit("SUB R0, R1")
                lbl_true = self.new_label("EQ_TRUE")
                lbl_end = self.new_label("EQ_END")
                self.emit(f"JZ {lbl_true}")
                self.emit("LOAD R0, 0")
                self.emit(f"JMP {lbl_end}")
                self.emit(f"{lbl_true}:")
                self.emit("LOAD R0, 1")
                self.emit(f"{lbl_end}:")
                
            elif expr.op == TOKEN_NEQ:
                self.emit("SUB R0, R1")
                lbl_true = self.new_label("NEQ_TRUE")
                lbl_end = self.new_label("NEQ_END")
                self.emit(f"JZ {lbl_true}") # If Zero (Equal) -> False
                self.emit("LOAD R0, 1")
                self.emit(f"JMP {lbl_end}")
                self.emit(f"{lbl_true}:")
                self.emit("LOAD R0, 0")
                self.emit(f"{lbl_end}:")
            
            elif expr.op == TOKEN_LT: # <
                self.emit("SUB R0, R1") # R0 - R1.
                lbl_true = self.new_label("LT_TRUE")
                lbl_end = self.new_label("LT_END")
                self.emit(f"JN {lbl_true}")
                self.emit("LOAD R0, 0")
                self.emit(f"JMP {lbl_end}")
                self.emit(f"{lbl_true}:")
                self.emit("LOAD R0, 1")
                self.emit(f"{lbl_end}:")
                
            elif expr.op == TOKEN_GT: # >  (a > b) <=> (b < a)
                # b-a < 0
                self.emit("SUB R1, R0")
                lbl_true = self.new_label("GT_TRUE")
                lbl_end = self.new_label("GT_END")
                self.emit(f"JN {lbl_true}")
                self.emit("LOAD R0, 0")
                self.emit(f"JMP {lbl_end}")
                self.emit(f"{lbl_true}:")
                self.emit("LOAD R0, 1")
                self.emit(f"{lbl_end}:")
                
            elif expr.op == TOKEN_LE: # <=  (not >) <=> not (b < a) <=> not (b-a < 0)
                # b-a < 0 -> GT -> False
                # else -> True
                self.emit("SUB R1, R0")
                lbl_false = self.new_label("LE_FALSE")
                lbl_end = self.new_label("LE_END")
                self.emit(f"JN {lbl_false}")
                self.emit("LOAD R0, 1")
                self.emit(f"JMP {lbl_end}")
                self.emit(f"{lbl_false}:")
                self.emit("LOAD R0, 0")
                self.emit(f"{lbl_end}:")
                
            elif expr.op == TOKEN_GE: # >= (not <) <=> not (a-b < 0)
                self.emit("SUB R0, R1")
                lbl_false = self.new_label("GE_FALSE")
                lbl_end = self.new_label("GE_END")
                self.emit(f"JN {lbl_false}")
                self.emit("LOAD R0, 1")
                self.emit(f"JMP {lbl_end}")
                self.emit(f"{lbl_false}:")
                self.emit("LOAD R0, 0")
                self.emit(f"{lbl_end}:")
                
            elif expr.op == TOKEN_AND: # &&
                # Stack has A, B.
                # R0=Left, R1=Right.
                # If R0 != 0 AND R1 != 0 -> 1.
                # If R0 == 0 -> 0.
                lbl_false = self.new_label("AND_FALSE")
                lbl_end = self.new_label("AND_END")
                
                # Check R0
                self.emit("LOAD R2, 0")
                self.emit("SUB R0, R2") # R0 - 0
                self.emit(f"JZ {lbl_false}")
                
                # Check R1
                self.emit("SUB R1, R2") # R1 - 0
                self.emit(f"JZ {lbl_false}")
                
                self.emit("LOAD R0, 1")
                self.emit(f"JMP {lbl_end}")
                
                self.emit(f"{lbl_false}:")
                self.emit("LOAD R0, 0")
                self.emit(f"{lbl_end}:")
                
            elif expr.op == TOKEN_OR: # ||
                lbl_true = self.new_label("OR_TRUE")
                lbl_check_r1 = self.new_label("OR_CHECK_R1")
                lbl_end = self.new_label("OR_END")
                lbl_false = self.new_label("OR_FALSE")
                
                # Check R0
                self.emit("LOAD R2, 0")
                self.emit("SUB R0, R2")
                self.emit(f"JZ {lbl_check_r1}")
                self.emit(f"JMP {lbl_true}")
                
                self.emit(f"{lbl_check_r1}:")
                self.emit("SUB R1, R2")
                self.emit(f"JZ {lbl_false}")
                self.emit(f"JMP {lbl_true}")
                
                self.emit(f"{lbl_true}:")
                self.emit("LOAD R0, 1")
                self.emit(f"JMP {lbl_end}")
                
                self.emit(f"{lbl_false}:")
                self.emit("LOAD R0, 0")
                self.emit(f"{lbl_end}:")

            self.emit("PUSH R0")
            
        elif isinstance(expr, UnaryOp):
            if expr.op == '*': # Dereference
                self.generate_expression(expr.expr) # PUSH Addr
                self.emit("POP R1")
                self.emit("LDI R0, R1")
                self.emit("PUSH R0")
