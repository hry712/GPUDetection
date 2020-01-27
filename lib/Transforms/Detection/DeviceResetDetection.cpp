#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Instruction.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
static cl::opt<bool> DeviceResetDetection("devrstdt", 
                                          cl::init(false), 
                                          cl::desc("Enable the detection for cudaDeviceReset() API."));

namespace {
struct DeviceResetDetection : public ModulePass {
  static char ID;
  bool kernelCalled;
  bool hasGPUKernel;
  DeviceResetDetection() : ModulePass(ID) {}
  virtual bool runOnModule(Module &M) {
      kernelCalled = false;
      hasGPUKernel = false;
      for (Module::iterator fi = M.begin(), fe = M.end(); fi != fe; fi++){
        // Get the current func name string
        std::string curFuncNameStr = (fi->getName()).str();
        for (Function::iterator bi = fi->begin(), be = fi->end(); bi != be; bi++){
          for (BasicBlock::iterator ii = bi->begin(), ie = bi->end(); ii != ie; ii++){
            if (auto* callOp = dyn_cast<CallInst>(&(*ii))) {
              Function* calledFunc = callOp->getCalledFunction();
              std::string calleeNameStr = (calledFunc->getName()).str();
              if (calleeNameStr.find("cudaLaunchKernel") != std::string::npos) {
                errs() << "We found the cudaLaunchKernel() called here!\n";
                hasGPUKernel = true;
              }
            }
          }
        }
      }
    return true;
  }
};
}

char DeviceResetDetection::ID = 0;
static RegisterPass<DeviceResetDetection> X("DeviceResetDetection", "GPU kernel malware detection Pass", false, false);