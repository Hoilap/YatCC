#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Dominators.h>
#include <unordered_set>
#include <unordered_map>
#include <vector>

class LoopInvariantCodeMotion : public llvm::PassInfoMixin<LoopInvariantCodeMotion>
{
public:
  explicit LoopInvariantCodeMotion(llvm::raw_ostream& out)
    : mOut(out)
  {
  }

  llvm::PreservedAnalyses run(llvm::Module& mod,
                              llvm::ModuleAnalysisManager& mam);

private:
  llvm::raw_ostream& mOut;
  
  // 简化的循环结构
  struct SimpleLoop {
    llvm::BasicBlock* header;           // 循环头
    std::unordered_set<llvm::BasicBlock*> blocks;  // 循环体包含的所有基本块
    llvm::BasicBlock* preheader;        // 循环前置块
    std::unordered_set<llvm::BasicBlock*> exits;   // 循环出口
    
    SimpleLoop() : header(nullptr), preheader(nullptr) {}
  };
  
  bool processFunction(llvm::Function& func);
  bool findAndProcessLoops(llvm::Function& func);
  bool identifyLoop(llvm::BasicBlock* header, SimpleLoop& loop);
  
  // 简化版本的方法
  bool findAndProcessSimpleLoops(llvm::Function& func);
  bool identifySimpleLoop(llvm::BasicBlock* header, SimpleLoop& loop);
  llvm::BasicBlock* findSimplePreheader(llvm::BasicBlock* header);
  bool isLoopInvariantSimple(llvm::Instruction* inst, const SimpleLoop& loop);
  bool isSafeToHoistSimple(llvm::Instruction* inst);
  
  // 循环不变性分析
  bool isLoopInvariant(llvm::Instruction* inst, const SimpleLoop& loop);
  bool isOperandLoopInvariant(llvm::Value* operand, const SimpleLoop& loop);
  
  // 安全性检查
  bool isSafeToHoist(llvm::Instruction* inst, const SimpleLoop& loop);
  bool hasNoSideEffects(llvm::Instruction* inst);
  bool dominatesAllExits(llvm::Instruction* inst, const SimpleLoop& loop);
  
  // 代码移动
  bool hoistInvariantInstructions(const SimpleLoop& loop);
  void hoistInstruction(llvm::Instruction* inst, llvm::BasicBlock* preheader);
  
  // 辅助函数
  std::unordered_set<llvm::BasicBlock*> findLoopBlocks(llvm::BasicBlock* header);
  llvm::BasicBlock* findOrCreatePreheader(const SimpleLoop& loop);
  bool isDefinedInLoop(llvm::Value* value, const SimpleLoop& loop);
  
  // 存储已处理的循环不变指令
  std::unordered_set<llvm::Instruction*> invariantInstructions;
};
