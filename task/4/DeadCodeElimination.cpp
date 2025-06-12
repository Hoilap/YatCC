#include "DeadCodeElimination.hpp"
#include<vector>
using namespace llvm;
//Val相同的多个 Use 组织成链表的形式，使用Next和Prev记录链表中前后两个Use  UD链
//%3 = add %1, %2中%3存在两个 Use，其(Val, Parent)分别为(%3, %1)和(%3, %2)


//对于 DCE，我们可以遍历每条指令的 use-def 链，如果该 use-def 链上所有端点都没有被使用（即端点的 use 次数为 0），且链中无函数调用与访存，说明该链上的所有计算结果都是无效计算，应该被删除。

// 判断指令是否可以安全删除
bool isInstructionTriviallyDead(Instruction *I) {
  if (!I->use_empty() || I->isTerminator())
    return false;

  // 遍历每条指令的 use-def 链:递归检查操作数是否也是死代码
  // for (Value *Op : I->operands()) {
  //   if (Instruction *OpI = dyn_cast<Instruction>(Op)) {
  //     if (!isInstructionTriviallyDead(OpI))
  //       return false;
  //   }
  // }

  return !(I->mayHaveSideEffects());
}

PreservedAnalyses DeadCodeElimination::run(Module &M, ModuleAnalysisManager &MAM) {
    bool Changed = false;

    for (Function &F : M) {
      if (F.isDeclaration()) continue;

      // 为了避免 iterator 失效，需要先收集需要删除的指令
      std::vector<Instruction*> ToDelete;

      for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
          if (isInstructionTriviallyDead(&I)) {
            ToDelete.push_back(&I);
          }
        }
      }

      for (Instruction *I : ToDelete) {
        I->eraseFromParent();
        Changed = true;
      }
    }

    return PreservedAnalyses::all();
}

//rfr
// #include "DeadCodeElimination.hpp"
// #include <queue>
// #include <set>

// using namespace llvm;

// PreservedAnalyses
// DeadCodeElimination::run(Module& mod, ModuleAnalysisManager& MAM)
// {
//   unsigned removedInstructions = 0;

//   std::set<Instruction*> toBeRemoved;
//   for (Function& Func : mod) {
//     for (BasicBlock& Bloc : Func) {
//       for (Instruction& Inst : Bloc) {
//         if (AllocaInst* AI = dyn_cast<AllocaInst>(&Inst)) {
//           if (AI->user_empty()) {
//             toBeRemoved.insert(AI);
//           }
//         } else if (StoreInst* SI = dyn_cast<StoreInst>(&Inst)) {
//           Value* Ptr = SI->getPointerOperand();

//           if (Ptr->hasOneUser() && !isa<GetElementPtrInst>(Ptr)) {
//             toBeRemoved.insert(SI);

//             std::queue<Value*> explorationQueue;
//             explorationQueue.push(SI->getValueOperand());
//             while (!explorationQueue.empty()) {
//               Value* CurrentVal = explorationQueue.front();
//               explorationQueue.pop();

//               for (User* UserInst : CurrentVal->users()) {
//                 if (Instruction* Instr = dyn_cast<Instruction>(UserInst)) {
//                   toBeRemoved.insert(Instr);
//                   explorationQueue.push(Instr);
//                 }
//               }
//             }
//           }
//         }
//       }
//     }
//   }

//   for (Instruction* Instr : toBeRemoved) {
//     Instr->eraseFromParent();
//     ++removedInstructions;
//   }
//   mOut << "DeadInstructionRemoval: Processed module, removed "
//        << removedInstructions << " instructions.\n";
//   return PreservedAnalyses::all();
// }