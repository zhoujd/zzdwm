// examples.cpp
#include "dag.hpp"
#include <iostream>
#include <chrono>
#include <thread>

// Example 1: Expression DAG for Common Subexpression Elimination
void expression_dag_example() {
    std::cout << "\n=== Expression DAG Example ===" << std::endl;
    std::cout << "Expression: a = b + c; d = b + c; e = a + d" << std::endl;
    
    DAG<std::string, int> dag;
    
    // Add nodes for variables and operations
    dag.add_node("b");
    dag.add_node("c");
    dag.add_node("b+c");  // Common subexpression
    dag.add_node("a");
    dag.add_node("d");
    dag.add_node("e");
    
    // Build DAG
    dag.add_edge("b", "b+c", 0);
    dag.add_edge("c", "b+c", 0);
    dag.add_edge("b+c", "a", 0);
    dag.add_edge("b+c", "d", 0);
    dag.add_edge("a", "e", 0);
    dag.add_edge("d", "e", 0);
    
    dag.print();
    
    auto depths = dag.compute_depths();
    std::cout << "\nNode Depths:" << std::endl;
    for (auto& [node, depth] : depths) {
        std::cout << "  " << node << ": depth " << depth << std::endl;
    }
    
    auto topo = dag.topological_sort();
    std::cout << "\nTopological Order: ";
    for (const auto& node : topo) {
        std::cout << node << " ";
    }
    std::cout << std::endl;
}

// Example 2: Task Scheduling with Critical Path
void task_scheduling_example() {
    std::cout << "\n=== Task Scheduling DAG Example ===" << std::endl;
    
    DAG<std::string, int> dag;
    
    // Add tasks with durations
    dag.add_node("Design");
    dag.add_node("Implementation");
    dag.add_node("Testing");
    dag.add_node("Documentation");
    dag.add_node("Deployment");
    dag.add_node("Maintenance");
    
    // Add dependencies with durations (weights)
    dag.add_edge("Design", "Implementation", 5);
    dag.add_edge("Design", "Documentation", 3);
    dag.add_edge("Implementation", "Testing", 4);
    dag.add_edge("Documentation", "Deployment", 2);
    dag.add_edge("Testing", "Deployment", 1);
    dag.add_edge("Deployment", "Maintenance", 6);
    
    dag.print();
    
    // Find critical path
    auto critical = dag.longest_path("Design", "Maintenance");
    if (critical) {
        auto& [path, duration] = *critical;
        std::cout << "\nCritical Path (Longest Path):" << std::endl;
        std::cout << "  Path: ";
        for (size_t i = 0; i < path.size(); ++i) {
            std::cout << path[i];
            if (i < path.size() - 1) std::cout << " -> ";
        }
        std::cout << std::endl;
        std::cout << "  Total Duration: " << duration << " days" << std::endl;
    }
    
    // Compute earliest start times (depths)
    auto depths = dag.compute_depths();
    std::cout << "\nEarliest Start Times:" << std::endl;
    for (auto& [task, depth] : depths) {
        std::cout << "  " << task << ": day " << depth << std::endl;
    }
}

