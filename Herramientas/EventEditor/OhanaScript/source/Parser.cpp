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
module ohana.parser;
import ohana.bytecode;
import ohana.ast;
import ohana.lexer;
ohana::Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens))
{

}
ohana::Program ohana::Parser::parse()
{
    Program program;
    while (!check(TokenKind::End))
    {
        if (match(TokenKind::Newline))
        {
            continue;
        }
        program.statements.push_back(statement());
    }
    return program;
}
const ohana::Token& ohana::Parser::cur() const 
{
    return tokens_[p_];
}
bool ohana::Parser::check(TokenKind kind) const
{
    return cur().kind == kind;
}
const ohana::Token& ohana::Parser::advance()
{
    return tokens_[p_++];
}
bool ohana::Parser::match(TokenKind kind) 
{
    if (!check(kind)) 
    {
        return false;
    }
    ++p_;
    return true;
}
void ohana::Parser::expect(TokenKind kind, const char* message) 
{
    if (!match(kind)) 
    {
        throw std::runtime_error(std::string(message) + " en línea " + std::to_string(cur().line));
    }
}
ohana::StmtPtr ohana::Parser::statement() 
{
    if (match(TokenKind::KeywordSi)) 
    {
        return ifStatement();
    }
    if (match(TokenKind::KeywordMientras)) 
    {
        auto result = std::make_unique<Stmt>();
        While node{ expression(), {} };
        expect(TokenKind::Colon, "se esperaba ':'");
        node.body = block();
        result->node = std::move(node);
        return result;
    }
    if (match(TokenKind::KeywordFuncion)) 
    {
        auto result = std::make_unique<Stmt>();
        Function node;
        node.name = advance().lexeme;
        expect(TokenKind::LParen, "se esperaba '('");
        if (!check(TokenKind::RParen)) 
        {
            do 
            {
                node.params.push_back(advance().lexeme);
            } while (match(TokenKind::Comma));
        }
        expect(TokenKind::RParen, "se esperaba ')'");
        expect(TokenKind::Colon, "se esperaba ':'");
        node.body = block();
        result->node = std::move(node);
        return result;
    }
    if (match(TokenKind::KeywordEvento)) 
    {
        auto result = std::make_unique<Stmt>();
        Event node;
        node.name = advance().lexeme;
        expect(TokenKind::Colon, "se esperaba ':'");
        node.body = block();
        result->node = std::move(node);
        return result;
    }
    if (match(TokenKind::KeywordDevolver)) 
    {
        auto result = std::make_unique<Stmt>();
        Return node{ check(TokenKind::Newline) ? nullptr : expression() };
        result->node = std::move(node);
        return result;
    }
    if (match(TokenKind::KeywordRomper))
    {
        auto result = std::make_unique<Stmt>();
        result->node = Break{};
        return result;
    }
    if (match(TokenKind::KeywordVariable)) 
    {
        auto result = std::make_unique<Stmt>();
        Assign node;
        node.name = advance().lexeme;
        node.op = "=";
        expect(TokenKind::Equal, "se esperaba '='");
        node.value = expression();
        result->node = std::move(node);
        return result;
    }
    if (check(TokenKind::Identifier) && (tokens_[p_ + 1].kind == TokenKind::Equal || tokens_[p_ + 1].kind == TokenKind::PlusEqual || tokens_[p_ + 1].kind == TokenKind::MinusEqual)) 
    {
        auto result = std::make_unique<Stmt>();
        Assign node;
        node.name = advance().lexeme;
        node.op = advance().lexeme;
        node.value = expression();
        result->node = std::move(node);
        return result;
    }
    auto result = std::make_unique<Stmt>();
    result->node = ExprStmt{ expression() };
    return result;
}

ohana::StmtPtr ohana::Parser::ifStatement()
{
    auto result = std::make_unique<Stmt>();
    If node{ expression(), {}, {} };
    expect(TokenKind::Colon, "se esperaba ':'");
    node.thenBranch = block();

    if (match(TokenKind::KeywordSino))
    {
        if (match(TokenKind::KeywordPero))
        {
            auto nested = ifStatement();
            node.elseBranch.push_back(std::move(nested));
        }
        else
        {
            expect(TokenKind::Colon, "se esperaba ':'");
            node.elseBranch = block();
        }
    }

    result->node = std::move(node);
    return result;
}

