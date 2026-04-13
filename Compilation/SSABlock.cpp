//
// Created by root on 4/12/26.
//

#include "SSABlock.h"
#include <llvm/IR/CFG.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>

namespace toy
{
    SSABlock::SSABlock(llvm::BasicBlock* block, bool sealed) : block(block), sealed(sealed)
    {

    }

    void SSABlock::mapVariable(std::size_t identifierId, llvm::Value* val)
    {
        variableMap[identifierId] = val;
    }

    void SSABlock::seal(const llvm::DenseMap<const llvm::BasicBlock*, SSABlock*>& blockMap, llvm::IRBuilder<>& builder)
    {
        sealed = true;

        for (auto [identId, phiNode] : phiNodes)
        {
            for (const auto pred : llvm::predecessors(block))
            {
                const auto ssa = blockMap.lookup(pred);
                assert(ssa != nullptr);
                phiNode->addIncoming(ssa->lookup(identId, builder, blockMap), pred);
            }
        }
    }

    llvm::Value* SSABlock::lookup(std::size_t identifierId, llvm::IRBuilder<>& builder, const llvm::DenseMap<const llvm::BasicBlock*, SSABlock*>& blockMap)
    {
        if (variableMap.contains(identifierId))
        {
            return variableMap[identifierId];
        }

        const auto insertBlock = builder.GetInsertBlock();

        if (!sealed)
        {
            builder.SetInsertPoint(block, block->getFirstInsertionPt());
            auto phi = builder.CreatePHI(builder.getInt64Ty(), 2);
            phiNodes.emplace_back(identifierId, phi);

            mapVariable(identifierId, phi);

            builder.SetInsertPoint(insertBlock);
            return phi;
        }

        unsigned predCount = llvm::pred_size(block);
        if (predCount > 1)
        {
            // Immediatly fill Phi
            builder.SetInsertPoint(block, block->getFirstInsertionPt());
            const auto phi = builder.CreatePHI(builder.getInt64Ty(), predCount);
            mapVariable(identifierId, phi);

            for (const auto pred : llvm::predecessors(block))
            {
                const auto ssa = blockMap.lookup(pred);
                assert(ssa != nullptr);
                phi->addIncoming(ssa->lookup(identifierId, builder, blockMap), pred);
            }

            builder.SetInsertPoint(insertBlock);

            return phi;
        }
        assert(predCount == 1);
        //Query Single Predecessor
        const auto ssa = blockMap.lookup(block->getSinglePredecessor());
        assert(ssa != nullptr);
        mapVariable(identifierId, ssa->lookup(identifierId, builder, blockMap));
        return variableMap[identifierId];
    }
}
