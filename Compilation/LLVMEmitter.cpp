#include "LLVMEmitter.h"

namespace toy
{
    LLVMEmitter::LLVMEmitter(const Program& program, llvm::LLVMContext& context) : program(program), module(std::make_unique<llvm::Module>("toy_code", context)), context(context), builder(context), i64Ty(llvm::Type::getInt64Ty(context))
    {

    }

    llvm::Value* LLVMEmitter::findValueInEntry(std::size_t identifierId)
    {
        return blockMap[&builder.GetInsertBlock()->getParent()->getEntryBlock()]->lookup(identifierId, builder, blockMap);
    }

    llvm::Value* LLVMEmitter::emit64BitValue(const ASTNode* ast, bool pointer)
    {
        auto value = emitExpression(ast, pointer);
        if (value->getType()->isIntegerTy(64)) return value;
        if (value->getType()->isPointerTy()) return builder.CreatePtrToInt(value, i64Ty);
        return builder.CreateZExt(value, i64Ty);
    }

    llvm::Value* LLVMEmitter::emitInlineBoolLogic(const ASTNode* ast)
    {
        llvm::BasicBlock *trueBranch = llvm::BasicBlock::Create(context, "inline_true", builder.GetInsertBlock()->getParent());
        llvm::BasicBlock *falseBranch = llvm::BasicBlock::Create(context, "inline_false", builder.GetInsertBlock()->getParent());
        llvm::BasicBlock *cont = llvm::BasicBlock::Create(context, "inline_cont", builder.GetInsertBlock()->getParent());

        makeSSABlock(trueBranch, true);
        makeSSABlock(falseBranch, true);

        emitCondition(ast, trueBranch, falseBranch);
        builder.SetInsertPoint(trueBranch);
        builder.CreateBr(cont);
        builder.SetInsertPoint(falseBranch);
        builder.CreateBr(cont);
        builder.SetInsertPoint(cont);
        ssa = makeSSABlock(cont, true);
        const auto phi = builder.CreatePHI(i64Ty, 2);
        phi->addIncoming(builder.getInt64(0), falseBranch);
        phi->addIncoming(builder.getInt64(1), trueBranch);

        return phi;
    }

