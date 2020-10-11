#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;
namespace {
struct LoopInfoModuleDetection : public ModulePass {
    static char ID;
    LoopInfoModuleDetection() : ModulePass(ID) {}
    virtual bool runOnModule(Module& M) {
        if (M.getTargetTriple().compare("nvptx64-nvidia-cuda") == 0) {
            errs()<< "Meet a CUDA module here...\n";
            errs()<< "Start to pass the func list...\n";
            Module::iterator funcItr = M.begin();
            Module::iterator funcEnd = M.end();
            while (funcItr != funcEnd) {
                errs()<< "Function name: " << (*funcItr).getName() << "\n";
                ++funcItr;
            }
        }
        return false;
    }   // end runOnModule()
};  // end LoopInfoModuleDetection struct
}   // end namespace

char LoopInfoModuleDetection::ID = 0;
static RegisterPass<LoopInfoModuleDetection> LIMD("LoopInfoModuleDetection",
                                        "Try to use the ModulePass to detect loops' info of functions.",
                                        false, false);
static llvm::RegisterStandardPasses LIMDY2(PassManagerBuilder::EP_EarlyAsPossible,
                                        [](const PassManagerBuilder &Builder,
                                        legacy::PassManagerBase &PM) { PM.add(new LoopInfoModuleDetection()); });