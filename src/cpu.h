#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdbool.h>
#include "instructions.h"
#include "cache.h"
#include "main_memory.h"

// Memory size: 64KB
#define MEMORY_SIZE 65536

typedef struct CPU {
    uint16_t registers[REG_COUNT];
    
    // Memory Hierarchy
    MainMemory* mainMemory;
    Cache* l2;
    Cache* l1i;
    Cache* l1d;

    uint16_t flags; // Flags register (ZF, IF, etc.)
    bool halted;

    // Hardware Timer
    uint32_t timer_counter;
    uint32_t timer_limit;
    bool timer_interrupt_pending;
} CPU;

// Config for L1 (Instruction & Data) and L2
CPU* CPU_create(CacheConfig l1iConfig, CacheConfig l1dConfig, CacheConfig l2Config, uint32_t timer_limit);
void CPU_destroy(CPU* self);

// Core execution loop
void CPU_run(CPU* self);

// Step execution (fetch-decode-execute one instruction)
bool CPU_step(CPU* self);

Instruction CPU_fetchDecode(CPU* self);
void CPU_execute(CPU* self, Instruction instr);

// Memory access
void CPU_loadProgram(CPU* self, const uint8_t* program, size_t programSize, uint16_t startAddr);

// Data Cache Access
uint8_t CPU_readByte(CPU* self, uint16_t addr);
void CPU_writeByte(CPU* self, uint16_t addr, uint8_t value);
uint16_t CPU_readWord(CPU* self, uint16_t addr);
void CPU_writeWord(CPU* self, uint16_t addr, uint16_t value);

// Instruction Cache Access
uint8_t CPU_fetchInstructionByte(CPU* self);
uint16_t CPU_fetchInstructionWord(CPU* self);

// Register access
uint16_t CPU_getRegister(const CPU* self, uint8_t reg);
void CPU_setRegister(CPU* self, uint8_t reg, uint16_t value);
void CPU_printState(const CPU* self, bool verboseCache);

#endif // CPU_H
