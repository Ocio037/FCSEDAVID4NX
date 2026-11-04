module;
#include <cstddef>
#include <vector>
#include <charconv>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <memory>
#include <variant>
export module ohana.parser;
import ohana.bytecode;
import ohana.ast;
import ohana.lexer;
export namespace ohana {
    class Parser {
    public:
        explicit Parser(std::vector<Token> tokens);
        Program parse();
    private:
        std::vector<Token> tokens_;
        std::size_t p_ = 0;
        const Token& cur() const;
        bool check(TokenKind) const;
        const Token& advance();
        bool match(TokenKind);
        void expect(TokenKind, const char*);
        StmtPtr statement();
        StmtPtr ifStatement();
        std::vector<StmtPtr> block();
        ExprPtr expression();
        ExprPtr equality();
        ExprPtr comparison();
        ExprPtr term();
        ExprPtr factor();
        ExprPtr unary();
        ExprPtr primary();
        std::vector<ExprPtr> callArgs();
    };
} // namespace ohana
