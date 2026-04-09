// dag_operations.h
#ifndef DAG_OPERATIONS_H
#define DAG_OPERATIONS_H

#include "dag.h"

// Advanced DAG operations
DAGNode* find_node(DAG* dag, const char* name);
void longest_path(DAG* dag, DAGNode* start, DAGNode* end);
void shortest_path(DAG* dag, DAGNode* start, DAGNode* end);
void find_all_paths(DAG* dag, DAGNode* start, DAGNode* end);
void compute_dag_depth(DAG* dag);
void transitive_closure(DAG* dag);
DAG* subgraph(DAG* dag, DAGNode** nodes, int node_count);

#endif
