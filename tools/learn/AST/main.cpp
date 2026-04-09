// main.cpp
#include "ast.hpp"
#include "ast_visitors.hpp"
#include "parser.hpp"
#include <iostream>
#include <memory>

// Function to create a sample AST manually
std::unique_ptr<ProgramNode> create_sample_ast() {
    auto program = std::make_unique<ProgramNode>();
    
    // Create a function: int add(int x, int y) { return x + y; }
    auto add_func = std::make_unique<FunctionDeclNode>("int", "add");
    add_func->add_parameter("int", "x");
    add_func->add_parameter("int", "y");
    
    auto body = std::make_unique<BlockStatementNode>();
    auto return_stmt = std::make_unique<ReturnStatementNode>(
        std::make_unique<BinaryOpNode>(
            BinaryOp::Add,
            std::make_unique<VariableNode>("x"),
            std::make_unique<VariableNode>("y")
        )
    );
    body->add_statement(std::move(return_stmt));
    add_func->set_body(std::move(body));
    
    program->add_function(std::move(add_func));
    
    // Create main function
    auto main_func = std::make_unique<FunctionDeclNode>("int", "main");
    auto main_body = std::make_unique<BlockStatementNode>();
    
    // int result = add(5, 3);
    auto var_decl = std::make_unique<VariableDeclNode>("int", "result",
        std::make_unique<FunctionCallNode>("add")
    );
    auto* call = dynamic_cast<FunctionCallNode*>(var_decl->initializer.get());
    if (call) {
        call->add_argument(std::make_unique<LiteralNode>(5));
        call->add_argument(std::make_unique<LiteralNode>(3));
    }
    main_body->add_statement(std::move(var_decl));
    
    // return result;
    main_body->add_statement(std::make_unique<ReturnStatementNode>(
        std::make_unique<VariableNode>("result")
    ));
    
    main_func->set_body(std::move(main_body));
    program->add_function(std::move(main_func));
    
    return program;
}

// Example 1: Expression AST
void expression_ast_example() {
    std::cout << "\n=== Expression AST Example ===" << std::endl;
    std::cout << "Expression: (a + b) * (c - d)" << std::endl;
    
    // Build AST: (a + b) * (c - d)
    auto ast = std::make_unique<BinaryOpNode>(
        BinaryOp::Multiply,
        std::make_unique<BinaryOpNode>(
            BinaryOp::Add,
            std::make_unique<VariableNode>("a"),
            std::make_unique<VariableNode>("b")
        ),
        std::make_unique<BinaryOpNode>(
            BinaryOp::Subtract,
            std::make_unique<VariableNode>("c"),
            std::make_unique<VariableNode>("d")
        )
    );
    
    std::cout << "AST Representation: " << ast->to_string() << std::endl;
    
    // Pretty print
    PrettyPrinter printer;
    ast->accept(printer);
    std::cout << "\nPretty Printed:\n" << printer.get_output() << std::endl;
}

// Example 2: Control Flow AST
void control_flow_ast_example() {
    std::cout << "\n=== Control Flow AST Example ===" << std::endl;
    std::cout << "Code: if (x > 0) { y = 1; } else { y = -1; }" << std::endl;
    
    auto if_stmt = std::make_unique<IfStatementNode>(
        std::make_unique<BinaryOpNode>(
            BinaryOp::GreaterThan,
            std::make_unique<VariableNode>("x"),
            std::make_unique<LiteralNode>(0)
        ),
        std::make_unique<BlockStatementNode>()
    );
    
    auto* then_block = dynamic_cast<BlockStatementNode*>(if_stmt->then_branch.get());
    then_block->add_statement(std::make_unique<AssignmentNode>("y", std::make_unique<LiteralNode>(1)));
    
    auto else_block = std::make_unique<BlockStatementNode>();
    else_block->add_statement(std::make_unique<AssignmentNode>("y", std::make_unique<LiteralNode>(-1)));
    if_stmt->set_else_branch(std::move(else_block));
    
    PrettyPrinter printer;
    if_stmt->accept(printer);
    std::cout << "\n" << printer.get_output() << std::endl;
}

// Example 3: Function AST
void function_ast_example() {
    std::cout << "\n=== Function AST Example ===" << std::endl;
    std::cout << "Function: int factorial(int n) { if (n <= 1) return 1; else return n * factorial(n-1); }" << std::endl;
    
    auto factorial = std::make_unique<FunctionDeclNode>("int", "factorial");
    factorial->add_parameter("int", "n");
    
    auto body = std::make_unique<BlockStatementNode>();
    
    auto if_stmt = std::make_unique<IfStatementNode>(
        std::make_unique<BinaryOpNode>(
            BinaryOp::LessEqual,
            std::make_unique<VariableNode>("n"),
            std::make_unique<LiteralNode>(1)
        ),
        std::make_unique<ReturnStatementNode>(std::make_unique<LiteralNode>(1))
    );
    
    auto else_branch = std::make_unique<ReturnStatementNode>(
        std::make_unique<BinaryOpNode>(
            BinaryOp::Multiply,
            std::make_unique<VariableNode>("n"),
            std::make_unique<FunctionCallNode>("factorial")
        )
    );
    
    auto* call = dynamic_cast<FunctionCallNode*>(
        dynamic_cast<BinaryOpNode*>(else_branch->value.get())->right.get()
    );
    if (call) {
        call->add_argument(std::make_unique<BinaryOpNode>(
            BinaryOp::Subtract,
            std::make_unique<VariableNode>("n"),
            std::make_unique<LiteralNode>(1)
        ));
    }
    
    if_stmt->set_else_branch(std::move(else_branch));
    body->add_statement(std::move(if_stmt));
    factorial->set_body(std::move(body));
    
    PrettyPrinter printer;
    factorial->accept(printer);
    std::cout << "\n" << printer.get_output() << std::endl;
}

