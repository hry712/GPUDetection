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
    Detection() : FunctionPass(ID) {}

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
        AU.setPreservesCFG();
        AU.addRequired<LoopInfoWrapperPass>();
    }

    virtual bool runOnFunction(Function &F) {
        LoopT* LT = nullptr;
        PHINode* indctVar = nullptr;
        if ((F.getParent())->getTargetTriple().compare("nvptx64-nvidia-cuda") == 0) {
            LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
            for (LoopInfo::iterator i=LI.begin(), e=LI.end(); i!=e; i++) {
                if ((LT=(*i)) != nullptr) {
                    errs()<< "Start detecting the loop: " << *LT << "\n";
                    if ((indctVar=LT->getInductionVariable()) != nullptr) {
                        errs()<< "Loop " << *LT << " contains an induction variable " << *indctVar << "\n";
                    } else {
                        errs()<< "WARNING: Loop " << *LT << "does not have an induction variable--Maybe it's an infinite loop.\n";
                    }
                } else {
                    errs()<< "The LoopT* vector contains nothing for the LoopInfo obj: " << LI << "\n";
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