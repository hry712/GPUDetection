#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
struct Detection : public ModulePass {
  static char ID;
  Detection() : ModulePass(ID) {}

  virtual bool runOnModule(Module &M) {
    
    return false;
  }
}; 
}  // end of anonymous namespace

char Detection::ID = 0;
static RegisterPass<Detection> DT1("detection", "GPU kernel malware detection Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);