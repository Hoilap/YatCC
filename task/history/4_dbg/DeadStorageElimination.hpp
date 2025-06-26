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

class DeadStorageElimination : public llvm::PassInfoMixin<DeadStorageElimination>
{
public:
  explicit DeadStorageElimination(llvm::raw_ostream& out)
    : mOut(out)
  {
  }

  llvm::PreservedAnalyses run(llvm::Module& mod,
                              llvm::ModuleAnalysisManager& mam);

private:
  llvm::raw_ostream& mOut;
  
  // 主要处理方法
  bool processFunction(llvm::Function& func);
  bool eliminateDeadStores(llvm::Function& func);
  bool eliminateDeadAllocas(llvm::Function& func);
  
  // 辅助方法
  bool isGloballyVisible(llvm::Value* value);

  // 分析方法
  bool isStoreUseful(llvm::StoreInst* store);
  bool escapesThroughCall(llvm::Value* value, llvm::Function& func);
  bool isVolatileOrAtomic(llvm::Instruction* inst);
  
  // 收集方法
  void collectStores(llvm::Function& func, std::vector<llvm::StoreInst*>& stores);
  void collectAllocas(llvm::Function& func, std::vector<llvm::AllocaInst*>& allocas);
  
    
  // 增强分析方法
  bool mayReachLoad(llvm::StoreInst* store, llvm::LoadInst* load);
  void eliminateAllocaAndUses(llvm::AllocaInst* alloca);
  bool isAllocaDefinitelyDead(llvm::AllocaInst* alloca);
  bool areAllStoresUseless(const std::vector<llvm::StoreInst*>& stores);
  bool isLoadResultMeaningful(llvm::LoadInst* load);
  // 统计信息
  struct Stats {
    int deadStoresEliminated = 0;
    int deadAllocasEliminated = 0;
    int totalInstructionsRemoved = 0;
  };
  
  Stats stats;
};
