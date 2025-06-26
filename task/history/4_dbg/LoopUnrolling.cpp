#include "LoopUnrolling.hpp"
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/CFG.h>
#include <queue>
#include <algorithm>

using namespace llvm;

PreservedAnalyses
LoopUnrolling::run(Module& mod, ModuleAnalysisManager& mam)
{
  int functionsProcessed = 0;
  int totalUnrolledLoops = 0;
  bool changed = false;

  mOut << "------------------------------------------------\n";
  mOut << "      Enhanced Loop Unrolling Analysis\n";
  mOut << "------------------------------------------------\n";

  // 遍历所有函数
  for (auto& func : mod) {
    if (func.isDeclaration())
      continue;
    

    
    int unrolledInThisFunc = 0;
    if (unrollLoopsInFunction(func)) {
      changed = true;
      functionsProcessed++;
      unrolledInThisFunc = 1; // 简化计数
    }
    
    totalUnrolledLoops += unrolledInThisFunc;
  }
  return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

bool
LoopUnrolling::unrollLoopsInFunction(Function& func)
{
  bool changed = false;
  std::vector<BasicBlock*> blocks;
  
  // 收集所有基本块
  for (auto& bb : func) {
    blocks.push_back(&bb);
  }
  
  // 查找并展开循环
  for (auto* bb : blocks) {
    if (bb->getParent() == nullptr) continue; // 已被删除
    
    EnhancedLoop loop;
    if (idLoop(bb, loop)) {
      
      if (shouldUnrollLoop(loop)) {
        unsigned unrollFactor = computeUnrollFactor(loop);
        
        if (unrollFactor == loop.tripCount && loop.tripCount > 0) {
          // 完全展开
          if (performFullUnroll(loop)) {
            changed = true;
            break; // 一次只处理一个循环
          }
        } else if (unrollFactor > 1) {
          // 部分展开
          if (performPartialUnroll(loop, unrollFactor)) {
            changed = true;
            break;
          }
        }
      } else {
      }
    }
  }
  
  return changed;
}

bool
LoopUnrolling::idLoop(BasicBlock* bb, EnhancedLoop& loop)
{
  // 尝试不同的循环模式识别
  if (identifyCountedLoop(bb, loop)) {
    return true;
  }
  
  if (identifySimpleForLoop(bb, loop)) {
    return true;
  }
  
  if (identifySimpleWhileLoop(bb, loop)) {
    return true;
  }
  
  return false;
}

bool
LoopUnrolling::identifyCountedLoop(BasicBlock* bb, EnhancedLoop& loop)
{
  // 识别有确定次数的循环
  
  // 检查基本条件：有PHI节点和条件分支
  if (bb->empty() || !isa<PHINode>(bb->front())) {
    return false;
  }
  
  auto* br = dyn_cast<BranchInst>(bb->getTerminator());
  if (!br || !br->isConditional()) {
    return false;
  }
  
  auto* cmp = dyn_cast<CmpInst>(br->getCondition());
  if (!cmp) {
    return false;
  }
  
  loop.header = bb;
  
  // 查找归纳变量
  if (!findInductionVariable(loop)) {
    return false;
  }
  
  // 分析循环结构
  if (!analyzeLoopStructure(loop)) {
    return false;
  }
  
  // 计算循环次数
  loop.tripCount = calculateTripCount(loop);
  if (loop.tripCount <= 0 || loop.tripCount > (int)mMaxUnrollCount) {
    return false;
  }
  
  // 估计循环大小
  loop.estimatedSize = estimateLoopSize(loop);
  
  return true;
}

bool
LoopUnrolling::identifySimpleForLoop(BasicBlock* bb, EnhancedLoop& loop)
{
  auto* phi = dyn_cast_or_null<PHINode>(&bb->front());
  if (!phi || phi->getNumIncomingValues() != 2) {
    return false;
  }
  
  loop.header = bb;
  loop.inductionVar = phi;
  
  // 查找初始值和更新值
  BasicBlock* preheaderCandidate = nullptr;
  BasicBlock* latchCandidate = nullptr;
  
  for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
    BasicBlock* incomingBB = phi->getIncomingBlock(i);
    
    // 检查是否是回边
    bool isBackEdge = false;
    for (auto* succ : successors(incomingBB)) {
      if (succ == bb) {
        // 检查是否真的是循环内的块
        if (incomingBB == bb || 
            std::find(succ_begin(bb), succ_end(bb), incomingBB) != succ_end(bb)) {
          isBackEdge = true;
          break;
        }
      }
    }
    
    if (isBackEdge) {
      latchCandidate = incomingBB;
    } else {
      preheaderCandidate = incomingBB;
      loop.initValue = phi->getIncomingValue(i);
    }
  }
  
  if (!preheaderCandidate || !latchCandidate) {
    return false;
  }
  
  loop.preheader = preheaderCandidate;
  loop.latch = latchCandidate;
  
  // 查找步长更新
  Value* updateValue = phi->getIncomingValueForBlock(latchCandidate);
  if (auto* addInst = dyn_cast<BinaryOperator>(updateValue)) {
    if (addInst->getOpcode() == Instruction::Add) {
      if (addInst->getOperand(0) == phi) {
        loop.stepValue = addInst->getOperand(1);
      } else if (addInst->getOperand(1) == phi) {
        loop.stepValue = addInst->getOperand(0);
      } else {
        return false;
      }
    } else {
      return false;
    }
  } else {
    return false;
  }
  
  // 分析循环条件
  auto* br = dyn_cast<BranchInst>(bb->getTerminator());
  if (!br || !br->isConditional()) {
    return false;
  }
  
  auto* cmp = dyn_cast<CmpInst>(br->getCondition());
  if (!cmp) {
    return false;
  }
  
  // 确定比较的操作数和谓词
  if (cmp->getOperand(0) == phi) {
    loop.boundValue = cmp->getOperand(1);
    loop.pred = cmp->getPredicate();
  } else if (cmp->getOperand(1) == phi) {
    loop.boundValue = cmp->getOperand(0);
    loop.pred = cmp->getSwappedPredicate();
  } else {
    return false;
  }
  
  // 确定循环体和出口
  BasicBlock* trueBB = br->getSuccessor(0);
  BasicBlock* falseBB = br->getSuccessor(1);
  
  // 根据比较结果确定哪个是循环体，哪个是出口
  // 简化：假设true分支继续循环
  if (trueBB == latchCandidate || trueBB == bb) {
    loop.bodyBlocks.push_back(trueBB);
    loop.exitBlocks.push_back(falseBB);
  } else {
    loop.bodyBlocks.push_back(falseBB);
    loop.exitBlocks.push_back(trueBB);
    // 需要反转谓词
    loop.pred = CmpInst::getInversePredicate(loop.pred);
  }
  
  loop.isSimpleLoop = (loop.bodyBlocks.size() == 1);
  return true;
}

