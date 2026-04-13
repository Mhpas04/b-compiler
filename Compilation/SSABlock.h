#pragma once

#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/IRBuilder.h>

namespace toy
{
    class SSABlock
    {
        llvm::DenseMap<std::size_t, llvm::Value*> variableMap;
        llvm::BasicBlock* block;
        llvm::SmallVector<std::pair<std::size_t, llvm::PHINode*>> phiNodes;
        bool sealed;
    public:
        explicit SSABlock(llvm::BasicBlock* block, bool sealed = false);
        void mapVariable(std::size_t identifierId, llvm::Value* val);
        void seal(const llvm::DenseMap<const llvm::BasicBlock*, SSABlock*>& blockMap, llvm::IRBuilder<>& builder);
        llvm::Value* lookup(std::size_t identifierId, llvm::IRBuilder<>& builder, const llvm::DenseMap<const llvm::BasicBlock*, SSABlock*>& blockMap);
    };
}