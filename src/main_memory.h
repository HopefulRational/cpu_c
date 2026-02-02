#ifndef MAIN_MEMORY_H
#define MAIN_MEMORY_H

#include "memory_accessor.h"
#include <stddef.h>
#include <stdint.h>

typedef struct MainMemory {
    MemoryAccessor base;
    uint8_t* memory;
    size_t size;
} MainMemory;

MainMemory* MainMemory_create(size_t size);
// MainMemory_destroy is generic enough to be compatible but let's be explicit if needed
// Actually we can keep MainMemory_destroy as taking MemoryAccessor* since that's how it's used.

// Specific implementations exposed for MemoryAccessor dispatch
uint8_t MainMemory_read(MemoryAccessor* self, uint16_t addr);
void MainMemory_write(MemoryAccessor* self, uint16_t addr, uint8_t value);
void MainMemory_readBlock(MemoryAccessor* self, uint16_t addr, uint8_t* buffer, size_t count);
void MainMemory_writeBlock(MemoryAccessor* self, uint16_t addr, const uint8_t* buffer, size_t count);
void MainMemory_destroy(MemoryAccessor* self);

#endif // MAIN_MEMORY_H
