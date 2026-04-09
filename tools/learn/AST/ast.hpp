// ast.hpp
#ifndef AST_HPP
#define AST_HPP

#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <variant>
#include <unordered_map>
#include <functional>
#include <sstream>
#include <any>
#include <optional>

// Forward declarations
class ASTNode;
class ASTVisitor;

// Base AST Node class
class ASTNode {
public:
    using Pointer = std::unique_ptr<ASTNode>;
    using NodeList = std::vector<Pointer>;
    
    int line = 0;
    int column = 0;
    
    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor& visitor) = 0;
    virtual std::string to_string() const = 0;
    virtual std::string get_node_type() const = 0;
};

// AST Visitor pattern
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    
    virtual void visit(class ProgramNode& node) {}
    virtual void visit(class FunctionDeclNode& node) {}
    virtual void visit(class VariableDeclNode& node) {}
    virtual void visit(class IfStatementNode& node) {}
    virtual void visit(class WhileStatementNode& node) {}
    virtual void visit(class ReturnStatementNode& node) {}
    virtual void visit(class BlockStatementNode& node) {}
    virtual void visit(class BinaryOpNode& node) {}
    virtual void visit(class UnaryOpNode& node) {}
    virtual void visit(class LiteralNode& node) {}
    virtual void visit(class VariableNode& node) {}
    virtual void visit(class FunctionCallNode& node) {}
    virtual void visit(class AssignmentNode& node) {}
};

// Expression nodes
enum class BinaryOp {
    Add, Subtract, Multiply, Divide, Modulo,
    Equal, NotEqual, LessThan, GreaterThan,
    LessEqual, GreaterEqual, And, Or
};

enum class UnaryOp {
    Negate, Not, Increment, Decrement
};

enum class LiteralType {
    Integer, Float, String, Boolean, Null
};

// Literal Node
class LiteralNode : public ASTNode {
public:
    LiteralType type;
    std::variant<int, double, std::string, bool> value;
    
    LiteralNode(int val) : type(LiteralType::Integer), value(val) {}
    LiteralNode(double val) : type(LiteralType::Float), value(val) {}
    LiteralNode(const std::string& val) : type(LiteralType::String), value(val) {}
    LiteralNode(bool val) : type(LiteralType::Boolean), value(val) {}
    
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    
    std::string to_string() const override {
        std::stringstream ss;
        switch (type) {
            case LiteralType::Integer:
                ss << std::get<int>(value);
                break;
            case LiteralType::Float:
                ss << std::get<double>(value);
                break;
            case LiteralType::String:
                ss << "\"" << std::get<std::string>(value) << "\"";
                break;
            case LiteralType::Boolean:
                ss << (std::get<bool>(value) ? "true" : "false");
                break;
            case LiteralType::Null:
                ss << "null";
                break;
        }
        return ss.str();
    }
    
    std::string get_node_type() const override { return "Literal"; }
};

// Variable Node
class VariableNode : public ASTNode {
public:
    std::string name;
    
    VariableNode(const std::string& name) : name(name) {}
    
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    
    std::string to_string() const override { return name; }
    
    std::string get_node_type() const override { return "Variable"; }
};

// Binary Operation Node
class BinaryOpNode : public ASTNode {
public:
    BinaryOp op;
    ASTNode::Pointer left;
    ASTNode::Pointer right;
    
    BinaryOpNode(BinaryOp op, ASTNode::Pointer left, ASTNode::Pointer right)
        : op(op), left(std::move(left)), right(std::move(right)) {}
    
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    
    std::string to_string() const override {
        std::string op_str;
        switch (op) {
            case BinaryOp::Add: op_str = "+"; break;
            case BinaryOp::Subtract: op_str = "-"; break;
            case BinaryOp::Multiply: op_str = "*"; break;
            case BinaryOp::Divide: op_str = "/"; break;
            case BinaryOp::Modulo: op_str = "%"; break;
            case BinaryOp::Equal: op_str = "=="; break;
            case BinaryOp::NotEqual: op_str = "!="; break;
            case BinaryOp::LessThan: op_str = "<"; break;
            case BinaryOp::GreaterThan: op_str = ">"; break;
            case BinaryOp::LessEqual: op_str = "<="; break;
            case BinaryOp::GreaterEqual: op_str = ">="; break;
            case BinaryOp::And: op_str = "&&"; break;
            case BinaryOp::Or: op_str = "||"; break;
        }
        return "(" + left->to_string() + " " + op_str + " " + right->to_string() + ")";
    }
    
    std::string get_node_type() const override { return "BinaryOp"; }
};

// Unary Operation Node
class UnaryOpNode : public ASTNode {
public:
    UnaryOp op;
    ASTNode::Pointer operand;
    
    UnaryOpNode(UnaryOp op, ASTNode::Pointer operand)
        : op(op), operand(std::move(operand)) {}
    
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    
    std::string to_string() const override {
        std::string op_str;
        switch (op) {
            case UnaryOp::Negate: op_str = "-"; break;
            case UnaryOp::Not: op_str = "!"; break;
            case UnaryOp::Increment: op_str = "++"; break;
            case UnaryOp::Decrement: op_str = "--"; break;
        }
        return op_str + operand->to_string();
    }
    
    std::string get_node_type() const override { return "UnaryOp"; }
};

// Assignment Node
class AssignmentNode : public ASTNode {
public:
    std::string variable;
    ASTNode::Pointer value;
    
