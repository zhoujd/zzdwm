// main.cpp

#include <iostream>
#include <chrono>
#include <vector>
#include <memory>
#include <numeric>

#include "memory_pool.hpp"
#include "my_object.hpp"
#include "object_pool.hpp"
#include "pool_allocator.hpp"

int main() {
    std::cout << "=== Basic Memory Pool Example ===" << std::endl;
    
    // Test basic memory pool
    MemoryPool pool(64, 100);  // 100 blocks of 64 bytes each
    
    std::vector<void*> allocations;
    
    // Allocate all blocks
    for (int i = 0; i < 100; ++i) {
        void* ptr = pool.allocate();
        if (ptr) {
            allocations.push_back(ptr);
            std::cout << "Allocated block " << i << " at " << ptr << std::endl;
        } else {
            std::cout << "Failed to allocate block " << i << std::endl;
        }
    }
    
    // Try to allocate one more (should fail)
    void* extra = pool.allocate();
    if (!extra) {
        std::cout << "Expected: Out of memory!" << std::endl;
    }
    
    // Deallocate some blocks
    for (int i = 0; i < 50; ++i) {
        pool.deallocate(allocations[i]);
        std::cout << "Deallocated block " << i << std::endl;
    }
    
    // Reallocate freed blocks
    for (int i = 0; i < 50; ++i) {
        void* ptr = pool.allocate();
        if (ptr) {
            std::cout << "Reallocated block at " << ptr << std::endl;
        }
    }
    
    std::cout << "\n=== Object Pool Example ===" << std::endl;
    
    // Test object pool
    ObjectPool<MyObject> objPool(10, 5);
    
    std::vector<MyObject*> objects;
    
    // Allocate objects
    for (int i = 0; i < 15; ++i) {
        MyObject* obj = objPool.allocate(i, i * 2.5, "Object");
        objects.push_back(obj);
        obj->display();
    }
    
    // Deallocate objects
    for (auto obj : objects) {
        objPool.deallocate(obj);
    }
    
    std::cout << "\n=== STL Container with Custom Allocator ===" << std::endl;
    
    // Use memory pool with STL containers
    MemoryPool stlPool(32, 1000);
    PoolAllocator<int> alloc(&stlPool);
    
    std::vector<int, PoolAllocator<int>> vec(alloc);
    for (int i = 0; i < 100; ++i) {
        vec.push_back(i);
    }
    
    std::cout << "Vector size: " << vec.size() << std::endl;
    std::cout << "Vector sum: " << std::accumulate(vec.begin(), vec.end(), 0) << std::endl;
    
    return 0;
}