// Example 3: Data Flow Analysis in Compiler
void data_flow_example() {
    std::cout << "\n=== Data Flow Analysis DAG Example ===" << std::endl;
    
    DAG<std::string, std::string> dag;  // Weight as string for data flow info
    
    // Basic blocks
    dag.add_node("BB1");
    dag.add_node("BB2");
    dag.add_node("BB3");
    dag.add_node("BB4");
    dag.add_node("BB5");
    
    // Control flow edges with data flow information
    dag.add_edge("BB1", "BB2", "x = 10");
    dag.add_edge("BB1", "BB3", "y = 20");
    dag.add_edge("BB2", "BB4", "z = x + y");
    dag.add_edge("BB3", "BB4", "z = x - y");
    dag.add_edge("BB4", "BB5", "print(z)");
    
    dag.print();
    
    // Find all possible paths
    auto paths = dag.find_all_paths("BB1", "BB5");
    std::cout << "\nAll Possible Execution Paths:" << std::endl;
    for (size_t i = 0; i < paths.size(); ++i) {
        std::cout << "Path " << i + 1 << ": ";
        for (size_t j = 0; j < paths[i].size(); ++j) {
            std::cout << paths[i][j];
            if (j < paths[i].size() - 1) std::cout << " -> ";
        }
        std::cout << std::endl;
        
        // Show data flow along the path
        std::cout << "  Data flow: ";
        for (size_t j = 0; j < paths[i].size() - 1; ++j) {
            auto weight = dag.get_edge_weight(paths[i][j], paths[i][j + 1]);
            if (weight) {
                std::cout << *weight;
                if (j < paths[i].size() - 2) std::cout << ", ";
            }
        }
        std::cout << std::endl;
    }
}

// Example 4: Build System Dependency Graph
void build_system_example() {
    std::cout << "\n=== Build System DAG Example ===" << std::endl;
    
    DAG<std::string, std::chrono::milliseconds> dag;
    
    // Build targets
    dag.add_node("clean");
    dag.add_node("configure");
    dag.add_node("compile_utils");
    dag.add_node("compile_core");
    dag.add_node("compile_ui");
    dag.add_node("link");
    dag.add_node("test");
    dag.add_node("package");
    
    // Add build dependencies with estimated times
    dag.add_edge("clean", "configure", std::chrono::milliseconds(100));
    dag.add_edge("configure", "compile_utils", std::chrono::milliseconds(500));
    dag.add_edge("configure", "compile_core", std::chrono::milliseconds(1000));
    dag.add_edge("configure", "compile_ui", std::chrono::milliseconds(800));
    dag.add_edge("compile_utils", "link", std::chrono::milliseconds(200));
    dag.add_edge("compile_core", "link", std::chrono::milliseconds(200));
    dag.add_edge("compile_ui", "link", std::chrono::milliseconds(200));
    dag.add_edge("link", "test", std::chrono::milliseconds(3000));
    dag.add_edge("test", "package", std::chrono::milliseconds(1000));
    
    dag.print();
    
    // Find parallel build opportunities
    auto topo = dag.topological_sort();
    std::cout << "\nParallel Build Order (levels):" << std::endl;
    
    auto depths = dag.compute_depths();
    std::unordered_map<int, std::vector<std::string>> levels;
    for (auto& [target, depth] : depths) {
        levels[depth].push_back(target);
    }
    
    for (auto& [level, targets] : levels) {
        std::cout << "Level " << level << ": ";
        for (const auto& target : targets) {
            std::cout << target << " ";
        }
        std::cout << std::endl;
    }
    
    // Find critical path for build time estimation
    auto critical = dag.longest_path("configure", "package");
    if (critical) {
        auto& [path, duration] = *critical;
        std::cout << "\nCritical Build Path:" << std::endl;
        std::cout << "  ";
        for (size_t i = 0; i < path.size(); ++i) {
            std::cout << path[i];
            if (i < path.size() - 1) std::cout << " -> ";
        }
        std::cout << std::endl;
        std::cout << "  Estimated minimum build time: " 
                  << std::chrono::duration_cast<std::chrono::seconds>(duration).count()
                  << " seconds" << std::endl;
    }
}

