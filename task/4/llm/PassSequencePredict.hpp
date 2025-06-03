#pragma once

#include "LLMHelper.hpp"
#include <functional>
#include <initializer_list>
#include <llvm/IR/PassManager.h>
#include <string>


//PassSequencePredict 的工作逻辑有两步：
//1. 对输入的一系列 LLVM Pass 进行 Pass 分析（代码总结）pass_summary
//2. 再分析完所有给定的 LLVM Pass 后，将各个 Pass 的分析结果拼接，连同输入的 LLVM IR，一起发送给大语言模型进行 Pass 序列的预测， run 函数：
class PassSequencePredict : public llvm::PassInfoMixin<PassSequencePredict>
{
public:
  struct PassInfo
  {
    std::string mClassName;
    std::string mHppPath;
    std::string mCppPath;
    std::string mSummaryPath;
    std::function<void(llvm::ModulePassManager&)> mAddPass;
  };

  PassSequencePredict(llvm::StringRef apiKey,
                      llvm::StringRef baseURL,
                      std::initializer_list<PassInfo> passesInfo);

  llvm::PreservedAnalyses run(llvm::Module& mod,
                              llvm::ModuleAnalysisManager& mam);

private:
  LLMHelper mHelper;

  std::vector<PassInfo> mPassesInfo;

  std::string pass_summary(PassInfo& passInfo);
};