    llvm::Value* LLVMEmitter::emitExpression(const ASTNode* ast, bool pointer)
    {
        switch (ast->kind)
        {
        case ASTNode::Kind::ADD:
            {
                return builder.CreateAdd(emit64BitValue(ast->child, pointer), emit64BitValue(ast->child->sibling, pointer));
            }
        case ASTNode::Kind::SUB:
            {
                return builder.CreateSub(emit64BitValue(ast->child, pointer), emit64BitValue(ast->child->sibling, pointer));
            }
        case ASTNode::Kind::MUL:
            {
                // We only support 64 bit multiplication result
                return builder.CreateMul(emit64BitValue(ast->child, pointer), emit64BitValue(ast->child->sibling, pointer));
            }
        case ASTNode::Kind::DIV:
            {
                return builder.CreateSDiv(emit64BitValue(ast->child, pointer), emit64BitValue(ast->child->sibling, pointer));
            }
        case ASTNode::Kind::VARIABLE:
            {
                if (ast->variable.addressable)
                {
                    if (pointer) return ssa->lookup(ast->variable.id, builder, blockMap);
                    return builder.CreateLoad(i64Ty, findValueInEntry(ast->variable.id));
                }
                return ssa->lookup(ast->variable.id, builder, blockMap);
            }
        case ASTNode::Kind::NUMBER:
            {
                return builder.getInt64(ast->value);
            }
        case ASTNode::Kind::CALL:
            {
                llvm::SmallVector<llvm::Value*> args;
                auto current = ast->child;
                while (current)
                {
                    args.emplace_back(emit64BitValue(current, pointer));
                    current = current->sibling;
                }
                return builder.CreateCall(functionCallees[ast->function], args);
            }
        case ASTNode::Kind::UNARY_MINUS:
            {
                return builder.CreateNeg(emit64BitValue(ast->child, pointer));
            }
        case ASTNode::Kind::NOT:
            {
                llvm::Value* zero = llvm::ConstantInt::get(i64Ty, 0);
                return builder.CreateICmpEQ(emit64BitValue(ast->child, pointer), zero);
            }
        case ASTNode::Kind::NEGATE:
            {
                return builder.CreateXor(emit64BitValue(ast->child, pointer), builder.getInt64(-1));
            }
        case ASTNode::Kind::REFERENCE:
            {
                return emitExpression(ast->child, true);
            }
        case ASTNode::Kind::SUBSCRIPT:
            {
                // We interpret in a[b] a's value as address and b as offset
                const auto addr = builder.CreateIntToPtr(emitExpression(ast->child, false), builder.getPtrTy());
                const auto rawOffset = emitExpression(ast->child->sibling, false);
                const auto offset = builder.CreateMul(rawOffset, builder.getInt64(ast->sizeSpec));
                llvm::Type *type = i64Ty;

                switch (ast->sizeSpec)
                {
                case 1:
                    type = builder.getInt8Ty();
                    break;
                case 2:
                    type = builder.getInt16Ty();
                    break;
                case 4:
                    type = builder.getInt32Ty();
                    break;
                }

                const auto ptr = builder.CreateGEP(builder.getInt8Ty(), addr, {offset});
                if (pointer) return ptr;
                const auto value = builder.CreateLoad(type, ptr);
                return builder.CreateSExt(value, i64Ty);
            }
        case ASTNode::Kind::MODULO:
            {
                return builder.CreateSRem(emit64BitValue(ast->child, pointer), emit64BitValue(ast->child->sibling, pointer));
            }
        case ASTNode::Kind::LSHIFT:
            {
                return builder.CreateShl(emit64BitValue(ast->child, pointer), emit64BitValue(ast->child->sibling, pointer));
            }
        case ASTNode::Kind::RSHIFT:
            {
                return builder.CreateAShr(emit64BitValue(ast->child, pointer), emit64BitValue(ast->child->sibling, pointer));
            }
        case ASTNode::Kind::LT:
            {
                return builder.CreateICmpSLT(emit64BitValue(ast->child, pointer), emit64BitValue(ast->child->sibling, pointer));
            }
        case ASTNode::Kind::GT:
            {
                return builder.CreateICmpSGT(emit64BitValue(ast->child, pointer), emit64BitValue(ast->child->sibling, pointer));
            }
        case ASTNode::Kind::LEQ:
            {
                return builder.CreateICmpSLE(emit64BitValue(ast->child, pointer), emit64BitValue(ast->child->sibling, pointer));
            }
        case ASTNode::Kind::GEQ:
            {
                return builder.CreateICmpSGE(emit64BitValue(ast->child, pointer), emit64BitValue(ast->child->sibling, pointer));
            }
        case ASTNode::Kind::EQEQ:
            {
                return builder.CreateICmpEQ(emit64BitValue(ast->child, pointer), emit64BitValue(ast->child->sibling, pointer));
            }
        case ASTNode::Kind::NOTEQ:
            {
                return builder.CreateICmpNE(emit64BitValue(ast->child, pointer), emit64BitValue(ast->child->sibling, pointer));
            }
        case ASTNode::Kind::AND:
            {
                return builder.CreateAnd(emit64BitValue(ast->child, pointer), emit64BitValue(ast->child->sibling, pointer));
            }
        case ASTNode::Kind::OR:
            {
                return builder.CreateOr(emit64BitValue(ast->child, pointer), emit64BitValue(ast->child->sibling, pointer));
            }
        case ASTNode::Kind::XOR:
            {
                return builder.CreateXor(emit64BitValue(ast->child, pointer), emit64BitValue(ast->child->sibling, pointer));
            }
        case ASTNode::Kind::ANDAND:
        case ASTNode::Kind::OROR:
            {
                return emitInlineBoolLogic(ast);
            }
        case ASTNode::Kind::ASSIGN:
            {
                auto value = emit64BitValue(ast->child->sibling);

                // LHS can either be Subscript or variable
                if (ast->child->kind == ASTNode::Kind::VARIABLE)
                {
                    if (ast->child->variable.addressable)
                    {
                        builder.CreateStore(value, findValueInEntry(ast->child->variable.id));
                    }else
                    {
                        ssa->mapVariable(ast->child->variable.id, value);
                    }

                    return value;
                }

                // Now we must have a asubscript. First fix value byte width so we can store
                switch (ast->child->sizeSpec)
                {
                case 1:
                    value = builder.CreateTrunc(value, builder.getInt8Ty());
                    break;
                case 2:
                    value = builder.CreateTrunc(value, builder.getInt16Ty());
                    break;
                case 4:
                    value = builder.CreateTrunc(value, builder.getInt32Ty());
                    break;
                }

                auto ptr = emitExpression(ast->child, true);
                if (!ptr->getType()->isPointerTy())
                    ptr = builder.CreateIntToPtr(ptr, builder.getPtrTy());

                builder.CreateStore(value, ptr);

                return value;
            }
        default:
            throw std::runtime_error("unknown ASTNode kind");
        }
    }

