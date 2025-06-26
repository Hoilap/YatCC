#include "DeadStorageElimination.hpp"
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/CFG.h>
#include <algorithm>
#include <queue>

using namespace llvm;

PreservedAnalyses
DeadStorageElimination::run(Module& mod, ModuleAnalysisManager& mam)
{
  bool changed = false;
  int functionsProcessed = 0;
  
  // 重置统计信息
  stats = Stats();

  mOut << "----------------------------------------------\n";
  mOut << "   Dead Storage Elimination Analysis Results\n";
  mOut << "----------------------------------------------\n";

  // 遍历所有函数
  for (auto& func : mod) {
    if (func.isDeclaration())
      continue;
    
    mOut << "\nProcessing function: " << func.getName() << "\n";
    
    if (processFunction(func)) {
      changed = true;
      functionsProcessed++;
    }
  }

  return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

bool
DeadStorageElimination::processFunction(Function& func)
{
  bool changed = false;
  
  // 多轮处理，直到没有更多优化
  bool iterationChanged = true;
  int iteration = 0;
  while (iterationChanged && iteration < 5) {
    iterationChanged = false;
    iteration++;
    
    // 1. 先消除明显的死alloca
    if (eliminateDeadAllocas(func)) {
      iterationChanged = true;
      changed = true;
    }
    
    // 2. 然后消除死存储指令
    if (eliminateDeadStores(func)) {
      iterationChanged = true;
      changed = true;
    }
  }
  
  return changed;
}

bool
DeadStorageElimination::eliminateDeadAllocas(Function& func)
{
  bool changed = false;
  std::vector<AllocaInst*> allocas;
  
  // 收集所有分配指令
  collectAllocas(func, allocas);
  
  // 使用更激进但安全的策略识别死alloca
  for (auto* alloca : allocas) {
    if (alloca->getParent() == nullptr) continue; // 已被删除
    
    if (isAllocaDefinitelyDead(alloca)) {
      mOut << "  Eliminating definitely dead alloca: ";
      alloca->print(mOut);
      mOut << "\n";
      
      eliminateAllocaAndUses(alloca);
      changed = true;
    }
  }
  
  return changed;
}

bool
DeadStorageElimination::isAllocaDefinitelyDead(AllocaInst* alloca)
{
  // 1. 如果alloca逃逸到函数外部，不能删除
  if (escapesThroughCall(alloca, *alloca->getFunction())) {
    return false;
  }
  
  // 2. 如果没有任何使用，肯定是死的
  if (alloca->use_empty()) {
    return true;
  }
  
  // 3. 更保守的分析 - 只删除确实没有被使用的alloca
  bool hasLoad = false;
  bool hasStore = false;
  
  for (auto* user : alloca->users()) {
    if (isa<LoadInst>(user)) {
      hasLoad = true;
    } else if (auto* store = dyn_cast<StoreInst>(user)) {
      if (store->getPointerOperand() == alloca) {
        hasStore = true;
      }
    } else {
      // 有其他类型的使用，保守处理
      return false;
    }
  }
  
  // 只有在确实没有load且没有store的情况下才删除
  return !hasLoad && !hasStore;
}

bool
DeadStorageElimination::isGloballyVisible(Value* value)
{
  // 1. 全局变量
  if (isa<GlobalVariable>(value)) {
    return true;
  }
  
  // 2. 函数参数（可能指向全局内存）
  if (isa<Argument>(value)) {
    return true;
  }
  
  // 3. 函数调用返回值
  if (isa<CallInst>(value)) {
    return true;
  }
  
  // 4. 递归检查GEP和类型转换
  if (auto* gep = dyn_cast<GetElementPtrInst>(value)) {
    return isGloballyVisible(gep->getPointerOperand());
  }
  
  if (auto* bitcast = dyn_cast<BitCastInst>(value)) {
    return isGloballyVisible(bitcast->getOperand(0));
  }
  
  // alloca总是局部的
  if (isa<AllocaInst>(value)) {
    return false;
  }
  
  // 保守：其他情况认为是全局可见的
  return true;
}

bool
DeadStorageElimination::areAllStoresUseless(const std::vector<StoreInst*>& stores)
{
  // 检查所有store是否都是无用的
  for (auto* store : stores) {
    Value* storedValue = store->getValueOperand();
    
    // 如果存储的是函数调用结果，保守处理
    if (isa<CallInst>(storedValue)) {
      return false;
    }
    
    // 如果存储的是从其他地方load的值，可能是有用的数据传递
    if (auto* loadInst = dyn_cast<LoadInst>(storedValue)) {
      // 检查load的来源
      Value* loadSrc = loadInst->getPointerOperand();
      if (!isa<AllocaInst>(loadSrc) || isGloballyVisible(loadSrc)) {
        return false;
      }
    }
    
    // 如果存储的是非平凡的计算结果，可能有用
    if (isa<BinaryOperator>(storedValue) || isa<GetElementPtrInst>(storedValue)) {
      // 进一步检查这个计算是否依赖于有意义的输入
      if (auto* binOp = dyn_cast<BinaryOperator>(storedValue)) {
        for (auto& operand : binOp->operands()) {
          if (!isa<Constant>(operand) && !isa<AllocaInst>(operand)) {
            return false; // 依赖于外部输入，可能有用
          }
        }
      }
    }
  }
  
  return true; // 所有store都是无用的
}

bool
DeadStorageElimination::isLoadResultMeaningful(LoadInst* load)
{
  if (load->use_empty()) {
    return false; // 没有使用，肯定无意义
  }
  
  // 检查load的每个使用
  for (auto* user : load->users()) {
    if (auto* userInst = dyn_cast<Instruction>(user)) {
      // 如果被用于控制流，认为是有意义的
      if (isa<BranchInst>(userInst) || isa<SwitchInst>(userInst)) {
        return true;
      }
      
      // 如果被用于函数调用，认为是有意义的
      if (isa<CallInst>(userInst)) {
        return true;
      }
      
      // 如果被用于返回，认为是有意义的
      if (isa<ReturnInst>(userInst)) {
        return true;
      }
      
      // 如果被存储到全局可见位置，认为是有意义的
      if (auto* store = dyn_cast<StoreInst>(userInst)) {
        if (store->getValueOperand() == load) {
          Value* ptr = store->getPointerOperand();
          if (isGloballyVisible(ptr) || !isa<AllocaInst>(ptr)) {
            return true;
          }
        }
      }
      
      // 如果被用于有副作用的操作，认为是有意义的
      if (userInst->mayHaveSideEffects()) {
        return true;
      }
      
      // 如果被用于非平凡的计算
      if (isa<BinaryOperator>(userInst) || isa<CmpInst>(userInst)) {
        // 递归检查计算结果是否被有意义地使用
        for (auto* computeUser : userInst->users()) {
          if (auto* computeUserInst = dyn_cast<Instruction>(computeUser)) {
            if (isa<BranchInst>(computeUserInst) || isa<CallInst>(computeUserInst) ||
                isa<ReturnInst>(computeUserInst) || computeUserInst->mayHaveSideEffects()) {
              return true;
            }
          }
        }
      }
    }
  }
  
  return false; // 没有找到有意义的使用
}

void
DeadStorageElimination::eliminateAllocaAndUses(AllocaInst* alloca)
{
  // 收集所有使用该alloca的指令
  std::vector<Instruction*> usesToRemove;
  
  for (auto it = alloca->user_begin(); it != alloca->user_end(); ) {
    User* user = *it;
    ++it; // 先递增迭代器
    
    if (auto* inst = dyn_cast<Instruction>(user)) {
      if (isa<LoadInst>(inst) || isa<StoreInst>(inst)) {
        usesToRemove.push_back(inst);
      }
    }
  }
  
  // 删除使用指令
  for (auto* inst : usesToRemove) {
    if (inst->getParent() != nullptr) {
      inst->eraseFromParent();
      stats.totalInstructionsRemoved++;
    }
  }
  
  // 删除alloca本身
  if (alloca->getParent() != nullptr) {
    alloca->eraseFromParent();
    stats.deadAllocasEliminated++;
    stats.totalInstructionsRemoved++;
  }
}

bool
DeadStorageElimination::eliminateDeadStores(Function& func)
{
  bool changed = false;
  std::vector<StoreInst*> stores;
  
  // 收集所有存储指令
  collectStores(func, stores);
  
  // 检查每个存储指令是否有用
  for (auto* store : stores) {
    if (store->getParent() == nullptr) continue; // 已被删除
    
    if (!isStoreUseful(store)) {
      mOut << "  Eliminating dead store: ";
      store->print(mOut);
      mOut << "\n";
      
      store->eraseFromParent();
      stats.deadStoresEliminated++;
      stats.totalInstructionsRemoved++;
      changed = true;
    }
  }
  
  return changed;
}

bool
DeadStorageElimination::isStoreUseful(StoreInst* store)
{
  // 1. Volatile或原子存储总是有用的
  if (isVolatileOrAtomic(store)) {
    return true;
  }
  
  Value* ptr = store->getPointerOperand();
  
  // 2. 存储到全局变量总是有用的
  if (isGloballyVisible(ptr)) {
    return true;
  }
  
  // 3. 如果指针可能逃逸，存储是有用的
  if (escapesThroughCall(ptr, *store->getFunction())) {
    return true;
  }
  
  // 4. 更保守的处理：只分析明确的局部alloca
  auto* alloca = dyn_cast<AllocaInst>(ptr);
  if (!alloca) {
    return true; // 非alloca的存储保守处理为有用
  }
  
  // 5. 检查此存储是否会被后续读取
  // 简化的分析：检查同一基本块内是否有后续的load
  BasicBlock* storeBB = store->getParent();
  auto storeIter = store->getIterator();
  ++storeIter; // 从store的下一条指令开始
  
  for (auto instEnd = storeBB->end(); storeIter != instEnd; ++storeIter) {
    if (auto* load = dyn_cast<LoadInst>(&*storeIter)) {
      if (load->getPointerOperand() == alloca) {
        return true; // 找到后续的load
      }
    } else if (auto* laterStore = dyn_cast<StoreInst>(&*storeIter)) {
      if (laterStore->getPointerOperand() == alloca) {
        // 被后续存储覆盖，但为了安全，仍然保留
        return true;
      }
    }
  }
  
  // 6. 检查其他基本块中的使用（简化版本）
  for (auto* user : alloca->users()) {
    if (auto* load = dyn_cast<LoadInst>(user)) {
      if (load->getParent() != storeBB) {
        // 有其他基本块的load，保守处理为有用
        return true;
      }
    }
  }
  
  // 只有在非常确定的情况下才认为无用
  return false;
}

bool
DeadStorageElimination::mayReachLoad(StoreInst* store, LoadInst* load)
{
  BasicBlock* storeBB = store->getParent();
  BasicBlock* loadBB = load->getParent();
  
  // 如果是同一个基本块，已经在isStoreUseful中处理过了
  if (storeBB == loadBB) {
    return false;
  }
  
  // 简化的可达性分析：检查loadBB是否在storeBB的后继路径上
  std::unordered_set<BasicBlock*> visited;
  std::queue<BasicBlock*> worklist;
  
  // 从storeBB的后继开始搜索
  for (auto* succ : successors(storeBB)) {
    worklist.push(succ);
    visited.insert(succ);
  }
  
  while (!worklist.empty()) {
    BasicBlock* current = worklist.front();
    worklist.pop();
    
    if (current == loadBB) {
      return true; // 找到load的基本块
    }
    
    // 限制搜索深度，避免无限循环
    if (visited.size() > 50) {
      return true; // 保守假设：可能可达
    }
    
    for (auto* succ : successors(current)) {
      if (visited.find(succ) == visited.end()) {
        visited.insert(succ);
        worklist.push(succ);
      }
    }
  }
  
  return false; // load不可达
}

void
DeadStorageElimination::collectAllocas(Function& func, std::vector<AllocaInst*>& allocas)
{
  for (auto& bb : func) {
    for (auto& inst : bb) {
      if (auto* alloca = dyn_cast<AllocaInst>(&inst)) {
        allocas.push_back(alloca);
      }
    }
  }
}

void
DeadStorageElimination::collectStores(Function& func, std::vector<StoreInst*>& stores)
{
  for (auto& bb : func) {
    for (auto& inst : bb) {
      if (auto* store = dyn_cast<StoreInst>(&inst)) {
        stores.push_back(store);
      }
    }
  }
}

bool
DeadStorageElimination::isVolatileOrAtomic(Instruction* inst)
{
  if (auto* load = dyn_cast<LoadInst>(inst)) {
    return load->isVolatile() || load->isAtomic();
  }
  if (auto* store = dyn_cast<StoreInst>(inst)) {
    return store->isVolatile() || store->isAtomic();
  }
  return false;
}

bool
DeadStorageElimination::escapesThroughCall(Value* value, Function& func)
{
  // 更精确但保守的逃逸分析
  for (auto* user : value->users()) {
    if (auto* call = dyn_cast<CallInst>(user)) {
      // 检查是否作为参数传递
      for (unsigned i = 0; i < call->arg_size(); ++i) {
        if (call->getArgOperand(i) == value) {
          return true; // 作为参数传递，可能逃逸
        }
      }
    } else if (auto* ret = dyn_cast<ReturnInst>(user)) {
      if (ret->getReturnValue() == value) {
        return true; // 作为返回值，逃逸
      }
    } else if (auto* store = dyn_cast<StoreInst>(user)) {
      // 如果value被存储到其他地方（不是作为指针），可能逃逸
      if (store->getValueOperand() == value) {
        Value* dest = store->getPointerOperand();
        // 如果存储到非局部变量，认为逃逸
        if (!isa<AllocaInst>(dest)) {
          return true;
        }
      }
    } else if (auto* gep = dyn_cast<GetElementPtrInst>(user)) {
      // 递归检查GEP
      if (escapesThroughCall(gep, func)) {
        return true;
      }
    } else if (auto* bitcast = dyn_cast<BitCastInst>(user)) {
      // 递归检查类型转换
      if (escapesThroughCall(bitcast, func)) {
        return true;
      }
    }
  }
  return false;
}