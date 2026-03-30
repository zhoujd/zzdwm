// Example class to test memory pool

#pragma once

#include <iostream>
#include <vector>
#include <cstring>
#include <cassert>
#include <memory>
#include <mutex>

class MyObject {
private:
    int id;
    double value;
    char name[32];
    
public:
    MyObject(int id = 0, double value = 0.0, const char* name = "") 
        : id(id), value(value) {
        strncpy(this->name, name, 31);
        this->name[31] = '\0';
        std::cout << "MyObject constructed: " << id << std::endl;
    }
    
    ~MyObject() {
        std::cout << "MyObject destructed: " << id << std::endl;
    }
    
    void display() const {
        std::cout << "Object [ID: " << id 
                  << ", Value: " << value 
                  << ", Name: " << name << "]" << std::endl;
    }
};
