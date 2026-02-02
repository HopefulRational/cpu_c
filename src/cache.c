#include "cache.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Forward declarations
void Cache_parseAddress(const Cache* self, uint16_t addr, uint16_t* tag, uint32_t* index, uint32_t* offset);
void Cache_flushLine(Cache* self, const CacheLine* line, uint32_t index);
CacheLine* Cache_accessLine(Cache* self, uint16_t addr, bool isWrite);

uint8_t Cache_read(MemoryAccessor* self, uint16_t addr) {
    Cache* cache = (Cache*)self;
    uint32_t offset = addr % cache->config.blockSize;
    CacheLine* line = Cache_accessLine(cache, addr, false);
    return line->data[offset];
}

void Cache_write(MemoryAccessor* self, uint16_t addr, uint8_t value) {
    Cache* cache = (Cache*)self;
    uint32_t offset = addr % cache->config.blockSize;
    CacheLine* line = Cache_accessLine(cache, addr, true);
    line->data[offset] = value;
    line->dirty = true;
}

void Cache_readBlock(MemoryAccessor* self, uint16_t addr, uint8_t* buffer, size_t size) {
    Cache* cache = (Cache*)self;
    size_t bytesRead = 0;
    while (bytesRead < size) {
        uint16_t currentAddr = addr + bytesRead;
        uint32_t offset = currentAddr % cache->config.blockSize;
        
        size_t remainingInBlock = cache->config.blockSize - offset;
        size_t bytesToRead = (size - bytesRead) < remainingInBlock ? (size - bytesRead) : remainingInBlock;

        CacheLine* line = Cache_accessLine(cache, currentAddr, false);
        memcpy(buffer + bytesRead, line->data + offset, bytesToRead);

        bytesRead += bytesToRead;
    }
}

void Cache_writeBlock(MemoryAccessor* self, uint16_t addr, const uint8_t* buffer, size_t size) {
    Cache* cache = (Cache*)self;
    size_t bytesWritten = 0;
    while (bytesWritten < size) {
        uint16_t currentAddr = addr + bytesWritten;
        uint32_t offset = currentAddr % cache->config.blockSize;
        
        size_t remainingInBlock = cache->config.blockSize - offset;
        size_t bytesToWrite = (size - bytesWritten) < remainingInBlock ? (size - bytesWritten) : remainingInBlock;

        CacheLine* line = Cache_accessLine(cache, currentAddr, true);
        memcpy(line->data + offset, buffer + bytesWritten, bytesToWrite);
        line->dirty = true;

        bytesWritten += bytesToWrite;
    }
}

void Cache_destroy(MemoryAccessor* self) {
    Cache* cache = (Cache*)self;
    // Flush all dirty lines
    for (uint32_t i = 0; i < cache->numSets; ++i) {
        for (uint32_t j = 0; j < cache->config.associativity; ++j) {
            CacheLine* line = &cache->sets[i].lines[j];
            if (line->valid && line->dirty) {
                Cache_flushLine(cache, line, i);
            }
            free(line->data);
        }
        free(cache->sets[i].lines);
    }
    free(cache->sets);
    free(cache);
}

Cache* Cache_create(CacheConfig config, MemoryAccessor* nextLevel) {
    Cache* cache = (Cache*)malloc(sizeof(Cache));
    if (!cache) return NULL;
    
    cache->base.type = MEM_TYPE_CACHE;
    
    // Validate config
    if (config.blockSize == 0) config.blockSize = 1;
    if (config.associativity == 0) config.associativity = 1;
    if (config.cacheSize == 0) config.cacheSize = 1024; // Default
    
    cache->config = config;
    cache->nextLevel = nextLevel;
    cache->accessCounter = 0;
    cache->readHits = 0;
    cache->readMisses = 0;
    cache->writeHits = 0;
    cache->writeMisses = 0;
    
    // Calculate number of sets
    cache->numSets = config.cacheSize / (config.blockSize * config.associativity);
    if (cache->numSets == 0) cache->numSets = 1; // Minimum 1 set
    
    cache->sets = (Set*)malloc(sizeof(Set) * cache->numSets);
    for (uint32_t i = 0; i < cache->numSets; ++i) {
        cache->sets[i].lines = (CacheLine*)malloc(sizeof(CacheLine) * config.associativity);
        for (uint32_t j = 0; j < config.associativity; ++j) {
            cache->sets[i].lines[j].valid = false;
            cache->sets[i].lines[j].dirty = false;
            cache->sets[i].lines[j].tag = 0;
            cache->sets[i].lines[j].lastAccessTime = 0;
            cache->sets[i].lines[j].data = (uint8_t*)malloc(config.blockSize);
            memset(cache->sets[i].lines[j].data, 0, config.blockSize);
        }
    }
    
    return cache;
}

