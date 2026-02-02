# C-Subset Compiler

This directory contains a custom compiler that translates a subset of C language into assembly code for the virtual CPU project.

## Overview

The compiler is built in Python and follows a standard architecture:
1. **Lexer** (`lexer.py`): Tokenizes source code.
2. **Parser** (`parser.py`): Generates an Abstract Syntax Tree (AST) using recursive descent.
3. **Code Generator** (`codegen.py`): Traverses the AST and emits assembly instructions.

## Usage

To compile a `.lang` file to `.asm`:

```bash
python3 compiler/main.py <input.lang> <output.asm>
```

**Example:**
```bash
python3 compiler/main.py test.lang test.asm
```

## Language Specification

### Supported Types
- `int`: Signed integers (16-bit).
- `void`: For functions not returning values.
- `int*`: Pointers to integers.

### Control Structures
- `if (condition) { ... } else { ... }`
- `while (condition) { ... }`
- `return value;`

### Functions
- Function definition and calling supported.
- Arguments passed on stack (Right-to-Left push order).
- Return values passed in `R0`.
- Caller cleans up arguments.

### Global Variables
- Global variables can be declared at the top level.
- Initializers must be constant expressions (currently).
- Stored in a fixed data area starting at `0x4000`.

### Inline Assembly
Directly insert assembly instructions using the `asm` keyword:
```c
asm("MOV R0, R1");
```

### Operators
- **Arithmetic**: `+`, `-`
- **Comparison**: `==`, `!=`, `<`, `>`, `<=`, `>=`
- **Logical**: `&&`, `||`
- **Assignment**: `=`
- **Memory**: `*` (Dereference for reading/writing pointers)

### Example Code

```c
int global_val = 100;

int add(int a, int b) {
    return a + b;
}

int main() {
    int x = 10;
    int y = 20;
    
    if (x < y) {
        global_val = add(x, y);
    }
    
    asm("HALT");
}
```

## Current Status & Limitations

### Implemented
- **Variables**: Local (stack-based) and Global (fixed address).
- **Arithmetic**: `+`, `-`.
- **Comparisons**: `==`, `!=`, `<`, `>`, `<=`, `>=`.
- **Logical**: `&&`, `||`.
- **Control Flow**: `if/else`, `while`, Function Calls.
- **Pointers**: Basic pointer arithmetic and dereferencing.
- **Inline Assembly**.

### Pending / Limitations
- **Multiplication**: The `*` operator for multiplication is not supported (CPU lacks `MUL`).
- **Complex Expressions**: Operator precedence is basic.
- **Types**: Only `int` and `int*`. No structs or arrays yet.
