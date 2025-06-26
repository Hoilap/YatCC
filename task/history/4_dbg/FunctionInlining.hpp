#pragma once

#include <llvm/IR/PassManager.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <unordered_set>
#include <unordered_map>
#include <vector>

class FunctionInlining : public llvm::PassInfoMixin<FunctionInlining>
{
public:
  explicit FunctionInlining(llvm::raw_ostream& out)
    : mOut(out), mMaxInlineSize(100), mMaxCallDepth(5)
  {
  }

  explicit FunctionInlining(llvm::raw_ostream& out, unsigned maxInlineSize, unsigned maxCallDepth = 5)
    : mOut(out), mMaxInlineSize(maxInlineSize), mMaxCallDepth(maxCallDepth)
  {
  }

  llvm::PreservedAnalyses run(llvm::Module& mod,
                              llvm::ModuleAnalysisManager& mam);

private:
  llvm::raw_ostream& mOut;
  unsigned mMaxInlineSize;
  unsigned mMaxCallDepth;
  
  // 函数调用图相关
  struct CallGraphNode {
    llvm::Function* function;
    std::unordered_set<CallGraphNode*> callees;    // 该函数调用的函数
    std::unordered_set<CallGraphNode*> callers;    // 调用该函数的函数
    bool inCycle = false;                          // 是否在环中
    unsigned estimatedSize = 0;                    // 函数大小估计
    unsigned callCount = 0;                        // 被调用次数
    
    CallGraphNode(llvm::Function* f) : function(f) {}
  };
  
  using CallGraph = std::unordered_map<llvm::Function*, std::unique_ptr<CallGraphNode>>;
  
  // 主要处理方法
  bool performInlining(llvm::Module& mod);
  
  // 调用图分析
  CallGraph buildCallGraph(llvm::Module& mod);
  void detectCycles(CallGraph& cg);
  void markCyclesFromNode(CallGraphNode* node, std::unordered_set<CallGraphNode*>& visiting,
                         std::unordered_set<CallGraphNode*>& visited);
  
  // 内联决策
  bool shouldInlineFunction(const CallGraphNode* node, llvm::CallInst* callInst);
  unsigned estimateFunctionSize(llvm::Function* func);
  bool isSimpleFunction(llvm::Function* func);
  
  // 内联实现
  bool inlineCallSite(llvm::CallInst* callInst);
  bool inlineFunctionCalls(llvm::Function* caller, llvm::Function* callee);
  
  // 清理工作
  void removeUnusedFunctions(llvm::Module& mod, const CallGraph& cg);
  
  // 统计信息
  struct Stats {
    int functionsInlined = 0;
    int callSitesInlined = 0;
    int functionsRemoved = 0;
    int totalSizeReduction = 0;
  };
  
  Stats stats;
};
