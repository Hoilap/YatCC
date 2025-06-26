#include "LoopInvariantCodeMotion.hpp"
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/IRBuilder.h>
#include <queue>
#include <algorithm>

using namespace llvm;

PreservedAnalyses
LoopInvariantCodeMotion::run(Module& mod, ModuleAnalysisManager& mam)
{
  int functionsProcessed = 0;
  int totalHoistedInsts = 0;
  bool changed = false;

  mOut << "---------------------------------------------\n";
  mOut << "    Loop Invariant Code Motion Analysis\n";
  mOut << "---------------------------------------------\n";

  // 遍历所有函数
  for (auto& func : mod) {
    if (func.isDeclaration())
      continue;
    
    int hoistedInThisFunc = 0;
    if (processFunction(func)) {
      changed = true;
      functionsProcessed++;
      hoistedInThisFunc = 1; // 简化计数
    }
    totalHoistedInsts += hoistedInThisFunc;
  }

  return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

bool
LoopInvariantCodeMotion::processFunction(Function& func)
{
  bool changed = false;
  invariantInstructions.clear();
  
  // 简化的循环查找：只查找最简单的自循环
  if (findAndProcessSimpleLoops(func)) {
    changed = true;
  }
  
  return changed;
}

bool
LoopInvariantCodeMotion::findAndProcessSimpleLoops(Function& func)
{
  bool changed = false;
  
  // 扩展循环识别：不仅查找自循环，还查找简单的while循环
  for (auto& bb : func) {
    bool isLoopHeader = false;
    
    // 检查是否有指向自己的边（自循环）
    for (auto* succ : successors(&bb)) {
      if (succ == &bb) {
        isLoopHeader = true;
        break;
      }
    }
    
    // 检查是否是while循环头：有回边从后面的块指向自己
    if (!isLoopHeader) {
      for (auto* pred : predecessors(&bb)) {
        // 简单检查：如果前驱在CFG中位置靠后，可能是回边
        bool isPotentialBackEdge = false;
        for (auto* succ : successors(&bb)) {
          if (succ == pred) {
            isPotentialBackEdge = true;
            break;
          }
        }
        if (isPotentialBackEdge) {
          isLoopHeader = true;
          break;
        }
      }
    }
    
    if (isLoopHeader) {
      SimpleLoop loop;
      if (identifySimpleLoop(&bb, loop)) {
        if (hoistInvariantInstructions(loop)) {
          changed = true;
        }
      }
    }
  }
  
  return changed;
}

bool
LoopInvariantCodeMotion::identifySimpleLoop(BasicBlock* header, SimpleLoop& loop)
{
  loop.header = header;
  loop.blocks.insert(header);
  
  // 简化：对于自循环，循环体就是header本身
  loop.preheader = findSimplePreheader(header);
  
  // 查找循环出口
  for (auto* succ : successors(header)) {
    if (succ != header) {
      loop.exits.insert(succ);
    }
  }
  
  return loop.preheader != nullptr;
}
// 简化的辅助函数实现
bool
LoopInvariantCodeMotion::findAndProcessLoops(Function& func)
{
  // 使用简化版本
  return findAndProcessSimpleLoops(func);
}

bool
LoopInvariantCodeMotion::identifyLoop(BasicBlock* header, SimpleLoop& loop)
{
  // 使用简化版本
  return identifySimpleLoop(header, loop);
}

bool
LoopInvariantCodeMotion::isLoopInvariant(Instruction* inst, const SimpleLoop& loop)
{
  // 使用简化版本
  return isLoopInvariantSimple(inst, loop);
}

bool
LoopInvariantCodeMotion::isOperandLoopInvariant(Value* operand, const SimpleLoop& loop)
{
  // 常量和参数总是不变的
  if (isa<Constant>(operand) || isa<Argument>(operand)) {
    return true;
  }
  
  // 如果是指令，检查是否在循环外定义
  if (auto* operandInst = dyn_cast<Instruction>(operand)) {
    return loop.blocks.find(operandInst->getParent()) == loop.blocks.end();
  }
  
  return false;
}

bool
LoopInvariantCodeMotion::isSafeToHoist(Instruction* inst, const SimpleLoop& loop)
{
  // 使用简化版本
  return isSafeToHoistSimple(inst);
}

bool
LoopInvariantCodeMotion::hasNoSideEffects(Instruction* inst)
{
  return isSafeToHoistSimple(inst);
}

bool
LoopInvariantCodeMotion::dominatesAllExits(Instruction* inst, const SimpleLoop& loop)
{
  // 简化：对于自循环总是返回true
  return true;
}

BasicBlock*
LoopInvariantCodeMotion::findOrCreatePreheader(const SimpleLoop& loop)
{
  // 使用简化版本
  return findSimplePreheader(loop.header);
}

bool
LoopInvariantCodeMotion::isDefinedInLoop(Value* value, const SimpleLoop& loop)
{
  if (auto* inst = dyn_cast<Instruction>(value)) {
    return loop.blocks.count(inst->getParent()) > 0;
  }
  return false;
}

BasicBlock*
LoopInvariantCodeMotion::findSimplePreheader(BasicBlock* header)
{
  BasicBlock* preheader = nullptr;
  int nonLoopPreds = 0;
  
  // 查找非循环的前驱（即preheader候选）
  for (auto* pred : predecessors(header)) {
    if (pred != header) { // 不是自循环边
      preheader = pred;
      nonLoopPreds++;
    }
  }
  
  // 只有一个非循环前驱才是有效的preheader
  return (nonLoopPreds == 1) ? preheader : nullptr;
}

bool
LoopInvariantCodeMotion::hoistInvariantInstructions(const SimpleLoop& loop)
{
  bool changed = false;
  std::vector<Instruction*> toHoist;
  
  // 只分析循环头中的指令
  for (auto& inst : *loop.header) {
    // 跳过PHI节点、终结指令和已处理的指令
    if (isa<PHINode>(&inst) || inst.isTerminator()) {
      continue;
    }
    
    // 检查是否是循环不变
    if (isLoopInvariantSimple(&inst, loop) && isSafeToHoistSimple(&inst)) toHoist.push_back(&inst);

  }
  
  // 执行代码提升
  for (auto* inst : toHoist) {
    hoistInstruction(inst, loop.preheader);
    changed = true;
  }
  
  if (!toHoist.empty()) {
    mOut << "  Hoisted " << toHoist.size() << " instructions to preheader: " 
         << loop.preheader->getName() << "\n";
  }
  
  return changed;
}

bool
LoopInvariantCodeMotion::isLoopInvariantSimple(Instruction* inst, const SimpleLoop& loop)
{
  // 更保守的不变性检查
  
  // 只处理简单的算术运算
  if (!isa<BinaryOperator>(inst) && !isa<CastInst>(inst)) {
    return false;
  }
  
  // 检查所有操作数是否都是循环不变的
  for (unsigned i = 0; i < inst->getNumOperands(); ++i) {
    Value* operand = inst->getOperand(i);
    
    // 常量和参数总是不变的
    if (isa<Constant>(operand) || isa<Argument>(operand)) {
      continue;
    }
    
    // 如果操作数是指令，检查是否在循环外定义
    if (auto* operandInst = dyn_cast<Instruction>(operand)) {
      if (operandInst->getParent() == loop.header) {
        // 操作数在循环内定义，不是不变的
        return false;
      }
    }
  }
  
  return true;
}

bool
LoopInvariantCodeMotion::isSafeToHoistSimple(Instruction* inst)
{
  // 简化的安全性检查
  
  // Store指令不能提升
  if (isa<StoreInst>(inst)) {
    return false;
  }
  
  // 函数调用不能提升（可能有副作用）
  if (isa<CallInst>(inst)) {
    return false;
  }
  
  // Volatile load不能提升
  if (auto* loadInst = dyn_cast<LoadInst>(inst)) {
    return !loadInst->isVolatile();
  }
  
  // 原子操作不能提升
  if (isa<AtomicRMWInst>(inst) || isa<AtomicCmpXchgInst>(inst)) {
    return false;
  }
  
  // 可能抛出异常的指令需要谨慎处理
  if (inst->mayThrow()) {
    return false;
  }
  
  return true;
}

void
LoopInvariantCodeMotion::hoistInstruction(Instruction* inst, BasicBlock* preheader)
{
  // 将指令移动到preheader的末尾（终结指令之前）
  inst->removeFromParent();
  
  if (preheader->getTerminator()) {
    inst->insertBefore(preheader->getTerminator());
  } else {
    inst->insertInto(preheader, preheader->end());
  }
}

