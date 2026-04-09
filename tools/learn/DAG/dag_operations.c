// dag_operations.c
#include "dag_operations.h"
#include <limits.h>

// Find node by name
DAGNode* find_node(DAG* dag, const char* name) {
    for (int i = 0; i < dag->node_count; i++) {
        if (strcmp(dag->nodes[i]->name, name) == 0) {
            return dag->nodes[i];
        }
    }
    return NULL;
}

// Find longest path between two nodes (weighted by data)
void longest_path(DAG* dag, DAGNode* start, DAGNode* end) {
    if (!start || !end) {
        printf("Invalid start or end node!\n");
        return;
    }
    
    // Perform topological sort first
    topological_sort(dag);
    
    // Initialize distances
    int* dist = (int*)malloc(sizeof(int) * dag->node_count);
    DAGNode** prev = (DAGNode**)malloc(sizeof(DAGNode*) * dag->node_count);
    
    for (int i = 0; i < dag->node_count; i++) {
        dist[i] = INT_MIN;
        prev[i] = NULL;
    }
    dist[start->id] = start->data;
    
    // Process nodes in topological order
    for (int i = 0; i < dag->node_count; i++) {
        DAGNode* u = dag->nodes[i];
        if (dist[u->id] != INT_MIN) {
            for (int j = 0; j < u->child_count; j++) {
                DAGNode* v = u->children[j];
                if (dist[v->id] < dist[u->id] + v->data) {
                    dist[v->id] = dist[u->id] + v->data;
                    prev[v->id] = u;
                }
            }
        }
    }
    
    if (dist[end->id] == INT_MIN) {
        printf("No path from %s to %s\n", start->name, end->name);
    } else {
        printf("Longest path from %s to %s: weight = %d\n", 
               start->name, end->name, dist[end->id]);
        
        // Reconstruct path
        printf("Path: ");
        DAGNode* path[100];
        int path_len = 0;
        DAGNode* current = end;
        
        while (current != NULL) {
            path[path_len++] = current;
            current = prev[current->id];
        }
        
        for (int i = path_len - 1; i >= 0; i--) {
            printf("%s", path[i]->name);
            if (i > 0) printf(" -> ");
        }
        printf("\n");
    }
    
    free(dist);
    free(prev);
}

// Compute depth of each node in DAG
void compute_dag_depth(DAG* dag) {
    if (has_cycle(dag)) {
        printf("Cannot compute depth on cyclic graph!\n");
        return;
    }
    
    int* depth = (int*)calloc(dag->node_count, sizeof(int));
    bool changed;
    
    // Dynamic programming to compute depths
    do {
        changed = false;
        for (int i = 0; i < dag->node_count; i++) {
            DAGNode* node = dag->nodes[i];
            int max_depth = 0;
            
            for (int j = 0; j < node->parent_count; j++) {
                if (depth[node->parents[j]->id] + 1 > max_depth) {
                    max_depth = depth[node->parents[j]->id] + 1;
                }
            }
            
            if (depth[node->id] != max_depth) {
                depth[node->id] = max_depth;
                changed = true;
            }
        }
    } while (changed);
    
    printf("\n=== Node Depths ===\n");
    for (int i = 0; i < dag->node_count; i++) {
        printf("Node %s: depth = %d\n", dag->nodes[i]->name, depth[i]);
    }
    
    free(depth);
}
