import sys
from lexer import Lexer
from parser import Parser
from codegen import CodeGen

def compile_file(input_file, output_file):
    try:
        with open(input_file, 'r') as f:
            code = f.read()
            
        print(f"Compiling {input_file}...")
        
        # Lexing
        lexer = Lexer(code)
        tokens = lexer.tokenize()
        
        # Parsing
        parser = Parser(tokens)
        ast = parser.parse()
        
        # Code Generation
        cg = CodeGen()
        asm = cg.generate_program(ast)
        
        with open(output_file, 'w') as f:
            f.write(asm)
            
        print(f"Successfully compiled to {output_file}")
        
    except Exception as e:
        print(f"Compilation Failed: {e}")
        sys.exit(1)

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: python3 compiler/main.py <input.lang> <output.asm>")
        sys.exit(1)
        
    compile_file(sys.argv[1], sys.argv[2])
