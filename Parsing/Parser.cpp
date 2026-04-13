#include "Parser.h"

#include <format>
#include <iostream>
#include <ostream>
#include <llvm/ADT/StringMap.h>
#include <llvm/ADT/SmallVector.h>

#include "Tokenizer.h"

namespace
{
        void Scope::nest()
        {
            definedInScope.emplace_back();
        }

        void Scope::declare(std::string_view var, IdentiferDescription desc)
        {
            if (definedInScope.back().contains(var))
            {
                throw std::runtime_error(std::format("Variable {} can't be redeclared in current Scope", var));
            }

            definedInScope.back().insert(var);
            definitions[var].emplace_back(desc);
        }

        IdentiferDescription Scope::lookup(std::string_view var)
        {
            if (!definitions.contains(var) || definitions[var].size() == 0)
            {
                throw std::runtime_error(std::format("Scope Error: {} not defined in current Scope", var));
            }

            return definitions[var].back();
        }

        void Scope::unnest()
        {
            const auto& declared = definedInScope.back();

            for (const auto var : declared)
            {
                definitions[var].pop_back();
            }

            definedInScope.pop_back();
        };

        constexpr std::size_t UNARY_PREC = 13;
        constexpr std::array<std::tuple<toy::ASTNode::Kind, uint8_t, bool>, toy::Token::TokenType::SENTINEL> getOperators() {
            std::array<std::tuple<toy::ASTNode::Kind, uint8_t, bool>, toy::Token::TokenType::SENTINEL> res;

            // AST Operator Type, Precedence, Right Associative

            res[toy::Token::LSQAURE] = {toy::ASTNode::SUBSCRIPT, 14, false};

            res[toy::Token::STAR] = {toy::ASTNode::MUL, 12, false};
            res[toy::Token::SLASH] = {toy::ASTNode::DIV, 12, false};
            res[toy::Token::MODULO] = {toy::ASTNode::MODULO, 12, false};

            res[toy::Token::PLUS] = {toy::ASTNode::ADD, 11, false};
            res[toy::Token::MINUS] = {toy::ASTNode::SUB, 11, false};

            res[toy::Token::LSHIFT] = {toy::ASTNode::LSHIFT, 10, false};
            res[toy::Token::RSHIFT] = {toy::ASTNode::RSHIFT, 10, false};

            res[toy::Token::LT] = {toy::ASTNode::LT, 9, false};
            res[toy::Token::GT] = {toy::ASTNode::GT, 9, false};
            res[toy::Token::LEQ] = {toy::ASTNode::LEQ, 9, false};
            res[toy::Token::GEQ] = {toy::ASTNode::GEQ, 9, false};

            res[toy::Token::EQEQ] = {toy::ASTNode::EQEQ, 8, false};
            res[toy::Token::NEQ] = {toy::ASTNode::NOTEQ, 8, false};

            res[toy::Token::AND] = {toy::ASTNode::AND, 7, false};

            res[toy::Token::XOR] = {toy::ASTNode::XOR, 6, false};

            res[toy::Token::OR] = {toy::ASTNode::OR, 5, false};

            res[toy::Token::ANDAND] = {toy::ASTNode::ANDAND, 4, false};

            res[toy::Token::OROR] = {toy::ASTNode::OROR, 3, false};

            res[toy::Token::EQ] = {toy::ASTNode::ASSIGN, 2, true};

            return res;
        }

        constexpr std::array<std::tuple<toy::ASTNode::Kind, uint8_t, bool>, toy::Token::TokenType::SENTINEL> OPERATORS = getOperators();
};

namespace toy
{
    Parser::Parser(std::string_view program) : t(program), program(program) {}

    const Token& Parser::peekToken()
    {
        if (!peekTok.has_value())
        {
            peekTok = t.next();
        }
        return *peekTok;
    }

    Token Parser::nextToken()
    {
        if (peekTok.has_value())
        {
            const Token next = *peekTok;
            peekTok.reset();
            return next;
        }

        return t.next();
    }

