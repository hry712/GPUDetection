#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Instruction.h"
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

  virtual bool runOnModule(Module &M) {
      kernelCalled = false;
      hasGPUKernel = false;
      gpuKernelNameStrList.clear();
// #ifndef DEBUG_DRD
//       if (DRD){
// #endif
        // errs() << "We entered the devrstdt pass module. The current Module is "<< M.getName().str() <<"\n";
        // for (Module::iterator fi = M.begin(), fe = M.end(); fi != fe; fi++){
        //   // Get the current func name string
        //   std::string curFuncNameStr = (fi->getName()).str();
        //   // errs() << "The current func name is "<< curFuncNameStr <<"\n";
        //   errs() << "We have entered the first for loop body\n";
        //   for (Function::iterator bi = fi->begin(), be = fi->end(); bi != be; bi++){
        //     for (BasicBlock::iterator ii = bi->begin(), ie = bi->end(); ii != ie; ii++){
        //       if (CallInst* callOp = dyn_cast<CallInst>(&(*ii))) {
        //         Function* calledFunc = callOp->getCalledFunction();
        //         std::string calleeNameStr = (calledFunc->getName()).str();
        //         errs() << "The called func name in the"<< curFuncNameStr <<" is "<< calleeNameStr <<"\n";
        //         if (calleeNameStr.find("cudaLaunchKernel") != std::string::npos) {
        //           errs() << "We found the cudaLaunchKernel() called here!\n";
        //           hasGPUKernel = true;
        //           // Get the Param List of cudaLaunchKernel and withdraw the first one for further comparing
        //           // if (BitCastInst* bitcastOp = dyn_cast<BitCastInst>(&(*(calledFunc->arg_begin())))) {
        //           //   errs() << "We can identify the BitCastInst in the first argu position.\n";
        //           //   if (PointerType* OpndTy = dyn_cast<PointerType>(bitcastOp->getOperand(0)->getType())) {
        //           //     errs() << "Now we think the current Function is of GPU kernel.";
        //           //   }
        //           //   if (FunctionType* srcFuncTy = dyn_cast<FunctionType>(bitcastOp->getOperand(0))) {
        //           //     errs() << "We can transform the bitcast operation's 1st opnd into the FunctionType.\n";
        //           //     if (srcFuncTy->getName().str() == calleeNameStr) {
        //           //       errs() << "We are now comparing the opnd func name with the name of defined func.Cheers!\n";
        //           //       gpuKernelNameStrList.push_back(calleeNameStr);
        //           //     }
        //           //   }
        //           // }
        //         }
        //       }
        //     }
        //   }
        // }
// #ifndef DEBUG_DRD
//       } else
//         errs() << "The option devrstdt seems not work well.\n";
// #endif
          errs() << "We are going to leave this devrstdt Pass Module\n";
      
    return false;
  }
};
}

char DeviceResetDetection::ID = 0;
// INITIALIZE_PASS(DeviceResetDetection, "devrstdt", "DeviceResetDetectionPass", false, false)
static RegisterPass<DeviceResetDetection> DT2("DeviceResetDetection", "GPU kernel malware detection Pass", false, false);
static llvm::RegisterStandardPasses Y(llvm::PassManagerBuilder::EP_EarlyAsPossible,
                                      [](const llvm::PassManagerBuilder &Builder,
                                      llvm::legacy::PassManagerBase &PM) { PM.add(new DeviceResetDetection()); });
Pass *llvm::createDRD() {
  return new DeviceResetDetection();
}