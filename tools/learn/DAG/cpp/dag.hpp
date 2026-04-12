// dag.hpp
#ifndef DAG_HPP
#define DAG_HPP

#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <stack>
#include <memory>
#include <algorithm>
#include <optional>
#include <variant>
#include <concepts>
#include <ranges>
#include <fmt/format.h>  // Optional: for better formatting
#include <functional>

template<typename T>
concept Hashable = requires(T a) {
    { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
};

template<typename T, typename Weight = int>
class DAG {
private:
    struct Node {
        T id;
        std::unordered_map<Node*, Weight> children;
        std::unordered_map<Node*, Weight> parents;
        std::optional<Weight> distance;
        int topological_order = -1;
        
        explicit Node(const T& id) : id(id) {}
    };
    
    std::unordered_map<T, std::unique_ptr<Node>> nodes;
    std::vector<Node*> topological_order_cache;
    bool is_topological_valid = false;
    
    // DFS utilities
    bool has_cycle_util(Node* node, std::unordered_set<Node*>& visited, 
                        std::unordered_set<Node*>& rec_stack) {
        visited.insert(node);
        rec_stack.insert(node);
        
        for (auto& [child, weight] : node->children) {
            if (!visited.contains(child)) {
                if (has_cycle_util(child, visited, rec_stack))
                    return true;
            } else if (rec_stack.contains(child)) {
                return true;
            }
        }
        
        rec_stack.erase(node);
        return false;
    }
    
    void topological_sort_util(Node* node, std::unordered_set<Node*>& visited, 
                               std::vector<Node*>& order) {
        visited.insert(node);
        
        for (auto& [child, weight] : node->children) {
            if (!visited.contains(child)) {
                topological_sort_util(child, visited, order);
            }
        }
        
        order.push_back(node);
    }
    
public:
    DAG() = default;
    
    // Add a node to the DAG
    bool add_node(const T& id) {
        if (nodes.contains(id)) {
            return false;
        }
        nodes[id] = std::make_unique<Node>(id);
        is_topological_valid = false;
        return true;
    }
    
    // Add edge with optional weight
    bool add_edge(const T& from_id, const T& to_id, Weight weight = Weight{}) {
        if (!nodes.contains(from_id) || !nodes.contains(to_id)) {
            return false;
        }
        
        Node* from = nodes[from_id].get();
        Node* to = nodes[to_id].get();
        
        // Check if edge already exists
        if (from->children.contains(to)) {
            return false;
        }
        
        // Add edge
        from->children[to] = weight;
        to->parents[from] = weight;
        
        // Check for cycles
        if (has_cycle()) {
            // Rollback if cycle detected
            from->children.erase(to);
            to->parents.erase(from);
            return false;
        }
        
        is_topological_valid = false;
        return true;
    }
    
    // Remove edge
    bool remove_edge(const T& from_id, const T& to_id) {
        if (!nodes.contains(from_id) || !nodes.contains(to_id)) {
            return false;
        }
        
        Node* from = nodes[from_id].get();
        Node* to = nodes[to_id].get();
        
        if (!from->children.contains(to)) {
            return false;
        }
        
        from->children.erase(to);
        to->parents.erase(from);
        is_topological_valid = false;
        return true;
    }
    
    // Remove node and all connected edges
    bool remove_node(const T& id) {
        if (!nodes.contains(id)) {
            return false;
        }
        
        Node* node = nodes[id].get();
        
        // Remove all edges to children
        for (auto& [child, weight] : node->children) {
            child->parents.erase(node);
        }
        
        // Remove all edges from parents
        for (auto& [parent, weight] : node->parents) {
            parent->children.erase(node);
        }
        
        nodes.erase(id);
        is_topological_valid = false;
        return true;
    }
    
    // Check if graph has cycles
    bool has_cycle() const {
        std::unordered_set<Node*> visited;
        std::unordered_set<Node*> rec_stack;
        
        for (auto& [id, node] : nodes) {
            if (!visited.contains(node.get())) {
                if (const_cast<DAG*>(this)->has_cycle_util(node.get(), visited, rec_stack)) {
                    return true;
                }
            }
        }
        return false;
    }
    
    // Get topological sort
    std::vector<T> topological_sort() {
        if (has_cycle()) {
            throw std::runtime_error("Cannot sort cyclic graph");
        }
        
        if (!is_topological_valid) {
            topological_order_cache.clear();
            std::unordered_set<Node*> visited;
            
            for (auto& [id, node] : nodes) {
                if (!visited.contains(node.get())) {
                    topological_sort_util(node.get(), visited, topological_order_cache);
                }
            }
            
            std::reverse(topological_order_cache.begin(), topological_order_cache.end());
            
            // Assign topological order indices
            for (size_t i = 0; i < topological_order_cache.size(); ++i) {
                topological_order_cache[i]->topological_order = i;
            }
            
            is_topological_valid = true;
        }
        
        std::vector<T> result;
        result.reserve(topological_order_cache.size());
        for (auto* node : topological_order_cache) {
            result.push_back(node->id);
        }
        return result;
    }
    
    // Kahn's algorithm for topological sort (alternative)
    std::vector<T> topological_sort_kahn() {
        if (has_cycle()) {
            throw std::runtime_error("Cannot sort cyclic graph");
        }
        
        std::unordered_map<Node*, int> in_degree;
        std::queue<Node*> queue;
        std::vector<T> result;
        
        // Calculate in-degrees
        for (auto& [id, node] : nodes) {
            in_degree[node.get()] = node->parents.size();
            if (node->parents.empty()) {
                queue.push(node.get());
            }
        }
        
        // Process nodes
        while (!queue.empty()) {
            Node* current = queue.front();
            queue.pop();
            result.push_back(current->id);
            
            for (auto& [child, weight] : current->children) {
                if (--in_degree[child] == 0) {
                    queue.push(child);
                }
            }
        }
        
        return result;
    }
    
    // Find longest path (critical path)
    std::optional<std::pair<std::vector<T>, Weight>> longest_path(const T& start_id, const T& end_id) {
        if (!nodes.contains(start_id) || !nodes.contains(end_id)) {
            return std::nullopt;
        }
        
        // First ensure we have topological order
        auto topo_order = topological_sort();
        
        // Initialize distances
        std::unordered_map<Node*, Weight> dist;
        std::unordered_map<Node*, Node*> prev;
        
        for (auto& [id, node] : nodes) {
            dist[node.get()] = std::numeric_limits<Weight>::min();
            prev[node.get()] = nullptr;
        }
        
        Node* start = nodes[start_id].get();
        Node* end = nodes[end_id].get();
        dist[start] = Weight{};
        
        // Process in topological order
        for (const T& id : topo_order) {
            Node* u = nodes[id].get();
            if (dist[u] != std::numeric_limits<Weight>::min()) {
                for (auto& [v, weight] : u->children) {
                    if (dist[v] < dist[u] + weight) {
                        dist[v] = dist[u] + weight;
                        prev[v] = u;
                    }
                }
            }
        }
        
        if (dist[end] == std::numeric_limits<Weight>::min()) {
            return std::nullopt;
        }
        
        // Reconstruct path
        std::vector<T> path;
        Node* current = end;
        while (current != nullptr) {
            path.push_back(current->id);
            current = prev[current];
        }
        std::reverse(path.begin(), path.end());
        
        return std::make_pair(path, dist[end]);
    }
    
    // Find shortest path (using topological order)
    std::optional<std::pair<std::vector<T>, Weight>> shortest_path(const T& start_id, const T& end_id) {
        if (!nodes.contains(start_id) || !nodes.contains(end_id)) {
            return std::nullopt;
        }
        
        auto topo_order = topological_sort();
        
        std::unordered_map<Node*, Weight> dist;
        std::unordered_map<Node*, Node*> prev;
        
        for (auto& [id, node] : nodes) {
            dist[node.get()] = std::numeric_limits<Weight>::max();
            prev[node.get()] = nullptr;
        }
        
        Node* start = nodes[start_id].get();
        Node* end = nodes[end_id].get();
        dist[start] = Weight{};
        
        for (const T& id : topo_order) {
            Node* u = nodes[id].get();
            if (dist[u] != std::numeric_limits<Weight>::max()) {
                for (auto& [v, weight] : u->children) {
                    if (dist[v] > dist[u] + weight) {
                        dist[v] = dist[u] + weight;
                        prev[v] = u;
                    }
                }
            }
        }
        
        if (dist[end] == std::numeric_limits<Weight>::max()) {
            return std::nullopt;
        }
        
        std::vector<T> path;
        Node* current = end;
        while (current != nullptr) {
            path.push_back(current->id);
            current = prev[current];
        }
        std::reverse(path.begin(), path.end());
        
        return std::make_pair(path, dist[end]);
    }
    
    // Get all paths between two nodes
    std::vector<std::vector<T>> find_all_paths(const T& start_id, const T& end_id) {
        if (!nodes.contains(start_id) || !nodes.contains(end_id)) {
            return {};
        }
        
        std::vector<std::vector<T>> all_paths;
        std::vector<T> current_path;
        std::unordered_set<Node*> visited;
        
        Node* start = nodes[start_id].get();
        Node* end = nodes[end_id].get();
        
        std::function<void(Node*)> dfs = [&](Node* node) {
            current_path.push_back(node->id);
            visited.insert(node);
            
            if (node == end) {
                all_paths.push_back(current_path);
            } else {
                for (auto& [child, weight] : node->children) {
                    if (!visited.contains(child)) {
                        dfs(child);
                    }
                }
            }
            
            current_path.pop_back();
            visited.erase(node);
        };
        
        dfs(start);
        return all_paths;
    }
    
    // Compute node depths
    std::unordered_map<T, int> compute_depths() {
        auto topo_order = topological_sort();
        std::unordered_map<T, int> depths;
        
        for (const T& id : topo_order) {
            Node* node = nodes[id].get();
            int max_depth = 0;
            
            for (auto& [parent, weight] : node->parents) {
                max_depth = std::max(max_depth, depths[parent->id] + 1);
            }
            
            depths[id] = max_depth;
        }
        
        return depths;
    }
    
    // Get predecessors
    std::vector<T> get_predecessors(const T& id) const {
        if (!nodes.contains(id)) {
            return {};
        }
        
        std::vector<T> preds;
        for (auto& [parent, weight] : nodes.at(id)->parents) {
            preds.push_back(parent->id);
        }
        return preds;
    }
    
    // Get successors
    std::vector<T> get_successors(const T& id) const {
        if (!nodes.contains(id)) {
            return {};
        }
        
        std::vector<T> succs;
        for (auto& [child, weight] : nodes.at(id)->children) {
            succs.push_back(child->id);
        }
        return succs;
    }
    
    // Get edge weight
    std::optional<Weight> get_edge_weight(const T& from_id, const T& to_id) const {
        if (!nodes.contains(from_id) || !nodes.contains(to_id)) {
            return std::nullopt;
        }
        
        Node* from = nodes.at(from_id).get();
        Node* to = nodes.at(to_id).get();
        
        if (!from->children.contains(to)) {
            return std::nullopt;
        }
        
        return from->children.at(to);
    }
    
    // Get all nodes
    std::vector<T> get_all_nodes() const {
        std::vector<T> result;
        result.reserve(nodes.size());
        for (auto& [id, node] : nodes) {
            result.push_back(id);
        }
        return result;
    }
    
    // Check if node exists
    bool has_node(const T& id) const {
        return nodes.contains(id);
    }
    
    // Get node count
    size_t size() const {
        return nodes.size();
    }
    
    // Check if graph is empty
    bool empty() const {
        return nodes.empty();
    }
    
    // Clear graph
    void clear() {
        nodes.clear();
        topological_order_cache.clear();
        is_topological_valid = false;
    }
    
    // Print DAG structure
    void print() const {
        std::cout << "=== DAG Structure ===" << std::endl;
        std::cout << "Nodes: " << nodes.size() << std::endl;
        
        for (auto& [id, node] : nodes) {
            std::cout << "\nNode: " << id;
            if (node->topological_order != -1) {
                std::cout << " (order: " << node->topological_order << ")";
            }
            std::cout << std::endl;
            
            if (!node->parents.empty()) {
                std::cout << "  Parents: ";
                for (auto& [parent, weight] : node->parents) {
                    std::cout << parent->id;
                    if constexpr (!std::is_same_v<Weight, void>) {
                        std::cout << "(" << weight << ")";
                    }
                    std::cout << " ";
                }
                std::cout << std::endl;
            }
            
            if (!node->children.empty()) {
                std::cout << "  Children: ";
                for (auto& [child, weight] : node->children) {
                    std::cout << child->id;
                    if constexpr (!std::is_same_v<Weight, void>) {
                        std::cout << "(" << weight << ")";
                    }
                    std::cout << " ";
                }
                std::cout << std::endl;
            }
        }
    }
};

#endif // DAG_HPP
