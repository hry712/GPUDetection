#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/raw_ostream.h"
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
        int BBNums = 0;
        for (Loop::block_iterator bbItr = L->block_begin(), endItr = L->block_end(); bbItr!=endItr; bbItr++) {
            // errs()<< *(*bbItr) << "\n";
            ++BBNums;
        }
        errs()<< "The BB number of the current loop is: " << BBNums << "\n";
        BBNums = 0;
        std::vector<Loop*> subLoops = L->getSubLoops();
        Loop::iterator subLpItr = subLoops.begin();
        Loop::iterator subLpEnd = subLoops.end();
        while (subLpItr != subLpEnd) {
            printBBsOfLoop(*subLpItr);
            ++subLpItr;
        }
    }

    virtual void getAnalysisUsage(AnalysisUsage& AU) const override {
        AU.addRequired<LoopInfoWrapperPass>();
    }

    virtual bool runOnFunction(Function& F) {
        // raw_ostream output;
        if (F.getParent()->getTargetTriple().compare("nvptx64-nvidia-cuda") == 0) {
            errs()<< "Entered the LoopInfoDetection pass module for nvidia cuda func: "<< F.getName() <<"\n";
            // Try to use DominatorTree for Analyze methods' work
            // DominatorTree* DT = new DominatorTree();
            // DT->recaculate(F);
            // LoopInfoBase<BasicBlock, Loop>* KLoop = new LoopInfoBase<BasicBlock, Loop>();
            // KLoop->releaseMemory();
            // KLoop->analyze(DT->Base());
            //=====---------------------------------------------------------------------=====
            LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
            if (LI.empty()) {
                errs()<< "Function: " << F.getName() << " has no loop.\n\n";
                return false;
            } else {
                errs()<< "Function: " << F.getName() << " contains loops!!!\n";
                errs()<< "Try to print out the Loop's info.\n";
                // the main info of a loop is recorded in the LoopInfoBase class
                DominatorTree DT = DominatorTree();
                DT.recalculate(F);
                LoopInfoBase<BasicBlock, Loop>* LpInf = new LoopInfoBase<BasicBlock, Loop>();
                LpInf->releaseMemory();
                LpInf->analyze(DT);
                // errs()<< "Try to print the LoopInfo through the LoopInfoBase print() method.\n";
                LpInf->print(errs());
            }
            // for (LoopInfo::iterator LIT = LI.begin(), LEND = LI.end(); LIT!=LEND ; LIT++) {
            //     printBBsOfLoop(*LIT);
            // }
            errs()<< "LoopInfoDetection pass finished.\n\n";
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