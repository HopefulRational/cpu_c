#include "main_memory.h"
#include <stdlib.h>
#include <string.h>

uint8_t MainMemory_read(MemoryAccessor* self, uint16_t addr) {
    MainMemory* mm = (MainMemory*)self;
    if (addr < mm->size) {
        return mm->memory[addr];
    }
    return 0; // Out of bounds
}

void MainMemory_write(MemoryAccessor* self, uint16_t addr, uint8_t value) {
    MainMemory* mm = (MainMemory*)self;
    if (addr < mm->size) {
        mm->memory[addr] = value;
    }
}

void MainMemory_readBlock(MemoryAccessor* self, uint16_t addr, uint8_t* buffer, size_t count) {
    MainMemory* mm = (MainMemory*)self;
    if (addr + count <= mm->size) {
        memcpy(buffer, &mm->memory[addr], count);
    } else {
        // Handle boundary
        for (size_t i = 0; i < count; ++i) {
            buffer[i] = MainMemory_read(self, addr + i);
        }
    }
}

void MainMemory_writeBlock(MemoryAccessor* self, uint16_t addr, const uint8_t* buffer, size_t count) {
    MainMemory* mm = (MainMemory*)self;
    if (addr + count <= mm->size) {
        memcpy(&mm->memory[addr], buffer, count);
    } else {
        for (size_t i = 0; i < count; ++i) {
            MainMemory_write(self, addr + i, buffer[i]);
        }
    }
}

void MainMemory_destroy(MemoryAccessor* self) {
    MainMemory* mm = (MainMemory*)self;
    free(mm->memory);
    free(mm);
}

MainMemory* MainMemory_create(size_t size) {
    MainMemory* mm = (MainMemory*)malloc(sizeof(MainMemory));
    if (!mm) return NULL;
    
    mm->base.type = MEM_TYPE_MAIN_MEMORY;
    mm->memory = (uint8_t*)malloc(size);
    mm->size = size;
    
    return mm;
}
