#include "InstructionCombination.hpp"
using namespace llvm;
// 指令合并：将多个相同的指令合并为一个指令，减少冗余计算
// const-propagation.sysu.c 有非常多load公共变量的情况
// instruction-combining.c 非常多常数加法指令

bool optimizeAddChain(BinaryOperator *LastAdd) {
    // 检查是否是加法链的末端（如 %addN = add %add(N-1), x）
    Value *LHS = LastAdd->getOperand(0);
    Value *RHS = LastAdd->getOperand(1);

    // 情况1：连续加同一个值（%addN = add %add(N-1), %x）
    if (auto *PrevAdd = dyn_cast<BinaryOperator>(LHS)) {
        if (PrevAdd->getOpcode() == Instruction::Add && PrevAdd->hasNoSignedWrap()) {
            int ChainLength = 1;
            Value *Increment = RHS;
            Value *BaseInput = nullptr;

            // 回溯加法链
            BinaryOperator *Current = PrevAdd;
            std::vector<BinaryOperator*> Chain;
            Chain.push_back(Current);
            while (Current) {
                if (Current->getOperand(1) == Increment) {
                    ChainLength++;
                    Chain.push_back(Current);
                    if (auto *Next = dyn_cast<BinaryOperator>(Current->getOperand(0))) {
                        Current = Next;
                    } else {
                        BaseInput = Current->getOperand(0);
                        break;
                    }
                } else {
                    break;
                }
            }

            // 如果链长度>1，替换为乘法
            if (ChainLength > 1 && BaseInput) {
                //删除链中的所有加法指令
                for (auto *C : Chain) {
                    C->eraseFromParent();
                }
                // 创建新的乘法指令
                IRBuilder<> Builder(LastAdd);
                Value *Multiplier = ConstantInt::get(Increment->getType(), ChainLength);
                Value *Scaled = Builder.CreateNSWMul(Increment, Multiplier);
                Value *NewAdd = Builder.CreateNSWAdd(BaseInput, Scaled);
                LastAdd->replaceAllUsesWith(NewAdd);
                return true;
            }
        }
    }
    return false;
}

PreservedAnalyses InstructionCombination::run(Module &M, ModuleAnalysisManager &AM) {
    for(auto &F : M) {
        //处理load指令的问题(其实应该放进CSE里面)
        for (auto &BB : F) {
            //保存计算后的结果
            std::unordered_map<std::string, Instruction*> exprMap;
            std::vector<Instruction*> toErase;
            DenseMap<Value *, Value *> LoadedGlobals;//应该放在基本块内
            for (auto &I : BB) {
                // 特殊处理load指令: 
                if (auto *LI = dyn_cast<LoadInst>(&I)) {
                    Value *Ptr = LI->getPointerOperand();
                    if (auto *GV = dyn_cast<GlobalVariable>(Ptr)) {
                        // 如果已加载过该全局变量，替换当前加载
                        if (LoadedGlobals.count(GV)) {
                            LI->replaceAllUsesWith(LoadedGlobals[GV]);
                            toErase.push_back(LI);
                            mOut << "Replacing load of " << GV->getName() << " with previous loaded value.\n";
                        }
                        else {
                            LoadedGlobals[GV] = LI;
                        }
                    }
                }
                if(auto *SI = dyn_cast<StoreInst>(&I)){//应该改成I.mayWriteToMemory()
                    Value *Ptr = SI->getPointerOperand(); 
                    if (auto *GV = dyn_cast<GlobalVariable>(Ptr)) {
                        // 这个store指令的全局变量已经load过,那要删除这表中的条目
                        LoadedGlobals.erase(GV);
                    }
                }
            }
            // 按逆序删除指令以避免使用已删除的指令
            for (auto *I : llvm::reverse(toErase)) {
                I->eraseFromParent();
            }
        }

        //处理加法问题:注意迭代器问题
        for (auto &BB : F) {
            // 逆序遍历指令
            for (auto It = BB.rbegin(); It != BB.rend(); ) {
                Instruction &I = *It;
                ++It; // 提前递增迭代器，避免删除时失效
                if (auto *Add = dyn_cast<BinaryOperator>(&I)) {
                    if (Add->getOpcode() == Instruction::Add && Add->hasNoSignedWrap()) {
                        // 处理加法链优化
                        optimizeAddChain(Add);
                    }
                }
            }
        }

    }
    mOut << "InstructionCombination running...\n";
    return PreservedAnalyses::all();
}