// Example 4: Complete Program AST
void complete_program_example() {
    std::cout << "\n=== Complete Program AST Example ===" << std::endl;
    
    auto program = create_sample_ast();
    
    PrettyPrinter printer;
    program->accept(printer);
    std::cout << printer.get_output() << std::endl;
    
    // Semantic analysis
    SemanticAnalyzer analyzer;
    program->accept(analyzer);
    
    if (analyzer.has_errors()) {
        analyzer.print_errors();
    } else {
        std::cout << "Semantic analysis passed!" << std::endl;
    }
}

// Example 5: Nested Expressions
void nested_expression_example() {
    std::cout << "\n=== Nested Expression AST Example ===" << std::endl;
    
    // Complex expression: ((a + b) * (c - d)) / (e + (f * g))
    auto ast = std::make_unique<BinaryOpNode>(
        BinaryOp::Divide,
        std::make_unique<BinaryOpNode>(
            BinaryOp::Multiply,
            std::make_unique<BinaryOpNode>(
                BinaryOp::Add,
                std::make_unique<VariableNode>("a"),
                std::make_unique<VariableNode>("b")
            ),
            std::make_unique<BinaryOpNode>(
                BinaryOp::Subtract,
                std::make_unique<VariableNode>("c"),
                std::make_unique<VariableNode>("d")
            )
        ),
        std::make_unique<BinaryOpNode>(
            BinaryOp::Add,
            std::make_unique<VariableNode>("e"),
            std::make_unique<BinaryOpNode>(
                BinaryOp::Multiply,
                std::make_unique<VariableNode>("f"),
                std::make_unique<VariableNode>("g")
            )
        )
    );
    
    std::cout << "Expression: ((a + b) * (c - d)) / (e + (f * g))" << std::endl;
    std::cout << "AST: " << ast->to_string() << std::endl;
    
    // Calculate depth of AST
    std::function<int(ASTNode*)> get_depth = [&](ASTNode* node) -> int {
        if (auto* binop = dynamic_cast<BinaryOpNode*>(node)) {
            return 1 + std::max(get_depth(binop->left.get()), get_depth(binop->right.get()));
        }
        return 1;
    };
    
    std::cout << "AST Depth: " << get_depth(ast.get()) << std::endl;
}

// Example 6: AST Transformation (Constant Folding)
class ConstantFolder : public ASTVisitor {
private:
    bool changed = false;
    
public:
    void visit(BinaryOpNode& node) override {
        node.left->accept(*this);
        node.right->accept(*this);
        
        // Check if both children are literals
        auto* left_lit = dynamic_cast<LiteralNode*>(node.left.get());
        auto* right_lit = dynamic_cast<LiteralNode*>(node.right.get());
        
        if (left_lit && right_lit && left_lit->type == LiteralType::Integer && 
            right_lit->type == LiteralType::Integer) {
            
            int left_val = std::get<int>(left_lit->value);
            int right_val = std::get<int>(right_lit->value);
            int result = 0;
            
            switch (node.op) {
                case BinaryOp::Add: result = left_val + right_val; break;
                case BinaryOp::Subtract: result = left_val - right_val; break;
                case BinaryOp::Multiply: result = left_val * right_val; break;
                case BinaryOp::Divide: result = left_val / right_val; break;
                default: return;
            }
            
            // Replace node with literal
            changed = true;
            // In a real implementation, you'd replace the node here
            std::cout << "Folding " << left_val << " + " << right_val << " = " << result << std::endl;
        }
    }
    
    bool was_changed() const { return changed; }
};

void constant_folding_example() {
    std::cout << "\n=== Constant Folding Example ===" << std::endl;
    
    // Expression: (5 + 3) * (10 - 2)
    auto ast = std::make_unique<BinaryOpNode>(
        BinaryOp::Multiply,
        std::make_unique<BinaryOpNode>(
            BinaryOp::Add,
            std::make_unique<LiteralNode>(5),
            std::make_unique<LiteralNode>(3)
        ),
        std::make_unique<BinaryOpNode>(
            BinaryOp::Subtract,
            std::make_unique<LiteralNode>(10),
            std::make_unique<LiteralNode>(2)
        )
    );
    
    std::cout << "Original: " << ast->to_string() << std::endl;
    
    ConstantFolder folder;
    ast->accept(folder);
    
    if (folder.was_changed()) {
        std::cout << "After constant folding: 8 * 8 = 64" << std::endl;
    }
}

int main() {
    std::cout << "=== Abstract Syntax Tree (AST) Examples ===" << std::endl;
    std::cout << "Demonstrating various AST patterns and operations" << std::endl;
    
    expression_ast_example();
    control_flow_ast_example();
    function_ast_example();
    complete_program_example();
    nested_expression_example();
    constant_folding_example();
    
    return 0;
}
