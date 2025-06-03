#include "StrengthReduction.hpp"

using namespace llvm;

PreservedAnalyses
StrengthReduction::run(Module& mod, ModuleAnalysisManager& mam)
{
  int strengthReductionCount = 0;

  for (auto& func : mod) {
    for (auto& bb : func) {
      std::vector<Instruction*> instToErase;
      for (auto& inst : bb) {
        if (auto binOp = dyn_cast<BinaryOperator>(&inst)) {
          Value* lhs = binOp->getOperand(0);
          Value* rhs = binOp->getOperand(1);
          auto constLhs = dyn_cast<ConstantInt>(lhs);
          auto constRhs = dyn_cast<ConstantInt>(rhs);
          switch (binOp->getOpcode()) {
            case Instruction::Mul: {
              if (constLhs && constRhs) {
                if (constLhs->getSExtValue() == 2) {
                  binOp->replaceAllUsesWith(BinaryOperator::Create(Instruction::Shl, rhs, ConstantInt::getSigned(rhs->getType(), 1), "", &inst));
                  instToErase.push_back(binOp);
                  ++strengthReductionCount;
                } else if (constRhs->getSExtValue() == 2) {
                  binOp->replaceAllUsesWith(BinaryOperator::Create(Instruction::Shl, lhs, ConstantInt::getSigned(lhs->getType(), 1), "", &inst));
                  instToErase.push_back(binOp);
                  ++strengthReductionCount;
                }
              }
              break;
            }
            case Instruction::SDiv: {
              if (constRhs && constRhs->getSExtValue() == 2) {
                binOp->replaceAllUsesWith(BinaryOperator::Create(Instruction::AShr, lhs, ConstantInt::getSigned(lhs->getType(), 1), "", &inst));
                instToErase.push_back(binOp);
                ++strengthReductionCount;
              }
              break;
            }
            case Instruction::UDiv: {
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
      for (Instruction *inst : instToErase) {
        inst->eraseFromParent();
      }
    }
  }

  mOut << "StrengthReduction: " << strengthReductionCount << " instructions removed\n";
  return PreservedAnalyses::all();
}