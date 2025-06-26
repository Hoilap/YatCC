#include "ConstantPropagation.hpp"
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>

using namespace llvm;

PreservedAnalyses
ConstantPropagation::run(Module& mod, ModuleAnalysisManager& mam)
{
  bool changed = propagateConstantsInModule(mod);
  
  mOut << "ConstantPropagation running...\n";
  return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

bool
ConstantPropagation::propagateConstantsInModule(Module& mod)
{
  // 第一步：收集所有被写入的地址
  std::unordered_set<Value*> writtenAddresses;
  collectWrittenAddresses(mod, writtenAddresses);
  
  // 第二步：识别真正的常量（全局变量 + 未被写入的局部变量）
  std::unordered_map<Value*, Constant*> constMap;
  identifyConstants(mod, writtenAddresses, constMap);
  
  // 第三步：进行常量替换
  return replaceWithConstants(mod, constMap);
}

void
ConstantPropagation::collectWrittenAddresses(Module& mod, 
                                            std::unordered_set<Value*>& writtenAddresses)
{
  // 遍历所有函数、基本块和指令，收集所有store指令的指针操作数
  for (auto& func : mod) {
    if (func.isDeclaration()) continue;
    
    for (auto& bb : func) {
      for (auto& inst : bb) {
        if (auto* storeInst = dyn_cast<StoreInst>(&inst)) {
          writtenAddresses.insert(storeInst->getPointerOperand());
        }
      }
    }
  }
}

void
ConstantPropagation::identifyConstants(Module& mod,
                                      const std::unordered_set<Value*>& writtenAddresses,
                                      std::unordered_map<Value*, Constant*>& constMap)
{
  // 识别常量全局变量
  for (auto& globalVar : mod.globals()) {
    // 检查是否有常量初始值且未被写入
    if (globalVar.hasInitializer() && 
        writtenAddresses.find(&globalVar) == writtenAddresses.end()) {
      if (auto* constInit = dyn_cast<Constant>(globalVar.getInitializer())) {
        // 确保全局变量和其初始值类型兼容
        if (isTypeCompatible(globalVar.getValueType(), constInit)) {
          constMap[&globalVar] = constInit;
        }
      }
    }
  }
  
  // 识别局部常量变量（通过分析每个函数内的alloca和store模式）
  for (auto& func : mod) {
    if (func.isDeclaration()) continue;
    
    for (auto& bb : func) {
      for (auto& inst : bb) {
        if (auto* allocaInst = dyn_cast<AllocaInst>(&inst)) {
          // 检查是否只有一个store指令写入常量
          StoreInst* singleStore = nullptr;
          int storeCount = 0;
          
          for (auto* user : allocaInst->users()) {
            if (auto* storeInst = dyn_cast<StoreInst>(user)) {
              if (storeInst->getPointerOperand() == allocaInst) {
                singleStore = storeInst;
                storeCount++;
                if (storeCount > 1) break; // 多于一次store，不是常量
              }
            }
          }
          
          // 如果只有一次store且存储的是常量
          if (storeCount == 1 && singleStore) {
            if (auto* constant = dyn_cast<Constant>(singleStore->getValueOperand())) {
              // 确保类型兼容
              if (isTypeCompatible(allocaInst->getAllocatedType(), constant)) {
                constMap[allocaInst] = constant;
              }
            }
          }
        }
      }
    }
  }
}

bool
ConstantPropagation::replaceWithConstants(Module& mod,
                                         const std::unordered_map<Value*, Constant*>& constMap)
{
  bool changed = false;
  std::vector<Instruction*> toDelete;
  
  // 遍历所有指令进行替换
  for (auto& func : mod) {
    if (func.isDeclaration()) continue;
    
    for (auto& bb : func) {
      // 使用向量收集需要处理的指令，避免迭代器失效
      std::vector<Instruction*> instructions;
      for (auto& inst : bb) {
        instructions.push_back(&inst);
      }
      
      for (auto* inst : instructions) {
        // 处理load指令
        if (auto* loadInst = dyn_cast<LoadInst>(inst)) {
          Value* pointer = loadInst->getPointerOperand();
          auto it = constMap.find(pointer);
          
          if (it != constMap.end()) {
            // 获取类型兼容的常量
            Constant* compatibleConst = getCompatibleConstant(loadInst->getType(), it->second);
            if (compatibleConst) {
              // 用常量替换所有使用
              loadInst->replaceAllUsesWith(compatibleConst);
              toDelete.push_back(loadInst);
              changed = true;
            }
          }
        }
        // 处理二元运算指令
        else if (auto* binOp = dyn_cast<BinaryOperator>(inst)) {
          bool operandChanged = false;
          
          // 检查左操作数
          Value* lhs = binOp->getOperand(0);
          auto it = constMap.find(lhs);
          if (it != constMap.end()) {
            Constant* compatibleConst = getCompatibleConstant(lhs->getType(), it->second);
            if (compatibleConst) {
              binOp->setOperand(0, compatibleConst);
              operandChanged = true;
            }
          }
          
          // 检查右操作数
          Value* rhs = binOp->getOperand(1);
          it = constMap.find(rhs);
          if (it != constMap.end()) {
            Constant* compatibleConst = getCompatibleConstant(rhs->getType(), it->second);
            if (compatibleConst) {
              binOp->setOperand(1, compatibleConst);
              operandChanged = true;
            }
          }
          
          if (operandChanged) {
            changed = true;
          }
        }
        // 处理比较指令
        else if (auto* cmpInst = dyn_cast<CmpInst>(inst)) {
          bool operandChanged = false;
          
          // 检查左操作数
          Value* lhs = cmpInst->getOperand(0);
          auto it = constMap.find(lhs);
          if (it != constMap.end()) {
            Constant* compatibleConst = getCompatibleConstant(lhs->getType(), it->second);
            if (compatibleConst) {
              cmpInst->setOperand(0, compatibleConst);
              operandChanged = true;
            }
          }
          
          // 检查右操作数
          Value* rhs = cmpInst->getOperand(1);
          it = constMap.find(rhs);
          if (it != constMap.end()) {
            Constant* compatibleConst = getCompatibleConstant(rhs->getType(), it->second);
            if (compatibleConst) {
              cmpInst->setOperand(1, compatibleConst);
              operandChanged = true;
            }
          }
          
          if (operandChanged) {
            changed = true;
          }
        }
        // 处理其他可能的指令类型
        else if (auto* callInst = dyn_cast<CallInst>(inst)) {
          // 处理函数调用的参数
          bool operandChanged = false;
          for (unsigned i = 0; i < callInst->getNumOperands() - 1; ++i) { // -1 因为最后一个操作数是被调用函数
            Value* operand = callInst->getOperand(i);
            auto it = constMap.find(operand);
            if (it != constMap.end()) {
              Constant* compatibleConst = getCompatibleConstant(operand->getType(), it->second);
              if (compatibleConst) {
                callInst->setOperand(i, compatibleConst);
                operandChanged = true;
              }
            }
          }
          if (operandChanged) {
            changed = true;
          }
        }
      }
    }
  }
  
  // 安全地删除被替换的load指令
  for (auto* inst : toDelete) {
    if (inst->use_empty()) {
      inst->eraseFromParent();
    }
  }
  
  return changed;
}

bool
ConstantPropagation::isTypeCompatible(Type* expectedType, Constant* constant)
{
  if (!expectedType || !constant) return false;
  
  Type* constantType = constant->getType();
  
  // 直接类型匹配
  if (expectedType == constantType) {
    return true;
  }
  
  // 指针类型特殊处理
  if (expectedType->isPointerTy() && constantType->isPointerTy()) {
    return true; // 指针类型之间通常可以转换
  }
  
  // 整数类型特殊处理
  if (expectedType->isIntegerTy() && constantType->isIntegerTy()) {
    return true; // 整数类型之间可以转换
  }
  
  return false;
}

Constant*
ConstantPropagation::getCompatibleConstant(Type* expectedType, Constant* constant)
{
  if (!expectedType || !constant) return nullptr;
  
  Type* constantType = constant->getType();
  
  // 直接类型匹配
  if (expectedType == constantType) {
    return constant;
  }
  
  // 整数类型转换
  if (expectedType->isIntegerTy() && constantType->isIntegerTy()) {
    if (auto* constInt = dyn_cast<ConstantInt>(constant)) {
      return ConstantInt::get(expectedType, constInt->getZExtValue());
    }
  }
  
  // 指针类型转换
  if (expectedType->isPointerTy() && constantType->isPointerTy()) {
    return ConstantExpr::getBitCast(constant, expectedType);
  }
  
  // 如果无法安全转换，返回nullptr
  return nullptr;
}