    void LLVMEmitter::emitCondition(const ASTNode* ast, llvm::BasicBlock* trueBlock, llvm::BasicBlock* falseBlock)
    {
        if (ast->kind == ASTNode::Kind::OROR)
        {
            auto condition = emitExpression(ast->child);
            if (!condition->getType()->isIntegerTy(1))
                condition = builder.CreateICmpNE(condition, builder.getInt64(0));

            llvm::BasicBlock *orCont = llvm::BasicBlock::Create(context, "or_cont", builder.GetInsertBlock()->getParent());
            builder.CreateCondBr(condition, trueBlock, orCont);
            builder.SetInsertPoint(orCont);
            ssa = makeSSABlock(orCont, true);
            emitCondition(ast->child->sibling, trueBlock, falseBlock);
        }else if (ast->kind == ASTNode::Kind::ANDAND)
        {
            auto condition = emitExpression(ast->child);
            if (!condition->getType()->isIntegerTy(1))
                condition = builder.CreateICmpNE(condition, builder.getInt64(0));

            llvm::BasicBlock *andCont = llvm::BasicBlock::Create(context, "and_cont", builder.GetInsertBlock()->getParent());
            builder.CreateCondBr(condition, andCont, falseBlock);
            builder.SetInsertPoint(andCont);
            ssa = makeSSABlock(andCont, true);
            emitCondition(ast->child->sibling, trueBlock, falseBlock);
        }else
        {
            auto condition = emitExpression(ast);
            if (!condition->getType()->isIntegerTy(1))
                condition = builder.CreateICmpNE(condition, builder.getInt64(0));

            builder.CreateCondBr(condition, trueBlock, falseBlock);
        }
    }

    void LLVMEmitter::emitWhile(const ASTNode* ast)
    {
        llvm::BasicBlock *whileHeader = llvm::BasicBlock::Create(context, "while_header", builder.GetInsertBlock()->getParent());
        llvm::BasicBlock *whileBody = llvm::BasicBlock::Create(context, "while_body", builder.GetInsertBlock()->getParent());
        llvm::BasicBlock *whileCont = llvm::BasicBlock::Create(context, "while_cont", builder.GetInsertBlock()->getParent());

        builder.CreateBr(whileHeader);
        builder.SetInsertPoint(whileHeader);
        auto header = makeSSABlock(whileHeader, false);
        ssa = header;
        emitCondition(ast->child, whileBody, whileCont);
        builder.SetInsertPoint(whileBody);
        ssa = makeSSABlock(whileBody, true);
        emitStatement(ast->child->sibling);

        if (!builder.GetInsertBlock()->getTerminator())
            builder.CreateBr(whileHeader);

        header->seal(blockMap, builder);

        builder.SetInsertPoint(whileCont);
        ssa = makeSSABlock(whileCont, true);
    }

    void LLVMEmitter::emitIf(const ASTNode* ast)
    {
        llvm::BasicBlock *ifBody = llvm::BasicBlock::Create(context, "if_body", builder.GetInsertBlock()->getParent());
        llvm::BasicBlock *ifCont = llvm::BasicBlock::Create(context, "if_cont", builder.GetInsertBlock()->getParent());
        llvm::BasicBlock *elseBody = nullptr;
        if (ast->child->sibling->sibling)
        {
            elseBody = llvm::BasicBlock::Create(context, "else_body", builder.GetInsertBlock()->getParent());
            emitCondition(ast->child, ifBody, elseBody);
        }else
        {
            emitCondition(ast->child, ifBody, ifCont);
        }

        builder.SetInsertPoint(ifBody);
        ssa = makeSSABlock(ifBody, true);
        emitStatement(ast->child->sibling);

        if (!builder.GetInsertBlock()->getTerminator())
        {
            builder.CreateBr(ifCont);
        }

        if (elseBody)
        {
            builder.SetInsertPoint(elseBody);
            ssa = makeSSABlock(elseBody, true);
            emitStatement(ast->child->sibling->sibling);
            if (!builder.GetInsertBlock()->getTerminator())
            {
                builder.CreateBr(ifCont);
            }
        }

        builder.SetInsertPoint(ifCont);
        ssa = makeSSABlock(ifCont, true);
    }

