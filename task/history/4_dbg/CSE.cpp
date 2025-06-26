// CSE.cpp
#include "CSE.hpp"
#include <unordered_map>
#include <vector>

using namespace llvm;

std::string CSE::getInstructionKey(Instruction *I) {
	std::string key;
	raw_string_ostream rso(key);
  
	// 包含指令类型信息
	I->getType()->print(rso);
	rso << " " << I->getOpcodeName() << " ";
	
	for (auto &Op : I->operands()) {
		if (isa<BasicBlock>(Op)) {
			// 基本块作为操作数(如phi节点)需要特殊处理
			rso << "block:" << cast<BasicBlock>(Op)->getName();
    	} 
		else {
			Op->printAsOperand(rso, false);
		}
		rso << " ";
	}
  	return rso.str();
}

bool CSE::isSafeToCSE(Instruction *I) {
  // 排除更多不安全情况
  if (I->isTerminator() || isa<AllocaInst>(I) )
    return false;
  return true;
}

PreservedAnalyses CSE::run(Module &M, ModuleAnalysisManager &mam) {
    for (auto &Func : M) {

      //删除公共子表达式
      for (auto &BB : Func) {
          //保存计算后的结果
          std::unordered_map<std::string, Instruction*> exprMap;
          std::vector<Instruction*> toErase;
          for (auto &I : BB) {
              // 特殊处理load指令
              if (auto *LI = dyn_cast<LoadInst>(&I)) {
                // 如果load的结果没有被使用，可以删除
                if (LI->use_empty()) {
                    toErase.push_back(LI);
                }
                continue;
              }

              // 二元计算指令的公共子表达式消除
              if (auto binOp = dyn_cast<BinaryOperator>(&I)) {
                  //生成这个指令的键
                  std::string binOpKey = getInstructionKey(binOp);

                  auto it = exprMap.find(binOpKey);
                  if (it != exprMap.end()) {
                      // 若已经计算过，则直接替换为之前的结果
                      binOp->replaceAllUsesWith(it->second);
                      toErase.push_back(&I);
                  } else {
                      // 若未计算过，则将结果存入map
                      exprMap[binOpKey] = &I;
                  }
              }
          }
          // 按逆序删除指令以避免使用已删除的指令
          for (auto *I : llvm::reverse(toErase)) {
              I->eraseFromParent();
          }
      }

      //处理load指令的问题
        for (auto &BB : Func) {
            //保存计算后的结果
            std::unordered_map<std::string, Instruction*> exprMap;
            std::vector<Instruction*> toErase;
            DenseMap<Value *, Value *> LoadedGlobals;//应该放在基本块内
            for (auto &I : BB) {
                // 特殊处理load指令: 
                if (auto *LI = dyn_cast<LoadInst>(&I)) {
                    Value *Ptr = LI->getPointerOperand();
                    if (auto *GV = dyn_cast<GlobalVariable>(Ptr)) {
                        // 如果已加载过该全局变量，替换当前加载
                        if (LoadedGlobals.count(GV)) {
                            LI->replaceAllUsesWith(LoadedGlobals[GV]);
                            toErase.push_back(LI);
                            //mOut << "Replacing load of " << GV->getName() << " with previous loaded value.\n";
                        }
                        else {
                            LoadedGlobals[GV] = LI;
                        }
                    }
                }
                if(auto *SI = dyn_cast<StoreInst>(&I)){//应该改成I.mayWriteToMemory()
                    Value *Ptr = SI->getPointerOperand(); 
                    if (auto *GV = dyn_cast<GlobalVariable>(Ptr)) {
                        // 这个store指令的全局变量已经load过,那要删除这表中的条目
                        LoadedGlobals.erase(GV);
                    }
                }
            }
            // 按逆序删除指令以避免使用已删除的指令
            for (auto *I : llvm::reverse(toErase)) {
                I->eraseFromParent();
            }
        }
    }
    
    mOut << "CSE Common Subexpression Elimination running...\n";
    return PreservedAnalyses::all();
}
