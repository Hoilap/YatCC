#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Support/raw_ostream.h>

//Analysis Pass 的继承对象AnalysisInfoMixin继承于 Transform Pass 的继承对象PassInfoMixin
class StaticCallCounter : public llvm::AnalysisInfoMixin<StaticCallCounter>
{
public:
  using Result = llvm::MapVector<const llvm::Function*, unsigned>;
  Result run(llvm::Module& mod, llvm::ModuleAnalysisManager&);
  //Analysis Pass 的 run 函数返回自定义的结果
private:
  // A special type used by analysis passes to provide an address that
  // identifies that particular analysis pass type.
  static llvm::AnalysisKey Key; //其将作为 Analysis Pass 区别于其他 pass 的唯一标识符被AnalysisInfoMixin::ID()函数返回
  friend struct llvm::AnalysisInfoMixin<StaticCallCounter>;  //允许 llvm::AnalysisInfoMixin<StaticCallCounter> 访问 StaticCallCounter 中的私有或受保护成员，包括上面的 Key。
};