bool
LoopUnrolling::identifySimpleWhileLoop(BasicBlock* bb, EnhancedLoop& loop)
{
  // 识别简单的while循环
  // 类似于for循环，但结构可能稍有不同
  return identifySimpleForLoop(bb, loop); // 简化实现
}

bool
LoopUnrolling::findInductionVariable(EnhancedLoop& loop)
{
  if (!loop.header) return false;
  
  // 查找归纳变量PHI节点
  for (auto& inst : *loop.header) {
    if (auto* phi = dyn_cast<PHINode>(&inst)) {
      if (phi->getNumIncomingValues() == 2) {
        // 检查是否有简单的递增/递减模式
        for (auto* user : phi->users()) {
          if (auto* binOp = dyn_cast<BinaryOperator>(user)) {
            if (binOp->getOpcode() == Instruction::Add ||
                binOp->getOpcode() == Instruction::Sub) {
              // 检查是否其中一个操作数是常量
              if (isa<ConstantInt>(binOp->getOperand(1)) ||
                  isa<ConstantInt>(binOp->getOperand(0))) {
                loop.inductionVar = phi;
                return true;
              }
            }
          }
        }
      }
    } else {
      break; // PHI节点都在开头
    }
  }
  
  return false;
}

int
LoopUnrolling::calculateTripCount(const EnhancedLoop& loop)
{
  int initVal, stepVal, boundVal;
  
  if (!isConstantInt(loop.initValue, initVal) ||
      !isConstantInt(loop.stepValue, stepVal) ||
      !isConstantInt(loop.boundValue, boundVal)) {
    return -1;
  }
  
  if (stepVal == 0) return -1;
  
  int tripCount = 0;
  
  switch (loop.pred) {
    case CmpInst::ICMP_SLT:
      if (stepVal > 0 && initVal < boundVal) {
        tripCount = (boundVal - initVal + stepVal - 1) / stepVal;
      }
      break;
    case CmpInst::ICMP_SLE:
      if (stepVal > 0 && initVal <= boundVal) {
        tripCount = (boundVal - initVal) / stepVal + 1;
      }
      break;
    case CmpInst::ICMP_SGT:
      if (stepVal < 0 && initVal > boundVal) {
        tripCount = (initVal - boundVal + (-stepVal) - 1) / (-stepVal);
      }
      break;
    case CmpInst::ICMP_SGE:
      if (stepVal < 0 && initVal >= boundVal) {
        tripCount = (initVal - boundVal) / (-stepVal) + 1;
      }
      break;
    case CmpInst::ICMP_ULT:
      if (stepVal > 0 && (unsigned)initVal < (unsigned)boundVal) {
        tripCount = ((unsigned)boundVal - (unsigned)initVal + stepVal - 1) / stepVal;
      }
      break;
    default:
      return -1;
  }
  
  return (tripCount > 0 && tripCount <= (int)mMaxUnrollCount) ? tripCount : -1;
}

