#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Support/raw_ostream.h>

class InstructionCombination : public llvm::PassInfoMixin<InstructionCombination> {
public:
  explicit InstructionCombination(llvm::raw_ostream& out)
    : mOut(out)
  {
  }

  // 注意参数类型是 llvm::Function 而不是 llvm::Module,意味着我们只需要处理Function即可
  llvm::PreservedAnalyses run(llvm::Module &F,llvm::ModuleAnalysisManager &AM);
private:    
  llvm::raw_ostream& mOut;
};  
