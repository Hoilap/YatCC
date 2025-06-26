#include "InstructionCombining.hpp"
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <vector>

using namespace llvm;

Instruction*
InstructionCombining::combineBinaryOperator(BinaryOperator* binOp)
{
  switch (binOp->getOpcode()) {
    case Instruction::Add:
      return combineAdd(binOp);
    case Instruction::Sub:
      return combineSub(binOp);
    case Instruction::Mul:
      return combineMul(binOp);
    case Instruction::UDiv:
    case Instruction::SDiv:
      return combineDiv(binOp);
    case Instruction::Shl:
    case Instruction::LShr:
    case Instruction::AShr:
      return combineShift(binOp);
    case Instruction::And:
    case Instruction::Or:
    case Instruction::Xor:
      return combineBitwise(binOp);
    default:
      return nullptr;
  }
}

Instruction*
InstructionCombining::combineAdd(BinaryOperator* add)
{
  Value* lhs = add->getOperand(0);
  Value* rhs = add->getOperand(1);
  
  // 情况1: (a + C1) + C2 => a + (C1 + C2)
  if (auto* rhsConst = dyn_cast<ConstantInt>(rhs)) {
    if (auto* lhsAdd = dyn_cast<BinaryOperator>(lhs)) {
      if (lhsAdd->getOpcode() == Instruction::Add && hasOneUse(lhs)) {
        if (auto* lhsRhsConst = dyn_cast<ConstantInt>(lhsAdd->getOperand(1))) {
          // 检查类型匹配
          if (rhsConst->getType() != lhsRhsConst->getType()) {
            return nullptr;
          }
          
          // 计算 C1 + C2
          APInt newConst = lhsRhsConst->getValue() + rhsConst->getValue();
          Constant* combinedConst = ConstantInt::get(add->getType(), newConst);
          
          // 创建新的加法指令: a + (C1 + C2)
          return createBinaryOperator(Instruction::Add, 
                                    lhsAdd->getOperand(0), 
                                    combinedConst, 
                                    add);
        }
      }
    }
  }
  
  // 情况2: (a - C1) + C2 => a + (C2 - C1)
  if (auto* rhsConst = dyn_cast<ConstantInt>(rhs)) {
    if (auto* lhsSub = dyn_cast<BinaryOperator>(lhs)) {
      if (lhsSub->getOpcode() == Instruction::Sub && hasOneUse(lhs)) {
        if (auto* lhsRhsConst = dyn_cast<ConstantInt>(lhsSub->getOperand(1))) {
          // 计算 C2 - C1
          APInt newConst = rhsConst->getValue() - lhsRhsConst->getValue();
          Constant* combinedConst = ConstantInt::get(add->getType(), newConst);
          
          // 如果结果为0，直接返回a
          if (newConst.isZero()) {
            return cast<Instruction>(lhsSub->getOperand(0));
          }
          
          // 创建新的加法指令: a + (C2 - C1)
          return createBinaryOperator(Instruction::Add, 
                                    lhsSub->getOperand(0), 
                                    combinedConst, 
                                    add);
        }
      }
    }
  }
  
  return nullptr;
}

Instruction*
InstructionCombining::combineSub(BinaryOperator* sub)
{
  Value* lhs = sub->getOperand(0);
  Value* rhs = sub->getOperand(1);
  
  // 情况1: (a + C1) - C2 => a + (C1 - C2)
  if (auto* rhsConst = dyn_cast<ConstantInt>(rhs)) {
    if (auto* lhsAdd = dyn_cast<BinaryOperator>(lhs)) {
      if (lhsAdd->getOpcode() == Instruction::Add && hasOneUse(lhs)) {
        if (auto* lhsRhsConst = dyn_cast<ConstantInt>(lhsAdd->getOperand(1))) {
          // 计算 C1 - C2
          APInt newConst = lhsRhsConst->getValue() - rhsConst->getValue();
          Constant* combinedConst = ConstantInt::get(sub->getType(), newConst);
          
          // 如果结果为0，直接返回a
          if (newConst.isZero()) {
            return cast<Instruction>(lhsAdd->getOperand(0));
          }
          
          // 创建新的加法指令: a + (C1 - C2)
          return createBinaryOperator(Instruction::Add, 
                                    lhsAdd->getOperand(0), 
                                    combinedConst, 
                                    sub);
        }
      }
    }
  }
  
  // 情况2: (a - C1) - C2 => a - (C1 + C2)
  if (auto* rhsConst = dyn_cast<ConstantInt>(rhs)) {
    if (auto* lhsSub = dyn_cast<BinaryOperator>(lhs)) {
      if (lhsSub->getOpcode() == Instruction::Sub && hasOneUse(lhs)) {
        if (auto* lhsRhsConst = dyn_cast<ConstantInt>(lhsSub->getOperand(1))) {
          // 计算 C1 + C2
          APInt newConst = lhsRhsConst->getValue() + rhsConst->getValue();
          Constant* combinedConst = ConstantInt::get(sub->getType(), newConst);
          
          // 创建新的减法指令: a - (C1 + C2)
          return createBinaryOperator(Instruction::Sub, 
                                    lhsSub->getOperand(0), 
                                    combinedConst, 
                                    sub);
        }
      }
    }
  }
  
  return nullptr;
}