unsigned int
LoopUnrolling::estimateLoopSize(const EnhancedLoop& loop)
{
  unsigned int loopsize = 0;
  
  // 估计循环头大小
  for (auto& inst : *loop.header) {
    if (!isa<PHINode>(&inst) && !inst.isTerminator()) loopsize++;
  }
  
  // 估计循环体大小
  for (auto* bb : loop.bodyBlocks) {
    for (auto& inst : *bb) 
      if (!inst.isTerminator()) loopsize++;
  }
  // 估计latch块大小
  if (loop.latch && loop.latch != loop.header) {
    for (auto& inst : *loop.latch) if (!inst.isTerminator())loopsize++;
  }
  
  return loopsize;
}

bool
LoopUnrolling::shouldUnrollLoop(const EnhancedLoop& loop)
{
  // 展开决策条件
  
  // 1. 必须有确定的循环次数
  if (loop.tripCount <= 0) {
    return false;
  }
  
  // 2. 循环次数不能太大
  if (loop.tripCount > (int)mMaxUnrollCount) {
    return false;
  }
  
  // 3. 展开后的代码大小不能过大
  unsigned expandedSize = loop.estimatedSize * loop.tripCount;
  if (expandedSize > mCodeSizeThreshold) {
    return false;
  }
  
  // 4. 必须是简单循环（单一循环体）
  if (!loop.isSimpleLoop) {
    return false;
  }
  
  // 5. 小循环更适合展开
  if (loop.tripCount <= 4 && loop.estimatedSize <= 8) {
    return true;
  }
  
  // 6. 对于较大的循环，需要更严格的条件
  if (loop.tripCount <= 2 && loop.estimatedSize <= 16) {
    return true;
  }
  
  return false;
}

