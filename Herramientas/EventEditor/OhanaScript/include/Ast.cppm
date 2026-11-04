module;
#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>
export module ohana.ast;
export namespace ohana {
    struct Expr;
    struct Stmt;
    using ExprPtr = std::unique_ptr<Expr>;
    using StmtPtr = std::unique_ptr<Stmt>;
    struct Literal
    {
        std::variant<std::monostate, bool, int64_t, double, std::string> value;
    };
    struct Name 
    {
        std::string value;
    };
    struct Unary 
    {
        std::string op;
        ExprPtr rhs;
    };
    struct Binary 
    {
        ExprPtr lhs;
        std::string op;
        ExprPtr rhs;
    };
    struct Call 
    {
        ExprPtr callee;
        std::vector<ExprPtr> args;
    };
    struct Expr 
    {
        std::variant<Literal, Name, Unary, Binary, Call> node;
    };
    struct ExprStmt 
    {
        ExprPtr expr;
    };
    struct Assign 
    {
        std::string name;
        std::string op;
        ExprPtr value;
    };
    struct If 
    {
        ExprPtr condition;
        std::vector<StmtPtr> thenBranch;
        std::vector<StmtPtr> elseBranch;
    };
    struct While 
    {
        ExprPtr condition;
        std::vector<StmtPtr> body;
    };
    struct Return 
    {
        ExprPtr value;
    };
    struct Break {};
    struct Function 
    {
        std::string name;
        std::vector<std::string> params;
        std::vector<StmtPtr> body;
    };
    struct Event 
    {
        std::string name;
        std::vector<StmtPtr> body;
    };
    struct Stmt 
    {
        std::variant<ExprStmt, Assign, If, While, Return, Break, Function, Event> node;
    };
    struct Program 
    {
        std::vector<StmtPtr> statements;
    };
} // namespace ohana