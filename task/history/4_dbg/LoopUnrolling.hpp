#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/CFG.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <unordered_set>
#include <unordered_map>
#include <vector>

class LoopUnrolling : public llvm::PassInfoMixin<LoopUnrolling>
{
public:
  explicit LoopUnrolling(llvm::raw_ostream& out)
    : mOut(out), mMaxUnrollCount(8), mCodeSizeThreshold(32) // 降低默认展开次数，增加代码大小阈值
  {
  }

  explicit LoopUnrolling(llvm::raw_ostream& out, unsigned maxUnrollCount, unsigned codeSizeThreshold = 32)
    : mOut(out), mMaxUnrollCount(maxUnrollCount), mCodeSizeThreshold(codeSizeThreshold)
  {
  }

  llvm::PreservedAnalyses run(llvm::Module& mod,
                              llvm::ModuleAnalysisManager& mam);

private:
  llvm::raw_ostream& mOut;
  unsigned mMaxUnrollCount;
  unsigned mCodeSizeThreshold; // 代码大小阈值
  
  // 增强的循环结构定义
  struct EnhancedLoop {
    llvm::BasicBlock* header;      // 循环头
    std::vector<llvm::BasicBlock*> bodyBlocks; // 循环体块（支持多块）
    llvm::BasicBlock* latch;       // 循环尾（回跳块）
    std::vector<llvm::BasicBlock*> exitBlocks; // 循环出口（支持多出口）
    llvm::BasicBlock* preheader;   // 循环前置块
    llvm::PHINode* inductionVar;   // 归纳变量
    llvm::Value* initValue;        // 初始值
    llvm::Value* stepValue;        // 步长
    llvm::Value* boundValue;       // 边界值
    llvm::CmpInst::Predicate pred; // 比较谓词
    int tripCount;                 // 循环次数
    bool isSimpleLoop;             // 是否是简单循环
    unsigned estimatedSize;        // 循环体估计大小
    
    EnhancedLoop() : header(nullptr), latch(nullptr), preheader(nullptr),
                     inductionVar(nullptr), initValue(nullptr), stepValue(nullptr), 
                     boundValue(nullptr), pred(llvm::CmpInst::BAD_ICMP_PREDICATE), 
                     tripCount(-1), isSimpleLoop(false), estimatedSize(0) {}
  };
  
  // 主要处理函数
  bool unrollLoopsInFunction(llvm::Function& func);
  
  // 增强的循环识别
  bool idLoop(llvm::BasicBlock* bb, EnhancedLoop& loop);
  bool identifySimpleForLoop(llvm::BasicBlock* bb, EnhancedLoop& loop);
  bool identifySimpleWhileLoop(llvm::BasicBlock* bb, EnhancedLoop& loop);
  bool identifyCountedLoop(llvm::BasicBlock* bb, EnhancedLoop& loop);
  
  // 循环分析
  bool analyzeLoopStructure(EnhancedLoop& loop);
  bool findInductionVariable(EnhancedLoop& loop);
  int calculateTripCount(const EnhancedLoop& loop);
  unsigned estimateLoopSize(const EnhancedLoop& loop);
  
  // 展开决策
  bool shouldUnrollLoop(const EnhancedLoop& loop);
  unsigned computeUnrollFactor(const EnhancedLoop& loop);
  
  // 循环展开实现
  bool performFullUnroll(const EnhancedLoop& loop);
  bool performPartialUnroll(const EnhancedLoop& loop, unsigned factor);
  
  // 辅助函数
  llvm::BasicBlock* findOrCreatePreheader(const EnhancedLoop& loop);
  std::vector<llvm::BasicBlock*> findLoopBody(llvm::BasicBlock* header, llvm::BasicBlock* latch);
  bool isLoopInvariant(llvm::Value* value, const EnhancedLoop& loop);
  void cloneLoopIteration(const EnhancedLoop& loop, unsigned iteration, 
                         std::unordered_map<llvm::Value*, llvm::Value*>& valueMap);
  void updatePhiNodes(const EnhancedLoop& loop, const std::vector<llvm::BasicBlock*>& newBlocks);
  
  // 通用辅助函数
  bool isConstantInt(llvm::Value* val, int& result);
  bool dominates(llvm::BasicBlock* dominator, llvm::BasicBlock* dominated, llvm::Function& func);
  void removeLoop(const EnhancedLoop& loop);
  
  // 添加缺失的方法声明
  llvm::BasicBlock* getUniqueSuccessor(llvm::BasicBlock* bb);
  llvm::BasicBlock* getUniquePredecessor(llvm::BasicBlock* bb);
};
