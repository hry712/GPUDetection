#include "llvm/IR/Pass.h"
#include "llvm/Analysis/LoopPass.h"

using namespace llvm;

namespace {
    struct LoopDetection : public LoopPass {
        static char ID;
        LoopDetection() : LoopPass(ID) {}

        void printBBsOfLoop(Loop* L) {
            errs() << "Start to print the BBs from a loop...\n";
            for (Loop::block_iterator bbItr = L->block_begin(), endItr = L->block_end(); bbItr!=endItr; bbItr++) {
                errs() << *bbItr << "\n";
            }
        }

        virtual bool runOnLoop(Loop* L, LPPassManager& LPM) {
            printBBsOfLoop(L);
        }
    };
}

char LoopDetection::ID = 0;
static RegisterPass<LoopDetection> DT2("LoopDetection", "Try to use the default utility from LoopPass to detect loop info.", false, false);
static llvm::RegisterStandardPasses Y(llvm::PassManagerBuilder::EP_EarlyAsPossible,
                                      [](const llvm::PassManagerBuilder &Builder,
                                      llvm::legacy::PassManagerBase &PM) { PM.add(new LoopDetection()); });