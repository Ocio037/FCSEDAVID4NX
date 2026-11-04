module;
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
export module ohana.lexer;
export namespace ohana {
    enum class TokenKind 
    {
        End, Newline, Indent, Dedent,
        Identifier, Integer, Float, String,
        Colon, Comma, LParen, RParen,
        Plus, Minus, Star, Slash, Equal, EqualEqual,
        BangEqual, Less, LessEqual, Greater, GreaterEqual,
        PlusEqual, MinusEqual,
        KeywordSi, KeywordSino, KeywordPero, KeywordMientras, KeywordPara, KeywordEn,
        KeywordFuncion, KeywordDevolver, KeywordRomper, KeywordVerdadero, KeywordFalso,
        KeywordNada, KeywordVariable, KeywordEvento,
    };
    struct Token 
    {
        TokenKind kind{};
        std::string lexeme;
        std::size_t line = 1;
        std::size_t column = 1;
    };
    class Lexer {
    public:
        explicit Lexer(std::string_view source);
        std::vector<Token> tokenize() const;
    private:
        std::string_view source_;
    };
    std::string_view token_name(TokenKind kind);
} // namespace ohana