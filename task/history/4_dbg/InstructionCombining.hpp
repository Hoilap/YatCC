#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Constants.h>
#include <unordered_set>

class InstructionCombining : public llvm::PassInfoMixin<InstructionCombining>
{
public:
  explicit InstructionCombining(llvm::raw_ostream& out)
    : mOut(out)
  {
  }

  llvm::PreservedAnalyses run(llvm::Module& mod,
                              llvm::ModuleAnalysisManager& mam);

private:
  llvm::raw_ostream& mOut;
  
  bool combineInstructionsInFunction(llvm::Function& func);
  bool combineInstructionsInBasicBlock(llvm::BasicBlock& bb);
  
  // 尝试合并二元运算指令
  llvm::Instruction* combineBinaryOperator(llvm::BinaryOperator* binOp);
  
  // 具体的合并方法
  llvm::Instruction* combineAdd(llvm::BinaryOperator* add);
  llvm::Instruction* combineSub(llvm::BinaryOperator* sub);
  llvm::Instruction* combineMul(llvm::BinaryOperator* mul);
  llvm::Instruction* combineDiv(llvm::BinaryOperator* div);
  
  // 新增：更多合并模式
  llvm::Instruction* combineShift(llvm::BinaryOperator* shift);
  llvm::Instruction* combineBitwise(llvm::BinaryOperator* bitwise);
  llvm::Instruction* combineComparison(llvm::CmpInst* cmp);
  llvm::Instruction* combineSelectInst(llvm::SelectInst* select);
  
  // 新增：特殊模式识别
  llvm::Instruction* combineAddChain(llvm::BinaryOperator* add);
  llvm::Instruction* combineMulByConstant(llvm::BinaryOperator* mul);
  llvm::Value* combineNegation(llvm::Value* val);
  
  // 辅助函数
  bool hasOneUseOfType(llvm::Value* val, unsigned opcode);
  bool hasOneUse(llvm::Value* val);
  bool canonicalizeInstruction(llvm::BinaryOperator* binOp);
  llvm::BinaryOperator* createBinaryOperator(unsigned opcode, 
                                            llvm::Value* lhs, 
                                            llvm::Value* rhs,
                                            llvm::Instruction* insertBefore);
  
  // 新增：常数合并辅助
  bool isConstantOrCanBeConstant(llvm::Value* val);
  llvm::Constant* getConstantValue(llvm::Value* val);
};
