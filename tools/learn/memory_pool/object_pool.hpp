// Object Pool Example

#pragma once

#include <iostream>
#include <vector>
#include <cstring>
#include <cassert>
#include <memory>
#include <mutex>


template<typename T>
class ObjectPool {
private:
    struct Node {
        T data;
        Node* next;
    };
    
    Node* freeList;
    size_t poolSize;
    std::vector<Node*> poolChunks;
    std::mutex poolMutex;
    
    void growPool(size_t numObjects) {
        Node* chunk = static_cast<Node*>(::operator new(numObjects * sizeof(Node)));
        poolChunks.push_back(chunk);
        
        // Initialize free list with new nodes
        for (size_t i = 0; i < numObjects; ++i) {
            Node* node = &chunk[i];
            node->next = freeList;
            freeList = node;
        }
    }
    
public:
    ObjectPool(size_t initialSize = 64, size_t growSize = 32) 
        : freeList(nullptr), poolSize(0) {
        growPool(initialSize);
        poolSize = initialSize;
    }
    
    ~ObjectPool() {
        for (auto chunk : poolChunks) {
            ::operator delete(chunk);
        }
    }
    
    template<typename... Args>
    T* allocate(Args&&... args) {
        std::lock_guard<std::mutex> lock(poolMutex);
        
        if (freeList == nullptr) {
            growPool(poolSize);
        }
        
        Node* node = freeList;
        freeList = freeList->next;
        
        // Construct object in place
        new (&node->data) T(std::forward<Args>(args)...);
        return &node->data;
    }
    
    void deallocate(T* obj) {
        if (obj == nullptr) return;
        
        std::lock_guard<std::mutex> lock(poolMutex);
        
        // Call destructor
        obj->~T();
        
        // Add back to free list
        Node* node = reinterpret_cast<Node*>(obj);
        node->next = freeList;
        freeList = node;
    }
    
    size_t getPoolSize() const { return poolSize; }
};
