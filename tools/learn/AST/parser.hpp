// parser.hpp
#ifndef PARSER_HPP
#define PARSER_HPP

#include "ast.hpp"
#include <vector>
#include <string>

enum class TokenType {
    KEYWORD, IDENTIFIER, NUMBER, STRING,
    OPERATOR, PUNCTUATION, END
};

struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;
};

class Parser {
private:
    std::vector<Token> tokens;
    int current = 0;
    
    Token& peek() { return tokens[current]; }
    Token& advance() { return tokens[current++]; }
    bool is_at_end() { return current >= (int)tokens.size(); }
    
    bool match(TokenType type) {
        if (!is_at_end() && peek().type == type) {
            advance();
            return true;
        }
        return false;
    }
    
    ASTNode::Pointer parse_expression() {
        return parse_assignment();
    }
    
    ASTNode::Pointer parse_assignment() {
        auto node = parse_logical_or();
        
        if (match(TokenType::OPERATOR) && peek().value == "=") {
            advance(); // consume '='
            auto value = parse_assignment();
            
            if (auto* var_node = dynamic_cast<VariableNode*>(node.get())) {
                return std::make_unique<AssignmentNode>(var_node->name, std::move(value));
            }
        }
        
        return node;
    }
    
    ASTNode::Pointer parse_logical_or() {
        auto node = parse_logical_and();
        
        while (match(TokenType::OPERATOR) && peek().value == "||") {
            advance();
            auto right = parse_logical_and();
            node = std::make_unique<BinaryOpNode>(BinaryOp::Or, std::move(node), std::move(right));
        }
        
        return node;
    }
    
    ASTNode::Pointer parse_logical_and() {
        auto node = parse_equality();
        
        while (match(TokenType::OPERATOR) && peek().value == "&&") {
            advance();
            auto right = parse_equality();
            node = std::make_unique<BinaryOpNode>(BinaryOp::And, std::move(node), std::move(right));
        }
        
        return node;
    }
    
    ASTNode::Pointer parse_equality() {
        auto node = parse_comparison();
        
        while (match(TokenType::OPERATOR)) {
            if (peek().value == "==" || peek().value == "!=") {
                std::string op = peek().value;
                advance();
                auto right = parse_comparison();
                
                BinaryOp bin_op = (op == "==") ? BinaryOp::Equal : BinaryOp::NotEqual;
                node = std::make_unique<BinaryOpNode>(bin_op, std::move(node), std::move(right));
            } else {
                break;
            }
        }
        
        return node;
    }
    
    ASTNode::Pointer parse_comparison() {
        auto node = parse_term();
        
        while (match(TokenType::OPERATOR)) {
            if (peek().value == "<" || peek().value == ">" || 
                peek().value == "<=" || peek().value == ">=") {
                std::string op = peek().value;
                advance();
                auto right = parse_term();
                
                BinaryOp bin_op;
                if (op == "<") bin_op = BinaryOp::LessThan;
                else if (op == ">") bin_op = BinaryOp::GreaterThan;
                else if (op == "<=") bin_op = BinaryOp::LessEqual;
                else bin_op = BinaryOp::GreaterEqual;
                
                node = std::make_unique<BinaryOpNode>(bin_op, std::move(node), std::move(right));
            } else {
                break;
            }
        }
        
        return node;
    }
    
    ASTNode::Pointer parse_term() {
        auto node = parse_factor();
        
        while (match(TokenType::OPERATOR)) {
            if (peek().value == "+" || peek().value == "-") {
                std::string op = peek().value;
                advance();
                auto right = parse_factor();
                
                BinaryOp bin_op = (op == "+") ? BinaryOp::Add : BinaryOp::Subtract;
                node = std::make_unique<BinaryOpNode>(bin_op, std::move(node), std::move(right));
            } else {
                break;
            }
        }
        
        return node;
    }
    
    ASTNode::Pointer parse_factor() {
        auto node = parse_unary();
        
        while (match(TokenType::OPERATOR)) {
            if (peek().value == "*" || peek().value == "/" || peek().value == "%") {
                std::string op = peek().value;
                advance();
                auto right = parse_unary();
                
                BinaryOp bin_op;
                if (op == "*") bin_op = BinaryOp::Multiply;
                else if (op == "/") bin_op = BinaryOp::Divide;
                else bin_op = BinaryOp::Modulo;
                
                node = std::make_unique<BinaryOpNode>(bin_op, std::move(node), std::move(right));
            } else {
                break;
            }
        }
        
        return node;
    }
    
    ASTNode::Pointer parse_unary() {
        if (match(TokenType::OPERATOR)) {
            std::string op = peek().value;
            advance();
            auto operand = parse_unary();
            
            if (op == "-") {
                return std::make_unique<UnaryOpNode>(UnaryOp::Negate, std::move(operand));
            } else if (op == "!") {
                return std::make_unique<UnaryOpNode>(UnaryOp::Not, std::move(operand));
            }
        }
        
        return parse_primary();
    }
    
    ASTNode::Pointer parse_primary() {
        if (match(TokenType::NUMBER)) {
            return std::make_unique<LiteralNode>(std::stoi(tokens[current - 1].value));
        }
        
        if (match(TokenType::STRING)) {
            return std::make_unique<LiteralNode>(tokens[current - 1].value);
        }
        
        if (match(TokenType::IDENTIFIER)) {
            std::string name = tokens[current - 1].value;
            
            if (match(TokenType::PUNCTUATION) && peek().value == "(") {
                // Function call
                advance(); // consume '('
                auto call = std::make_unique<FunctionCallNode>(name);
                
                while (!match(TokenType::PUNCTUATION) || peek().value != ")") {
                    call->add_argument(parse_expression());
                    if (match(TokenType::PUNCTUATION) && peek().value == ",") {
                        advance();
                        continue;
                    }
                    break;
                }
                
                if (peek().value == ")") advance();
                
                return call;
            }
            
            return std::make_unique<VariableNode>(name);
        }
        
        if (match(TokenType::PUNCTUATION) && peek().value == "(") {
            advance(); // consume '('
            auto expr = parse_expression();
            if (match(TokenType::PUNCTUATION) && peek().value == ")") {
                advance();
            }
            return expr;
        }
        
        return nullptr;
    }
    
