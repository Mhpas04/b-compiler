#pragma once
#include <unordered_set>
#include <llvm/Support/Allocator.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringMap.h>

#include "Tokenizer.h"

namespace
{
    struct IdentiferDescription
    {
        std::size_t id;
        bool addressable = false;
    };

    class Scope
    {
        // TODO: How do you use an llvm::StringSet ?!?
        llvm::SmallVector<std::unordered_set<std::string_view>> definedInScope;
        llvm::StringMap<llvm::SmallVector<IdentiferDescription>> definitions;
    public:
        void nest();
        void declare(std::string_view var, IdentiferDescription desc);
        IdentiferDescription lookup(std::string_view var);
        void unnest();
    };
}

namespace toy
{
    struct ASTNode
    {
        enum Kind
        {
            DECLARATION, VARIABLE, CALL, NUMBER, UNARY_MINUS, NOT, NEGATE, REFERENCE, SUBSCRIPT,
            MUL, DIV, MODULO, ADD, SUB, LSHIFT, RSHIFT, LT, GT, LEQ, GEQ, EQEQ, NOTEQ, AND, OR, XOR, ANDAND, OROR, ASSIGN,
            RETURN, WHILE, IF, BLOCK
        };

        Kind kind;
        ASTNode* child;
        ASTNode* sibling;

        union
        {
            std::size_t function;
            IdentiferDescription variable;
            std::size_t value;
            unsigned sizeSpec;
        };

        explicit ASTNode(Kind kind, ASTNode* children): kind(kind), child(children), sibling(nullptr) {};
    };

    struct Function
    {
        std::string_view name;
        std::size_t nextIdentifierId;
        std::size_t argumentCount;

        ASTNode* children;
    };

    struct Program
    {
        llvm::SpecificBumpPtrAllocator<ASTNode> allocator;
        llvm::SmallVector<Function> functions;
    };

    class Parser
    {
        Tokenizer t;
        std::string_view program;
        Program prog;
        std::optional<Token> peekTok;
        Scope scope;
        llvm::StringMap<std::size_t> functionMap;
        std::size_t currentFnID;

        ASTNode* makeASTNode(ASTNode::Kind kind, ASTNode* children);
        const Token& peekToken();
        Token nextToken();
        Token nextTokenOfKind(Token::TokenType kind);

        [[nodiscard]] std::string_view getTokenValue(const Token& tok) const;

        std::size_t declareFunction(std::string_view name, std::size_t nextIdentifierId, std::size_t argumentCount);

        ASTNode* parseIfStatement();
        ASTNode* parseWhileStatement();
        ASTNode* parseReturnStatement();
        ASTNode* parseNumber();
        ASTNode* parsePrimaryExpression();
        ASTNode* parseExpression(unsigned minPrecedence = 1);
        ASTNode* parseStmt();
        ASTNode* parseDeclStmt();
        ASTNode* parseBlock(bool createScope);
        void parseFunction();

        public:
        explicit Parser(std::string_view program);
        Program parse();
    };
} // toy
