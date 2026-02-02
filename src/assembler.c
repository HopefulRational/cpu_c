#include "assembler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Helper helpers
static void emitByte(Program* program, uint8_t val) {
    if (program->size >= program->capacity) {
        program->capacity = program->capacity == 0 ? 64 : program->capacity * 2;
        program->data = (uint8_t*)realloc(program->data, program->capacity);
    }
    program->data[program->size++] = val;
}

static void emitWord(Program* program, uint16_t val) {
    emitByte(program, val & 0xFF);
    emitByte(program, (val >> 8) & 0xFF);
}

Program* Assembler_assemble(const InstructionList* instructions) {
    Program* program = (Program*)malloc(sizeof(Program));
    program->size = 0;
    program->capacity = 128;
    program->data = (uint8_t*)malloc(program->capacity);

    for (size_t i = 0; i < instructions->count; ++i) {
        Instruction instr = instructions->data[i];
        emitByte(program, instr.opcode);
        switch (instr.opcode) {
            case OP_LOAD: // LOAD R, Imm (4 bytes)
                emitByte(program, (uint8_t)instr.arg1);
                emitWord(program, (uint16_t)instr.arg2);
                break;
            case OP_ADD:  // ADD R1, R2 (3 bytes)
            case OP_SUB:
            case OP_MOV:
            case OP_LDI:
            case OP_STI:
                emitByte(program, (uint8_t)instr.arg1);
                emitByte(program, (uint8_t)instr.arg2);
                break;
            case OP_STORE: // STORE R, Addr (4 bytes)
            case OP_LD:
                emitByte(program, (uint8_t)instr.arg1);
                emitWord(program, (uint16_t)instr.arg2);
                break;
            case OP_PUSH:  // PUSH R (2 bytes)
            case OP_POP:
            case OP_PRINT:
                emitByte(program, (uint8_t)instr.arg1);
                break;
            case OP_JMP:   // JMP Addr (3 bytes)
            case OP_JZ:
            case OP_JN:
                emitWord(program, (uint16_t)instr.arg1); // Jump target is arg1
                break;
            case OP_IRET:  // IRET (1 byte)
            case OP_HALT:  // HALT (1 byte)
                break;
            default:
                fprintf(stderr, "Assembler: Unknown opcode 0x%X\n", instr.opcode);
        }
    }
    return program;
}

void Assembler_destroyProgram(Program* program) {
    if (program) {
        free(program->data);
        free(program);
    }
}

// Map strings to Enums manually since no std::map
typedef struct {
    const char* name;
    Opcode opcode;
} OpcodeEntry;

static const OpcodeEntry opcodeTable[] = {
    {"LOAD", OP_LOAD}, {"ADD", OP_ADD}, {"SUB", OP_SUB},
    {"MOV", OP_MOV}, {"STORE", OP_STORE}, {"LD", OP_LD},
    {"PUSH", OP_PUSH}, {"POP", OP_POP}, {"JMP", OP_JMP},
    {"JZ", OP_JZ}, {"JN", OP_JN}, {"PRINT", OP_PRINT}, {"HALT", OP_HALT},
    {"IRET", OP_IRET}, {"LDI", OP_LDI}, {"STI", OP_STI},
    {NULL, (Opcode)0}
};

typedef struct {
    const char* name;
    Register reg;
} RegisterEntry;

static const RegisterEntry regTable[] = {
    {"R0", R0}, {"R1", R1}, {"R2", R2}, {"R3", R3},
    {"R4", R4}, {"R5", R5}, {"R6", R6}, {"R7", R7},
    {"IP", IP}, {"SP", SP},
    {NULL, (Register)0}
};

// Symbol Table
typedef struct {
    char name[64];
    uint16_t address;
} Symbol;

static Symbol symbolTable[128];
static int symbolCount = 0;

static void addSymbol(const char* name, uint16_t addr) {
    if (symbolCount < 128) {
        strncpy(symbolTable[symbolCount].name, name, 63);
        symbolTable[symbolCount].address = addr;
        symbolCount++;
    }
}

static uint16_t getSymbolAddress(const char* name) {
    for (int i = 0; i < symbolCount; ++i) {
        if (strcmp(name, symbolTable[i].name) == 0) {
            return symbolTable[i].address;
        }
    }
    return 0; // Default or Error
}

static uint16_t parseOperand(const char* token) {
    if (!token) return 0;
    
    // Check registers
    for (int i = 0; regTable[i].name != NULL; ++i) {
        if (strcmp(token, regTable[i].name) == 0) {
            return regTable[i].reg;
        }
    }
    
    // Check symbols
    for (int i = 0; i < symbolCount; ++i) {
        if (strcmp(token, symbolTable[i].name) == 0) {
            return symbolTable[i].address;
        }
    }
    
    // Parse number
    return (uint16_t)strtol(token, NULL, 0);
}

// Simple dynamic array push
static void pushInstruction(InstructionList* list, Instruction instr) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity == 0 ? 16 : list->capacity * 2;
        list->data = (Instruction*)realloc(list->data, list->capacity * sizeof(Instruction));
    }
    list->data[list->count++] = instr;
}

