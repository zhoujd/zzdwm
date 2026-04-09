// ast_visitors.hpp
#ifndef AST_VISITORS_HPP
#define AST_VISITORS_HPP

#include "ast.hpp"
#include <iomanip>

// Pretty Printer Visitor
class PrettyPrinter : public ASTVisitor {
private:
    int indent_level = 0;
    std::stringstream output;
    
    void print_indent() {
        for (int i = 0; i < indent_level; ++i) {
            output << "  ";
        }
    }
    
public:
    void visit(ProgramNode& node) override {
        output << "Program:\n";
        indent_level++;
        for (auto& func : node.functions) {
            func->accept(*this);
        }
        for (auto& stmt : node.global_statements) {
            stmt->accept(*this);
        }
        indent_level--;
    }
    
    void visit(FunctionDeclNode& node) override {
        print_indent();
        output << "Function: " << node.return_type << " " << node.name << "(";
        for (size_t i = 0; i < node.parameters.size(); ++i) {
            if (i > 0) output << ", ";
            output << node.parameters[i].first << " " << node.parameters[i].second;
        }
        output << ")\n";
        indent_level++;
        if (node.body) {
            node.body->accept(*this);
        }
        indent_level--;
    }
    
    void visit(VariableDeclNode& node) override {
        print_indent();
        output << "Variable: " << node.type << " " << node.name;
        if (node.initializer) {
            output << " = ";
            node.initializer->accept(*this);
        }
        output << "\n";
    }
    
    void visit(IfStatementNode& node) override {
        print_indent();
        output << "IfStatement:\n";
        indent_level++;
        print_indent();
        output << "Condition: ";
        node.condition->accept(*this);
        output << "\n";
        print_indent();
        output << "Then:\n";
        indent_level++;
        node.then_branch->accept(*this);
        indent_level--;
        if (node.else_branch) {
            print_indent();
            output << "Else:\n";
            indent_level++;
            node.else_branch->accept(*this);
            indent_level--;
        }
        indent_level--;
    }
    
    void visit(WhileStatementNode& node) override {
        print_indent();
        output << "WhileStatement:\n";
        indent_level++;
        print_indent();
        output << "Condition: ";
        node.condition->accept(*this);
        output << "\n";
        print_indent();
        output << "Body:\n";
        indent_level++;
        node.body->accept(*this);
        indent_level--;
        indent_level--;
    }
    
    void visit(BlockStatementNode& node) override {
        print_indent();
        output << "Block:\n";
        indent_level++;
        for (auto& stmt : node.statements) {
            stmt->accept(*this);
        }
        indent_level--;
    }
    
    void visit(BinaryOpNode& node) override {
        output << "(";
        node.left->accept(*this);
        switch (node.op) {
            case BinaryOp::Add: output << " + "; break;
            case BinaryOp::Subtract: output << " - "; break;
            case BinaryOp::Multiply: output << " * "; break;
            case BinaryOp::Divide: output << " / "; break;
            default: output << " ? "; break;
        }
        node.right->accept(*this);
        output << ")";
    }
    
    void visit(LiteralNode& node) override {
        output << node.to_string();
    }
    
    void visit(VariableNode& node) override {
        output << node.name;
    }
    
    void visit(AssignmentNode& node) override {
        print_indent();
        output << "Assignment: " << node.variable << " = ";
        node.value->accept(*this);
        output << "\n";
    }
    
    void visit(FunctionCallNode& node) override {
        output << node.function_name << "(";
        for (size_t i = 0; i < node.arguments.size(); ++i) {
            if (i > 0) output << ", ";
            node.arguments[i]->accept(*this);
        }
        output << ")";
    }
    
    void visit(ReturnStatementNode& node) override {
        print_indent();
        output << "Return";
        if (node.value) {
            output << " ";
            node.value->accept(*this);
        }
        output << "\n";
    }
    
    std::string get_output() const { return output.str(); }
};

// Semantic Analyzer Visitor
class SemanticAnalyzer : public ASTVisitor {
private:
    std::unordered_map<std::string, std::string> symbol_table;
    std::vector<std::string> errors;
    std::string current_function;
    
public:
    void visit(ProgramNode& node) override {
        for (auto& func : node.functions) {
            func->accept(*this);
        }
    }
    
    void visit(FunctionDeclNode& node) override {
        current_function = node.name;
        symbol_table.clear();
        
        // Add parameters to symbol table
        for (const auto& param : node.parameters) {
            symbol_table[param.second] = param.first;
        }
        
        if (node.body) {
            node.body->accept(*this);
        }
        
        current_function.clear();
    }
    
    void visit(VariableDeclNode& node) override {
        if (symbol_table.find(node.name) != symbol_table.end()) {
            errors.push_back("Variable '" + node.name + "' already declared");
        } else {
            symbol_table[node.name] = node.type;
        }
        
        if (node.initializer) {
            node.initializer->accept(*this);
        }
    }
    
    void visit(VariableNode& node) override {
        if (symbol_table.find(node.name) == symbol_table.end()) {
            errors.push_back("Variable '" + node.name + "' not declared");
        }
    }
    
    void visit(AssignmentNode& node) override {
        if (symbol_table.find(node.variable) == symbol_table.end()) {
            errors.push_back("Variable '" + node.variable + "' not declared");
        }
        if (node.value) {
            node.value->accept(*this);
        }
    }
    
    bool has_errors() const { return !errors.empty(); }
    
    void print_errors() const {
        for (const auto& error : errors) {
            std::cout << "Semantic Error: " << error << std::endl;
        }
    }
};

// Code Generation Visitor (to LLVM IR)
class CodeGenerator : public ASTVisitor {
private:
    std::stringstream ir;
    int temp_counter = 0;
    
    std::string new_temp() {
        return "%t" + std::to_string(temp_counter++);
    }
    
public:
    void visit(LiteralNode& node) override {
        if (node.type == LiteralType::Integer) {
            ir << std::get<int>(node.value);
        } else if (node.type == LiteralType::Float) {
            ir << std::get<double>(node.value);
        }
    }
    
    void visit(BinaryOpNode& node) override {
        std::string left_temp = new_temp();
        std::string right_temp = new_temp();
        
        ir << left_temp << " = ";
        node.left->accept(*this);
        ir << "\n";
        
        ir << right_temp << " = ";
        node.right->accept(*this);
        ir << "\n";
        
        std::string result = new_temp();
        ir << result << " = ";
        
        switch (node.op) {
            case BinaryOp::Add:
                ir << "add i32 " << left_temp << ", " << right_temp;
                break;
            case BinaryOp::Subtract:
                ir << "sub i32 " << left_temp << ", " << right_temp;
                break;
            case BinaryOp::Multiply:
                ir << "mul i32 " << left_temp << ", " << right_temp;
                break;
            case BinaryOp::Divide:
                ir << "sdiv i32 " << left_temp << ", " << right_temp;
                break;
            default:
                break;
        }
        ir << "\n";
    }
    
    std::string get_ir() const { return ir.str(); }
};

#endif
