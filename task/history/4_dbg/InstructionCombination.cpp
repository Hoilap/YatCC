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
    int ChainLength = 1;
    Value *Increment = RHS;
    Value *BaseInput = LHS; //相当于回溯的指针
    std::vector<BinaryOperator*> Chain;
    Chain.push_back(LastAdd);
    // 回溯加法链
    while(auto *Add = dyn_cast<BinaryOperator>(BaseInput)) {
        if (Add->getOpcode() == Instruction::Add && Add->hasNoSignedWrap()) {
            if (Add->getOperand(1) == Increment) {
                ChainLength++;
                Chain.push_back(Add);
                BaseInput = Add->getOperand(0);//不断递归之后，baseinput去到加法链最头部
            } else {
                break;
            }
        } else {
            break;
        }
    }
    if(ChainLength > 1 && BaseInput) {
        //创建新的乘法指令
        IRBuilder<> Builder(LastAdd);
        Value *Multiplier = ConstantInt::get(Increment->getType(), ChainLength); //创建一个常量整数值
        Value *Scaled = Builder.CreateNSWMul(Increment, Multiplier); //生成一条乘法指令，计算 Increment * ChainLength
        Value *NewAdd = Builder.CreateNSWAdd(BaseInput, Scaled); //生成一条加法指令，计算 BaseInput + Scaled（即 BaseInput + (Increment * ChainLength)）
        LastAdd->replaceAllUsesWith(NewAdd);
        // 删除链中的所有加法指令（加入vector）
        for (auto *C : Chain) {
            C->eraseFromParent();
        }
        // << "Optimized add chain: Replaced " << ChainLength << " adds with a single multiply and add.\n";
        return true;
    }
    return false;
}

PreservedAnalyses InstructionCombination::run(Module &M, ModuleAnalysisManager &AM) {
    for(auto &F : M) {
        //处理加法问题:注意迭代器问题
        for (auto &BB : F) {
            // 逆序遍历指令；不断从rebegin()开始，知道迭代器能够到达rend位置
            for (auto It = BB.rbegin(); It != BB.rend(); ++It) {
                Instruction &I = *It;
                if (auto *Add = dyn_cast<BinaryOperator>(&I)) {
                    if (Add->getOpcode() == Instruction::Add && Add->hasNoSignedWrap()) {
                        // 处理加法链优化
                        mOut << Add->getName() << " is an add instruction.\n";
                        bool changed=optimizeAddChain(Add);
                        if(changed) {
                            // 如果优化成功，重新开始遍历
                            mOut << "Optimized add chain for " << Add->getName() << ".\n";
                            It = BB.rbegin(); // 重置迭代器到起始位置
                            continue; // 重新开始当前循环
                        }
                    }
                }
            }
        }

    }
    mOut << "InstructionCombination running...\n";
    return PreservedAnalyses::all();
}
