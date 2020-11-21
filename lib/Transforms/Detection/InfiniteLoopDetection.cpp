#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

namespace {
struct InfiniteLoopDetection : public FunctionPass {
    static char ID;
    InfiniteLoopDetection() : FunctionPass(ID) {}

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
        AU.setPreservesCFG();
        AU.addRequired<LoopInfoWrapperPass>();
    }

    virtual bool runOnFunction(Function &F) {
        Loop* LP = nullptr;
        PHINode* indctVar = nullptr;
        if ((F.getParent())->getTargetTriple().compare("nvptx64-nvidia-cuda") == 0) {
            errs()<< "Now, this pass is dealing with a GPU kernel module...\n";
            LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
            if (LI.empty()) {
                errs()<< "No Loop Structure exists in the function: " << F.getName() << "\n";
            } else {
                errs()<< "Start analyzing the LoopInfo obj in the Function: " << F.getName() << "\n";
                for (LoopInfo::iterator i=LI.begin(), e=LI.end(); i!=e; i++) {
                    if ((LP=(*i)) != nullptr) {
                        errs()<< "Start detecting the loop: " << *LP << "\n";
                        if ((indctVar=LP->getCanonicalInductionVariable()) != nullptr) {
                            errs()<< "Loop " << *LP << " contains an induction variable " << *indctVar << "\n";
                        } else {
                            errs()<< "WARNING: Loop " << *LP << "does not have an induction variable--Maybe it's an infinite loop.\n";
                        }
                    } else {
                        errs()<< "The LoopT* vector contains nothing for the LoopInfo obj:\n";
                        LI.print(errs());
                    }
                }
            }
        }
        return false;
    }
};
}

char InfiniteLoopDetection::ID = 0;
static RegisterPass<InfiniteLoopDetection> ILD1("InfiniteLoopDetection", "GPU kernel infinite loop detection Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);
static llvm::RegisterStandardPasses ILDY2(PassManagerBuilder::EP_EarlyAsPossible,
        [](const PassManagerBuilder &Builder,
        legacy::PassManagerBase &PM) { PM.add(new InfiniteLoopDetection()); });