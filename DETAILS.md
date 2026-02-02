# Project Implementation Details (C Version)

This document details the internal structure, classes, and interactions within the CPU Virtualization project (C Version).

## Components and Data Structures

### 1. `Instruction` Struct
**File:** `src/instructions.h`

A helper structure used to represent a decoded instruction internally before execution.
- **`opcode`** (`uint8_t`): The operation code (e.g., `OP_ADD`, `OP_LOAD`).
- **`arg1`** (`int`): First operand (usually a register index). `NO_ARG` (-1) if unused.
- **`arg2`** (`int`): Second operand (register index, immediate value, or address). `NO_ARG` (-1) if unused.
- **`arg3`** (`int`): Third operand (unused in current ISA but reserved for future expansion). `NO_ARG` (-1) if unused.

### 2. `Cache` Struct
**Files:** `src/cache.h`, `src/cache.c`

Manages memory access, providing a layer of buffering. The struct implements the `MemoryAccessor` interface, allowing caches to be chained (e.g., L1 backing to L2).

**Key Features:**
- **Configurable**: Support for variable Cache Size, Block Size, and Associativity via `CacheConfig`.
- **Set Associative**: Maps addresses to sets, using tags to identify blocks.
- **LRU Replacement**: Uses access timestamps to evict the Least Recently Used block in a set upon conflict.
- **Write Policy**: Write-Allocate and Write-Back (Dirty bits are used).

**Key Functions:**
- **`Cache_read(addr)`**: Checks cache for data. On miss, requests block from the `nextLevel` MemoryAccessor.
- **`Cache_write(addr, value)`**: Writes data to cache. On miss, requests block from `nextLevel` (Write Allocate), then writes.
- **`Cache_readBlock(addr, buffer, size)`**: Optimizes block fetches by iterating through cache lines efficiently.
- **`Cache_writeBlock(addr, buffer, size)`**: Efficiently writes a block of data, updating cache lines and dirty bits as needed.
- **`Cache_flushLine(line, index)`**: Writes a dirty block back to the `nextLevel`.
- **`Cache_printStats()`**: Displays hit/miss counts and rates.

### 3. `CPU` Struct
**Files:** `src/cpu.h`, `src/cpu.c`

The core struct representing the virtual machine. It manages the full memory hierarchy and execution loop.

**Memory Map:**
- `0x0000 - 0x00FF`: **IVT (Interrupt Vector Table)**.
- `0x0100 - 0xEFFF`: **User/Kernel Program Space**.
- `0xF000 - 0xF0FF`: **Memory Mapped I/O**.
- `0xFF00 - 0xFFFF`: **Stack**.

**Key State:**
- **`l1i`, `l1d`**: Level 1 Instruction and Data Caches (Instances of `Cache`).
- **`l2`**: Level 2 Unified Cache (Instance of `Cache`), backing L1.
- **`mainMemory`**: Instance of `MainMemory` (wraps the byte array), backing L2.
- **`registers[REG_COUNT]`**: Array of 16-bit registers.
    -   `registers[0]` is reserved/unused.
    -   `registers[1..8]` map to R0..R7.
    -   `registers[9]` is IP, `registers[10]` is SP.
- **`flags`**: 16-bit Status Register.
    -   **Bit 0 (ZF)**: Zero Flag (set by arithmetic operations).
    -   **Bit 1 (IF)**: Interrupt Flag (controls interrupt handling).
    -   **Bit 2 (NF)**: Negative Flag (set by arithmetic operations if result MSB is 1).
- **`halted`**: Boolean flag controlling the execution loop.
- **`timer_counter`**: Decrementing counter for hardware timer.
- **`timer_interrupt_pending`**: Boolean flag raised when timer hits zero.

**Key Functions:**
- **`CPU_loadProgram(program, startAddr)`**: Copies a raw byte sequence directly into the simulated memory (bypassing cache for initialization).
- **`CPU_run()`**: Starts the fetch-decode-execute loop until `halted` is true.
- **`CPU_step()`**: Executes a single instruction cycle.
- **`CPU_fetchDecode()`**: Fetches raw bytes from memory at the instruction pointer (IP) and decodes them into an `Instruction` struct.
- **`CPU_execute(instr)`**: Takes an `Instruction` struct and performs the corresponding operation (updating registers, memory, or flags).

### 4. Assembler Helper
**Files:** `src/assembler.h`, `src/assembler.c`

