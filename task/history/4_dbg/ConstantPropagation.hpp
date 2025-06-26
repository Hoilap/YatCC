#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/GlobalVariable.h>
#include <unordered_map>
#include <unordered_set>

class ConstantPropagation : public llvm::PassInfoMixin<ConstantPropagation>
{
public:
  explicit ConstantPropagation(llvm::raw_ostream& out)
    : mOut(out)
  {
  }

  llvm::PreservedAnalyses run(llvm::Module& mod,
                              llvm::ModuleAnalysisManager& mam);

private:
  llvm::raw_ostream& mOut;
  
  bool propagateConstantsInModule(llvm::Module& mod);
  void collectWrittenAddresses(llvm::Module& mod, 
                              std::unordered_set<llvm::Value*>& writtenAddresses);
  void identifyConstants(llvm::Module& mod,
                        const std::unordered_set<llvm::Value*>& writtenAddresses,
                        std::unordered_map<llvm::Value*, llvm::Constant*>& constMap);
  bool replaceWithConstants(llvm::Module& mod,
                           const std::unordered_map<llvm::Value*, llvm::Constant*>& constMap);
  
  // 辅助函数：检查类型兼容性
  bool isTypeCompatible(llvm::Type* expectedType, llvm::Constant* constant);
  // 辅助函数：安全地获取常量的类型兼容版本
  llvm::Constant* getCompatibleConstant(llvm::Type* expectedType, llvm::Constant* constant);
};
