// Memory Pool Example

#pragma once

#include <iostream>
#include <vector>
#include <cstring>
#include <cassert>
#include <memory>
#include <mutex>

class MemoryPool {
private:
    struct Block {
        Block* next;
    };
    
    Block* freeList;
    size_t blockSize;
    size_t poolSize;
    void* pool;
    std::mutex poolMutex;
    
public:
    MemoryPool(size_t blockSize, size_t numBlocks) 
        : blockSize(blockSize), poolSize(blockSize * numBlocks) {
        
        // Align block size to at least sizeof(Block*) for free list pointers
        if (this->blockSize < sizeof(Block*)) {
            this->blockSize = sizeof(Block*);
        }
        
        // Allocate raw memory
        pool = ::operator new(poolSize);
        freeList = static_cast<Block*>(pool);
        
        // Initialize free list
        Block* current = freeList;
        char* base = static_cast<char*>(pool);
        
        for (size_t i = 0; i < numBlocks - 1; ++i) {
            Block* next = reinterpret_cast<Block*>(base + (i + 1) * this->blockSize);
            current->next = next;
            current = next;
        }
        current->next = nullptr;
    }
    
    ~MemoryPool() {
        ::operator delete(pool);
    }
    
    void* allocate() {
        std::lock_guard<std::mutex> lock(poolMutex);
        
        if (freeList == nullptr) {
            return nullptr;  // Out of memory
        }
        
        void* block = freeList;
        freeList = freeList->next;
        return block;
    }
    
    void deallocate(void* block) {
        if (block == nullptr) return;
        
        std::lock_guard<std::mutex> lock(poolMutex);
        
        Block* newBlock = static_cast<Block*>(block);
        newBlock->next = freeList;
        freeList = newBlock;
    }
    
    size_t getBlockSize() const { return blockSize; }
    size_t getPoolSize() const { return poolSize; }
};