static int getInstructionSize(Opcode opcode) {
    switch (opcode) {
        case OP_LOAD: return 4;
        case OP_ADD: return 3;
        case OP_SUB: return 3;
        case OP_MOV: return 3;
        case OP_LDI: return 3;
        case OP_STI: return 3;
        case OP_STORE: return 4;
        case OP_LD: return 4;
        case OP_PUSH: return 2;
        case OP_POP: return 2;
        case OP_PRINT: return 2;
        case OP_JMP: return 3;
        case OP_JZ: return 3;
        case OP_JN: return 3;
        case OP_HALT: return 1;
        case OP_IRET: return 1;
        default: return 0;
    }
}

InstructionList* Assembler_loadProgramFromFile(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open file %s\n", filename);
        return NULL;
    }

    // Read all lines
    char** lines = NULL;
    int lineCount = 0;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), file)) {
        buffer[strcspn(buffer, "\n")] = 0;
        if (buffer[0] == 0 || buffer[0] == '#' || buffer[0] == ';') continue;
        
        lines = realloc(lines, sizeof(char*) * (lineCount + 1));
        lines[lineCount++] = strdup(buffer);
    }
    fclose(file);

    // Pass 1: Symbols
    symbolCount = 0;
    uint16_t currentAddr = 0x0100; // Start Address
    for (int i = 0; i < lineCount; ++i) {
        char* line = strdup(lines[i]); // Copy to tokenize
        char* token = strtok(line, " \t,");
        
        if (token) {
            int len = strlen(token);
            if (token[len-1] == ':') {
                token[len-1] = 0; // Remove colon
                addSymbol(token, currentAddr);
                // Label might be on same line as instruction?
                // Assuming label is on its own line or handled.
                // Simple implementation: "LABEL:" is one line.
                // Or "LABEL: INSTR".
                // Let's check next token.
                token = strtok(NULL, " \t,");
            }
            
            if (token) {
                 // It's an instruction
                Opcode opcode = (Opcode)0;
                for (int k = 0; opcodeTable[k].name != NULL; ++k) {
                    if (strcmp(token, opcodeTable[k].name) == 0) {
                        opcode = opcodeTable[k].opcode;
                        break;
                    }
                }
                currentAddr += getInstructionSize(opcode);
            }
        }
        free(line);
    }

    // Pass 2: Generation
    InstructionList* list = (InstructionList*)malloc(sizeof(InstructionList));
    list->count = 0;
    list->capacity = 0;
    list->data = NULL;

    for (int i = 0; i < lineCount; ++i) {
        char* line = lines[i]; // No need to copy, we can modify original
        char* token = strtok(line, " \t,");
        
        if (!token) continue;
        
        // Skip label if present
        int len = strlen(token);
        if (token[len-1] == ':') {
             token = strtok(NULL, " \t,");
        }
        
        if (!token) continue; // Just a label line
        
        Opcode opcode = (Opcode)0;
        int found = 0;
        for (int k = 0; opcodeTable[k].name != NULL; ++k) {
            if (strcmp(token, opcodeTable[k].name) == 0) {
                opcode = opcodeTable[k].opcode;
                found = 1;
                break;
            }
        }
        
        if (!found) continue;

        Instruction instr = {opcode, NO_ARG, NO_ARG, NO_ARG};
        char* arg1Str = NULL;
        char* arg2Str = NULL;

        switch (instr.opcode) {
            case OP_LOAD: // LOAD R, Imm
            case OP_STORE: // STORE R, Addr
            case OP_LD:    // LD R, Addr
                arg1Str = strtok(NULL, " \t,");
                arg2Str = strtok(NULL, " \t,");
                instr.arg1 = parseOperand(arg1Str);
                instr.arg2 = parseOperand(arg2Str);
                break;
            case OP_ADD: // ADD R1, R2
            case OP_SUB:
            case OP_MOV:
            case OP_LDI:
            case OP_STI:
                arg1Str = strtok(NULL, " \t,");
                arg2Str = strtok(NULL, " \t,");
                instr.arg1 = parseOperand(arg1Str);
                instr.arg2 = parseOperand(arg2Str);
                break;
            case OP_PUSH:
            case OP_POP:
            case OP_PRINT:
                arg1Str = strtok(NULL, " \t,");
                instr.arg1 = parseOperand(arg1Str);
                break;
            case OP_JMP:
            case OP_JZ:
            case OP_JN:
                arg1Str = strtok(NULL, " \t,");
                instr.arg1 = parseOperand(arg1Str);
                break;
            case OP_IRET:
            case OP_HALT:
                break;
            default:
                break;
        }
        pushInstruction(list, instr);
    }
    
    // Cleanup lines
    for(int i=0; i<lineCount; ++i) free(lines[i]);
    free(lines);
    
    return list;
}

void Assembler_destroyInstructionList(InstructionList* list) {
    if (list) {
        free(list->data);
        free(list);
    }
}