    Token Parser::nextTokenOfKind(Token::TokenType kind)
    {
        if (peekTok.has_value())
        {
            const Token& next = *peekTok;
            if (next.kind != kind) throw std::runtime_error(std::format("Unexpected token kind: {}", program.substr(next.start, next.end - next.start)));
            peekTok.reset();
            return next;
        }

        return t.nextOfType(kind);
    }

    std::string_view Parser::getTokenValue(const Token& tok) const
    {
        return program.substr(tok.start, tok.end - tok.start);
    }

    ASTNode* Parser::makeASTNode(const ASTNode::Kind kind, ASTNode* children)
    {
        return new (prog.allocator.Allocate()) ASTNode(kind, children);
    }

    std::size_t Parser::declareFunction(std::string_view name, std::size_t nextIid, std::size_t argumentCount)
    {
        if (functionMap.contains(name))
        {
            const std::size_t fnId = functionMap[name];
            if (prog.functions[fnId].nextIdentifierId < nextIid) prog.functions[fnId].nextIdentifierId = nextIid;
            return fnId;
        }

        prog.functions.emplace_back(name, nextIid, argumentCount);
        functionMap[name] = prog.functions.size()-1;
        return prog.functions.size()-1;
    }

    ASTNode* Parser::parseNumber()
    {
        const auto valueToken = nextTokenOfKind(Token::TokenType::NUMBER);
        const auto tokenStringView = getTokenValue(valueToken);
        const auto node = makeASTNode(ASTNode::Kind::NUMBER, nullptr);
        auto [_, ec] = std::from_chars(tokenStringView.data(), tokenStringView.data() + tokenStringView.size(), node->value);

        if (ec != std::errc())
        {
            throw std::runtime_error(std::format("Failed to parse number: {}", tokenStringView));
        }

        return node;
    }

    ASTNode* Parser::parsePrimaryExpression()
    {
        const auto& peek = peekToken();
        switch (peek.kind)
        {
        case Token::TokenType::LPAREN:
            {
                nextToken();
                const auto expr = parseExpression();
                nextTokenOfKind(Token::TokenType::RPAREN);
                return expr;
            }
        case Token::TokenType::IDENTIFIER:
            {
                const auto nameToken = nextTokenOfKind(Token::TokenType::IDENTIFIER);
                if (peekToken().kind == Token::TokenType::LPAREN)
                {
                    // Function Call
                    nextTokenOfKind(Token::TokenType::LPAREN);
                    ASTNode* currentParam = nullptr;
                    ASTNode* firstParam = nullptr;
                    std::size_t argumentCount = 0;
                    while (peekToken().kind != Token::TokenType::RPAREN)
                    {
                        const auto param = parseExpression();
                        if (currentParam) currentParam->sibling = param;
                        else firstParam = param;

                        currentParam = param;
                        ++argumentCount;
                        if (peekToken().kind == Token::TokenType::COMMA) nextToken();
                    }
                    nextTokenOfKind(Token::TokenType::RPAREN);

                    const auto callNode = makeASTNode(ASTNode::Kind::CALL, firstParam);
                    callNode->function = declareFunction(getTokenValue(nameToken), 0, argumentCount);

                    if (prog.functions[callNode->function].argumentCount != argumentCount)
                    {
                        throw std::runtime_error(std::format("Can't call {}. Expected {} parameters, Got {}", getTokenValue(nameToken), prog.functions[callNode->function].argumentCount, argumentCount));
                    }

                    return callNode;
                }
                // Normal Variable Usage
                const auto node = makeASTNode(ASTNode::Kind::VARIABLE, nullptr);
                node->variable = scope.lookup(getTokenValue(nameToken));
                return node;
            }
        case Token::TokenType::NUMBER:
            {
                return parseNumber();
            }
        case Token::TokenType::MINUS:
            {
                nextTokenOfKind(Token::TokenType::MINUS);
                return makeASTNode(ASTNode::Kind::UNARY_MINUS, parseExpression(UNARY_PREC));
            }
        case Token::TokenType::NOT:
            {
                nextTokenOfKind(Token::TokenType::NOT);
                return makeASTNode(ASTNode::Kind::NOT, parseExpression(UNARY_PREC));
            }
        case Token::TokenType::NEGATE:
            {
                nextTokenOfKind(Token::TokenType::NEGATE);
                return makeASTNode(ASTNode::Kind::NEGATE, parseExpression(UNARY_PREC));
            }
        case Token::TokenType::AND:
            {
                nextTokenOfKind(Token::TokenType::AND);
                const auto children = parseExpression(UNARY_PREC);

                if (children->kind != ASTNode::Kind::VARIABLE && children->kind != ASTNode::Kind::SUBSCRIPT || children->kind == ASTNode::Kind::VARIABLE && !children->variable.addressable)
                {
                    throw std::runtime_error("RHS of a Address-of Operator must be a addressable variable or a subscript");
                }

                return makeASTNode(ASTNode::Kind::REFERENCE, children);
            }
        default:
            return parseExpression();
        }
    }

