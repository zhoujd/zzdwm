// dag.h
#ifndef DAG_H
#define DAG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// DAG Node structure
typedef struct DAGNode {
    int id;
    char* name;
    int data;
    struct DAGNode** parents;    // Parent nodes (incoming edges)
    int parent_count;
    struct DAGNode** children;   // Child nodes (outgoing edges)
    int child_count;
    bool visited;                // For traversal algorithms
    int topological_order;       // For topological sorting
} DAGNode;

// DAG structure
typedef struct {
    DAGNode** nodes;
    int node_count;
    int capacity;
} DAG;

// Function declarations
DAG* create_dag(int initial_capacity);
DAGNode* create_node(DAG* dag, const char* name, int data);
void add_edge(DAG* dag, DAGNode* from, DAGNode* to);
bool has_cycle(DAG* dag);
bool has_cycle_util(DAGNode* node, bool* visited, bool* rec_stack);
void topological_sort(DAG* dag);
void topological_sort_util(DAGNode* node, bool* visited, int* stack, int* index);
void print_dag(DAG* dag);
void free_dag(DAG* dag);

#endif