    ASTNode::Pointer parse_statement() {
        if (match(TokenType::KEYWORD)) {
            std::string keyword = tokens[current - 1].value;
            
            if (keyword == "if") {
                return parse_if_statement();
            } else if (keyword == "while") {
                return parse_while_statement();
            } else if (keyword == "return") {
                return parse_return_statement();
            } else if (keyword == "int" || keyword == "float" || keyword == "string") {
                return parse_variable_declaration(keyword);
            }
        }
        
        if (match(TokenType::PUNCTUATION) && peek().value == "{") {
            return parse_block();
        }
        
        return parse_expression();
    }
    
    ASTNode::Pointer parse_if_statement() {
        if (!match(TokenType::PUNCTUATION) || peek().value != "(") {
            return nullptr;
        }
        advance(); // consume '('
        auto condition = parse_expression();
        if (!match(TokenType::PUNCTUATION) || peek().value != ")") {
            return nullptr;
        }
        advance(); // consume ')'
        
        auto then_branch = parse_statement();
        auto if_stmt = std::make_unique<IfStatementNode>(std::move(condition), std::move(then_branch));
        
        if (match(TokenType::KEYWORD) && tokens[current - 1].value == "else") {
            auto else_branch = parse_statement();
            if_stmt->set_else_branch(std::move(else_branch));
        }
        
        return if_stmt;
    }
    
    ASTNode::Pointer parse_while_statement() {
        if (!match(TokenType::PUNCTUATION) || peek().value != "(") {
            return nullptr;
        }
        advance(); // consume '('
        auto condition = parse_expression();
        if (!match(TokenType::PUNCTUATION) || peek().value != ")") {
            return nullptr;
        }
        advance(); // consume ')'
        
        auto body = parse_statement();
        return std::make_unique<WhileStatementNode>(std::move(condition), std::move(body));
    }
    
    ASTNode::Pointer parse_return_statement() {
        auto value = parse_expression();
        return std::make_unique<ReturnStatementNode>(std::move(value));
    }
    
    ASTNode::Pointer parse_variable_declaration(const std::string& type) {
        if (!match(TokenType::IDENTIFIER)) {
            return nullptr;
        }
        std::string name = tokens[current - 1].value;
        
        ASTNode::Pointer initializer = nullptr;
        if (match(TokenType::OPERATOR) && peek().value == "=") {
            advance(); // consume '='
            initializer = parse_expression();
        }
        
        return std::make_unique<VariableDeclNode>(type, name, std::move(initializer));
    }
    
    ASTNode::Pointer parse_block() {
        if (!match(TokenType::PUNCTUATION) || peek().value != "{") {
            return nullptr;
        }
        advance(); // consume '{'
        
        auto block = std::make_unique<BlockStatementNode>();
        
        while (!is_at_end() && !(peek().type == TokenType::PUNCTUATION && peek().value == "}")) {
            auto stmt = parse_statement();
            if (stmt) {
                block->add_statement(std::move(stmt));
            }
            
            // Consume semicolon if present
            if (match(TokenType::PUNCTUATION) && peek().value == ";") {
                advance();
            }
        }
        
        if (match(TokenType::PUNCTUATION) && peek().value == "}") {
            advance();
        }
        
        return block;
    }
    
    ASTNode::Pointer parse_function_declaration() {
        if (!match(TokenType::KEYWORD)) {
            return nullptr;
        }
        
        std::string return_type = tokens[current - 1].value;
        
        if (!match(TokenType::IDENTIFIER)) {
            return nullptr;
        }
        
        std::string name = tokens[current - 1].value;
        auto function = std::make_unique<FunctionDeclNode>(return_type, name);
        
        if (!match(TokenType::PUNCTUATION) || peek().value != "(") {
            return nullptr;
        }
        advance(); // consume '('
        
        // Parse parameters
        while (!is_at_end() && !(peek().type == TokenType::PUNCTUATION && peek().value == ")")) {
            if (!match(TokenType::KEYWORD)) break;
            std::string param_type = tokens[current - 1].value;
            if (!match(TokenType::IDENTIFIER)) break;
            std::string param_name = tokens[current - 1].value;
            
            function->add_parameter(param_type, param_name);
            
            if (match(TokenType::PUNCTUATION) && peek().value == ",") {
                advance();
                continue;
            }
            break;
        }
        
        if (!match(TokenType::PUNCTUATION) || peek().value != ")") {
            return nullptr;
        }
        advance(); // consume ')'
        
        auto body = parse_block();
        function->set_body(std::move(body));
        
        return function;
    }
    
public:
    Parser(const std::vector<Token>& tokens) : tokens(tokens) {}
    
    ASTNode::Pointer parse() {
        auto program = std::make_unique<ProgramNode>();
        
        while (!is_at_end()) {
            if (peek().type == TokenType::KEYWORD) {
                auto func = parse_function_declaration();
                if (func) {
                    program->add_function(std::move(func));
                    continue;
                }
            }
            
            auto stmt = parse_statement();
            if (stmt) {
                program->add_global_statement(std::move(stmt));
            }
            
            // Consume semicolon if present
            if (match(TokenType::PUNCTUATION) && peek().value == ";") {
                advance();
            }
        }
        
        return program;
    }
};

#endif
