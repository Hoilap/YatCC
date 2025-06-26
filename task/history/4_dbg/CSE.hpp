#pragma once//用于防止头文件被多次包含（即防止重复定义）

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Support/raw_ostream.h>


class CSE : public llvm::PassInfoMixin<CSE> {
public:
    explicit CSE(llvm::raw_ostream& out)
        : mOut(out)
    {
    }
    llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &mam);

private:
    bool isSafeToCSE(llvm::Instruction *I);
    std::string getInstructionKey(llvm::Instruction *I);
    llvm::raw_ostream& mOut;
};

