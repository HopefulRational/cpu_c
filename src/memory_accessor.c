#include "memory_accessor.h"
#include "main_memory.h"
#include "cache.h"
#include <stdio.h>

uint8_t MemoryAccessor_read(MemoryAccessor* self, uint16_t addr) {
    if (self->type == MEM_TYPE_MAIN_MEMORY) {
        return MainMemory_read(self, addr);
    } else if (self->type == MEM_TYPE_CACHE) {
        return Cache_read(self, addr);
    }
    return 0;
}

void MemoryAccessor_write(MemoryAccessor* self, uint16_t addr, uint8_t value) {
    if (self->type == MEM_TYPE_MAIN_MEMORY) {
        MainMemory_write(self, addr, value);
    } else if (self->type == MEM_TYPE_CACHE) {
        Cache_write(self, addr, value);
    }
}

void MemoryAccessor_readBlock(MemoryAccessor* self, uint16_t addr, uint8_t* buffer, size_t size) {
    if (self->type == MEM_TYPE_MAIN_MEMORY) {
        MainMemory_readBlock(self, addr, buffer, size);
    } else if (self->type == MEM_TYPE_CACHE) {
        Cache_readBlock(self, addr, buffer, size);
    }
}

void MemoryAccessor_writeBlock(MemoryAccessor* self, uint16_t addr, const uint8_t* buffer, size_t size) {
    if (self->type == MEM_TYPE_MAIN_MEMORY) {
        MainMemory_writeBlock(self, addr, buffer, size);
    } else if (self->type == MEM_TYPE_CACHE) {
        Cache_writeBlock(self, addr, buffer, size);
    }
}

void MemoryAccessor_destroy(MemoryAccessor* self) {
    if (self->type == MEM_TYPE_MAIN_MEMORY) {
        MainMemory_destroy(self);
    } else if (self->type == MEM_TYPE_CACHE) {
        Cache_destroy(self);
    }
}
