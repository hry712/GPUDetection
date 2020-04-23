#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Detection/DeviceResetDetection.h"
#include <vector>
#include <string>

using namespace llvm;
#define DEBUG_TYPE "dev-rst-dt"
#define DEBUG_DRD
// static cl::opt<bool> DRD("devrstdt",
//                         cl::Optional,
//                         cl::desc("Enable the detection for cudaDeviceReset() API."),
//                         cl::init(false));

namespace {
struct DeviceResetDetection : public ModulePass {
  static char ID;
  bool kernelCalled;
  bool hasGPUKernel;
  std::vector<std::string> gpuKernelNameStrList;

  DeviceResetDetection() : ModulePass(ID) {}

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

  void processGlobalVar(Module *M, std::vector<std::string>& globalStrs) {
    for (Module::global_iterator GVI = M->global_begin(), E = M->global_end();
      GVI != E; GVI++) {
        GlobalVariable *GV = &*GVI;
        if (!GV->hasName() && !GV->isDeclaration() && !GV->hasLocalLinkage()) {
          if (GV->getName().startswith(".str.")){
            ConstantDataSequential* globalVarArr = dyn_cast<ConstantDataSequential>(GV->getInitializer());
            std::string strContent = "";
            if (globalVarArr->isString()){
              strContent = globalVarArr->getAsString();
              globalStrs.push_back(strContent);
              errs() << "The current string is: " << strContent << "\n";
            } else if (globalVarArr->isCString()) {
              strContent = globalVarArr->getAsCString();
              globalStrs.push_back(strContent);
              errs() << "The current string is: " << strContent << "\n";
            } else {
              continue;
            }
          }
        }
    }  
  }

  void printQueue(std::vector<std::string>& Queue) {
    if (Queue.empty()) {
      errs() << "This queue contained nothing!!!\n";
    } else {
      for (std::string element : Queue)
        errs() << element << "\n";
    }
  }

  virtual bool runOnModule(Module &M) {
      std::vector<std::string> globalStrHolder;
      globalStrHolder.clear();
      kernelCalled = false;
      hasGPUKernel = false;
      gpuKernelNameStrList.clear();
      errs() << "We entered the devrstdt pass module. \n";
      processGlobalVar(&M, globalStrHolder);
      if (globalStrHolder.empty()) {
        errs() << "we did not find any global string @.str.xxx in the src file.\n";
      } else {
        printQueue(globalStrHolder);
      }
      // for (Module::iterator fi = M.begin(), fe = M.end(); fi != fe; fi++){
      //   errs() << "test whether we can use the iterator of Module class.\n";
      // }
    return false;
  }
};
}

char DeviceResetDetection::ID = 0;
static RegisterPass<DeviceResetDetection> DT2("DeviceResetDetection", "GPU kernel malware detection Pass", false, false);
static llvm::RegisterStandardPasses Y(llvm::PassManagerBuilder::EP_EarlyAsPossible,
                                      [](const llvm::PassManagerBuilder &Builder,
                                      llvm::legacy::PassManagerBase &PM) { PM.add(new DeviceResetDetection()); });
Pass *llvm::createDRD() {
  return new DeviceResetDetection();
}