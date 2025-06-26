#include "DominatorTree.hpp"
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/CFG.h>
#include <queue>
#include <algorithm>

using namespace llvm;

PreservedAnalyses
CustomDominatorTreeAnalysis::run(Module& mod, ModuleAnalysisManager& mam)
{
  int functionsAnalyzed = 0;
  
  mOut << "--------------------------------------------------\n";
  mOut << "        Dominator Tree Analysis Results\n";
  mOut << "--------------------------------------------------\n";

  // 遍历所有函数
  for (auto& func : mod) {
    if (func.isDeclaration())
      continue;
    
    buildDominatorTree(func);
    functionsAnalyzed++;
  }

  return PreservedAnalyses::all();
}

void
CustomDominatorTreeAnalysis::buildDominatorTree(Function& func)
{
  // 使用暴力算法计算支配关系
  auto dominators = computeDominators(func);
  
  // 构建支配树结构
  constructDomTree(func, dominators);
  
  // 应用示例
  findLoopHeaders(func);
  auto domFrontier = computeDominanceFrontier(func);
}

std::unordered_map<BasicBlock*, std::unordered_set<BasicBlock*>>
CustomDominatorTreeAnalysis::computeDominators(Function& func)
{
  std::vector<BasicBlock*> blocks;
  std::unordered_map<BasicBlock*, int> blockIndex;
  
  // 收集所有基本块
  for (auto& bb : func) {
    blockIndex[&bb] = blocks.size();
    blocks.push_back(&bb);
  }
  
  if (blocks.empty()) {
    return {};
  }
  
  BasicBlock* entry = &func.getEntryBlock();
  std::unordered_map<BasicBlock*, std::unordered_set<BasicBlock*>> dominators;
  
  // 初始化：入口节点只被自己支配，其他节点被所有节点支配
  dominators[entry] = {entry};
  for (auto* bb : blocks) {
    if (bb != entry) {
      dominators[bb] = std::unordered_set<BasicBlock*>(blocks.begin(), blocks.end());
    }
  }
  
  // 迭代计算支配关系
  bool changed = true;
  while (changed) {
    changed = false;
    
    for (auto* bb : blocks) {
      if (bb == entry) continue;
      
      // 计算新的支配集合：所有前驱的支配集合的交集，再加上自己
      std::unordered_set<BasicBlock*> newDoms;
      bool first = true;
      
      for (auto* pred : predecessors(bb)) {
        if (first) {
          newDoms = dominators[pred];
          first = false;
        } else {
          // 计算交集
          std::unordered_set<BasicBlock*> intersection;
          for (auto* domBB : newDoms) {
            if (dominators[pred].count(domBB)) {
              intersection.insert(domBB);
            }
          }
          newDoms = std::move(intersection);
        }
      }
      
      // 加上自己
      newDoms.insert(bb);
      
      // 检查是否有变化
      if (newDoms != dominators[bb]) {
        dominators[bb] = std::move(newDoms);
        changed = true;
      }
    }
  }
  
  return dominators;
}

void
CustomDominatorTreeAnalysis::constructDomTree(Function& func,
    const std::unordered_map<BasicBlock*, std::unordered_set<BasicBlock*>>& doms)
{
  auto& domTree = domTrees[&func];
  
  // 创建所有节点
  for (auto& bb : func) {
    domTree[&bb] = new DomTreeNode(&bb);
  }
  
  BasicBlock* entry = &func.getEntryBlock();
  domTreeRoots[&func] = domTree[entry];
  
  // 计算直接支配关系
  for (auto& bb : func) {
    if (&bb == entry) continue;
    
    BasicBlock* idom = nullptr;
    const auto& domSet = doms.at(&bb);
    
    // 找到直接支配节点：支配集合中除了自己之外最"近"的节点
    for (auto* domBB : domSet) {
      if (domBB == &bb) continue;
      
      if (!idom || doms.at(domBB).size() > doms.at(idom).size()) {
        idom = domBB;
      }
    }
    
    if (idom) {
      domTree[&bb]->idom = domTree[idom];
      domTree[idom]->children.push_back(domTree[&bb]);
    }
  }
  
  // 计算层级
  std::function<void(DomTreeNode*, int)> computeLevels = [&](DomTreeNode* node, int level) {
    node->level = level;
    for (auto* child : node->children) {
      computeLevels(child, level + 1);
    }
  };
  
  computeLevels(domTreeRoots[&func], 0);
}

void
CustomDominatorTreeAnalysis::findLoopHeaders(Function& func)
{
  
  if (!domTrees.count(&func)) return;
  
  auto& domTree = domTrees[&func];
  
  for (auto& bb : func) {
    for (auto* succ : successors(&bb)) {
      // 如果后继支配当前块，则存在回边，后继是循环头
      DomTreeNode* current = domTree[&bb];
      while (current) {
        if (current->block == succ) {
          mOut << "  Loop header: " << succ->getName() 
               << " (back edge from " << bb.getName() << ")\n";
          break;
        }
        current = current->idom;
      }
    }
  }
}

std::unordered_map<BasicBlock*, std::unordered_set<BasicBlock*>>
CustomDominatorTreeAnalysis::computeDominanceFrontier(Function& func)
{
  std::unordered_map<BasicBlock*, std::unordered_set<BasicBlock*>> domFrontier;
  
  if (!domTrees.count(&func)) return domFrontier;
  
  auto& domTree = domTrees[&func];
  
  // 对每个基本块计算其支配边界
  for (auto& bb : func) {
    for (auto* succ : successors(&bb)) {
      BasicBlock* runner = &bb;
      
      // 从当前块开始，沿着支配树向上走
      while (runner && domTree[runner]->idom && domTree[runner]->idom->block != succ) {
        // 如果runner不严格支配succ，则succ在runner的支配边界中
        bool strictlyDominates = false;
        DomTreeNode* current = domTree[succ];
        while (current && current->idom) {
          if (current->idom->block == runner) {
            strictlyDominates = true;
            break;
          }
          current = current->idom;
        }
        
        if (!strictlyDominates) {
          domFrontier[runner].insert(succ);
        }
        if(domTree[runner]->idom) runner = domTree[runner]->idom->block;
        else runner = nullptr;
      }
    }
  }
  
  return domFrontier;
}
