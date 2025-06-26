#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instructions.h>
#include <unordered_map>
#include <unordered_set>

class CommonSubexpressionElimination : public llvm::PassInfoMixin<CommonSubexpressionElimination>
{
public:
  explicit CommonSubexpressionElimination(llvm::raw_ostream& out)
    : mOut(out), mWindowSize(20), mNextValueNumber(1)
  {
  }

  llvm::PreservedAnalyses run(llvm::Module& mod,
                              llvm::ModuleAnalysisManager& mam);

private:
  llvm::raw_ostream& mOut;
  size_t mWindowSize;
  
  // GVN相关数据结构
  uint32_t mNextValueNumber;
  std::unordered_map<llvm::Value*, uint32_t> mValueNumbers;
  std::unordered_map<std::string, uint32_t> mExpressionToVN;
  std::unordered_map<uint32_t, llvm::Value*> mVNToLeader;
  
  bool EliminateCSEInFunction(llvm::Function& func);
  bool EliminateCSEInBasicBlock(llvm::BasicBlock& bb);
  
  // GVN相关方法
  void initializeGVN();
  uint32_t getValueNumber(llvm::Value* value);
  std::string getExpressionKey(llvm::Instruction* inst);
  uint32_t assignValueNumber(llvm::Instruction* inst);
  void updateLeader(uint32_t vn, llvm::Value* value);
  
  // 原有方法
  bool areInstructionsEquivalent(llvm::Instruction* inst1, llvm::Instruction* inst2);
  bool isCSECandidate(llvm::Instruction* inst);
  bool hasSideEffects(llvm::Instruction* inst);
  bool hasInterfering(llvm::Instruction* first, llvm::Instruction* second);
};
