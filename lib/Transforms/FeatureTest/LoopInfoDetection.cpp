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
        if (L == nullptr) {
            errs()<< "No loop was found in this code.\n";
            return ;
        }
        errs() << "Start to print the BBs from a loop...\n";
        for (Loop::block_iterator bbItr = L->block_begin(), endItr = L->block_end(); bbItr!=endItr; bbItr++) {
            errs() << bbItr << "\n";
        }
    }

    virtual bool runOnFunction(Function& F) {
        if (F.getParent()->getTargetTriple().compare("nvptx64-nvidia-cuda") == 0) {
            errs()<< "Entered the LoopInfoDetection pass module for nvidia cuda funcs.\n";
            LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo();
            errs()<< "Try to print out the Loops' info.\n";
            if (LI.empty()) {
                errs()<< "Function: " << F.getName() << " has no loop.\n";
                return false;
            }
            for (LoopInfo::iterator LIT = LI.begin(), LEND = LI.end(); LIT!=LEND ; LIT++) {
                printBBsOfLoop(*LIT);
            }
            errs()<< "LoopInfoDetection pass finished.\n";
        }
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