void Cache_parseAddress(const Cache* self, uint16_t addr, uint16_t* tag, uint32_t* index, uint32_t* offset) {
    *offset = addr % self->config.blockSize;
    uint32_t blockAddr = addr / self->config.blockSize;
    *index = blockAddr % self->numSets;
    *tag = blockAddr / self->numSets;
}

void Cache_flushLine(Cache* self, const CacheLine* line, uint32_t index) {
    // Reconstruct address
    uint32_t blockAddr = line->tag * self->numSets + index;
    uint32_t addr = blockAddr * self->config.blockSize;

    if (self->nextLevel) {
        MemoryAccessor_writeBlock(self->nextLevel, addr, line->data, self->config.blockSize);
    }
}

CacheLine* Cache_accessLine(Cache* self, uint16_t addr, bool isWrite) {
    uint16_t tag;
    uint32_t index;
    uint32_t offset;
    Cache_parseAddress(self, addr, &tag, &index, &offset);

    Set* set = &self->sets[index];
    self->accessCounter++;

    // Check for hit
    for (uint32_t i = 0; i < self->config.associativity; ++i) {
        CacheLine* line = &set->lines[i];
        if (line->valid && line->tag == tag) {
            // Hit
            line->lastAccessTime = self->accessCounter;
            if (isWrite) self->writeHits++; else self->readHits++;
            return line;
        }
    }

    // Miss
    if (isWrite) self->writeMisses++; else self->readMisses++;
    
    // Find victim (LRU)
    int victimIdx = -1;
    uint64_t minTime = (uint64_t)-1; // Max uint64
    
    // First look for invalid lines
    for (uint32_t i = 0; i < self->config.associativity; ++i) {
        if (!set->lines[i].valid) {
            victimIdx = i;
            break;
        }
    }

    // If no invalid line, find LRU
    if (victimIdx == -1) {
        minTime = set->lines[0].lastAccessTime;
        victimIdx = 0;
        for (uint32_t i = 1; i < self->config.associativity; ++i) {
            if (set->lines[i].lastAccessTime < minTime) {
                minTime = set->lines[i].lastAccessTime;
                victimIdx = i;
            }
        }
    }

    CacheLine* victim = &set->lines[victimIdx];

    // Write back if dirty
    if (victim->valid && victim->dirty) {
        Cache_flushLine(self, victim, index);
    }

    // Load new block
    uint32_t blockAddr = tag * self->numSets + index;
    uint32_t baseAddr = blockAddr * self->config.blockSize;
    
    victim->valid = true;
    victim->dirty = false; // Freshly loaded
    victim->tag = tag;
    victim->lastAccessTime = self->accessCounter;

    if (self->nextLevel) {
        MemoryAccessor_readBlock(self->nextLevel, baseAddr, victim->data, self->config.blockSize);
    } else {
        memset(victim->data, 0, self->config.blockSize);
    }

    return victim;
}

void Cache_printStats(const Cache* self) {
    printf("Config: Size=%uB, Block=%uB, Assoc=%u\n", 
           self->config.cacheSize, self->config.blockSize, self->config.associativity);
    printf("Read Hits: %lu, Read Misses: %lu\n", self->readHits, self->readMisses);
    printf("Write Hits: %lu, Write Misses: %lu\n", self->writeHits, self->writeMisses);
    
    double readHitRate = (self->readHits + self->readMisses) > 0 ? (double)self->readHits / (self->readHits + self->readMisses) * 100.0 : 0.0;
    double writeHitRate = (self->writeHits + self->writeMisses) > 0 ? (double)self->writeHits / (self->writeHits + self->writeMisses) * 100.0 : 0.0;
    
    printf("Read Hit Rate: %.2f%%\n", readHitRate);
    printf("Write Hit Rate: %.2f%%\n", writeHitRate);
}

void Cache_resetStats(Cache* self) {
    self->readHits = 0;
    self->readMisses = 0;
    self->writeHits = 0;
    self->writeMisses = 0;
}