- **`Assembler_assemble(instructions)`**: Takes a list of `Instruction` structs and serializes them into a byte array of machine code (respecting variable instruction lengths and little-endian byte order).
- **`Assembler_loadProgramFromFile(filename)`**: Reads an assembly file and produces a list of `Instruction` structs. Supports parsing of registers (including `IP`, `SP`) and labels.

## Cache Architecture & Terminology

This section details how memory addresses are interpreted and mapped to the cache structure.

### Address Decomposition

The 16-bit memory address is split into three parts to locate data within the cache:

```
+----------------+----------------+----------------+
|      Tag       |     Index      |     Offset     |
+----------------+----------------+----------------+
 <--- tagBits --> <-- indexBits -> <-- offsetBits ->
```

*   **Offset**: The lower bits of the address. It identifies the specific byte within a **Cache Block**.
    *   Size = $log_2(\text{BlockSize})$
*   **Index**: The middle bits. It selects the specific **Set** in the cache where the data might be stored.
    *   Size = $log_2(\text{Number of Sets})$
    *   $\text{Number of Sets} = \frac{\text{CacheSize}}{\text{BlockSize} \times \text{Associativity}}$
*   **Tag**: The upper bits. It is stored in the cache line and compared against the requested address's tag to verify if the block matches (Hit vs Miss).
    *   Size = $16 - (\text{IndexBits} + \text{OffsetBits})$

### Definitions

*   **Block (Cache Block)**: The smallest unit of data transferred between main memory and cache. If one byte is requested, the entire block containing that byte is loaded.
*   **Cache Line**: A container within the cache that holds a **Block** along with metadata.
    *   **Structure**: `[ Valid Bit | Dirty Bit | Tag | Data Block ]`
*   **Set**: A collection of Cache Lines. In an N-way set-associative cache, each set contains N lines. An address can be mapped to any line within its designated set.
*   **Way**: A specific line within a Set.
*   **Block Address**: The address of the block in memory, effectively `Address >> OffsetBits`. It ignores the specific byte offset.
*   **Base Address**: The starting address of a block, effectively `BlockAddress << OffsetBits` or `Address & ~(BlockSize - 1)`.

### Mapping Process

1.  **Select Set**: Use the **Index** bits to find the correct `Set` in the `sets` array.
2.  **Tag Comparison**: Iterate through all `Ways` (Lines) in that `Set`.
    *   Compare the line's stored **Tag** with the address's **Tag**.
    *   Check if the **Valid Bit** is true.
3.  **Hit**: If Tag matches and Valid is true:
    *   Read/Write data at `Data[Offset]`.
    *   Update LRU timestamp.
4.  **Miss**: If no match is found:
    *   **Eviction**: Select the Least Recently Used (LRU) line in the set.
    *   **Write-Back**: If the victim line is **Dirty**, write it back to the next level of memory.
    *   **Allocation**: Read the new block from the next level using the **Base Address**.
    *   **Update**: Store the new Tag, set Valid to true, clear Dirty (unless it's a Write-Allocate operation).

## Execution Flow

1.  **Initialization**:
    -   `main()` instantiates the `CPU` via `CPU_create()`.
    -   The `CPU` struct initializes registers to 0, SP to `0xFFFF`, and clears memory.

2.  **Program Creation**:
    -   The program is loaded from a file into an `InstructionList` via `Assembler_loadProgramFromFile`.
    -   `Assembler_assemble()` converts this structured list into a raw byte stream (`Program`).

3.  **Loading**:
    -   `CPU_loadProgram()` writes the byte stream into the `CPU`'s `mainMemory` starting at address `0x0100` (User/Kernel Space).
    -   The Instruction Pointer (`IP`) is set to the start address.

4.  **Execution Loop** (`CPU_run`):
    -   Calls `CPU_step()`.
    -   **Fetch/Decode Phase**:
        -   `CPU_fetchDecode()` reads the opcode at `memory[IP]`.
        -   Based on the opcode, it fetches additional operand bytes (register indices, immediate values) from `memory`.
        -   It constructs and returns an `Instruction` struct.
        -   The `IP` is advanced accordingly.
    -   **Execute Phase**:
        -   `CPU_execute()` receives the `Instruction`.
        -   A switch statement routes logic based on `instr.opcode`.
        -   Operations are performed (e.g., adding register values, writing to memory, pushing to stack).
    -   **Timer Update**:
        -   `timer_counter` is decremented.
        -   If it reaches zero, `timer_interrupt_pending` is set to true.
    -   The loop continues until a `HALT` instruction sets `halted` to true.