std::vector<ohana::StmtPtr> ohana::Parser::block() 
{
    expect(TokenKind::Newline, "se esperaba salto de línea");
    expect(TokenKind::Indent, "se esperaba sangría");
    std::vector<StmtPtr> statements;
    while (!check(TokenKind::Dedent) && !check(TokenKind::End)) 
    {
        if (match(TokenKind::Newline)) 
        {
            continue;
        }
        statements.push_back(statement());
    }
    expect(TokenKind::Dedent, "se esperaba fin de bloque");
    return statements;
}
ohana::ExprPtr ohana::Parser::expression() 
{
    return equality();
}
ohana::ExprPtr ohana::Parser::equality() 
{
    auto expression = comparison();
    while (check(TokenKind::EqualEqual) || check(TokenKind::BangEqual)) 
    {
        const auto op = advance().lexeme;
        expression = std::make_unique<Expr>(Expr{Binary{std::move(expression), op, comparison()} });
    }
    return expression;
}
ohana::ExprPtr ohana::Parser::comparison() 
{
    auto expression = term();
    while (check(TokenKind::Less) || check(TokenKind::LessEqual) || check(TokenKind::Greater) || check(TokenKind::GreaterEqual)) {
        const auto op = advance().lexeme;
        expression = std::make_unique<Expr>(Expr{Binary{std::move(expression), op, term()} });
    }
    return expression;
}
ohana::ExprPtr ohana::Parser::term()
{
    auto expression = factor();
    while (check(TokenKind::Plus) || check(TokenKind::Minus)) 
    {
        const auto op = advance().lexeme;
        expression = std::make_unique<Expr>(Expr{Binary{std::move(expression), op, factor()} });
    }
    return expression;
}
ohana::ExprPtr ohana::Parser::factor()
{
    auto expression = unary();
    while (check(TokenKind::Star) || check(TokenKind::Slash)) 
    {
        const auto op = advance().lexeme;
        expression = std::make_unique<Expr>(Expr{Binary{std::move(expression), op, unary()} });
    }
    return expression;
}
ohana::ExprPtr ohana::Parser::unary()
{
    if (check(TokenKind::Minus)) 
    {
        const auto op = advance().lexeme;
        return std::make_unique<Expr>(Expr{Unary{op, unary()} });
    }

    return primary();
}
ohana::ExprPtr ohana::Parser::primary() 
{
    if (match(TokenKind::Integer)) 
    {
        const std::string& lexeme = tokens_[p_ - 1].lexeme;
        int64_t value = 0;
        const auto result = std::from_chars(lexeme.data(), lexeme.data() + lexeme.size(), value);
        if (result.ec != std::errc{} || result.ptr != lexeme.data() + lexeme.size()) 
        {
            throw std::runtime_error("entero fuera del rango de int64 en línea " + std::to_string(tokens_[p_ - 1].line));
        }
        return std::make_unique<Expr>(Expr{Literal{value} });
    }
    if (match(TokenKind::Float)) 
    {
        return std::make_unique<Expr>(Expr{Literal{std::stod(tokens_[p_ - 1].lexeme)} });
    }
    if (match(TokenKind::String)) 
    {
        return std::make_unique<Expr>(Expr{Literal{tokens_[p_ - 1].lexeme} });
    }
    if (match(TokenKind::KeywordVerdadero)) 
    {
        return std::make_unique<Expr>(Expr{ Literal{true} });
    }
    if (match(TokenKind::KeywordFalso)) 
    {
        return std::make_unique<Expr>(Expr{ Literal{false} });
    }
    if (match(TokenKind::KeywordNada)) 
    {
        return std::make_unique<Expr>(Expr{ Literal{std::monostate{}} });
    }
    if (match(TokenKind::Identifier)) 
    {
        auto expression = std::make_unique<Expr>(Expr{Name{tokens_[p_ - 1].lexeme} });
        if (match(TokenKind::LParen)) 
        {
            Call call{ std::move(expression), callArgs() };
            return std::make_unique<Expr>(Expr{ std::move(call) });
        }
        return expression;
    }
    if (match(TokenKind::LParen)) 
    {
        auto grouped = this->expression();
        expect(TokenKind::RParen, "se esperaba ')'");
        return grouped;
    }
    throw std::runtime_error("expresión inesperada en línea " + std::to_string(cur().line));
}
std::vector<ohana::ExprPtr> ohana::Parser::callArgs() {
    std::vector<ExprPtr> arguments;
    if (!check(TokenKind::RParen)) 
    {
        do 
        {
            arguments.push_back(expression());
        } while (match(TokenKind::Comma));
    }
    expect(TokenKind::RParen, "se esperaba ')'");
    return arguments;
}
