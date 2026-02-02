#include "cpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper to fetch next byte/word and advance IP
static uint8_t fetchByte(CPU* self);
static uint16_t fetchWord(CPU* self);

// Stack operations
static void push(CPU* self, uint16_t value);
static uint16_t pop(CPU* self);

CPU* CPU_create(CacheConfig l1iConfig, CacheConfig l1dConfig, CacheConfig l2Config, uint32_t timer_limit) {
    CPU* self = (CPU*)malloc(sizeof(CPU));
    if (!self) return NULL;

    self->mainMemory = MainMemory_create(MEMORY_SIZE);
    
    // l2->nextLevel = &mainMemory
    self->l2 = Cache_create(l2Config, (MemoryAccessor*)self->mainMemory);
    
    // l1i->nextLevel = &l2
    self->l1i = Cache_create(l1iConfig, (MemoryAccessor*)self->l2);
    
    // l1d->nextLevel = &l2
    self->l1d = Cache_create(l1dConfig, (MemoryAccessor*)self->l2);
    
    self->registers[IP] = 0;
    self->flags = 0;
    self->halted = false;
    
    // Initialize Timer
    self->timer_limit = timer_limit;
    self->timer_counter = self->timer_limit;
    self->timer_interrupt_pending = false;

    // Initialize registers
    for (int i = 0; i < REG_COUNT; ++i) {
        self->registers[i] = 0;
    }
    self->registers[SP] = 0xFFFF; // Initialize Stack Pointer to end of memory

    return self;
}

void CPU_destroy(CPU* self) {
    Cache_destroy((MemoryAccessor*)self->l1d);
    Cache_destroy((MemoryAccessor*)self->l1i);
    Cache_destroy((MemoryAccessor*)self->l2);
    MainMemory_destroy((MemoryAccessor*)self->mainMemory);
    free(self);
}

void CPU_loadProgram(CPU* self, const uint8_t* program, size_t programSize, uint16_t startAddr) {
    if (startAddr + programSize > MEMORY_SIZE) {
        fprintf(stderr, "Error: Program too large for memory!\n");
        return;
    }
    
    // Write directly to main memory
    MemoryAccessor_writeBlock((MemoryAccessor*)self->mainMemory, startAddr, program, programSize);
    self->registers[IP] = startAddr; // Set Instruction Pointer to start
}

// Data Access
uint8_t CPU_readByte(CPU* self, uint16_t addr) {
    return MemoryAccessor_read((MemoryAccessor*)self->l1d, addr);
}

void CPU_writeByte(CPU* self, uint16_t addr, uint8_t value) {
    MemoryAccessor_write((MemoryAccessor*)self->l1d, addr, value);
}

uint16_t CPU_readWord(CPU* self, uint16_t addr) {
    // Little-endian
    uint16_t val = MemoryAccessor_read((MemoryAccessor*)self->l1d, addr);
    val |= (uint16_t)MemoryAccessor_read((MemoryAccessor*)self->l1d, addr + 1) << 8;
    return val;
}

void CPU_writeWord(CPU* self, uint16_t addr, uint16_t value) {
    // Little-endian
    MemoryAccessor_write((MemoryAccessor*)self->l1d, addr, value & 0xFF);
    MemoryAccessor_write((MemoryAccessor*)self->l1d, addr + 1, (value >> 8) & 0xFF);
}

// Instruction Access
uint8_t CPU_fetchInstructionByte(CPU* self) {
    uint16_t ip = self->registers[IP];
    uint8_t val = MemoryAccessor_read((MemoryAccessor*)self->l1i, ip);
    self->registers[IP]++;
    return val;
}

uint16_t CPU_fetchInstructionWord(CPU* self) {
    uint16_t ip = self->registers[IP];
    uint16_t val = MemoryAccessor_read((MemoryAccessor*)self->l1i, ip);
    val |= (uint16_t)MemoryAccessor_read((MemoryAccessor*)self->l1i, ip + 1) << 8;
    self->registers[IP] += 2;
    return val;
}

// Helpers that use Instruction Access
static uint8_t fetchByte(CPU* self) {
    return CPU_fetchInstructionByte(self);
}

static uint16_t fetchWord(CPU* self) {
    return CPU_fetchInstructionWord(self);
}

