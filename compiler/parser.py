from lexer import *

# AST Nodes
class Node: pass

class Program(Node):
    def __init__(self, globals, functions):
        self.globals = globals
        self.functions = functions
    def __repr__(self): return f"Program(Globals: {self.globals}, Functions: {self.functions})"

class FunctionDef(Node):
    def __init__(self, name, ret_type, params, body):
        self.name = name
        self.ret_type = ret_type
        self.params = params # List of (type, name)
        self.body = body     # List of statements
    def __repr__(self): return f"Func({self.ret_type} {self.name}({self.params}) {{...}})"

class VarDecl(Node):
    def __init__(self, type, name, initializer=None):
        self.type = type
        self.name = name
        self.initializer = initializer
    def __repr__(self): return f"VarDecl({self.type} {self.name} = {self.initializer})"

class AssignStmt(Node):
    def __init__(self, name, expr):
        self.name = name
        self.expr = expr
    def __repr__(self): return f"Assign({self.name} = {self.expr})"

class PointerAssignStmt(Node):
    def __init__(self, ptr_expr, value_expr):
        self.ptr_expr = ptr_expr
        self.value_expr = value_expr
    def __repr__(self): return f"PtrAssign(*{self.ptr_expr} = {self.value_expr})"

class IfStmt(Node):
    def __init__(self, condition, then_block, else_block=None):
        self.condition = condition
        self.then_block = then_block
        self.else_block = else_block
    def __repr__(self): return f"If({self.condition} {{...}})"

class WhileStmt(Node):
    def __init__(self, condition, body):
        self.condition = condition
        self.body = body
    def __repr__(self): return f"While({self.condition} {{...}})"

class ReturnStmt(Node):
    def __init__(self, expr):
        self.expr = expr
    def __repr__(self): return f"Return({self.expr})"

class AsmStmt(Node):
    def __init__(self, asm_code):
        self.asm_code = asm_code
    def __repr__(self): return f"Asm({self.asm_code})"

class BinOp(Node):
    def __init__(self, left, op, right):
        self.left = left
        self.op = op
        self.right = right
    def __repr__(self): return f"BinOp({self.left} {self.op} {self.right})"

class UnaryOp(Node):
    def __init__(self, op, expr):
        self.op = op
        self.expr = expr
    def __repr__(self): return f"Unary({self.op} {self.expr})"

class NumLiteral(Node):
    def __init__(self, value):
        self.value = value
    def __repr__(self): return f"Num({self.value})"

class StringLiteral(Node):
    def __init__(self, value):
        self.value = value
    def __repr__(self): return f"Str({self.value})"

class VarAccess(Node):
    def __init__(self, name):
        self.name = name
    def __repr__(self): return f"Var({self.name})"

class FuncCall(Node):
    def __init__(self, name, args):
        self.name = name
        self.args = args
    def __repr__(self): return f"Call({self.name}({self.args}))"


