#include "StrengthReduction.hpp"

using namespace llvm;

PreservedAnalyses
StrengthReduction::run(Module& mod, ModuleAnalysisManager& mam)
{
  int strengthReductionCount = 0; // 记录优化指令数

  // 遍历模块中的每个函数
  for (auto& func : mod) {
    // 遍历函数中的每个基本块
    for (auto& bb : func) {
      std::vector<Instruction*> instToErase; // 记录待删除的指令
      // 遍历基本块中的每条指令
      for (auto& inst : bb) {
        // 只处理二元运算指令
        if (auto binOp = dyn_cast<BinaryOperator>(&inst)) {
          Value* lhs = binOp->getOperand(0); // 左操作数
          Value* rhs = binOp->getOperand(1); // 右操作数
          auto constLhs = dyn_cast<ConstantInt>(lhs); // 判断左操作数是否为常数
          auto constRhs = dyn_cast<ConstantInt>(rhs); // 判断右操作数是否为常数
          switch (binOp->getOpcode()) {
            case Instruction::Mul: { // 乘法指令
              // 如果有一个操作数是常数2，则用移位替换乘法
              if (constLhs && constRhs) {
                if (constLhs->getSExtValue() == 2) {
                  // 2 * x => x << 1
                  binOp->replaceAllUsesWith(BinaryOperator::Create(Instruction::Shl, rhs, ConstantInt::getSigned(rhs->getType(), 1), "", &inst));
                  instToErase.push_back(binOp);
                  ++strengthReductionCount;
                } else if (constRhs->getSExtValue() == 2) {
                  // x * 2 => x << 1
                  binOp->replaceAllUsesWith(BinaryOperator::Create(Instruction::Shl, lhs, ConstantInt::getSigned(lhs->getType(), 1), "", &inst));
                  instToErase.push_back(binOp);
                  ++strengthReductionCount;
                }
              }
              break;
            }
            case Instruction::SDiv: { // 有符号除法
              // x / 2 => x >> 1（算术右移）
              if (constRhs && constRhs->getSExtValue() == 2) {
                binOp->replaceAllUsesWith(BinaryOperator::Create(Instruction::AShr, lhs, ConstantInt::getSigned(lhs->getType(), 1), "", &inst));
                instToErase.push_back(binOp);
                ++strengthReductionCount;
              }
              break;
            }
            case Instruction::UDiv: { // 无符号除法
              // x / 2 => x >> 1（逻辑右移）
              if (constRhs && constRhs->getSExtValue() == 2) {
                binOp->replaceAllUsesWith(BinaryOperator::Create(Instruction::LShr, lhs, ConstantInt::getSigned(lhs->getType(), 1), "", &inst));
                instToErase.push_back(binOp);
                ++strengthReductionCount;
              }
              break;
            }
          }
        }
      }
      // 删除已替换的原始指令
      for (Instruction *inst : instToErase) {
        inst->eraseFromParent();
      }
    }
  }

  // 输出优化信息
  mOut << "StrengthReduction: " << strengthReductionCount << " instructions removed\n";
  return PreservedAnalyses::all(); // 声明所有分析结果都被保留
}