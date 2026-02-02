#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <stdint.h>
#include <stddef.h>
#include "instructions.h"

// Struct to hold a list of instructions
typedef struct {
    Instruction* data;
    size_t count;
    size_t capacity;
} InstructionList;

// Struct to hold the machine code
typedef struct {
    uint8_t* data;
    size_t size;
    size_t capacity;
} Program;

// Parses a text file containing assembly code into Instruction structs
InstructionList* Assembler_loadProgramFromFile(const char* filename);
void Assembler_destroyInstructionList(InstructionList* list);

// Assembles a list of Instruction structs into machine code bytes
Program* Assembler_assemble(const InstructionList* instructions);
void Assembler_destroyProgram(Program* program);

#endif // ASSEMBLER_H
