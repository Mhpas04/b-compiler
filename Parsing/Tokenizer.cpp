//
// Created by Michel on 12.04.2026.
//

#include "Tokenizer.h"

#include <format>
#include <stdexcept>

namespace toy
{
    Tokenizer::Tokenizer(const std::string_view input) : input(input), current(0) {}

    Token Tokenizer::tokenizeIdentifier()
    {
        std::size_t start = current;
        if (input.at(current) >= '0' && input.at(current) <= '9')
        {
            // One could parse the identifier to the end to tell the user which identifier we mean.
            throw std::runtime_error("Identifier can't begin with a number");
        }

        while (current < input.size() && (input.at(current) >= '0' && input.at(current) <= '9' || input.at(current) == '_' || input.at(current) >= 'a' && input.at(current) <= 'z' || input.at(current) >= 'A' && input.at(current) <= 'Z'))
        {
            ++current;
        }

        auto identifier = input.substr(start, current - start);

        // A more efficient way would be to do a perfect hashing to match the known keywords. But with a small number like
        // this a simple if chain is sufficient

        // Also not case-insensitive just like in C
        Token::TokenType kind = Token::TokenType::IDENTIFIER;
        if (identifier == "auto")
        {
            kind = Token::TokenType::AUTO;
        }else if (identifier == "register")
        {
            kind = Token::TokenType::REGISTER;
        }else if (identifier == "return")
        {
            kind = Token::TokenType::RETURN;
        }else if (identifier == "while")
        {
            kind = Token::TokenType::WHILE;
        }else if (identifier == "if")
        {
            kind = Token::TokenType::IF;
        }else if (identifier == "else")
        {
            kind = Token::TokenType::ELSE;
        }

        return Token{kind, start, current};
    }

    Token Tokenizer::tokenizeNumber()
    {
        std::size_t start = current;
        while (input.at(current) >= '0' && input.at(current) <= '9')
        {
            ++current;
        }

        return Token{Token::TokenType::NUMBER, start, current};
    }

    Token Tokenizer::next()
    {
        // Skip Whitespace
        while (current < input.size() && std::isspace(input.at(current))) ++current;

        if (current >= input.size())
        {
            return Token{Token::TokenType::END, current, current};
        }

        Token::TokenType kind;
        switch (input.at(current))
        {
        case '(':
            kind = Token::TokenType::LPAREN;
            goto operatorCommon;
        case ')':
            kind = Token::TokenType::RPAREN;
            goto operatorCommon;
        case '{':
            kind = Token::TokenType::LCURLY;
            goto operatorCommon;
        case '}':
            kind = Token::TokenType::RCURLY;
            goto operatorCommon;
        case ';':
            kind = Token::TokenType::SEMICOLON;
            goto operatorCommon;
        case '@':
            kind = Token::TokenType::AT;
            goto operatorCommon;
       case ',':
            kind = Token::TokenType::COMMA;
            goto operatorCommon;
        case '-':
            kind = Token::TokenType::MINUS;
            goto operatorCommon;
        case '!':
            kind = Token::TokenType::NOT;
            if (current + 1 < input.size() && input.at(current+1) == '=')
            {
                ++current;
                kind = Token::TokenType::NEQ;
            }
            goto operatorCommon;
        case '~':
            kind = Token::TokenType::NEGATE;
            goto  operatorCommon;
        case '&':
            kind = Token::TokenType::AND;
            if (current + 1 < input.size() && input.at(current+1) == '&')
            {
                ++current;
                kind = Token::TokenType::ANDAND;
            }
            goto operatorCommon;
        case '[':
            kind = Token::TokenType::LSQAURE;
            goto operatorCommon;
        case ']':
            kind = Token::TokenType::RSQUARE;
            goto operatorCommon;
        case '+':
            kind = Token::TokenType::PLUS;
            goto operatorCommon;
        case '*':
            kind = Token::TokenType::STAR;
            goto operatorCommon;
        case '/':
            kind = Token::TokenType::SLASH;
            // Ign Comment
            if (current + 1 < input.size() && input.at(current+1) == '/')
            {
                ++current;
                while (current < input.size() && input.at(current) != '\n') ++current;
                return next();
            }
            goto operatorCommon;
        case '%':
            kind = Token::TokenType::MODULO;
            goto operatorCommon;
        case '<':
            kind = Token::TokenType::LT;
            if (current + 1 < input.size() && input.at(current+1) == '<')
            {
                ++current;
                kind = Token::LSHIFT;
            }else if (current + 1 < input.size() && input.at(current+1) == '=')
            {
                ++current;
                kind = Token::LEQ;
            }
            goto operatorCommon;
        case '>':
            kind = Token::TokenType::GT;
            if (current + 1 < input.size() && input.at(current+1) == '>')
            {
                ++current;
                kind = Token::RSHIFT;
            }else if (current + 1 < input.size() && input.at(current+1) == '=')
            {
                ++current;
                kind = Token::GEQ;
            }
            goto operatorCommon;
        case '=':
            kind = Token::TokenType::EQ;
            if (current + 1 < input.size() && input.at(current+1) == '=')
            {
                ++current;
                kind = Token::EQEQ;
            }
            goto operatorCommon;
        case '|':
            kind = Token::TokenType::OR;
            if (current + 1 < input.size() && input.at(current+1) == '|')
            {
                ++current;
                kind = Token::OROR;
            }
            goto operatorCommon;
        case '^':
            kind = Token::TokenType::XOR;
            goto operatorCommon;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            return tokenizeNumber();
        default:
            return tokenizeIdentifier();
            operatorCommon:
            return Token{kind, current, ++current};
        }
    }


    Token Tokenizer::nextOfType(Token::TokenType kind)
    {
        Token token = next();
        if (token.kind != kind) [[unlikely]]
        {
            // Should really do better erro handling
            throw std::runtime_error(std::format("Syntax error. Expected diffrent token txpe at {}", input.substr(token.start, token.end - token.start)));
        }

        return token;
    }   ;
} // toy
