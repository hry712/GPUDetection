#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LegacyPassManager.h"
// #include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

namespace {
struct Detection : public FunctionPass {
  static char ID;
  Detection() : FunctionPass(ID) {}
  virtual bool runOnFunction(Function &F) {
    errs() << "We are now in the Detection Pass Module.\n";

    return false;
  }
}; 
}  // end of anonymous namespace

char Detection::ID = 0;
static RegisterPass<Detection> DT1("detection", "GPU kernel malware detection Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);
static llvm::RegisterStandardPasses Y2(llvm::PassManagerBuilder::EP_EarlyAsPossible,
                                      [](const llvm::PassManagerBuilder &Builder,
                                      llvm::legacy::PassManagerBase &PM) { PM.add(new Detection()); });