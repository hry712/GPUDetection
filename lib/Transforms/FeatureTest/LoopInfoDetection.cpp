#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Analysis/LoopInfo.h"

using namespace llvm;
namespace {
struct LoopInfoDetection : public FunctionPass {
    static char ID;
    LoopInfoDetection() : FunctionPass(ID) {}

    void printBBsOfLoop(Loop* L) {
        errs() << "Start to print the BBs from a loop...\n";
        for (Loop::block_iterator bbItr = L->block_begin(), endItr = L->block_end(); bbItr!=endItr; bbItr++) {
            errs() << *bbItr << "\n";
        }
    }

    virtual bool runOnFunction(Function& F) {
        LoopInfo &LI = getAnalysis<LoopInfo>(F);
        for (LoopInfo::iterator LIT = LI.begin(), LEND = LI.end(); LIT!=LEND ; LIT++) {
            printBBsOfLoop(*LIT);
        }
    }
};
}

char LoopInfoDetection::ID = 0;
static RegisterPass<LoopInfoDetection> DT2("LoopInfoDetection", "Try to use the getAnalysis() to detect loops' info of functions.", false, false);
static llvm::RegisterStandardPasses Y(llvm::PassManagerBuilder::EP_EarlyAsPossible,
                                      [](const llvm::PassManagerBuilder &Builder,
                                      llvm::legacy::PassManagerBase &PM) { PM.add(new LoopInfoDetection()); });