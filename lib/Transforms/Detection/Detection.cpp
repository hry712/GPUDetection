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

#include <list>
#include <regex>

using namespace llvm;
using namespace std;

namespace {
struct Detection : public FunctionPass {
  bool hasGPUKernel = false;
  bool isReseted = false;
  Module* lastModule = nullptr;
  std::vector<std::string> cudaMallocHostArgQueue;
  std::vector<std::string> cudaMallocArgQueue;
  std::vector<std::string> cudaEventCreateArgQueue;
  std::vector<std::string> globalStrHolder;

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

  void split(const string& s, vector<string>& tokens, const string& delimiters = " ") {
    string::size_type lastPos = s.find_first_not_of(delimiters, 0);
    string::size_type pos = s.find_first_of(delimiters, lastPos);
    while (string::npos != pos || string::npos != lastPos) {
      tokens.push_back(s.substr(lastPos, pos - lastPos));
      lastPos = s.find_first_not_of(delimiters, pos);
      pos = s.find_first_of(delimiters, lastPos);
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

  bool hasCudaCalling(string Src, string APIName) {
    std::string voidPtrArgPattern("/[(]void [*]+[)][&][a-zA-Z]+[_0-9a-zA-Z]*/g");
    std::string cudaMallocHostCallingPattern("/^cudaMallocHost[(]/g");
    smatch results;
    if (APIName == "cudaMallocHost"){
      if (regex_match(Src, regex(cudaMallocHostCallingPattern))) {
        if (regex_match(Src, results, regex(voidPtrArgPattern))) {
          // find the (void **)&XXX here then get the arg name string after the "&" char
          // finally insert it into the recording queue for later matching
          std::string argName = *results.begin();
          vector<string> tokensArg;
          tokensArg.clear();
          split(argName, tokensArg, "&");
          cudaMallocHostArgQueue.push_back(tokensArg[0]);
          errs() << "get the arg name from cudaMallocHost : " << tokensArg[0] << "\n";
          return true;
        }
      }
      
    } else if (APIName == "cudaMalloc") {
      return false;

    } else if (APIName == "cudaEventCreate") {
      return false;

    } else
      return false;
  }

  // find out all the @global_strings which begin with ".str." and store
  // them into the globalStrs arg part
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

  virtual bool runOnFunction(Function &F) {
    errs() << "We are now in the Detection Pass Module.\n";
    Module* curModule = F.getParent();
    // record the current Module for the subsequent Functions' check
    if (lastModule != curModule) { // OK, we have met a new Module and a new round for check will begin
      errs() << "We have entered a function whicn belongs to a new Module.\n";
      if (hasGPUKernel) {
        errs() << "The former Module has GPU kernel.\n";
        globalStrHolder.clear();
        cudaMallocHostArgQueue.clear();
        cudaEventCreateArgQueue.clear();
        cudaMallocArgQueue.clear();
        processGlobalVar(lastModule, globalStrHolder);
        if (globalStrHolder.empty()) {
          errs() << "we did not find any global string @.str.xxx in the src file.\n";
        } else {
          printQueue(globalStrHolder);
        }
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
      // errs() << "We have entered a function whicn belongs to the same Module.\n";
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