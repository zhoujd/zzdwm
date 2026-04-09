// dag.c
#include "dag.h"

// Create a new DAG
DAG* create_dag(int initial_capacity) {
    DAG* dag = (DAG*)malloc(sizeof(DAG));
    dag->nodes = (DAGNode**)malloc(sizeof(DAGNode*) * initial_capacity);
    dag->node_count = 0;
    dag->capacity = initial_capacity;
    return dag;
}

// Create a new node in the DAG
DAGNode* create_node(DAG* dag, const char* name, int data) {
    // Check if node with same name already exists
    for (int i = 0; i < dag->node_count; i++) {
        if (strcmp(dag->nodes[i]->name, name) == 0) {
            printf("Node with name '%s' already exists!\n", name);
            return NULL;
        }
    }
    
    // Resize if necessary
    if (dag->node_count >= dag->capacity) {
        dag->capacity *= 2;
        dag->nodes = (DAGNode**)realloc(dag->nodes, sizeof(DAGNode*) * dag->capacity);
    }
    
    // Create new node
    DAGNode* node = (DAGNode*)malloc(sizeof(DAGNode));
    node->id = dag->node_count;
    node->name = strdup(name);
    node->data = data;
    node->parents = NULL;
    node->parent_count = 0;
    node->children = NULL;
    node->child_count = 0;
    node->visited = false;
    node->topological_order = -1;
    
    dag->nodes[dag->node_count++] = node;
    
    return node;
}

// Add edge from 'from' node to 'to' node
void add_edge(DAG* dag, DAGNode* from, DAGNode* to) {
    if (!from || !to) {
        printf("Invalid nodes for edge addition!\n");
        return;
    }
    
    // Check if edge already exists
    for (int i = 0; i < from->child_count; i++) {
        if (from->children[i] == to) {
            printf("Edge from %s to %s already exists!\n", from->name, to->name);
            return;
        }
    }
    
    // Add to children of 'from'
    from->children = (DAGNode**)realloc(from->children, 
                                        sizeof(DAGNode*) * (from->child_count + 1));
    from->children[from->child_count++] = to;
    
    // Add to parents of 'to'
    to->parents = (DAGNode**)realloc(to->parents, 
                                     sizeof(DAGNode*) * (to->parent_count + 1));
    to->parents[to->parent_count++] = from;
    
    // Check for cycles after adding edge
    if (has_cycle(dag)) {
        printf("Warning: Adding edge from %s to %s would create a cycle!\n", 
               from->name, to->name);
        
        // Remove the edge if it creates a cycle
        from->child_count--;
        from->children = (DAGNode**)realloc(from->children, 
                                            sizeof(DAGNode*) * from->child_count);
        to->parent_count--;
        to->parents = (DAGNode**)realloc(to->parents, 
                                         sizeof(DAGNode*) * to->parent_count);
    }
}

// Check if the graph has cycles (DFS-based cycle detection)
bool has_cycle(DAG* dag) {
    bool* visited = (bool*)calloc(dag->node_count, sizeof(bool));
    bool* rec_stack = (bool*)calloc(dag->node_count, sizeof(bool));
    
    for (int i = 0; i < dag->node_count; i++) {
        if (!visited[i]) {
            if (has_cycle_util(dag->nodes[i], visited, rec_stack)) {
                free(visited);
                free(rec_stack);
                return true;
            }
        }
    }
    
    free(visited);
    free(rec_stack);
    return false;
}

// Utility function for cycle detection
bool has_cycle_util(DAGNode* node, bool* visited, bool* rec_stack) {
    if (!visited[node->id]) {
        visited[node->id] = true;
        rec_stack[node->id] = true;
        
        for (int i = 0; i < node->child_count; i++) {
            DAGNode* child = node->children[i];
            if (!visited[child->id] && has_cycle_util(child, visited, rec_stack)) {
                return true;
            } else if (rec_stack[child->id]) {
                return true;
            }
        }
    }
    
    rec_stack[node->id] = false;
    return false;
}

// Perform topological sort on the DAG
void topological_sort(DAG* dag) {
    if (has_cycle(dag)) {
        printf("Cannot perform topological sort on a graph with cycles!\n");
        return;
    }
    
    bool* visited = (bool*)calloc(dag->node_count, sizeof(bool));
    int* stack = (int*)malloc(sizeof(int) * dag->node_count);
    int index = dag->node_count - 1;
    
    for (int i = 0; i < dag->node_count; i++) {
        if (!visited[i]) {
            topological_sort_util(dag->nodes[i], visited, stack, &index);
        }
    }
    
    printf("Topological Order: ");
    for (int i = 0; i < dag->node_count; i++) {
        printf("%s ", dag->nodes[stack[i]]->name);
        dag->nodes[stack[i]]->topological_order = i;
    }
    printf("\n");
    
    free(visited);
    free(stack);
}

// Utility function for topological sort (DFS-based)
void topological_sort_util(DAGNode* node, bool* visited, int* stack, int* index) {
    visited[node->id] = true;
    
    for (int i = 0; i < node->child_count; i++) {
        DAGNode* child = node->children[i];
        if (!visited[child->id]) {
            topological_sort_util(child, visited, stack, index);
        }
    }
    
    stack[(*index)--] = node->id;
}

// Print DAG structure
void print_dag(DAG* dag) {
    printf("\n=== DAG Structure ===\n");
    printf("Total nodes: %d\n\n", dag->node_count);
    
    for (int i = 0; i < dag->node_count; i++) {
        DAGNode* node = dag->nodes[i];
        printf("Node %d: %s (data: %d)\n", node->id, node->name, node->data);
        
        if (node->parent_count > 0) {
            printf("  Parents: ");
            for (int j = 0; j < node->parent_count; j++) {
                printf("%s ", node->parents[j]->name);
            }
            printf("\n");
        }
        
        if (node->child_count > 0) {
            printf("  Children: ");
            for (int j = 0; j < node->child_count; j++) {
                printf("%s ", node->children[j]->name);
            }
            printf("\n");
        }
        
        if (node->topological_order >= 0) {
            printf("  Topological order: %d\n", node->topological_order);
        }
        
        printf("\n");
    }
}

// Free DAG memory
void free_dag(DAG* dag) {
    for (int i = 0; i < dag->node_count; i++) {
        DAGNode* node = dag->nodes[i];
        free(node->name);
        free(node->parents);
        free(node->children);
        free(node);
    }
    free(dag->nodes);
    free(dag);
}
