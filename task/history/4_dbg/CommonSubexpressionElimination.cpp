#include "CommonSubexpressionElimination.hpp"
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <vector>

using namespace llvm;

PreservedAnalyses
CommonSubexpressionElimination::run(Module& mod, ModuleAnalysisManager& mam)
{
  int functionsProcessed = 0;
  int totalEliminatedInsts = 0;
  bool changed = false;

  // 遍历所有函数
  for (auto& func : mod) {
    if (func.isDeclaration())
      continue;
    
    // 为每个函数重新初始化GVN
    initializeGVN();
    
    int beforeCount = 0;
    int afterCount = 0;
    
    // 计算优化前的指令数量
    for (auto& bb : func) {
      for (auto& inst : bb) {
        beforeCount++;
      }
    }
    
    if (EliminateCSEInFunction(func)) {
      changed = true;
      functionsProcessed++;
      
      // 计算优化后的指令数量
      for (auto& bb : func) {
        for (auto& inst : bb) {
          afterCount++;
        }
      }
      
      totalEliminatedInsts += (beforeCount - afterCount);
    }
  }

  mOut << "CommonSubexpressionElimination running...\n";
  mOut << "Processed " << functionsProcessed << " functions\n";
  mOut << "Eliminated " << totalEliminatedInsts << " redundant instructions\n";

  return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

void
CommonSubexpressionElimination::initializeGVN()
{
  mNextValueNumber = 1;
  mValueNumbers.clear();
  mExpressionToVN.clear();
  mVNToLeader.clear();
}

bool
CommonSubexpressionElimination::EliminateCSEInBasicBlock(BasicBlock& bb)
{
  bool changed = false;
  std::vector<Instruction*> toErase;
  
  // 第一遍：为所有指令分配值编号
  for (auto& inst : bb) {
    if (inst.isTerminator()) {
      continue;
    }
    assignValueNumber(&inst);
  }
  
  // 第二遍：查找和消除冗余表达式
  for (auto& inst : bb) {
    if (inst.isTerminator()) {
      continue;
    }
    
    if (!isCSECandidate(&inst)) {
      continue;
    }
    
    uint32_t vn = mValueNumbers[&inst];
    auto leaderIt = mVNToLeader.find(vn);
    
    if (leaderIt != mVNToLeader.end() && leaderIt->second != &inst) {
      Value* leader = leaderIt->second;
      
      // 确保leader仍然有效且在同一个基本块中
      if (auto* leaderInst = dyn_cast<Instruction>(leader)) {
        if (leaderInst->getParent() == &bb && leaderInst->getParent() != nullptr) {
          // 检查是否有干扰
          if (!hasInterfering(leaderInst, &inst)) {
            // 执行公共子表达式消除
            inst.replaceAllUsesWith(leader);
            toErase.push_back(&inst);
            changed = true;
          }
        } else {
          // 更新leader为当前指令
          updateLeader(vn, &inst);
        }
      }
    }
  }
  
  // 删除冗余指令
  for (auto* inst : toErase) {
    inst->eraseFromParent();
  }
  
  return changed;
}

bool
CommonSubexpressionElimination::EliminateCSEInFunction(Function& func)
{
  bool changed = false;
  
  // 对每个基本块进行CSE优化
  for (auto& bb : func) {
    mOut << "  Processing basic block with " << bb.size() << " instructions\n";
    if (EliminateCSEInBasicBlock(bb)) {
      changed = true;
    }
  }
  
  return changed;
}

bool
CommonSubexpressionElimination::areInstructionsEquivalent(Instruction* inst1, Instruction* inst2)
{
  // 操作码必须相同
  if (inst1->getOpcode() != inst2->getOpcode()) {
    return false;
  }
  
  // 操作数数量必须相同
  if (inst1->getNumOperands() != inst2->getNumOperands()) {
    return false;
  }
  
  // 类型必须相同
  if (inst1->getType() != inst2->getType()) {
    return false;
  }
  
  // 检查所有操作数是否相同
  for (unsigned i = 0; i < inst1->getNumOperands(); ++i) {
    if (inst1->getOperand(i) != inst2->getOperand(i)) {
      return false;
    }
  }
  
  // 对于特定类型的指令，检查额外属性
  if (auto* cmpInst1 = dyn_cast<CmpInst>(inst1)) {
    auto* cmpInst2 = dyn_cast<CmpInst>(inst2);
    if (cmpInst1->getPredicate() != cmpInst2->getPredicate()) {
      return false;
    }
  }
  
  if (auto* castInst1 = dyn_cast<CastInst>(inst1)) {
    auto* castInst2 = dyn_cast<CastInst>(inst2);
    // Cast指令的源类型和目标类型都必须相同
    if (castInst1->getSrcTy() != castInst2->getSrcTy() ||
        castInst1->getDestTy() != castInst2->getDestTy()) {
      return false;
    }
  }
  
  return true;
}

bool
CommonSubexpressionElimination::isCSECandidate(Instruction* inst)
{
  // 有副作用的指令不能作为CSE候选
  if (hasSideEffects(inst)) {
    return false;
  }
  
  // Terminator指令不能作为CSE候选
  if (inst->isTerminator()) {
    return false;
  }
  
  // PHI节点通常不进行CSE（因为它们依赖于控制流）
  if (isa<PHINode>(inst)) {
    return false;
  }
  
  // Load指令需要特殊处理，暂时不作为候选
  if (isa<LoadInst>(inst)) {
    return false;
  }
  return isa<BinaryOperator>(inst) || 
         isa<CmpInst>(inst) || 
         isa<CastInst>(inst) ||
         isa<GetElementPtrInst>(inst) ||
         isa<SelectInst>(inst);
}
uint32_t
CommonSubexpressionElimination::getValueNumber(llvm::Value* value)
{
  auto it = mValueNumbers.find(value);
  if (it != mValueNumbers.end()) {
    return it->second;
  }
  
  // 为新值分配编号
  uint32_t vn = mNextValueNumber++;
  mValueNumbers[value] = vn;
  
  // 如果是常量或参数，直接设为leader
  if (isa<Constant>(value) || isa<Argument>(value)) {
    mVNToLeader[vn] = value;
  }
  
  return vn;
}

std::string
CommonSubexpressionElimination::getExpressionKey(llvm::Instruction* inst)
{
  std::string key;
  
  // 添加操作码
  key += std::to_string(inst->getOpcode()) + ":";
  
  // 添加操作数的值编号
  for (unsigned i = 0; i < inst->getNumOperands(); ++i) {
    uint32_t vn = getValueNumber(inst->getOperand(i));
    key += std::to_string(vn) + ",";
  }
  
  // 为特定指令类型添加额外信息
  if (auto* cmpInst = dyn_cast<CmpInst>(inst)) {
    key += "pred:" + std::to_string(cmpInst->getPredicate());
  } else if (auto* castInst = dyn_cast<CastInst>(inst)) {
    key += "src:" + std::to_string(castInst->getSrcTy()->getTypeID()) +
           ",dst:" + std::to_string(castInst->getDestTy()->getTypeID());
  }
  
  return key;
}

uint32_t
CommonSubexpressionElimination::assignValueNumber(llvm::Instruction* inst)
{
  // 检查是否为CSE候选
  if (!isCSECandidate(inst)) {
    uint32_t vn = mNextValueNumber++;
    mValueNumbers[inst] = vn;
    mVNToLeader[vn] = inst;
    return vn;
  }
  
  // 生成表达式键
  std::string exprKey = getExpressionKey(inst);
  
  // 检查是否已存在相同的表达式
  auto it = mExpressionToVN.find(exprKey);
  if (it != mExpressionToVN.end()) {
    uint32_t existingVN = it->second;
    mValueNumbers[inst] = existingVN;
    
    // 检查当前leader是否仍然有效
    auto leaderIt = mVNToLeader.find(existingVN);
    if (leaderIt != mVNToLeader.end()) {
      Value* currentLeader = leaderIt->second;
      if (auto* leaderInst = dyn_cast<Instruction>(currentLeader)) {
        // 如果当前leader已被删除或在不同的基本块中，更新leader
        if (leaderInst->getParent() == nullptr || 
            leaderInst->getParent() != inst->getParent()) {
          updateLeader(existingVN, inst);
        }
      }
    } else {
      updateLeader(existingVN, inst);
    }
    
    return existingVN;
  }
  
  // 创建新的值编号
  uint32_t newVN = mNextValueNumber++;
  mValueNumbers[inst] = newVN;
  mExpressionToVN[exprKey] = newVN;
  mVNToLeader[newVN] = inst;
  
  return newVN;
}

void
CommonSubexpressionElimination::updateLeader(uint32_t vn, llvm::Value* value)
{
  mVNToLeader[vn] = value;
}

bool
CommonSubexpressionElimination::hasSideEffects(Instruction* inst)
{
  // Store指令有副作用
  if (isa<StoreInst>(inst)) {
    return true;
  }
  
  // 函数调用可能有副作用
  if (auto* callInst = dyn_cast<CallInst>(inst)) {
    Function* calledFunc = callInst->getCalledFunction();
    if (!calledFunc) {
      return true; // 间接调用
    }
    
    StringRef funcName = calledFunc->getName();
    
    // 一些已知的无副作用函数
    if (funcName.starts_with("llvm.dbg.") ||
        funcName.starts_with("llvm.lifetime.") ||
        funcName.starts_with("llvm.sin") ||
        funcName.starts_with("llvm.cos") ||
        funcName.starts_with("llvm.sqrt") ||
        funcName.starts_with("llvm.fabs")) {
      return false;
    }
    
    // 检查函数属性
    if (calledFunc->hasFnAttribute(Attribute::ReadNone)) {
      return false;
    }
    
    return true; // 默认认为有副作用
  }
  
  // Volatile load有副作用
  if (auto* loadInst = dyn_cast<LoadInst>(inst)) {
    return loadInst->isVolatile();
  }
  
  // 原子操作有副作用
  if (isa<AtomicRMWInst>(inst) || isa<AtomicCmpXchgInst>(inst)) {
    return true;
  }
  
  // fence指令有副作用
  if (isa<FenceInst>(inst)) {
    return true;
  }
  
  return false;
}

bool
CommonSubexpressionElimination::hasInterfering(Instruction* first, Instruction* second)
{
  // 简化的干扰检查：在两个指令之间是否有store指令
  
  BasicBlock* bb = first->getParent();
  bool foundFirst = false;
  
  for (auto& inst : *bb) {
    if (&inst == first) {
      foundFirst = true;
      continue;
    }
    
    if (&inst == second) {
      break;
    }
    
    if (foundFirst) {
      // 检查是否有可能影响结果的指令
      if (isa<StoreInst>(&inst)) return true;
      if (isa<CallInst>(&inst)) 
        // 函数调用可能有副作用
        return true;
    }
  }
  
  return false;
}
