#include "FunctionInlining.hpp"
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <queue>
#include <algorithm>

using namespace llvm;

PreservedAnalyses
FunctionInlining::run(Module& mod, ModuleAnalysisManager& mam)
{
  bool changed = false;
  
  // 重置统计信息
  stats = Stats();

  mOut << "--------------------------------------\n";
  mOut << "   Function Inlining Analysis Results\n";
  mOut << "--------------------------------------\n";

  if (performInlining(mod)) {
    changed = true;
  }

  return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

bool
FunctionInlining::performInlining(Module& mod)
{
  bool changed = false;
  auto callGraph = buildCallGraph(mod);

  detectCycles(callGraph);
  
  // 收集所有可以内联的调用点
  std::vector<CallInst*> inlineCandidates;
  
  for (auto& func : mod) {
    if (func.isDeclaration()) continue;
    
    for (auto& bb : func) {
      for (auto& inst : bb) {
        if (auto* callInst = dyn_cast<CallInst>(&inst)) {
          Function* callee = callInst->getCalledFunction();
          if (!callee || callee->isDeclaration()) continue;
          
          auto it = callGraph.find(callee);
          if (it != callGraph.end() && shouldInlineFunction(it->second.get(), callInst)) {
            inlineCandidates.push_back(callInst);
          }
        }
      }
    }
  }
  
  // 按照被调用函数的大小排序，优先内联小函数
  std::sort(inlineCandidates.begin(), inlineCandidates.end(),
           [&](CallInst* a, CallInst* b) {
             Function* funcA = a->getCalledFunction();
             Function* funcB = b->getCalledFunction();
             return estimateFunctionSize(funcA) < estimateFunctionSize(funcB);
           });
  
  // 执行内联
  for (auto* callInst : inlineCandidates) {
    // 检查调用指令是否仍然有效（可能已被之前的内联删除）
    if (callInst->getParent() == nullptr) continue;
    
    Function* callee = callInst->getCalledFunction();
    if (!callee) continue;
    
    if (inlineCallSite(callInst)) {
      stats.callSitesInlined++;
      changed = true;
    }
  }
  
  // 清理未使用的函数
  removeUnusedFunctions(mod, callGraph);
  
  return changed;
}

FunctionInlining::CallGraph
FunctionInlining::buildCallGraph(Module& mod)
{
  CallGraph cg;
  
  // 首先为所有函数创建节点
  for (auto& func : mod) {
    if (!func.isDeclaration()) {
      auto node = std::make_unique<CallGraphNode>(&func);
      node->estimatedSize = estimateFunctionSize(&func);
      cg[&func] = std::move(node);
    }
  }
  
  // 然后建立调用关系
  for (auto& func : mod) {
    if (func.isDeclaration()) continue;
    
    auto callerIt = cg.find(&func);
    if (callerIt == cg.end()) continue;
    
    for (auto& bb : func) {
      for (auto& inst : bb) {
        if (auto* callInst = dyn_cast<CallInst>(&inst)) {
          Function* callee = callInst->getCalledFunction();
          if (!callee || callee->isDeclaration()) continue;
          
          auto calleeIt = cg.find(callee);
          if (calleeIt != cg.end()) {
            // 建立调用关系
            callerIt->second->callees.insert(calleeIt->second.get());
            calleeIt->second->callers.insert(callerIt->second.get());
            calleeIt->second->callCount++;
          }
        }
      }
    }
  }
  
  return cg;
}

void
FunctionInlining::detectCycles(CallGraph& cg)
{
  std::unordered_set<CallGraphNode*> visited;
  std::unordered_set<CallGraphNode*> visiting;
  
  for (auto& pair : cg) {
    if (visited.find(pair.second.get()) == visited.end()) {
      markCyclesFromNode(pair.second.get(), visiting, visited);
    }
  }
}

void
FunctionInlining::markCyclesFromNode(CallGraphNode* node,
                                    std::unordered_set<CallGraphNode*>& visiting,
                                    std::unordered_set<CallGraphNode*>& visited)
{
  if (visited.find(node) != visited.end()) return;
  if (visiting.find(node) != visiting.end()) {
    // 发现环，标记当前路径上的所有节点
    node->inCycle = true;
    return;
  }
  
  visiting.insert(node);
  
  for (auto* callee : node->callees) {
    markCyclesFromNode(callee, visiting, visited);
    if (callee->inCycle) {
      node->inCycle = true;
    }
  }
  
  visiting.erase(node);
  visited.insert(node);
}

bool
FunctionInlining::shouldInlineFunction(const CallGraphNode* node, CallInst* callInst)
{
  Function* callee = node->function;
  
  // 1. 不内联在环中的函数（递归函数）
  if (node->inCycle) {
    return false;
  }
  
  // 2. 不内联过大的函数
  if (node->estimatedSize > mMaxInlineSize) {
    return false;
  }
  
  // 3. 不内联变参函数
  if (callee->isVarArg()) {
    return false;
  }
  
  // 4. 不内联有地址被取的函数
  if (callee->hasAddressTaken()) {
    return false;
  }
  
  // 5. 总是内联简单的函数
  if (isSimpleFunction(callee)) {
    return true;
  }
  
  // 6. 对于小函数，如果调用次数不多，可以内联
  if (node->estimatedSize <= 20 && node->callCount <= 3) {
    return true;
  }
  
  // 7. 对于只被调用一次的函数，优先内联
  if (node->callCount == 1) {
    return true;
  }
  
  return false;
}

unsigned
FunctionInlining::estimateFunctionSize(Function* func)
{
  unsigned size = 0;
  for (auto& bb : *func) {
    for (auto& inst : bb) {
      // 简单计数，每个指令算1个单位
      size++;
      
      // 某些指令可能比较重，给予更高权重
      if (isa<CallInst>(&inst)) {
        size += 2; // 函数调用比较重
      } else if (isa<LoadInst>(&inst) || isa<StoreInst>(&inst)) {
        size += 1; // 内存访问有额外开销
      }
    }
  }
  return size;
}

bool
FunctionInlining::isSimpleFunction(Function* func)
{
  // 检查是否是简单函数（例如只有一个基本块，几条指令）
  if (func->size() != 1) return false;
  
  BasicBlock& bb = func->front();
  if (bb.size() > 5) return false; // 最多5条指令
  
  // 检查是否只包含简单操作
  for (auto& inst : bb) {
    if (isa<CallInst>(&inst) && !isa<ReturnInst>(&inst)) {
      return false; // 包含函数调用的不算简单函数
    }
    if (isa<LoadInst>(&inst) || isa<StoreInst>(&inst)) {
      return false; // 包含内存访问的不算简单函数
    }
  }
  
  return true;
}

bool
FunctionInlining::inlineCallSite(CallInst* callInst)
{
  Function* callee = callInst->getCalledFunction();
  if (!callee) return false;
  
  // 使用LLVM的内联工具
  InlineFunctionInfo IFI;
  
  // 执行内联
  InlineResult result = InlineFunction(*callInst, IFI);
  
  if (result.isSuccess()) {
    stats.functionsInlined++;
    return true;
  }
  
  return false;
}

void
FunctionInlining::removeUnusedFunctions(Module& mod, const CallGraph& cg)
{
  std::vector<Function*> toRemove;
  
  for (auto& func : mod) {
    if (func.isDeclaration()) continue;
    
    // 不删除main函数和外部可见的函数
    if (func.getName() == "main" || func.hasExternalLinkage()) {
      continue;
    }
    
    // 检查是否还有调用者
    auto it = cg.find(&func);
    if (it != cg.end() && it->second->callers.empty() && func.use_empty()) {
      toRemove.push_back(&func);
    }
  }
  
  // 删除未使用的函数
  for (auto* func : toRemove) {
    func->eraseFromParent();
    stats.functionsRemoved++;
  }
}
