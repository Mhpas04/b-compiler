#pragma once
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

#include "SSABlock.h"
#include "../Parsing/Parser.h"
;

namespace toy
{
    class LLVMEmitter
    {
        const Program& program;
        llvm::LLVMContext& context;
        std::unique_ptr<llvm::Module> module;

        llvm::SmallVector<llvm::FunctionCallee> functionCallees;

        llvm::SpecificBumpPtrAllocator<SSABlock> ssaBlockAllocator;
        llvm::DenseMap<const llvm::BasicBlock*, SSABlock*> blockMap;

        llvm::IRBuilder<> builder;
        llvm::Type *i64Ty;

        SSABlock* ssa;
        SSABlock* makeSSABlock(llvm::BasicBlock* block, bool sealed);

        llvm::Value* findValueInEntry(std::size_t identifierId);

        llvm::Value* emit64BitValue(const ASTNode* ast, bool pointer = false);
        llvm::Value* emitExpression(const ASTNode* ast, bool pointer = false);
        llvm::Value* emitInlineBoolLogic(const ASTNode* ast);
        void emitCondition(const ASTNode* ast, llvm::BasicBlock* trueBlock, llvm::BasicBlock* falseBlock);
        void emitWhile(const ASTNode* ast);
        void emitIf(const ASTNode* ast);
        void emitBlock(const ASTNode* ast);
        void emitStatement(const ASTNode* ast);
        void emitDeclStmt(const ASTNode* ast);
        void emitFunction(const Function& function);
    public:
        explicit LLVMEmitter(const Program& program, llvm::LLVMContext& context);

        std::unique_ptr<llvm::Module> buildModule();
    };
} // toy