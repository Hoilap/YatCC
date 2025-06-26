#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Analysis/PostDominators.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class CustomDominatorTreeAnalysis : public llvm::PassInfoMixin<CustomDominatorTreeAnalysis>
{
public:
  explicit CustomDominatorTreeAnalysis(llvm::raw_ostream& out)
    : mOut(out)
  {
  }

  llvm::PreservedAnalyses run(llvm::Module& mod,
                              llvm::ModuleAnalysisManager& mam);

private:
  llvm::raw_ostream& mOut;
  
  // 简化的支配树节点结构
  struct DomTreeNode {
    llvm::BasicBlock* block;
    DomTreeNode* idom;  // 直接支配节点
    std::vector<DomTreeNode*> children;  // 被支配的子节点
    int level;  // 在支配树中的层级
    
    DomTreeNode(llvm::BasicBlock* bb) : block(bb), idom(nullptr), level(0) {}
  };
  
  // 支配树构建
  void buildDominatorTree(llvm::Function& func);
  
  // 暴力算法求支配关系
  std::unordered_map<llvm::BasicBlock*, std::unordered_set<llvm::BasicBlock*>> 
  computeDominators(llvm::Function& func);
  
  // 构建支配树结构
  void constructDomTree(llvm::Function& func,const std::unordered_map<llvm::BasicBlock*, std::unordered_set<llvm::BasicBlock*>>& doms);
  
  // 应用示例：查找循环头
  void findLoopHeaders(llvm::Function& func);
  
  // 应用示例：计算支配边界
  std::unordered_map<llvm::BasicBlock*, std::unordered_set<llvm::BasicBlock*>>
  computeDominanceFrontier(llvm::Function& func);
  
  // 存储函数的支配树信息
  std::unordered_map<llvm::Function*, std::unordered_map<llvm::BasicBlock*, DomTreeNode*>> domTrees;
  std::unordered_map<llvm::Function*, DomTreeNode*> domTreeRoots;
};