class Parser:
    def __init__(self, tokens):
        self.tokens = tokens
        self.pos = 0

    def peek(self):
        if self.pos < len(self.tokens):
            return self.tokens[self.pos]
        return None

    def advance(self):
        if self.pos < len(self.tokens):
            t = self.tokens[self.pos]
            self.pos += 1
            return t
        return None

    def match(self, type):
        if self.pos < len(self.tokens) and self.tokens[self.pos].type == type:
            return self.advance()
        return None

    def expect(self, type):
        t = self.match(type)
        if not t:
            curr = self.peek()
            raise Exception(f"Expected {type}, got {curr.type if curr else 'EOF'} at line {curr.line if curr else '?'}")
        return t

    def parse(self):
        functions = []
        globals = []
        while self.peek().type != TOKEN_EOF:
            # Look ahead to distinguish Global Var vs Function
            # Type ID ...
            # If next is '(', it's function.
            # If next is '=' or ';', it's variable.
            
            # Save state to lookahead
            saved_pos = self.pos
            try:
                self.parse_type() # Type
                self.expect(TOKEN_ID) # Name
                if self.peek().type == TOKEN_LPAREN:
                    is_func = True
                else:
                    is_func = False
            except:
                # Should not happen in valid code, but handle gracefully
                is_func = False
                
            # Restore
            self.pos = saved_pos
            
            if is_func:
                functions.append(self.parse_function())
            else:
                globals.append(self.parse_var_decl())
                
        return Program(globals, functions)

    def parse_type(self):
        t = self.advance()
        if t.type not in (TOKEN_INT, TOKEN_VOID):
             raise Exception(f"Expected type, got {t.type} at line {t.line}")
        
        type_name = t.value
        # Handle pointers (int*)
        while self.match(TOKEN_STAR):
            type_name += '*'
        return type_name

    def parse_function(self):
        ret_type = self.parse_type()
        name = self.expect(TOKEN_ID).value
        self.expect(TOKEN_LPAREN)
        
        params = []
        if self.peek().type != TOKEN_RPAREN:
            while True:
                p_type = self.parse_type()
                p_name = self.expect(TOKEN_ID).value
                params.append((p_type, p_name))
                if not self.match(TOKEN_COMMA):
                    break
        
        self.expect(TOKEN_RPAREN)
        self.expect(TOKEN_LBRACE)
        body = self.parse_block()
        return FunctionDef(name, ret_type, params, body)

    def parse_block(self):
        statements = []
        while self.peek().type != TOKEN_RBRACE:
            statements.append(self.parse_statement())
        self.expect(TOKEN_RBRACE)
        return statements

    def parse_statement(self):
        t = self.peek()
        
        # Variable Declaration: Type ID ...
        if t.type in (TOKEN_INT, TOKEN_VOID):
            # Hack: Need to distinguish Decl from Expression (if implicit type)
            # But here declarations strictly start with type
            return self.parse_var_decl()
            
        if t.type == TOKEN_IF:
            return self.parse_if()
        
        if t.type == TOKEN_WHILE:
            return self.parse_while()
            
        if t.type == TOKEN_RETURN:
            self.advance()
            expr = None
            if self.peek().type != TOKEN_SEMI:
                expr = self.parse_expression()
            self.expect(TOKEN_SEMI)
            return ReturnStmt(expr)
            
        if t.type == TOKEN_ASM:
            self.advance()
            self.expect(TOKEN_LPAREN)
            asm_code = self.expect(TOKEN_STRING).value
            self.expect(TOKEN_RPAREN)
            self.expect(TOKEN_SEMI)
            return AsmStmt(asm_code)
            
        if t.type == TOKEN_LBRACE:
            self.advance()
            return self.parse_block() # Returns list of stmts, need to wrap?
            # Actually parse_block consumes RBRACE.
            # But my AST expects a list of statements for block. 
            # I should create a BlockStmt? For simplicity, flatten or keep as list?
            # Let's flatten later. For now parse_statement returns ONE node.
            # If it returns a list, the caller needs to handle it.
            # Let's avoid nested blocks for now or treat block as special.
            # Wait, `parse_block` returns `list`.
            # If I encounter `{`, I should recursively call parse_block? 
            # But FunctionDef uses parse_block.
            # Let's say: statements can be blocks.
            # But I don't have a BlockStmt node.
            # Let's skip standalone blocks support for now or implement BlockStmt.
            # Going with BlockStmt if needed.
            pass
            
        # Assignment or Expression
        # Try parse expr. If it's an assignment, `parse_expression` won't handle it 
        # because Assignment is not an expression in this simple C subset (usually).
        # Actually C allows assignment as expression.
        # Let's check for `ID =`
        if t.type == TOKEN_ID:
            # Look ahead for assignment
            # But wait, `*ptr = val` is also assignment.
            # `func()` is expression statement.
            pass
        
        # Simple Assignment: ID = Expr;
        if t.type == TOKEN_ID and self.tokens[self.pos+1].type == TOKEN_ASSIGN:
            name = self.advance().value
            self.advance() # =
            expr = self.parse_expression()
            self.expect(TOKEN_SEMI)
            return AssignStmt(name, expr)
            
        # Pointer Assignment: *Expr = Expr;
        if t.type == TOKEN_STAR:
             # This is tricky. `*ptr` is an expression. `*ptr = 10` is assignment.
             # We can parse unary expression. If next is `=`, it's assignment.
             # But parsing full expression might consume too much.
             # Let's assume ptr assignment starts explicitly with `*`.
             # Save pos
             start = self.pos
             self.advance() # *
             # Parse simple primary (ID or Parens)
             # This is a simplification. Real C parser is complex here.
             # Let's parse atomic expression for pointer.
             ptr_expr = self.parse_atomic() 
             if self.peek().type == TOKEN_ASSIGN:
                 self.advance()
                 val_expr = self.parse_expression()
                 self.expect(TOKEN_SEMI)
                 return PointerAssignStmt(ptr_expr, val_expr)
             else:
                 # Backtrack? Or just fail?
                 # It might be `*ptr;` (useless deref) or `*ptr + 1;`
                 # Reset pos and parse as expression statement
                 self.pos = start

        expr = self.parse_expression()
        self.expect(TOKEN_SEMI)
        return expr # Expression Statement (usually Function Call)

    def parse_var_decl(self):
        type = self.parse_type()
        name = self.expect(TOKEN_ID).value
        init = None
        if self.match(TOKEN_ASSIGN):
            init = self.parse_expression()
        self.expect(TOKEN_SEMI)
        return VarDecl(type, name, init)

    def parse_if(self):
        self.expect(TOKEN_IF)
        self.expect(TOKEN_LPAREN)
        cond = self.parse_expression()
        self.expect(TOKEN_RPAREN)
        
        # Expect block
        self.expect(TOKEN_LBRACE)
        then_block = self.parse_block()
        
        else_block = None
        if self.match(TOKEN_ELSE):
            if self.peek().type == TOKEN_IF:
                # else if -> treat as else { if ... }
                # Recursively parse if
                else_block = [self.parse_if()]
            else:
                self.expect(TOKEN_LBRACE)
                else_block = self.parse_block()
                
        return IfStmt(cond, then_block, else_block)

    def parse_while(self):
        self.expect(TOKEN_WHILE)
        self.expect(TOKEN_LPAREN)
        cond = self.parse_expression()
        self.expect(TOKEN_RPAREN)
        self.expect(TOKEN_LBRACE)
        body = self.parse_block()
        return WhileStmt(cond, body)

    def parse_expression(self):
        return self.parse_logical_or()

    def parse_logical_or(self):
        left = self.parse_logical_and()
        while self.peek().type == TOKEN_OR:
            op = self.advance().type
            right = self.parse_logical_and()
            left = BinOp(left, op, right)
        return left

    def parse_logical_and(self):
        left = self.parse_equality()
        while self.peek().type == TOKEN_AND:
            op = self.advance().type
            right = self.parse_equality()
            left = BinOp(left, op, right)
        return left

    def parse_equality(self):
        left = self.parse_relational()
        while self.peek().type in (TOKEN_EQ, TOKEN_NEQ):
            op = self.advance().type
            right = self.parse_relational()
            left = BinOp(left, op, right)
        return left

    def parse_relational(self):
        left = self.parse_additive()
        while self.peek().type in (TOKEN_GT, TOKEN_LT, TOKEN_GE, TOKEN_LE):
            op = self.advance().type
            right = self.parse_additive()
            left = BinOp(left, op, right)
        return left

    def parse_additive(self):
        left = self.parse_term()
        while self.peek().type in (TOKEN_PLUS, TOKEN_MINUS):
            op = self.advance().type
            right = self.parse_term()
            left = BinOp(left, op, right)
        return left

    def parse_term(self):
        left = self.parse_factor()
        while self.peek().type in (TOKEN_STAR, TOKEN_SLASH):
            # Ambiguity: * can be mul or pointer deref.
            # Here it is binary op (mul). Deref is handled in unary (factor).
            op = self.advance().type
            right = self.parse_factor()
            left = BinOp(left, op, right)
        return left

    def parse_factor(self):
        t = self.peek()
        if t.type == TOKEN_STAR:
            self.advance()
            expr = self.parse_factor() # Recursive for **ptr
            return UnaryOp(TOKEN_STAR, expr)
        # TODO: Handle AddressOf (&) if needed
        return self.parse_atomic()

    def parse_atomic(self):
        t = self.peek()
        if t.type == TOKEN_NUMBER:
            self.advance()
            return NumLiteral(t.value)
        if t.type == TOKEN_STRING:
            self.advance()
            return StringLiteral(t.value)
        if t.type == TOKEN_ID:
            self.advance()
            if self.peek().type == TOKEN_LPAREN:
                self.advance()
                args = []
                if self.peek().type != TOKEN_RPAREN:
                    while True:
                        args.append(self.parse_expression())
                        if not self.match(TOKEN_COMMA):
                            break
                self.expect(TOKEN_RPAREN)
                return FuncCall(t.value, args)
            return VarAccess(t.value)
        if t.type == TOKEN_LPAREN:
            self.advance()
            expr = self.parse_expression()
            self.expect(TOKEN_RPAREN)
            return expr
        
        raise Exception(f"Unexpected token in expression: {t}")

# Test
if __name__ == '__main__':
    code = """
    int main() {
        int x = 10;
        int* ptr = 0;
        *ptr = x;
        if (x > 5) {
            x = x + 1;
        }
        asm("HALT");
    }
    """
    lexer = Lexer(code)
    parser = Parser(lexer.tokenize())
    prog = parser.parse()
    print(prog)
