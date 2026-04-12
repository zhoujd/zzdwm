// main.c
#include "dag.h"
#include "dag_operations.h"

// Example: Expression tree as DAG (common subexpression elimination)
void example_expression_dag() {
    printf("\n=== Expression DAG Example ===\n");
    printf("Expression: a = b + c; d = b + c; e = a + d\n\n");
    
    DAG* dag = create_dag(10);
    
    // Create nodes for variables
    DAGNode* b = create_node(dag, "b", 5);
    DAGNode* c = create_node(dag, "c", 3);
    
    // Create node for common subexpression (b + c)
    DAGNode* bc_add = create_node(dag, "b+c", 8);
    
    // Create nodes for assignments
    DAGNode* a = create_node(dag, "a", 0);
    DAGNode* d = create_node(dag, "d", 0);
    DAGNode* e = create_node(dag, "e", 0);
    
    // Build the DAG
    add_edge(dag, b, bc_add);
    add_edge(dag, c, bc_add);
    add_edge(dag, bc_add, a);
    add_edge(dag, bc_add, d);
    add_edge(dag, a, e);
    add_edge(dag, d, e);
    
    print_dag(dag);
    compute_dag_depth(dag);
    topological_sort(dag);
    
    free_dag(dag);
}

// Example: Task scheduling DAG
void example_task_scheduling() {
    printf("\n=== Task Scheduling DAG Example ===\n");
    printf("Tasks: A(compile), B(link), C(test), D(deploy)\n");
    printf("Dependencies: A->B, A->C, B->D, C->D\n\n");
    
    DAG* dag = create_dag(10);
    
    // Create task nodes with estimated durations
    DAGNode* compile = create_node(dag, "Compile", 10);
    DAGNode* link = create_node(dag, "Link", 5);
    DAGNode* test = create_node(dag, "Test", 8);
    DAGNode* deploy = create_node(dag, "Deploy", 3);
    
    // Add dependencies
    add_edge(dag, compile, link);
    add_edge(dag, compile, test);
    add_edge(dag, link, deploy);
    add_edge(dag, test, deploy);
    
    print_dag(dag);
    topological_sort(dag);
    
    // Find critical path (longest path from start to end)
    longest_path(dag, compile, deploy);
    
    free_dag(dag);
}

// Example: Data flow analysis DAG
void example_data_flow() {
    printf("\n=== Data Flow Analysis DAG Example ===\n");
    printf("Basic blocks: B1, B2, B3, B4, B5\n");
    printf("Control flow edges showing dependencies\n\n");
    
    DAG* dag = create_dag(10);
    
    // Create basic block nodes
    DAGNode* b1 = create_node(dag, "B1", 1);
    DAGNode* b2 = create_node(dag, "B2", 2);
    DAGNode* b3 = create_node(dag, "B3", 3);
    DAGNode* b4 = create_node(dag, "B4", 4);
    DAGNode* b5 = create_node(dag, "B5", 5);
    
    // Add control flow edges (must be acyclic)
    add_edge(dag, b1, b2);
    add_edge(dag, b1, b3);
    add_edge(dag, b2, b4);
    add_edge(dag, b3, b4);
    add_edge(dag, b4, b5);
    
    print_dag(dag);
    compute_dag_depth(dag);
    
    // Find all paths from start to end
    printf("\nAll paths from B1 to B5:\n");
    find_all_paths(dag, b1, b5);
    
    free_dag(dag);
}

// Helper function for find_all_paths
void find_all_paths_util(DAGNode* current, DAGNode* end, 
                         DAGNode** path, int path_len, bool* visited) {
    if (current == end) {
        printf("Path: ");
        for (int i = 0; i <= path_len; i++) {
            printf("%s", path[i]->name);
            if (i < path_len) printf(" -> ");
        }
        printf("\n");
        return;
    }
    
    visited[current->id] = true;
    
    for (int i = 0; i < current->child_count; i++) {
        DAGNode* child = current->children[i];
        if (!visited[child->id]) {
            path[path_len + 1] = child;
            find_all_paths_util(child, end, path, path_len + 1, visited);
        }
    }
    
    visited[current->id] = false;
}

void find_all_paths(DAG* dag, DAGNode* start, DAGNode* end) {
    if (!start || !end) {
        printf("Invalid start or end node!\n");
        return;
    }
    
    bool* visited = (bool*)calloc(dag->node_count, sizeof(bool));
    DAGNode** path = (DAGNode**)malloc(sizeof(DAGNode*) * dag->node_count);
    path[0] = start;
    
    find_all_paths_util(start, end, path, 0, visited);
    
    free(visited);
    free(path);
}

int main() {
    printf("=== DAG (Directed Acyclic Graph) Examples in C ===\n");
    
    // Run examples
    example_expression_dag();
    example_task_scheduling();
    example_data_flow();
    
    return 0;
}
