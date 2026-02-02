#ifndef INSTRUCTIONS_H
#define INSTRUCTIONS_H

#include <stdint.h>

// Instruction Opcodes
typedef enum {
    OP_LOAD   = 0x01, // Load immediate: LOAD R, Imm
    OP_ADD    = 0x02, // Add registers: ADD R1, R2
    OP_SUB    = 0x03, // Subtract registers: SUB R1, R2
    OP_MOV    = 0x04, // Move register: MOV R1, R2
    OP_STORE  = 0x05, // Store to memory: STORE R, Addr
    OP_LD     = 0x06, // Load from memory: LD R, Addr
    OP_PUSH   = 0x07, // Push to stack: PUSH R
    OP_POP    = 0x08, // Pop from stack: POP R
    OP_JMP    = 0x09, // Jump absolute: JMP Addr
    OP_JZ     = 0x0A, // Jump if zero: JZ Addr
    OP_IRET   = 0x0B, // Return from Interrupt
    OP_LDI    = 0x0C, // Load Indirect: LDI R_dest, R_addr
    OP_STI    = 0x0D, // Store Indirect: STI R_src, R_addr
    OP_JN     = 0x0E, // Jump if negative: JN Addr
    OP_PRINT  = 0xEE, // Debug print: PRINT R
    OP_HALT   = 0xFF  // Halt execution
} Opcode;

// Register Identifiers
typedef enum {
    REG_INVALID = 0, // 0 is reserved for unused/invalid
    R0 = 1,
    R1 = 2,
    R2 = 3,
    R3 = 4,
    R4 = 5,
    R5 = 6,
    R6 = 7,
    R7 = 8,
    IP = 9,  // Instruction Pointer
    SP = 10, // Stack Pointer
    REG_COUNT = 11
} Register;

// FLAGS Register Bits
#define FLAG_ZF (1 << 0) // Zero Flag
#define FLAG_IF (1 << 1) // Interrupt Flag
#define FLAG_NF (1 << 2) // Negative Flag

#define NO_ARG -1

typedef struct {
    uint8_t opcode;
    int arg1;
    int arg2;
    int arg3;
} Instruction;

#endif // INSTRUCTIONS_H