uint16_t CPU_getRegister(const CPU* self, uint8_t reg) {
    if (reg < REG_COUNT) {
        return self->registers[reg];
    }
    return 0;
}

void CPU_setRegister(CPU* self, uint8_t reg, uint16_t value) {
    if (reg < REG_COUNT) {
        self->registers[reg] = value;
    }
}

static void push(CPU* self, uint16_t value) {
    uint16_t sp = self->registers[SP];
    sp -= 2; // Decrement stack pointer (stack grows down)
    CPU_writeWord(self, sp, value); // Uses L1D
    self->registers[SP] = sp;
}

static uint16_t pop(CPU* self) {
    uint16_t sp = self->registers[SP];
    uint16_t val = CPU_readWord(self, sp); // Uses L1D
    sp += 2; // Increment stack pointer
    self->registers[SP] = sp;
    return val;
}

void CPU_printState(const CPU* self, bool verboseCache) {
    printf("--- CPU State ---\n");
    printf("IP: 0x%04X  SP: 0x%04X  FLAGS: 0x%04X\n", self->registers[IP], self->registers[SP], self->flags);
    printf("R0: 0x%04X  R1: 0x%04X  R2: 0x%04X  R3: 0x%04X\n", 
           self->registers[R0], self->registers[R1], self->registers[R2], self->registers[R3]);
    printf("R4: 0x%04X  R5: 0x%04X  R6: 0x%04X  R7: 0x%04X\n", 
           self->registers[R4], self->registers[R5], self->registers[R6], self->registers[R7]);
    printf("Timer: %u (Pending: %s)\n", self->timer_counter, self->timer_interrupt_pending ? "Yes" : "No");
    printf("-----------------\n");
    
    if (verboseCache) {
        printf("--- L1 Instruction Cache ---\n");
        Cache_printStats(self->l1i);
        printf("--- L1 Data Cache ---\n");
        Cache_printStats(self->l1d);
        printf("--- L2 Unified Cache ---\n");
        Cache_printStats(self->l2);
        printf("----------------------------\n");
    }
}

bool CPU_step(CPU* self) {
    if (self->halted) return false;

    // Timer Logic
    if (self->timer_counter > 0) {
        self->timer_counter--;
    } else {
        self->timer_interrupt_pending = true;
        self->timer_counter = self->timer_limit; // Reset timer
    }

    // Interrupt Handling
    if (self->timer_interrupt_pending) {
        self->timer_interrupt_pending = false;
        
        // Save Context: Push Flags then IP
        push(self, self->flags);
        push(self, self->registers[IP]);
        
        // Lookup Interrupt Vector (Assume IVT at 0x0000)
        // For Timer Interrupt, let's say it uses the vector at 0x0000
        uint16_t handlerAddress = CPU_readWord(self, 0x0000);
        
        // Jump to Handler
        self->registers[IP] = handlerAddress;
        
        // We do NOT fetch/execute an instruction in this step, 
        // we just performed the hardware context switch.
        return true; 
    }

    Instruction instr = CPU_fetchDecode(self);
    CPU_execute(self, instr);
    
    return !self->halted;
}

Instruction CPU_fetchDecode(CPU* self) {
    Instruction instr;
    instr.opcode = fetchByte(self); // Uses fetchInstructionByte -> L1I
    
    // Default initialization
    instr.arg1 = NO_ARG;
    instr.arg2 = NO_ARG;
    instr.arg3 = NO_ARG;

    switch (instr.opcode) {
        case OP_LOAD: // LOAD R, Imm
            instr.arg1 = fetchByte(self);
            instr.arg2 = fetchWord(self);
            break;
        case OP_ADD:  // ADD R1, R2
        case OP_SUB:  // SUB R1, R2
        case OP_MOV:  // MOV R1, R2
        case OP_LDI:  // LDI R, R
        case OP_STI:  // STI R, R
            instr.arg1 = fetchByte(self);
            instr.arg2 = fetchByte(self);
            break;
        case OP_STORE: // STORE R, Addr
        case OP_LD:    // LD R, Addr
            instr.arg1 = fetchByte(self);
            instr.arg2 = fetchWord(self);
            break;
        case OP_PUSH:  // PUSH R
        case OP_POP:   // POP R
        case OP_PRINT: // PRINT R
            instr.arg1 = fetchByte(self);
            break;
        case OP_JMP:   // JMP Addr
        case OP_JZ:    // JZ Addr
        case OP_JN:    // JN Addr
            instr.arg1 = fetchWord(self);
            break;
        case OP_IRET:
        case OP_HALT:
            break;
        default:
            // For unknown opcodes, we just return the opcode, execution will handle error
            break;
    }
    return instr;
}

