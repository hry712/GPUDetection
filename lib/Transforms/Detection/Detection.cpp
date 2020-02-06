#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/LegacyPassManager.h"
// #include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

namespace {
struct Detection : public FunctionPass {
  bool hasGPUKernel = false;
  bool isReseted = false;
  Module* lastModule = nullptr;
  static char ID;
  Detection() : FunctionPass(ID) {}

  bool hasGPUKernelCheck(Function &F) {
    CallInst *Call = nullptr;
    for (Instruction &I : instructions(F)) {
      Call = dyn_cast<CallInst>(&I);
      if (!Call)
        continue;
      if (Function *Callee = Call->getCalledFunction()) {
        std::string calleeNameStr = Callee->getName().str();
        if (calleeNameStr == "cudaLaunchKernel") {
          errs() << "We have found cudaLaunchKernel in the "<< F.getName().str() <<"\n";
          return true;
        }
      }
    }
    return false;
  }

  bool hasDeviceResetCheck(Function &F) {
    CallInst *Call = nullptr;
    for (Instruction &I : instructions(F)) {
      Call = dyn_cast<CallInst>(&I);
      if (!Call)
        continue;
      if (Function *Callee = Call->getCalledFunction()) {
        std::string calleeNameStr = Callee->getName().str();
        if (calleeNameStr == "cudaDeviceReset") {
          errs() << "We have found cudaDeviceReset in the "<< F.getName().str() <<"\n";
          return true;
        }
      }
    }
    return false;
  }

  virtual bool runOnFunction(Function &F) {
    errs() << "We are now in the Detection Pass Module.\n";
    Module* curModule = F.getParent();
    
    // record the current Module for the subsequent Functions' check
    if (lastModule != curModule) { // OK, we have met a new Module and a new round for check will begin
      errs() << "We have entered a function whicn belongs to a new Module.\n";
      if (hasGPUKernel) {
        errs() << "The former Module has GPU kernel.\n";
        if (isReseted)
          errs() << "The former Module is safe.\n";
        else
          errs() << "The former Module is not safe!!!\n";
        hasGPUKernel = false;
      }
      isReseted = false;
      lastModule = curModule;
      hasGPUKernel = hasGPUKernelCheck(F);
      if (hasGPUKernel)
        isReseted = hasDeviceResetCheck(F);
    } else {                       // We entered the Function which exists in the same Module.
      errs() << "We have entered a function whicn belongs to the same Module.\n";
      if (hasGPUKernel) {
        if (!isReseted)
          isReseted = hasDeviceResetCheck(F);
      } else {
        hasGPUKernel = hasGPUKernelCheck(F);
      }
    }

    return false;
  }

  virtual bool doFinalization(Module &M) {
    errs() << "We have entered the doFinalization process.\n";
    if (hasGPUKernel) {
      errs() << "The former Module has GPU kernel.\n";
      if (isReseted)
        errs() << "The former Module is safe.\n";
      else
        errs() << "The former Module is not safe!!!\n";
      // hasGPUKernel = false;
    }
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