    ASTNode* Parser::parseExpression(unsigned minPrecedence)
    {
        auto lhs = parsePrimaryExpression();
        while (true)
        {
            const auto& operatorToken = peekToken();
            auto [nodeKind, prec, rassoc] = OPERATORS[operatorToken.kind];
            if (!prec || prec < minPrecedence) break;

            // Consume Operator
            nextTokenOfKind(operatorToken.kind);

            if (operatorToken.kind == Token::TokenType::LSQAURE)
            {
                lhs->sibling = parseExpression();
                unsigned sizeSpec = 8;
                // SubScript operator
                if (peekToken().kind == Token::TokenType::AT)
                {
                    nextToken(); // Consume @
                    const auto numberToken = nextTokenOfKind(Token::TokenType::NUMBER);
                    const auto tokenStringView = getTokenValue(numberToken);
                    auto [_, ec] = std::from_chars(tokenStringView.data(), tokenStringView.data() + tokenStringView.size(), sizeSpec);

                    if (sizeSpec != 1 && sizeSpec != 2 && sizeSpec != 4 && sizeSpec != 8)
                    {
                        throw std::runtime_error("Size Specification after @ must be either 1, 2, 4 or 8 bytes");
                    }

                    if (ec != std::errc())
                    {
                        throw std::runtime_error(std::format("Failed to parse number: {}", tokenStringView));
                    }
                }
                nextTokenOfKind(Token::TokenType::RSQUARE);
                lhs = makeASTNode(ASTNode::Kind::SUBSCRIPT, lhs);
                lhs->sizeSpec = sizeSpec;
                continue;
            }

            lhs->sibling = parseExpression(prec + (rassoc ? 0 : 1));
            lhs = makeASTNode(nodeKind, lhs);
        }

        return lhs;
    }

    ASTNode* Parser::parseReturnStatement()
    {
        nextTokenOfKind(Token::TokenType::RETURN);
        if (peekToken().kind == Token::TokenType::SEMICOLON)
        {
            nextToken();
            return makeASTNode(ASTNode::Kind::RETURN, nullptr);
        }

        const auto expr = parseExpression();
        nextTokenOfKind(Token::TokenType::SEMICOLON);

        return makeASTNode(ASTNode::Kind::RETURN, expr);
    }

    ASTNode* Parser::parseWhileStatement()
    {
        nextTokenOfKind(Token::TokenType::WHILE);
        nextTokenOfKind(Token::TokenType::LPAREN);
        const auto condition = parseExpression();
        nextTokenOfKind(Token::TokenType::RPAREN);
        condition->sibling = parseStmt();
        return makeASTNode(ASTNode::Kind::WHILE, condition);
    }

    ASTNode* Parser::parseIfStatement()
    {
        nextTokenOfKind(Token::TokenType::IF);
        nextTokenOfKind(Token::TokenType::LPAREN);
        const auto condition = parseExpression();
        nextTokenOfKind(Token::TokenType::RPAREN);
        condition->sibling = parseStmt();

        if (peekToken().kind == Token::TokenType::ELSE)
        {
            nextTokenOfKind(Token::TokenType::ELSE);
            condition->sibling->sibling = parseStmt();
        }

        return makeASTNode(ASTNode::Kind::IF, condition);
    }