void CPU_execute(CPU* self, Instruction instr) {
    switch (instr.opcode) {
        case OP_LOAD: {
            CPU_setRegister(self, (uint8_t)instr.arg1, instr.arg2);
            break;
        }
        case OP_ADD: {
            uint16_t val1 = CPU_getRegister(self, (uint8_t)instr.arg1);
            uint16_t val2 = CPU_getRegister(self, (uint8_t)instr.arg2);
            uint16_t result = val1 + val2;
            CPU_setRegister(self, (uint8_t)instr.arg1, result);
            
            if (result == 0) self->flags |= FLAG_ZF;
            else self->flags &= ~FLAG_ZF;
            
            if (result & 0x8000) self->flags |= FLAG_NF;
            else self->flags &= ~FLAG_NF;
            break;
        }
        case OP_SUB: {
            uint16_t val1 = CPU_getRegister(self, (uint8_t)instr.arg1);
            uint16_t val2 = CPU_getRegister(self, (uint8_t)instr.arg2);
            uint16_t result = val1 - val2;
            CPU_setRegister(self, (uint8_t)instr.arg1, result);
            
            if (result == 0) self->flags |= FLAG_ZF;
            else self->flags &= ~FLAG_ZF;
            
            if (result & 0x8000) self->flags |= FLAG_NF;
            else self->flags &= ~FLAG_NF;
            break;
        }
        case OP_MOV: {
            CPU_setRegister(self, (uint8_t)instr.arg1, CPU_getRegister(self, (uint8_t)instr.arg2));
            break;
        }
        case OP_STORE: {
            CPU_writeWord(self, instr.arg2, CPU_getRegister(self, (uint8_t)instr.arg1));
            break;
        }
        case OP_LD: {
            CPU_setRegister(self, (uint8_t)instr.arg1, CPU_readWord(self, instr.arg2));
            break;
        }
        case OP_PUSH: {
            push(self, CPU_getRegister(self, (uint8_t)instr.arg1));
            break;
        }
        case OP_POP: {
            CPU_setRegister(self, (uint8_t)instr.arg1, pop(self));
            break;
        }
        case OP_JMP: {
            self->registers[IP] = instr.arg1;
            break;
        }
        case OP_JZ: {
            if (self->flags & FLAG_ZF) {
                self->registers[IP] = instr.arg1;
            }
            break;
        }
        case OP_JN: {
            if (self->flags & FLAG_NF) {
                self->registers[IP] = instr.arg1;
            }
            break;
        }
        case OP_IRET: {
            // Restore state: Pop IP then Flags (Reverse of interrupt sequence)
            self->registers[IP] = pop(self);
            self->flags = pop(self);
            break;
        }
        case OP_LDI: {
            uint16_t addr = CPU_getRegister(self, (uint8_t)instr.arg2);
            uint16_t val = CPU_readWord(self, addr);
            CPU_setRegister(self, (uint8_t)instr.arg1, val);
            break;
        }
        case OP_STI: {
            uint16_t val = CPU_getRegister(self, (uint8_t)instr.arg1);
            uint16_t addr = CPU_getRegister(self, (uint8_t)instr.arg2);
            CPU_writeWord(self, addr, val);
            break;
        }
        case OP_PRINT: {
            printf("OUT: %u\n", CPU_getRegister(self, (uint8_t)instr.arg1));
            break;
        }
        case OP_HALT: {
            self->halted = true;
            printf("CPU Halted.\n");
            break;
        }
        default:
            fprintf(stderr, "Unknown Opcode: 0x%X\n", (int)instr.opcode);
            self->halted = true;
            break;
    }
}

void CPU_run(CPU* self) {
    while (!self->halted) {
        if (!CPU_step(self)) break;
    }
}