Instruction*
InstructionCombining::combineMul(BinaryOperator* mul)
{
  Value* lhs = mul->getOperand(0);
  Value* rhs = mul->getOperand(1);
  
  // 情况1: (a * C1) * C2 => a * (C1 * C2)
  if (auto* rhsConst = dyn_cast<ConstantInt>(rhs)) {
    if (auto* lhsMul = dyn_cast<BinaryOperator>(lhs)) {
      if (lhsMul->getOpcode() == Instruction::Mul && hasOneUse(lhs)) {
        if (auto* lhsRhsConst = dyn_cast<ConstantInt>(lhsMul->getOperand(1))) {
          // 计算 C1 * C2
          APInt newConst = lhsRhsConst->getValue() * rhsConst->getValue();
          Constant* combinedConst = ConstantInt::get(mul->getType(), newConst);
          
          // 创建新的乘法指令: a * (C1 * C2)
          return createBinaryOperator(Instruction::Mul, 
                                    lhsMul->getOperand(0), 
                                    combinedConst, 
                                    mul);
        }
      }
    }
  }
  
  return nullptr;
}
PreservedAnalyses
InstructionCombining::run(Module& mod, ModuleAnalysisManager& mam)
{
  int functionsProcessed = 0;
  int totalCombinedInsts = 0;
  bool changed = false;

  // 遍历所有函数
  for (auto& func : mod) {
    if (func.isDeclaration())
      continue;
    
    int beforeCount = 0;
    int afterCount = 0;
    
    // 计算优化前的指令数量
    for (auto& bb : func) {
      for (auto& inst : bb) {
        beforeCount++;
      }
    }
    
    if (combineInstructionsInFunction(func)) {
      changed = true;
      functionsProcessed++;
      
      // 计算优化后的指令数量
      for (auto& bb : func) {
        for (auto& inst : bb) {
          afterCount++;
        }
      }
      
      totalCombinedInsts += (beforeCount - afterCount);
    }
  }

  mOut << "InstructionCombining running...\n";
  mOut << "Processed " << functionsProcessed << " functions\n";
  mOut << "Combined " << totalCombinedInsts << " instructions\n";

  return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

bool
InstructionCombining::combineInstructionsInFunction(Function& func)
{
  bool changed = false;
  
  // 多轮迭代，直到没有更多可以合并的指令
  bool iterationChanged = true;
  while (iterationChanged) {
    iterationChanged = false;
    
    for (auto& bb : func) {
      if (combineInstructionsInBasicBlock(bb)) {
        iterationChanged = true;
        changed = true;
      }
    }
  }
  
  return changed;
}

bool
InstructionCombining::combineInstructionsInBasicBlock(BasicBlock& bb)
{
  bool changed = false;
  std::vector<Instruction*> instructions;
  
  // 收集所有指令
  for (auto& inst : bb) {
    instructions.push_back(&inst);
  }
  
  // 首先规范化所有指令
  for (auto* inst : instructions) {
    if (inst->getParent() == nullptr) continue;
    
    if (auto* binOp = dyn_cast<BinaryOperator>(inst)) {
      if (canonicalizeInstruction(binOp)) {
        changed = true;
      }
    }
  }
  
  // 尝试合并指令
  for (auto* inst : instructions) {
    if (inst->getParent() == nullptr) continue;
    
    Instruction* newInst = nullptr;
    
    if (auto* binOp = dyn_cast<BinaryOperator>(inst)) {
      newInst = combineBinaryOperator(binOp);
    } else if (auto* cmpInst = dyn_cast<CmpInst>(inst)) {
      newInst = combineComparison(cmpInst);
    } else if (auto* selectInst = dyn_cast<SelectInst>(inst)) {
      newInst = combineSelectInst(selectInst);
    }
    
    if (newInst) {
      inst->replaceAllUsesWith(newInst);
      inst->eraseFromParent();
      changed = true;
    }
  }
  
  return changed;
}

Instruction*
InstructionCombining::combineDiv(BinaryOperator* div)
{
  Value* lhs = div->getOperand(0);
  Value* rhs = div->getOperand(1);
  
  // 情况1: (a * C1) / C2 => a * (C1 / C2) 当C1能被C2整除时
  if (auto* rhsConst = dyn_cast<ConstantInt>(rhs)) {
    if (auto* lhsMul = dyn_cast<BinaryOperator>(lhs)) {
      if (lhsMul->getOpcode() == Instruction::Mul && hasOneUse(lhs)) {
        if (auto* lhsRhsConst = dyn_cast<ConstantInt>(lhsMul->getOperand(1))) {
          // 检查C1是否能被C2整除
          if (!rhsConst->getValue().isZero() && 
              lhsRhsConst->getValue().srem(rhsConst->getValue()).isZero()) {
            // 计算 C1 / C2
            APInt newConst = lhsRhsConst->getValue().sdiv(rhsConst->getValue());
            Constant* combinedConst = ConstantInt::get(div->getType(), newConst);
            
            // 创建新的乘法指令: a * (C1 / C2)
            return createBinaryOperator(Instruction::Mul, 
                                      lhsMul->getOperand(0), 
                                      combinedConst, 
                                      div);
          }
        }
      }
    }
  }
  
  return nullptr;
}

Instruction*
InstructionCombining::combineShift(BinaryOperator* shift)
{
  Value* lhs = shift->getOperand(0);
  Value* rhs = shift->getOperand(1);
  
  // 情况1: (a << C1) << C2 => a << (C1 + C2)
  if (auto* rhsConst = dyn_cast<ConstantInt>(rhs)) {
    if (auto* lhsShift = dyn_cast<BinaryOperator>(lhs)) {
      if (lhsShift->getOpcode() == shift->getOpcode() && hasOneUse(lhs)) {
        if (auto* lhsRhsConst = dyn_cast<ConstantInt>(lhsShift->getOperand(1))) {
          APInt newShift = lhsRhsConst->getValue() + rhsConst->getValue();
          // 检查位移是否超出类型宽度
          if (newShift.ult(shift->getType()->getIntegerBitWidth())) {
            Constant* combinedConst = ConstantInt::get(shift->getType(), newShift);
            return createBinaryOperator(shift->getOpcode(),
                                      lhsShift->getOperand(0),
                                      combinedConst,
                                      shift);
          }
        }
      }
    }
  }
  
  // 情况2: a << 0 => a
  if (auto* rhsConst = dyn_cast<ConstantInt>(rhs)) {
    if (rhsConst->isZero()) {
      return cast<Instruction>(lhs);
    }
  }
  
  return nullptr;
}

Instruction*
InstructionCombining::combineBitwise(BinaryOperator* bitwise)
{
  Value* lhs = bitwise->getOperand(0);
  Value* rhs = bitwise->getOperand(1);
  
  // 情况1: (a & C1) & C2 => a & (C1 & C2)
  if (auto* rhsConst = dyn_cast<ConstantInt>(rhs)) {
    if (auto* lhsBitwise = dyn_cast<BinaryOperator>(lhs)) {
      if (lhsBitwise->getOpcode() == bitwise->getOpcode() && hasOneUse(lhs)) {
        if (auto* lhsRhsConst = dyn_cast<ConstantInt>(lhsBitwise->getOperand(1))) {
          APInt newConst;
          switch (bitwise->getOpcode()) {
            case Instruction::And:
              newConst = lhsRhsConst->getValue() & rhsConst->getValue();
              break;
            case Instruction::Or:
              newConst = lhsRhsConst->getValue() | rhsConst->getValue();
              break;
            case Instruction::Xor:
              newConst = lhsRhsConst->getValue() ^ rhsConst->getValue();
              break;
            default:
              return nullptr;
          }
          
          Constant* combinedConst = ConstantInt::get(bitwise->getType(), newConst);
          return createBinaryOperator(bitwise->getOpcode(),
                                    lhsBitwise->getOperand(0),
                                    combinedConst,
                                    bitwise);
        }
      }
    }
  }
  
  return nullptr;
}

Instruction*
InstructionCombining::combineComparison(CmpInst* cmp)
{
  Value* lhs = cmp->getOperand(0);
  Value* rhs = cmp->getOperand(1);
  
  // 情况1: (a + C1) == C2 => a == (C2 - C1)
  if (auto* rhsConst = dyn_cast<ConstantInt>(rhs)) {
    if (auto* lhsAdd = dyn_cast<BinaryOperator>(lhs)) {
      if (lhsAdd->getOpcode() == Instruction::Add && hasOneUse(lhs)) {
        if (auto* lhsRhsConst = dyn_cast<ConstantInt>(lhsAdd->getOperand(1))) {
          APInt newConst = rhsConst->getValue() - lhsRhsConst->getValue();
          Constant* combinedConst = ConstantInt::get(cmp->getOperand(1)->getType(), newConst);
          
          return CmpInst::Create(cmp->getOpcode(),
                               cmp->getPredicate(),
                               lhsAdd->getOperand(0),
                               combinedConst);
        }
      }
    }
  }
  
  return nullptr;
}

Instruction*
InstructionCombining::combineSelectInst(SelectInst* select)
{
  Value* cond = select->getCondition();
  Value* trueVal = select->getTrueValue();
  Value* falseVal = select->getFalseValue();
  
  // 情况1: select(cond, C1, C2) where C1 and C2 are constants
  if (auto* trueConst = dyn_cast<Constant>(trueVal)) {
    if (auto* falseConst = dyn_cast<Constant>(falseVal)) {
      // 如果两个值相等，直接返回该值
      if (trueConst == falseConst) {
        return cast<Instruction>(trueConst);
      }
    }
  }
  
  // 情况2: select(cond, a, a) => a
  if (trueVal == falseVal) {
    return cast<Instruction>(trueVal);
  }
  
  return nullptr;
}

Instruction*
InstructionCombining::combineAddChain(BinaryOperator* add)
{
  // 查找形如 a + C1 + C2 + ... 的链式加法
  SmallVector<Value*, 8> terms;
  SmallVector<APInt, 8> constants;
  APInt totalConst(32, 0); // 假设32位
  
  // 收集所有项
  std::function<void(Value*)> collectTerms = [&](Value* val) {
    if (auto* addInst = dyn_cast<BinaryOperator>(val)) {
      if (addInst->getOpcode() == Instruction::Add && hasOneUse(val)) {
        collectTerms(addInst->getOperand(0));
        collectTerms(addInst->getOperand(1));
        return;
      }
    }
    
    if (auto* constInt = dyn_cast<ConstantInt>(val)) {
      totalConst += constInt->getValue();
    } else {
      terms.push_back(val);
    }
  };
  
  collectTerms(add);
  
  // 如果有常数项且有至少一个变量项，进行合并
  if (!totalConst.isZero() && !terms.empty()) {
    Value* result = terms[0];
    for (size_t i = 1; i < terms.size(); ++i) {
      IRBuilder<> builder(add);
      result = builder.CreateAdd(result, terms[i]);
    }
    
    if (!totalConst.isZero()) {
      IRBuilder<> builder(add);
      Constant* constVal = ConstantInt::get(add->getType(), totalConst);
      result = builder.CreateAdd(result, constVal);
    }
    
    return cast<Instruction>(result);
  }
  
  return nullptr;
}

bool
InstructionCombining::hasOneUseOfType(Value* val, unsigned opcode)
{
  if (!hasOneUse(val)) return false;
  
  if (auto* inst = dyn_cast<Instruction>(*val->user_begin())) {
    return inst->getOpcode() == opcode;
  }
  return false;
}

bool
InstructionCombining::hasOneUse(Value* val)
{
  return val->hasOneUse();
}

bool
InstructionCombining::canonicalizeInstruction(BinaryOperator* binOp)
{
  // 对于满足交换律的运算，将常数移到右边
  if (binOp->isCommutative()) {
    Value* lhs = binOp->getOperand(0);
    Value* rhs = binOp->getOperand(1);
    
    // 如果左操作数是常数而右操作数不是，交换它们
    if (isa<Constant>(lhs) && !isa<Constant>(rhs)) {
      binOp->swapOperands();
      return true;
    }
  }
  
  return false;
}

BinaryOperator*
InstructionCombining::createBinaryOperator(unsigned opcode, 
                                          Value* lhs, 
                                          Value* rhs,
                                          Instruction* insertBefore)
{
  IRBuilder<> builder(insertBefore);
  
  switch (opcode) {
    case Instruction::Add:
      return cast<BinaryOperator>(builder.CreateAdd(lhs, rhs));
    case Instruction::Sub:
      return cast<BinaryOperator>(builder.CreateSub(lhs, rhs));
    case Instruction::Mul:
      return cast<BinaryOperator>(builder.CreateMul(lhs, rhs));
    case Instruction::UDiv:
      return cast<BinaryOperator>(builder.CreateUDiv(lhs, rhs));
    case Instruction::SDiv:
      return cast<BinaryOperator>(builder.CreateSDiv(lhs, rhs));
    default:
      return nullptr;
  }
}