    void LLVMEmitter::emitStatement(const ASTNode* ast)
    {
        switch (ast->kind)
        {
        case ASTNode::Kind::RETURN:
            if (ast->child)
            {
                builder.CreateRet(emit64BitValue(ast->child));
            }
            else
            {
                builder.CreateRet(llvm::UndefValue::get(i64Ty));
            }
            break;
        case ASTNode::Kind::BLOCK:
            emitBlock(ast);
            break;
        case ASTNode::Kind::WHILE:
            emitWhile(ast);
            break;
        case ASTNode::Kind::IF:
            emitIf(ast);
            break;
        default:
            emitExpression(ast);
        }
    }

    void LLVMEmitter::emitDeclStmt(const ASTNode* ast)
    {
        if (ast->kind == ASTNode::Kind::DECLARATION)
        {
            if (ast->child->variable.addressable)
            {
                // Make Stack Allocation
                const auto insertBlock = builder.GetInsertBlock();
                // Insert Alloca in entry block
                const auto entryBlock = &builder.GetInsertBlock()->getParent()->getEntryBlock();
                builder.SetInsertPoint(entryBlock);
                const auto stackSlot = builder.CreateAlloca(i64Ty, builder.getInt64(1));
                builder.CreateStore(emit64BitValue(ast->child->sibling), stackSlot);
                blockMap[entryBlock]->mapVariable(ast->child->variable.id, stackSlot);
                builder.SetInsertPoint(insertBlock);
            }else
            {
                // SSA Value
                ssa->mapVariable(ast->child->variable.id, emit64BitValue(ast->child->sibling));
            }
            return;
        }

        emitStatement(ast);
    }

    SSABlock* LLVMEmitter::makeSSABlock(llvm::BasicBlock* block, bool sealed)
    {
        auto ssaBlock = new (ssaBlockAllocator.Allocate()) SSABlock(block, sealed);
        blockMap[block] = ssaBlock;
        return ssaBlock;
    }

    void LLVMEmitter::emitBlock(const ASTNode* ast)
    {
        auto current = ast->child;
        while (current)
        {
            emitDeclStmt(current);
            current = current->sibling;
        }
    }

    void LLVMEmitter::emitFunction(const Function& function)
    {
        auto* llvmFunc = module->getFunction(function.name);

        if (function.children == nullptr)
        {
            // Just a declaration might be a external function like puts from c stdlib
            llvmFunc->setCallingConv(llvm::CallingConv::C);
            return;
        }

        llvm::BasicBlock *entryBB = llvm::BasicBlock::Create(context, "entry", llvmFunc);
        builder.SetInsertPoint(entryBB);

        ssa = makeSSABlock(entryBB, true);
        std::size_t paramIdentId = 0;
        for (auto& arg : llvmFunc->args())
        {
            ssa->mapVariable(++paramIdentId, &arg);
        }

        emitBlock(function.children);

        if (!builder.GetInsertBlock()->getTerminator())
        {
            builder.CreateRet(llvm::UndefValue::get(i64Ty));
        }
    }

    std::unique_ptr<llvm::Module> LLVMEmitter::buildModule()
    {
        //Register LLVM Functions
        for (const auto& function : program.functions)
        {
            llvm::SmallVector<llvm::Type *> params;
            for (uint32_t i = 0; i < function.argumentCount; i++)
            {
                params.emplace_back(i64Ty);
            }

            const auto fnTy = llvm::FunctionType::get(i64Ty, params, false);
            functionCallees.emplace_back(module->getOrInsertFunction(function.name, fnTy));
        }

        for (const auto& function : program.functions)
        {
            emitFunction(function);
        }

        return std::move(module);
    }
} // toy