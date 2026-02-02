#ifndef CACHE_H
#define CACHE_H

#include "memory_accessor.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct CacheConfig {
    uint32_t cacheSize;     // Total size in bytes
    uint32_t blockSize;     // Block size in bytes
    uint32_t associativity; // Ways per set
} CacheConfig;

typedef struct CacheLine {
    bool valid;
    bool dirty;
    uint16_t tag;
    uint8_t* data; // Dynamic array of size config.blockSize
    uint64_t lastAccessTime;
} CacheLine;

typedef struct Set {
    CacheLine* lines; // Dynamic array of size config.associativity
} Set;

typedef struct Cache {
    MemoryAccessor base; // Base class
    
    CacheConfig config;
    MemoryAccessor* nextLevel;

    Set* sets; // Dynamic array of size numSets
    
    uint32_t numSets;
    uint32_t indexBits;
    uint32_t offsetBits;
    uint32_t tagBits;

    uint64_t accessCounter;

    // Statistics
    uint64_t readHits;
    uint64_t readMisses;
    uint64_t writeHits;
    uint64_t writeMisses;
} Cache;

Cache* Cache_create(CacheConfig config, MemoryAccessor* nextLevel);

void Cache_printStats(const Cache* self);
void Cache_resetStats(Cache* self);

// Specific implementations exposed for MemoryAccessor dispatch
uint8_t Cache_read(MemoryAccessor* self, uint16_t addr);
void Cache_write(MemoryAccessor* self, uint16_t addr, uint8_t value);
void Cache_readBlock(MemoryAccessor* self, uint16_t addr, uint8_t* buffer, size_t size);
void Cache_writeBlock(MemoryAccessor* self, uint16_t addr, const uint8_t* buffer, size_t size);
void Cache_destroy(MemoryAccessor* self);

#endif // CACHE_H
