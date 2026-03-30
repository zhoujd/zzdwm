// STL-compatible Allocator

#pragma once

#include <iostream>
#include <vector>
#include <cstring>
#include <cassert>
#include <memory>
#include <mutex>

#include "memory_pool.hpp"

template<typename T>
class PoolAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using size_type = size_t;
    
    template<typename U>
    struct rebind {
        using other = PoolAllocator<U>;
    };
    
    PoolAllocator(MemoryPool* pool = nullptr) : m_pool(pool) {}
    
    template<typename U>
    PoolAllocator(const PoolAllocator<U>& other) : m_pool(other.m_pool) {}
    
    pointer allocate(size_type n) {
        if (n != 1) {
            return static_cast<pointer>(::operator new(n * sizeof(T)));
        }
        
        if (m_pool) {
            return static_cast<pointer>(m_pool->allocate());
        }
        return static_cast<pointer>(::operator new(sizeof(T)));
    }
    
    void deallocate(pointer p, size_type n) {
        if (n != 1) {
            ::operator delete(p);
            return;
        }
        
        if (m_pool) {
            m_pool->deallocate(p);
        } else {
            ::operator delete(p);
        }
    }
    
    template<typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        new (p) U(std::forward<Args>(args)...);
    }
    
    template<typename U>
    void destroy(U* p) {
        p->~U();
    }
    
private:
    MemoryPool* m_pool;
    
    template<typename U>
    friend class PoolAllocator;
};

template<typename T, typename U>
bool operator==(const PoolAllocator<T>& a, const PoolAllocator<U>& b) {
    return true;  // Simplified for example
}

template<typename T, typename U>
bool operator!=(const PoolAllocator<T>& a, const PoolAllocator<U>& b) {
    return false;  // Simplified for example
}
