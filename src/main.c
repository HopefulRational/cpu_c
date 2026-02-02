#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"
#include "assembler.h"

void setupBIOS(CPU* cpu) {
    // 1. Create Default Interrupt Handler
    InstructionList* list = (InstructionList*)malloc(sizeof(InstructionList));
    list->count = 0;
    list->capacity = 10;
    list->data = (Instruction*)malloc(sizeof(Instruction) * list->capacity);
    
    // PUSH R0
    list->data[list->count++] = (Instruction){OP_PUSH, R0, NO_ARG, NO_ARG};
    // LOAD R0, 0xEEEE (Interrupt Signal)
    // list->data[list->count++] = (Instruction){OP_LOAD, R0, 0xEEEE, NO_ARG};
    // PRINT R0
    // list->data[list->count++] = (Instruction){OP_PRINT, R0, NO_ARG, NO_ARG};
    // POP R0
    list->data[list->count++] = (Instruction){OP_POP, R0, NO_ARG, NO_ARG};
    // IRET
    list->data[list->count++] = (Instruction){OP_IRET, NO_ARG, NO_ARG, NO_ARG};
    
    // Assemble
    Program* handler = Assembler_assemble(list);
    
    // 2. Load Handler into Memory at 0x0010
    CPU_loadProgram(cpu, handler->data, handler->size, 0x0010);
    
    // 3. Set Interrupt Vector Table (IVT) at 0x0000 to point to 0x0010
    CPU_writeWord(cpu, 0x0000, 0x0010);
    
    printf("BIOS: Loaded Interrupt Handler at 0x0010. IVT[0] = 0x0010.\n");
    
    Assembler_destroyProgram(handler);
    Assembler_destroyInstructionList(list);
}

void printUsage(const char* progName) {
    printf("Usage: %s [filename] [options]\n", progName);
    printf("Options:\n");
    printf("  --l1i-size <bytes>      Set L1 Instruction cache size (default: 512)\n");
    printf("  --l1i-assoc <ways>      Set L1 Instruction associativity (default: 2)\n");
    printf("  --l1d-size <bytes>      Set L1 Data cache size (default: 512)\n");
    printf("  --l1d-assoc <ways>      Set L1 Data associativity (default: 2)\n");
    printf("  --l2-size <bytes>       Set L2 Unified cache size (default: 4096)\n");
    printf("  --l2-assoc <ways>       Set L2 Unified associativity (default: 4)\n");
    printf("  --block-size <bytes>    Set block size for all caches (default: 64)\n");
    printf("  --verbose-cache         Show detailed cache statistics at the end\n");
    printf("  --help                  Show this help\n");
}

int main(int argc, char* argv[]) {
    printf("Starting CPU Emulator...\n");

    CacheConfig l1iConfig = {512, 64, 2};
    CacheConfig l1dConfig = {512, 64, 2};
    CacheConfig l2Config = {4096, 64, 4};
    bool verboseCache = false;
    const char* filename = NULL;

    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (strcmp(arg, "--verbose-cache") == 0) {
            verboseCache = true;
        } else if (strcmp(arg, "--l1i-size") == 0 && i + 1 < argc) {
            l1iConfig.cacheSize = atoi(argv[++i]);
        } else if (strcmp(arg, "--l1i-assoc") == 0 && i + 1 < argc) {
            l1iConfig.associativity = atoi(argv[++i]);
        } else if (strcmp(arg, "--l1d-size") == 0 && i + 1 < argc) {
            l1dConfig.cacheSize = atoi(argv[++i]);
        } else if (strcmp(arg, "--l1d-assoc") == 0 && i + 1 < argc) {
            l1dConfig.associativity = atoi(argv[++i]);
        } else if (strcmp(arg, "--l2-size") == 0 && i + 1 < argc) {
            l2Config.cacheSize = atoi(argv[++i]);
        } else if (strcmp(arg, "--l2-assoc") == 0 && i + 1 < argc) {
            l2Config.associativity = atoi(argv[++i]);
        } else if (strcmp(arg, "--block-size") == 0 && i + 1 < argc) {
            int blockSize = atoi(argv[++i]);
            l1iConfig.blockSize = blockSize;
            l1dConfig.blockSize = blockSize;
            l2Config.blockSize = blockSize;
        } else if (strcmp(arg, "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else {
            if (filename == NULL) {
                filename = arg;
            } else {
                fprintf(stderr, "Unknown argument or multiple filenames: %s\n", arg);
                printUsage(argv[0]);
                return 1;
            }
        }
    }

    // Timer limit 50 (Frequent interrupts for demo)
    CPU* cpu = CPU_create(l1iConfig, l1dConfig, l2Config, 50);
    
    // Initialize BIOS / OS Structures
    setupBIOS(cpu);

    InstructionList* source = NULL;

    if (filename != NULL) {
        printf("Reading program from %s...\n", filename);
        source = Assembler_loadProgramFromFile(filename);
    } else {
        printf("No input file specified. Using default example.\n");
        source = (InstructionList*)malloc(sizeof(InstructionList));
        source->capacity = 5;
        source->count = 5;
        source->data = (Instruction*)malloc(sizeof(Instruction) * 5);
        
        source->data[0] = (Instruction){OP_LOAD, R0, 10, NO_ARG};
        source->data[1] = (Instruction){OP_LOAD, R1, 20, NO_ARG};
        source->data[2] = (Instruction){OP_ADD, R0, R1, NO_ARG};
        source->data[3] = (Instruction){OP_PRINT, R0, NO_ARG, NO_ARG};
        source->data[4] = (Instruction){OP_HALT, NO_ARG, NO_ARG, NO_ARG};
    }

    if (!source || source->count == 0) {
        fprintf(stderr, "No instructions to execute.\n");
        if (source) Assembler_destroyInstructionList(source);
        CPU_destroy(cpu);
        return 1;
    }

    printf("Assembling program...\n");
    Program* program = Assembler_assemble(source);

    printf("Loading program (%lu bytes)...\n", program->size);
    // Load at 0x0100 (User/Kernel Space), 0x0000-0x00FF is reserved for IVT
    CPU_loadProgram(cpu, program->data, program->size, 0x0100);

    printf("Running...\n");
    CPU_run(cpu);

    printf("Final State:\n");
    CPU_printState(cpu, verboseCache);
    
    // Cleanup
    Assembler_destroyProgram(program);
    Assembler_destroyInstructionList(source);
    CPU_destroy(cpu);

    return 0;
}
