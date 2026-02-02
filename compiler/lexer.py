import re

# Token Types
TOKEN_INT = 'INT'           # int keyword
TOKEN_VOID = 'VOID'         # void keyword
TOKEN_IF = 'IF'
TOKEN_ELSE = 'ELSE'
TOKEN_WHILE = 'WHILE'
TOKEN_RETURN = 'RETURN'
TOKEN_ASM = 'ASM'           # asm keyword

TOKEN_ID = 'ID'             # Identifier
TOKEN_NUMBER = 'NUMBER'     # Integer literal
TOKEN_STRING = 'STRING'     # String literal

TOKEN_PLUS = 'PLUS'
TOKEN_MINUS = 'MINUS'
TOKEN_STAR = 'STAR'         # * (Multiply or Pointer)
TOKEN_SLASH = 'SLASH'
TOKEN_ASSIGN = 'ASSIGN'     # =
TOKEN_EQ = 'EQ'             # ==
TOKEN_NEQ = 'NEQ'           # !=
TOKEN_GT = 'GT'             # >
TOKEN_LT = 'LT'             # <
TOKEN_GE = 'GE'             # >=
TOKEN_LE = 'LE'             # <=
TOKEN_AND = 'AND'           # &&
TOKEN_OR = 'OR'             # ||

TOKEN_LPAREN = 'LPAREN'     # (
TOKEN_RPAREN = 'RPAREN'     # )
TOKEN_LBRACE = 'LBRACE'     # {
TOKEN_RBRACE = 'RBRACE'     # }
TOKEN_SEMI = 'SEMI'         # ;
TOKEN_COMMA = 'COMMA'       # ,

TOKEN_EOF = 'EOF'

# Regex patterns for tokens
KEYWORDS = {
    'int': TOKEN_INT,
    'void': TOKEN_VOID,
    'if': TOKEN_IF,
    'else': TOKEN_ELSE,
    'while': TOKEN_WHILE,
    'return': TOKEN_RETURN,
    'asm': TOKEN_ASM,
}

class Token:
    def __init__(self, type, value, line):
        self.type = type
        self.value = value
        self.line = line

    def __repr__(self):
        return f"Token({self.type}, {repr(self.value)}, Line:{self.line})"

class Lexer:
    def __init__(self, source):
        self.source = source
        self.pos = 0
        self.line = 1
        self.length = len(source)

    def peek(self):
        if self.pos < self.length:
            return self.source[self.pos]
        return None

    def advance(self):
        if self.pos < self.length:
            char = self.source[self.pos]
            self.pos += 1
            if char == '\n':
                self.line += 1
            return char
        return None

    def tokenize(self):
        tokens = []
        while self.pos < self.length:
            char = self.peek()

            # Skip whitespace
            if char.isspace():
                self.advance()
                continue

            # Skip comments
            if char == '/' and self.pos + 1 < self.length and self.source[self.pos + 1] == '/':
                while self.pos < self.length and self.peek() != '\n':
                    self.advance()
                continue

            # Identifiers and Keywords
            if char.isalpha() or char == '_':
                start = self.pos
                while self.pos < self.length and (self.peek().isalnum() or self.peek() == '_'):
                    self.advance()
                text = self.source[start:self.pos]
                type = KEYWORDS.get(text, TOKEN_ID)
                tokens.append(Token(type, text, self.line))
                continue

            # Numbers
            if char.isdigit():
                start = self.pos
                while self.pos < self.length and self.peek().isdigit():
                    self.advance()
                tokens.append(Token(TOKEN_NUMBER, int(self.source[start:self.pos]), self.line))
                continue

            # String Literals
            if char == '"':
                self.advance() # Skip opening "
                start = self.pos
                while self.pos < self.length and self.peek() != '"':
                    self.advance()
                text = self.source[start:self.pos]
                self.advance() # Skip closing "
                tokens.append(Token(TOKEN_STRING, text, self.line))
                continue

            # Symbols
            if char == '+':
                self.advance()
                tokens.append(Token(TOKEN_PLUS, '+', self.line))
                continue
            if char == '-':
                self.advance()
                tokens.append(Token(TOKEN_MINUS, '-', self.line))
                continue
            if char == '*':
                self.advance()
                tokens.append(Token(TOKEN_STAR, '*', self.line))
                continue
            if char == '/':
                self.advance()
                tokens.append(Token(TOKEN_SLASH, '/', self.line))
                continue
            if char == '=':
                self.advance()
                if self.peek() == '=':
                    self.advance()
                    tokens.append(Token(TOKEN_EQ, '==', self.line))
                else:
                    tokens.append(Token(TOKEN_ASSIGN, '=', self.line))
                continue
            if char == '!':
                self.advance()
                if self.peek() == '=':
                    self.advance()
                    tokens.append(Token(TOKEN_NEQ, '!=', self.line))
                else:
                    raise Exception(f"Unexpected char '!' at line {self.line}")
                continue
            if char == '>':
                self.advance()
                if self.peek() == '=':
                    self.advance()
                    tokens.append(Token(TOKEN_GE, '>=', self.line))
                else:
                    tokens.append(Token(TOKEN_GT, '>', self.line))
                continue
            if char == '<':
                self.advance()
                if self.peek() == '=':
                    self.advance()
                    tokens.append(Token(TOKEN_LE, '<=', self.line))
                else:
                    tokens.append(Token(TOKEN_LT, '<', self.line))
                continue
            if char == '&':
                self.advance()
                if self.peek() == '&':
                    self.advance()
                    tokens.append(Token(TOKEN_AND, '&&', self.line))
                else:
                    raise Exception(f"Unexpected char '&' at line {self.line}")
                continue
            if char == '|':
                self.advance()
                if self.peek() == '|':
                    self.advance()
                    tokens.append(Token(TOKEN_OR, '||', self.line))
                else:
                    raise Exception(f"Unexpected char '|' at line {self.line}")
                continue
            if char == '(':
                self.advance()
                tokens.append(Token(TOKEN_LPAREN, '(', self.line))
                continue
            if char == ')':
                self.advance()
                tokens.append(Token(TOKEN_RPAREN, ')', self.line))
                continue
            if char == '{':
                self.advance()
                tokens.append(Token(TOKEN_LBRACE, '{', self.line))
                continue
            if char == '}':
                self.advance()
                tokens.append(Token(TOKEN_RBRACE, '}', self.line))
                continue
            if char == ';':
                self.advance()
                tokens.append(Token(TOKEN_SEMI, ';', self.line))
                continue
            if char == ',':
                self.advance()
                tokens.append(Token(TOKEN_COMMA, ',', self.line))
                continue

            raise Exception(f"Unknown character '{char}' at line {self.line}")

        tokens.append(Token(TOKEN_EOF, '', self.line))
        return tokens

# Test
if __name__ == '__main__':
    code = """
    int main() {
        int x = 10;
        asm("MOV R1, R0");
        if (x > 5) {
            x = x + 1;
        }
    }
    """
    lexer = Lexer(code)
    for t in lexer.tokenize():
        print(t)
