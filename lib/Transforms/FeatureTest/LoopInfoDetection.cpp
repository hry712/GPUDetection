#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

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
        errs()<< "Entered the LoopInfoDetection pass module.\n";
        LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo();
        errs()<< "Try to print out the Loops' info.\n";
        for (LoopInfo::iterator LIT = LI.begin(), LEND = LI.end(); LIT!=LEND ; LIT++) {
            printBBsOfLoop(*LIT);
        }
        errs()<< "LoopInfoDetection pass finished.\n";
        return false;
    }
};
}

char LoopInfoDetection::ID = 0;
static RegisterPass<LoopInfoDetection> LID("LoopInfoDetection",
                                        "Try to use the getAnalysis() to detect loops' info of functions.",
                                        false, false);
static llvm::RegisterStandardPasses LIDY2(PassManagerBuilder::EP_EarlyAsPossible,
                                      [](const PassManagerBuilder &Builder,
                                      legacy::PassManagerBase &PM) { PM.add(new LoopInfoDetection()); });