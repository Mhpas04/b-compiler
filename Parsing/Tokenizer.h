#pragma once
#include <iosfwd>
#include <string_view>


namespace toy
{
    struct Token
    {
        enum TokenType
        {
            LPAREN, RPAREN, RCURLY, LCURLY, COMMA, AT, SEMICOLON, MINUS, PLUS, STAR, SLASH, MODULO, AND, ANDAND, NOT, NEGATE, LSQAURE, RSQUARE, LSHIFT, RSHIFT, LT, GT, LEQ, GEQ, EQEQ, NEQ, OR, OROR, XOR, EQ,
            NUMBER, IDENTIFIER,
            AUTO, REGISTER, WHILE, IF, ELSE, RETURN,
            END,
            SENTINEL
        };

        TokenType kind;
        std::size_t start;
        std::size_t end;
    };

    class Tokenizer
    {
        const std::string_view input;
        std::size_t current;

        Token tokenizeIdentifier();
        Token tokenizeNumber();

    public:
        explicit Tokenizer(std::string_view input);
        Token next();
        Token nextOfType(Token::TokenType kind);
    };
}// toy