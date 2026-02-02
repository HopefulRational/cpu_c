#ifndef MEMORY_ACCESSOR_H
#define MEMORY_ACCESSOR_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    MEM_TYPE_MAIN_MEMORY,
    MEM_TYPE_CACHE
} MemoryType;

typedef struct MemoryAccessor {
    MemoryType type;
} MemoryAccessor;

// Public Interface
uint8_t MemoryAccessor_read(MemoryAccessor* self, uint16_t addr);
void    MemoryAccessor_write(MemoryAccessor* self, uint16_t addr, uint8_t value);
void    MemoryAccessor_readBlock(MemoryAccessor* self, uint16_t addr, uint8_t* buffer, size_t size);
void    MemoryAccessor_writeBlock(MemoryAccessor* self, uint16_t addr, const uint8_t* buffer, size_t size);
void    MemoryAccessor_destroy(MemoryAccessor* self);

#endif // MEMORY_ACCESSOR_H