    ASTNode* Parser::parseStmt()
    {
        const auto& peek = peekToken();

        switch (peek.kind)
        {
        case Token::TokenType::LCURLY:
            return parseBlock(/*createScope=*/true);
        case Token::TokenType::RETURN:
            return parseReturnStatement();
        case Token::TokenType::WHILE:
            return parseWhileStatement();
        case Token::TokenType::IF:
            return parseIfStatement();
        default:
            const auto expr = parseExpression();
            nextTokenOfKind(Token::TokenType::SEMICOLON);
            return expr;
        }
    }

    ASTNode* Parser::parseDeclStmt()
    {
        const auto& peek = peekToken();
        if (peek.kind == Token::TokenType::AUTO || peek.kind == Token::TokenType::REGISTER)
        {
            nextTokenOfKind(peek.kind);
            //Declaration
            const bool addressable = peek.kind == Token::TokenType::AUTO;
            const auto& nameToken = nextTokenOfKind(Token::TokenType::IDENTIFIER);
            nextTokenOfKind(Token::TokenType::EQ);
            const auto varName = getTokenValue(nameToken);
            const auto varNode = makeASTNode(ASTNode::VARIABLE, nullptr);
            varNode->sibling = parseExpression();
            varNode->variable = {++prog.functions[currentFnID].nextIdentifierId, addressable};
            const auto newNode = makeASTNode(ASTNode::DECLARATION, varNode);

            scope.declare(varName,  varNode->variable);
            nextTokenOfKind(Token::TokenType::SEMICOLON);
            return newNode;
        }

        return parseStmt();
    }

    ASTNode* Parser::parseBlock(bool createScope)
    {
        if (createScope) scope.nest();

        ASTNode* blockNode = makeASTNode(ASTNode::Kind::BLOCK, nullptr);

        nextTokenOfKind(Token::TokenType::LCURLY);
        ASTNode* currentDeclStmt = nullptr;
        while (peekToken().kind != Token::TokenType::RCURLY)
        {
            const auto decl = parseDeclStmt();

            if (currentDeclStmt != nullptr)
            {
                currentDeclStmt->sibling = decl;
            }else
            {
                blockNode->child = decl;
            }

            currentDeclStmt = decl;
        }
        nextTokenOfKind(Token::TokenType::RCURLY);

        if (createScope) scope.unnest();

        return blockNode;
    }

    void Parser::parseFunction()
    {
        scope.nest();
        const auto& functionName = nextTokenOfKind(Token::TokenType::IDENTIFIER);
        std::size_t nextIdentifierId = 0;
        std::size_t arguments = 0;

        nextTokenOfKind(Token::TokenType::LPAREN);
        while (peekToken().kind != Token::TokenType::RPAREN)
        {
            const auto& param = nextTokenOfKind(Token::TokenType::IDENTIFIER);
            scope.declare(getTokenValue(param), {++nextIdentifierId, false});
            if (peekToken().kind == Token::TokenType::COMMA)
            {
                nextTokenOfKind(Token::TokenType::COMMA);
            }
            ++arguments;
        }
        nextTokenOfKind(Token::TokenType::RPAREN);
        const auto name = getTokenValue(functionName);
        if (functionMap.contains(name) && (prog.functions[functionMap[name]].children || prog.functions[functionMap[name]].argumentCount != arguments))
        {
            if (prog.functions[functionMap[name]].argumentCount != arguments)
            {
                throw std::runtime_error(std::format("Definition of {} doesnt match call. Got {} parameters in definition, called with {} parameters", name, arguments, prog.functions[functionMap[name]].argumentCount));
            }
            throw std::runtime_error(std::format("Cannot redefine function {}", name));
        }
        currentFnID = declareFunction(name, nextIdentifierId, arguments);

        prog.functions[currentFnID].children = parseBlock(/*createScope=*/false);

        scope.unnest();
    }

    Program Parser::parse()
    {
        while (peekToken().kind != Token::TokenType::END)
            parseFunction();

        return std::move(prog);
    }

} // toy
