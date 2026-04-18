# Simple CPU Virtualization Project

## Overview
This project implements a simple virtual CPU emulator in C. It simulates a basic Register-based architecture with a custom instruction set. This allows running binary programs in a controlled virtual environment, demonstrating the basics of CPU virtualization.
Note: AI code generation is used in the development of this project.

The project also includes a **Compiler** that supports a subset of the C language, allowing you to write high-level code and compile it to the CPU's assembly language.

## Architecture Specification

### 1. Memory System
- **Address Space**: 64KB (65536 bytes) linear address space.
- **Address Bus**: 16-bit addressing ($2^{16} = 65536$ locations).
- **Data Access**: Access mediated by a Multi-level Cache Hierarchy. Underlying memory is byte-addressable (`uint8_t memory[]`).
- **Endianness**: **Little-Endian**. Multi-byte values (16-bit words) are stored with the least significant byte at the lower address.

#### Memory Map
The memory is organized as follows:
- `0x0000 - 0x00FF`: **IVT (Interrupt Vector Table)**. 256 bytes (128 vectors).
- `0x0100 - 0xEFFF`: **User/Kernel Program Space**. Code and Data.
- `0xF000 - 0xF0FF`: **Memory Mapped I/O**. Reserved for Display/Keyboard buffers.
- `0xFF00 - 0xFFFF`: **Stack**. Grows downwards from `0xFFFF`.

### 2. Cache System
The CPU implements a 2-Level Cache Hierarchy:
1.  **L1 Cache**: Split into **Instruction Cache (L1i)** and **Data Cache (L1d)**. Faster, smaller, connected directly to CPU.
2.  **L2 Cache**: Unified (Instruction + Data). Larger, slower, backs the L1 caches.
3.  **Main Memory**: The final backing store.

### 3. Registers
The CPU features a set of 16-bit registers.
- **Index 0**: Reserved/Invalid.
- **General Purpose**:
  - **R0 (1) - R7 (8)**: Used for arithmetic, logic, and temporary storage.
- **Special Purpose**:
  - **IP (Instruction Pointer)**: 16-bit register holding the address of the next instruction to execute.
  - **SP (Stack Pointer)**: 16-bit register pointing to the top of the stack. Initialized to `0xFFFF` (End of Memory).
  - **FLAGS**: 16-bit internal status register.
    - **Bit 0 (ZF)**: Zero Flag. Set if the result of an arithmetic operation is zero.
    - **Bit 1 (IF)**: Interrupt Flag. Used to enable/disable interrupts.
    - **Bit 2 (NF)**: Negative Flag. Set if the result of an arithmetic operation is negative (MSB set).

### 4. Hardware Timer
The CPU includes a simulated hardware timer.
- **Counter**: Decrements on every CPU cycle (instruction step).
- **Interrupt**: When the counter reaches 0, it resets and raises a pending interrupt flag.

### 5. Instruction Set Architecture (ISA)

#### Opcode Table

| Opcode | Mnemonic | Operands        | Format (Bytes) | Description |
|:-------|:---------|:----------------|:---------------|:------------|
| 0x01   | LOAD     | R, Imm (16-bit) | `[Op] [R] [L] [H]` | Load 16-bit immediate value into Register R |
| 0x02   | ADD      | R1, R2          | `[Op] [R1] [R2]`   | Add R2 to R1, result in R1. Updates ZF, NF. |
| 0x03   | SUB      | R1, R2          | `[Op] [R1] [R2]`   | Subtract R2 from R1, result in R1. Updates ZF, NF. |
| 0x04   | MOV      | R1, R2          | `[Op] [R1] [R2]`   | Move value from R2 to R1 |
| 0x05   | STORE    | R, Addr (16-bit)| `[Op] [R] [L] [H]` | Store value of R into Memory Address |
| 0x06   | LD       | R, Addr (16-bit)| `[Op] [R] [L] [H]` | Load value from Memory Address into R |
| 0x07   | PUSH     | R               | `[Op] [R]`         | Push R onto stack |
| 0x08   | POP      | R               | `[Op] [R]`         | Pop stack into R |
| 0x09   | JMP      | Addr (16-bit)   | `[Op] [L] [H]`     | Unconditional Jump to Address |
| 0x0A   | JZ       | Addr (16-bit)   | `[Op] [L] [H]`     | Jump to Address if Zero Flag is set |
| 0x0B   | IRET     | -               | `[Op]`             | Return from Interrupt |
| 0x0C   | LDI      | R_dest, R_addr  | `[Op] [Rd] [Ra]`   | Load Indirect: Rd = MEM[Ra] |
| 0x0D   | STI      | R_src, R_addr   | `[Op] [Rs] [Ra]`   | Store Indirect: MEM[Ra] = Rs |
| 0x0E   | JN       | Addr (16-bit)   | `[Op] [L] [H]`     | Jump to Address if Negative Flag is set |
| 0xEE   | PRINT    | R               | `[Op] [R]`         | Print value of R (System Call / Debug) |
| 0xFF   | HALT     | -               | `[Op]`             | Stop execution |

## Compiler

The project includes a compiler (`compiler/`) that translates `.lang` files (C-subset) into `.asm` files.

**Features:**
- Variables (Local & Global)
- Arithmetic (`+`, `-`)
- Comparisons (`<`, `>`, `==`)
- Control Flow (`if`, `while`, Function Calls)
- Pointers

**Usage:**
```bash
python3 compiler/main.py <input.lang> <output.asm>
```

See `compiler/README.md` for full details.

## File Structure

```
cpu_c/
├── Makefile              # Build script
├── README.md             # Project documentation
├── DETAILS.md            # Implementation details
├── bin/                  # Compiled executable
├── obj/                  # Intermediate object files
├── compiler/             # Compiler source code
│   ├── main.py
│   ├── lexer.py
│   ├── parser.py
│   ├── codegen.py
│   └── README.md
├── examples/             # Example programs
│   └── lang/             # High-level language examples
└── src/                  # CPU Emulator source code
    ├── main.c
    ├── cpu.h / cpu.c
    ├── cache.h / cache.c
    ├── instructions.h
    ├── assembler.h / assembler.c
    └── ...
```

## Building and Running

### Prerequisites
- GCC Compiler
- Make
- Python 3 (for Compiler)

### Build
To compile the emulator:
```bash
make
```

### Run
To execute the emulator:
```bash
./bin/cpu_emu [program_file] [options]
```

**Example:**
1. Compile a high-level program:
   ```bash
   python3 compiler/main.py examples/lang/factorial.lang factorial.asm
   ```
2. Run it on the emulator:
   ```bash
   ./bin/cpu_emu factorial.asm
   ```

### Clean
To remove build artifacts:
```bash
make clean
