module;
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
module ohana.lexer;
ohana::Lexer::Lexer(std::string_view source) : source_(source) 
{

}
std::vector<ohana::Token> ohana::Lexer::tokenize() const 
{
    static const std::unordered_map<std::string_view, TokenKind> keywords
    {
        {"si", TokenKind::KeywordSi},
        {"sino", TokenKind::KeywordSino},
        {"si_no", TokenKind::KeywordSino},
        {"pero", TokenKind::KeywordPero},
        {"mientras", TokenKind::KeywordMientras},
        {"para", TokenKind::KeywordPara},
        {"en", TokenKind::KeywordEn},
        {"funcion", TokenKind::KeywordFuncion},
        {"función", TokenKind::KeywordFuncion},
        {"func", TokenKind::KeywordFuncion},
        {"fc", TokenKind::KeywordFuncion},
        {"funconesis", TokenKind::KeywordFuncion},
        {"devolver", TokenKind::KeywordDevolver},
        {"romper", TokenKind::KeywordRomper},
        {"verdadero", TokenKind::KeywordVerdadero},
        {"falso", TokenKind::KeywordFalso},
        {"nada", TokenKind::KeywordNada},
        {"variable", TokenKind::KeywordVariable},
        {"evento", TokenKind::KeywordEvento},
    };
    std::vector<Token> output;
    std::vector<std::size_t> indents{ 0 };
    std::size_t index = 0;
    std::size_t line = 1;
    std::size_t column = 1;
    bool beginning_of_line = true;
    auto add = [&](TokenKind kind, std::string lexeme, std::size_t token_line, std::size_t token_column) {
        output.push_back(Token{
            kind,
            std::move(lexeme),
            token_line,
            token_column });
        };
    auto add_simple = [&](TokenKind kind, char character) {
        add(kind, std::string(1, character), line, column);
        ++index;
        ++column;
        };
    while (index < source_.size()) 
    {
        if (beginning_of_line) 
        {
            std::size_t spaces = 0;
            while (index < source_.size() && (source_[index] == ' ' || source_[index] == '\t')) 
            {
                spaces += source_[index] == '\t' ? 4U : 1U;
                ++index;
                ++column;
            }
            if (index < source_.size() && source_[index] == '\n') 
            {
                ++index;
                ++line;
                column = 1;
                continue;
            }
            if (index < source_.size() && source_[index] == '#') 
            {
                while (index < source_.size() && source_[index] != '\n') 
                {
                    ++index;
                }
                beginning_of_line = false;
                continue;
            }
            if (spaces > indents.back()) 
            {
                indents.push_back(spaces);
                add(TokenKind::Indent, "", line, 1);
            }
            else 
            {
                while (spaces < indents.back()) 
                {
                    indents.pop_back();
                    add(TokenKind::Dedent, "", line, 1);
                }
                if (spaces != indents.back()) 
                {
                    throw std::runtime_error( "sangría inconsistente en " + std::to_string(line) + ":" + std::to_string(column));
                }
            }
            beginning_of_line = false;
        }
        if (index >= source_.size()) 
        {
            break;
        }
        const char character = source_[index];
        if (character == ' ' || character == '\t' || character == '\r') 
        {
            ++index;
            ++column;
            continue;
        }
        if (character == '\n') 
        {
            add(TokenKind::Newline, "\n", line, column);
            ++index;
            ++line;
            column = 1;
            beginning_of_line = true;
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(character)) || character == '_' || static_cast<unsigned char>(character) >= 128) {
            const auto token_line = line;
            const auto token_column = column;
            const auto begin = index;
            ++index;
            ++column;
            while (index < source_.size() && (std::isalnum(static_cast<unsigned char>(source_[index])) || source_[index] == '_' || static_cast<unsigned char>(source_[index]) >= 128))
            {
                ++index;
                ++column;
            }
            std::string lexeme(source_.substr(begin, index - begin));
            const auto keyword = keywords.find(lexeme);
            add(keyword == keywords.end() ? TokenKind::Identifier : keyword->second, std::move(lexeme), token_line, token_column);
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(character))) 
        {
            const auto token_line = line;
            const auto token_column = column;
            const auto begin = index;
            bool has_dot = false;
            while (index < source_.size()) 
            {
                const char current = source_[index];
                if (std::isdigit(static_cast<unsigned char>(current))) 
                {
                    ++index;
                    ++column;
                    continue;
                }
                if (!has_dot && current == '.') 
                {
                    has_dot = true;
                    ++index;
                    ++column;
                    continue;
                }
                break;
            }

            add(has_dot ? TokenKind::Float : TokenKind::Integer, std::string(source_.substr(begin, index - begin)), token_line, token_column);
            continue;
        }
        if (character == '"' || character == '\'') 
        {
            const char quote = character;
            const auto token_line = line;
            const auto token_column = column;
            std::string value;
            ++index;
            ++column;
            while (index < source_.size() && source_[index] != quote) 
            {
                if (source_[index] == '\\' && index + 1 < source_.size()) 
                {
                    ++index;
                    ++column;
                    const char escaped = source_[index++];
                    ++column;

                    switch (escaped) {
                    case 'n':
                        value += '\n';
                        break;
                    case 't':
                        value += '\t';
                        break;
                    default:
                        value += escaped;
                        break;
                    }
                }
                else 
                {
                    value += source_[index++];
                    ++column;
                }
            }
            if (index >= source_.size()) 
            {
                throw std::runtime_error("cadena sin cerrar");
            }
            ++index;
            ++column;
            add(TokenKind::String, std::move(value), token_line, token_column);
            continue;
        }
        if (character == ':') 
        {
            add_simple(TokenKind::Colon, character);
            continue;
        }
        if (character == ',') 
        {
            add_simple(TokenKind::Comma, character);
            continue;
        }
        if (character == '(') 
        {
            add_simple(TokenKind::LParen, character);
            continue;
        }
        if (character == ')') 
        {
            add_simple(TokenKind::RParen, character);
            continue;
        }
        if (character == '+' && index + 1 < source_.size() && source_[index + 1] == '=') 
        {
            add(TokenKind::PlusEqual, "+=", line, column);
            index += 2;
            column += 2;
            continue;
        }
        if (character == '+') 
        {
            add_simple(TokenKind::Plus, character);
            continue;
        }
        if (character == '-' && index + 1 < source_.size() && source_[index + 1] == '=') 
        {
            add(TokenKind::MinusEqual, "-=", line, column);
            index += 2;
            column += 2;
            continue;
        }
        if (character == '-') 
        {
            add_simple(TokenKind::Minus, character);
            continue;
        }
        if (character == '*') 
        {
            add_simple(TokenKind::Star, character);
            continue;
        }
        if (character == '/') 
        {
            add_simple(TokenKind::Slash, character);
            continue;
        }
        if (character == '=' && index + 1 < source_.size() && source_[index + 1] == '=') 
        {
            add(TokenKind::EqualEqual, "==", line, column);
            index += 2;
            column += 2;
            continue;
        }
        if (character == '=') 
        {
            add_simple(TokenKind::Equal, character);
            continue;
        }
        if (character == '!' && index + 1 < source_.size() && source_[index + 1] == '=') 
        {
            add(TokenKind::BangEqual, "!=", line, column);
            index += 2;
            column += 2;
            continue;
        }
        if (character == '<' && index + 1 < source_.size() && source_[index + 1] == '=') 
        {
            add(TokenKind::LessEqual, "<=", line, column);
            index += 2;
            column += 2;
            continue;
        }
        if (character == '<') 
        {
            add_simple(TokenKind::Less, character);
            continue;
        }
        if (character == '>' && index + 1 < source_.size() && source_[index + 1] == '=') 
        {
            add(TokenKind::GreaterEqual, ">=", line, column);
            index += 2;
            column += 2;
            continue;
        }
        if (character == '>') 
        {
            add_simple(TokenKind::Greater, character);
            continue;
        }
        throw std::runtime_error("carácter inesperado en " + std::to_string(line) + ":" + std::to_string(column));
    }
    while (indents.size() > 1) 
    {
        indents.pop_back();
        add(TokenKind::Dedent, "", line, column);
    }
    add(TokenKind::End, "", line, column);
    return output;
}
std::string_view ohana::token_name(TokenKind kind)
{
    switch (kind) {
    case TokenKind::End:
        return "fin";
    case TokenKind::Newline:
        return "salto";
    case TokenKind::Indent:
        return "sangría";
    case TokenKind::Dedent:
        return "des-sangría";
    case TokenKind::Identifier:
        return "identificador";
    case TokenKind::Integer:
        return "entero";
    case TokenKind::Float:
        return "decimal";
    case TokenKind::String:
        return "cadena";
    case TokenKind::Colon:
        return ":";
    case TokenKind::Comma:
        return ",";
    case TokenKind::LParen:
        return "(";
    case TokenKind::RParen:
        return ")";
    case TokenKind::Plus:
        return "+";
    case TokenKind::Minus:
        return "-";
    case TokenKind::Star:
        return "*";
    case TokenKind::Slash:
        return "/";
    case TokenKind::Equal:
        return "=";
    case TokenKind::EqualEqual:
        return "==";
    case TokenKind::BangEqual:
        return "!=";
    case TokenKind::Less:
        return "<";
    case TokenKind::LessEqual:
        return "<=";
    case TokenKind::Greater:
        return ">";
    case TokenKind::GreaterEqual:
        return ">=";
    case TokenKind::PlusEqual:
        return "+=";
    case TokenKind::MinusEqual:
        return "-=";
    case TokenKind::KeywordSi:
        return "si";
    case TokenKind::KeywordSino:
        return "sino";
    case TokenKind::KeywordPero:
        return "pero";
    case TokenKind::KeywordMientras:
        return "mientras";
    case TokenKind::KeywordPara:
        return "para";
    case TokenKind::KeywordEn:
        return "en";
    case TokenKind::KeywordFuncion:
        return "función";
    case TokenKind::KeywordDevolver:
        return "devolver";
    case TokenKind::KeywordRomper:
        return "romper";
    case TokenKind::KeywordVerdadero:
        return "verdadero";
    case TokenKind::KeywordFalso:
        return "falso";
    case TokenKind::KeywordNada:
        return "nada";
    case TokenKind::KeywordVariable:
        return "variable";
    case TokenKind::KeywordEvento:
        return "evento";
    }
    return "?";
}