unsigned
LoopUnrolling::computeUnrollFactor(const EnhancedLoop& loop)
{
  // 计算展开因子
  
  if (loop.tripCount <= 0) {
    return 1;
  }
  
  // 对于小循环，完全展开
  if (loop.tripCount <= 4 && loop.estimatedSize <= 8) {
    return loop.tripCount;
  }
  
  // 对于中等循环，部分展开
  unsigned maxFactor = mCodeSizeThreshold / (loop.estimatedSize > 0 ? loop.estimatedSize : 1);
  unsigned factor = std::min(maxFactor, (unsigned)loop.tripCount);
  factor = std::min(factor, mMaxUnrollCount);
  
  // 选择合理的展开因子（2的幂或小质数）
  if (factor >= 4) return 4;
  if (factor >= 2) return 2;
  
  return 1;
}

bool
LoopUnrolling::performFullUnroll(const EnhancedLoop& loop)
{
  // 完全展开循环
  
  if (loop.tripCount <= 0 || !loop.isSimpleLoop) {
    return false;
  }
  
  // 简化实现：只处理最简单的单块循环
  if (loop.bodyBlocks.size() != 1 || loop.bodyBlocks[0] != loop.header) {
    return false;
  }
  
  BasicBlock* loopBlock = loop.header;
  IRBuilder<> builder(loopBlock->getContext());
  
  // 创建展开的指令序列
  std::vector<Value*> inductionValues;
  
  // 计算每次迭代的归纳变量值
  int initVal, stepVal;
  if (!isConstantInt(loop.initValue, initVal) || !isConstantInt(loop.stepValue, stepVal)) {
    return false;
  }
  
  for (int i = 0; i < loop.tripCount; ++i) {
    int currentVal = initVal + i * stepVal;
    inductionValues.push_back(ConstantInt::get(loop.inductionVar->getType(), currentVal));
  }
  
  // 在循环前插入展开的代码
  builder.SetInsertPoint(loopBlock->getTerminator());
  
  // 收集需要展开的指令（除了PHI、比较和分支）
  std::vector<Instruction*> toUnroll;
  for (auto& inst : *loopBlock) {
    if (!isa<PHINode>(&inst) && !isa<CmpInst>(&inst) && !inst.isTerminator()) {
      toUnroll.push_back(&inst);
    }
  }
  
  // 为每次迭代创建指令副本
  for (int iter = 0; iter < loop.tripCount; ++iter) {
    std::unordered_map<Value*, Value*> valueMap;
    valueMap[loop.inductionVar] = inductionValues[iter];
    
    for (auto* inst : toUnroll) {
      Instruction* clonedInst = inst->clone();
      
      // 更新操作数
      for (unsigned i = 0; i < clonedInst->getNumOperands(); ++i) {
        Value* operand = clonedInst->getOperand(i);
        if (valueMap.find(operand) != valueMap.end()) {
          clonedInst->setOperand(i, valueMap[operand]);
        }
      }
      
      clonedInst->insertBefore(loopBlock->getTerminator());
      valueMap[inst] = clonedInst;
    }
  }
  
  // 修改控制流：直接跳转到循环出口
  if (!loop.exitBlocks.empty()) {
    builder.SetInsertPoint(loopBlock->getTerminator());
    builder.CreateBr(loop.exitBlocks[0]);
    loopBlock->getTerminator()->eraseFromParent();
  }
  
  // 清理原始循环指令
  loop.inductionVar->replaceAllUsesWith(loop.initValue);
  
  return true;
}

bool
LoopUnrolling::performPartialUnroll(const EnhancedLoop& loop, unsigned factor)
{
  // 部分展开循环 - 简化实现
  // 这里只返回false，表示暂不支持部分展开
  return false;
}

bool
LoopUnrolling::analyzeLoopStructure(EnhancedLoop& loop)
{
  // 分析循环结构的完整性
  return loop.header && loop.inductionVar && loop.initValue && 
         loop.stepValue && loop.boundValue;
}

bool
LoopUnrolling::isConstantInt(Value* val, int& result)
{
  if (auto* ci = dyn_cast<ConstantInt>(val)) {
    result = ci->getSExtValue();
    return true;
  }
  return false;
}
