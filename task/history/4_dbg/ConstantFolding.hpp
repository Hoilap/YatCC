#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Support/raw_ostream.h>

//所有 Transform Pass 都继承于llvm::PassInfoMixin，该类会接受模板参数中 Transform Pass 的名称将 pass 初始化
class ConstantFolding : public llvm::PassInfoMixin<ConstantFolding>
{
public:
  explicit ConstantFolding(llvm::raw_ostream& out)
    : mOut(out)
  {
  }

  llvm::PreservedAnalyses run(llvm::Module& mod,
                              llvm::ModuleAnalysisManager& mam);

private:
  llvm::raw_ostream& mOut;
};