    AssignmentNode(const std::string& var, ASTNode::Pointer val)
        : variable(var), value(std::move(val)) {}
    
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    
    std::string to_string() const override {
        return variable + " = " + value->to_string();
    }
    
    std::string get_node_type() const override { return "Assignment"; }
};

// Function Call Node
class FunctionCallNode : public ASTNode {
public:
    std::string function_name;
    std::vector<ASTNode::Pointer> arguments;
    
    FunctionCallNode(const std::string& name) : function_name(name) {}
    
    void add_argument(ASTNode::Pointer arg) {
        arguments.push_back(std::move(arg));
    }
    
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    
    std::string to_string() const override {
        std::stringstream ss;
        ss << function_name << "(";
        for (size_t i = 0; i < arguments.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << arguments[i]->to_string();
        }
        ss << ")";
        return ss.str();
    }
    
    std::string get_node_type() const override { return "FunctionCall"; }
};

// Statement nodes
class BlockStatementNode : public ASTNode {
public:
    std::vector<ASTNode::Pointer> statements;
    
    void add_statement(ASTNode::Pointer stmt) {
        statements.push_back(std::move(stmt));
    }
    
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    
    std::string to_string() const override {
        std::stringstream ss;
        ss << "{\n";
        for (const auto& stmt : statements) {
            ss << "  " << stmt->to_string() << ";\n";
        }
        ss << "}";
        return ss.str();
    }
    
    std::string get_node_type() const override { return "Block"; }
};

// Variable Declaration Node
class VariableDeclNode : public ASTNode {
public:
    std::string type;
    std::string name;
    ASTNode::Pointer initializer;
    
    VariableDeclNode(const std::string& type, const std::string& name, ASTNode::Pointer init = nullptr)
        : type(type), name(name), initializer(std::move(init)) {}
    
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    
    std::string to_string() const override {
        std::string result = type + " " + name;
        if (initializer) {
            result += " = " + initializer->to_string();
        }
        return result;
    }
    
    std::string get_node_type() const override { return "VariableDecl"; }
};

// Function Declaration Node
class FunctionDeclNode : public ASTNode {
public:
    std::string return_type;
    std::string name;
    std::vector<std::pair<std::string, std::string>> parameters;
    ASTNode::Pointer body;
    
    FunctionDeclNode(const std::string& ret_type, const std::string& name)
        : return_type(ret_type), name(name) {}
    
    void add_parameter(const std::string& type, const std::string& name) {
        parameters.emplace_back(type, name);
    }
    
    void set_body(ASTNode::Pointer func_body) {
        body = std::move(func_body);
    }
    
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    
    std::string to_string() const override {
        std::stringstream ss;
        ss << return_type << " " << name << "(";
        for (size_t i = 0; i < parameters.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << parameters[i].first << " " << parameters[i].second;
        }
        ss << ") ";
        if (body) {
            ss << body->to_string();
        }
        return ss.str();
    }
    
    std::string get_node_type() const override { return "FunctionDecl"; }
};

// If Statement Node
class IfStatementNode : public ASTNode {
public:
    ASTNode::Pointer condition;
    ASTNode::Pointer then_branch;
    ASTNode::Pointer else_branch;
    
    IfStatementNode(ASTNode::Pointer cond, ASTNode::Pointer then_stmt)
        : condition(std::move(cond)), then_branch(std::move(then_stmt)) {}
    
    void set_else_branch(ASTNode::Pointer else_stmt) {
        else_branch = std::move(else_stmt);
    }
    
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    
    std::string to_string() const override {
        std::stringstream ss;
        ss << "if (" << condition->to_string() << ") " << then_branch->to_string();
        if (else_branch) {
            ss << " else " << else_branch->to_string();
        }
        return ss.str();
    }
    
    std::string get_node_type() const override { return "IfStatement"; }
};

// While Statement Node
class WhileStatementNode : public ASTNode {
public:
    ASTNode::Pointer condition;
    ASTNode::Pointer body;
    
    WhileStatementNode(ASTNode::Pointer cond, ASTNode::Pointer stmt_body)
        : condition(std::move(cond)), body(std::move(stmt_body)) {}
    
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    
    std::string to_string() const override {
        return "while (" + condition->to_string() + ") " + body->to_string();
    }
    
    std::string get_node_type() const override { return "WhileStatement"; }
};

// Return Statement Node
class ReturnStatementNode : public ASTNode {
public:
    ASTNode::Pointer value;
    
    ReturnStatementNode(ASTNode::Pointer val = nullptr) : value(std::move(val)) {}
    
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    
    std::string to_string() const override {
        if (value) {
            return "return " + value->to_string();
        }
        return "return";
    }
    
    std::string get_node_type() const override { return "ReturnStatement"; }
};

// Program Node (Root)
class ProgramNode : public ASTNode {
public:
    std::vector<ASTNode::Pointer> functions;
    std::vector<ASTNode::Pointer> global_statements;
    
    void add_function(ASTNode::Pointer func) {
        functions.push_back(std::move(func));
    }
    
    void add_global_statement(ASTNode::Pointer stmt) {
        global_statements.push_back(std::move(stmt));
    }
    
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    
    std::string to_string() const override {
        std::stringstream ss;
        for (const auto& func : functions) {
            ss << func->to_string() << "\n\n";
        }
        for (const auto& stmt : global_statements) {
            ss << stmt->to_string() << ";\n";
        }
        return ss.str();
    }
    
    std::string get_node_type() const override { return "Program"; }
};

#endif //AST_HPP