// Example 5: Course Prerequisite Graph
void course_prerequisite_example() {
    std::cout << "\n=== Course Prerequisite DAG Example ===" << std::endl;
    
    DAG<std::string, int> dag;  // Weight represents difficulty/complexity
    
    // Add courses
    dag.add_node("CS101");
    dag.add_node("CS201");
    dag.add_node("CS202");
    dag.add_node("CS301");
    dag.add_node("CS302");
    dag.add_node("CS401");
    dag.add_node("CS402");
    
    // Add prerequisites with complexity weights
    dag.add_edge("CS101", "CS201", 1);
    dag.add_edge("CS101", "CS202", 1);
    dag.add_edge("CS201", "CS301", 2);
    dag.add_edge("CS202", "CS302", 2);
    dag.add_edge("CS301", "CS401", 3);
    dag.add_edge("CS302", "CS401", 3);
    dag.add_edge("CS401", "CS402", 2);
    
    dag.print();
    
    // Find all possible paths to graduation
    auto paths = dag.find_all_paths("CS101", "CS402");
    std::cout << "\nPossible Course Sequences:" << std::endl;
    for (size_t i = 0; i < paths.size(); ++i) {
        std::cout << "Path " << i + 1 << ": ";
        for (size_t j = 0; j < paths[i].size(); ++j) {
            std::cout << paths[i][j];
            if (j < paths[i].size() - 1) std::cout << " -> ";
        }
        std::cout << std::endl;
    }
    
    // Find easiest path (minimum total complexity)
    auto easiest = dag.shortest_path("CS101", "CS402");
    if (easiest) {
        auto& [path, complexity] = *easiest;
        std::cout << "\nEasiest Path (Minimum Complexity):" << std::endl;
        std::cout << "  Path: ";
        for (const auto& course : path) {
            std::cout << course << " ";
        }
        std::cout << std::endl;
        std::cout << "  Total Complexity: " << complexity << std::endl;
    }
}

// Example 6: Pipeline Processing DAG
template<typename T>
class PipelineProcessor {
private:
    DAG<std::string, T> dag;
    std::unordered_map<std::string, T> results;
    
public:
    void add_stage(const std::string& name, T (*processor)(const T&)) {
        dag.add_node(name);
    }
    
    void add_dependency(const std::string& from, const std::string& to) {
        dag.add_edge(from, to);
    }
    
    T process(const std::string& start, const std::string& end, const T& input) {
        auto order = dag.topological_sort();
        results[start] = input;
        
        for (const auto& stage : order) {
            if (stage != start && results.find(stage) == results.end()) {
                // Compute based on predecessors
                auto preds = dag.get_predecessors(stage);
                T sum{};
                for (const auto& pred : preds) {
                    sum += results[pred];
                }
                results[stage] = sum;
            }
        }
        
        return results[end];
    }
};

void pipeline_example() {
    std::cout << "\n=== Pipeline Processing DAG Example ===" << std::endl;
    
    DAG<std::string, int> dag;
    
    // Processing stages
    dag.add_node("input");
    dag.add_node("filter");
    dag.add_node("transform");
    dag.add_node("aggregate");
    dag.add_node("output");
    
    // Pipeline dependencies
    dag.add_edge("input", "filter", 1);
    dag.add_edge("filter", "transform", 1);
    dag.add_edge("transform", "aggregate", 1);
    dag.add_edge("aggregate", "output", 1);
    
    // Parallel processing branches
    dag.add_node("parallel_1");
    dag.add_node("parallel_2");
    dag.add_edge("filter", "parallel_1", 1);
    dag.add_edge("filter", "parallel_2", 1);
    dag.add_edge("parallel_1", "aggregate", 1);
    dag.add_edge("parallel_2", "aggregate", 1);
    
    dag.print();
    
    // Analyze parallelism
    auto depths = dag.compute_depths();
    std::cout << "\nPipeline Stage Levels (Parallelism opportunities):" << std::endl;
    for (auto& [stage, depth] : depths) {
        std::cout << "  Level " << depth << ": " << stage << std::endl;
    }
}

int main() {
    std::cout << "=== DAG (Directed Acyclic Graph) Examples in C++ ===" << std::endl;
    std::cout << "Using Modern C++17/20 Features" << std::endl;
    
    try {
        expression_dag_example();
        task_scheduling_example();
        data_flow_example();
        build_system_example();
        course_prerequisite_example();
        pipeline_example();
        
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
    
    return 0;
}
