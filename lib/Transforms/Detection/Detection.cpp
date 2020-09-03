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
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/CFG.h"


#include <list>
#include <map>
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
  std::map<const BasicBlock*, int> BBVisitedMap;
  std::vector<BasicBlock*> VisitedBBs;
  std::vector<const BasicBlock*> TracedBBs;
  std::vector<const BasicBlock*>* PathBBs;
  std::vector<std::vector<const BasicBlock*>*> BBLoopPaths;

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
        if (!GV->hasName()) {
          errs() << "the global var name is: " << GV->getName() << "\n";
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
    // errs()<< F.getParent()->getTargetTriple().compare("nvptx64-nvidia-cuda") << "\n";
    // errs()<< "The current func name is: " << F.getName() << "\n";
    if (F.getParent()->getTargetTriple().compare("nvptx64-nvidia-cuda") == 0) {
      // Now we are parsing a nvptx function
      errs()<< "\nStart to detect Loop CFG in the GPU Kernel function: "<< F.getName() <<"...\n";
      BBVisitedMap.clear();
      TracedBBs.clear();
      if (hasLoopCFG(F)) {
        errs() << "Caution: Function " << F.getName() << " contains a Loop CFG !!\n";
        // hasLoopBRInst(F);
        // debug trace path
        LoopPathScaning(&(F.getEntryBlock()));
        errs() << "Try to print out the loop path in the CFG...\n";
        for (auto path: BBLoopPaths) {
          errs()<<"Path: ";
          for (auto bb: *path) {
            StringRef bbName(bb->getName());
            errs()<< bbName << " ";
          }
          errs()<< "\nPath print finished.\n";
        }
        std::map<const BasicBlock*, int>::iterator visitedIter = BBVisitedMap.begin();
        while (visitedIter != BBVisitedMap.end()) {
          if (visitedIter->second == 2)
            break;
          visitedIter++;
        }
        const Instruction* terminatorInst = visitedIter->first->getTerminator();
        if (const BranchInst* brInst = dyn_cast<const BranchInst>(terminatorInst)) {
          if (brInst->isConditional()) {        // loop control BB ----- for, while{}
            if (hasSpecialBrInst(visitedIter->first)) {
              // make a further confirm that this function contains a loop structure
              errs()<< "For/While loop exists in this function!\n";
              // TO-DO: withdraw the control variable from the branchInst and monitor its changing
              // ......
            } else {
              errs()<< "May be this is an unknown/intricate loop structure...\n";
            }
          } else {                            // loop body BB ----- do {} while
            if (visitedIter->first->hasNPredecessors(2)) {
              for (const BasicBlock* predBB : predecessors(visitedIter->first)) {
                if (hasSpecialBrInst(predBB)) {
                  errs()<< "Do-while loop exists in this function!\n";
                  break;
                }
              }
            } else {
              errs()<< "May be this is an unknown/intricate loop structure...\n";
            }            
          }
        }

      } else {
        errs() << F.getName() << " is Loop safe.\n";
      }
      return false;
    } else {
      // just skip the CPU IR codes
      return false;
    }
  }

  int hasBB(std::vector<const BasicBlock*> src, const BasicBlock* target) {
    int i = 0;
    int len = src.size();
    while (i<len) {
      if (src[i] == target)
        return i;
      i++;
    }
    return -1;
  }

  bool hasLoopCFG(const Function &F) {
    const BasicBlock* entryBB = &(F.getEntryBlock());
    if (entryBB != nullptr) {
      // return DFSCycleDetecting(entryBB, 10);
      return DFSCycleDetecting(entryBB, 10);
    } else {
      return false;
    }
  }

  bool DFSCycleDetecting(const BasicBlock* BB, int LoopLimit) {
    const Instruction* terminatorInst = BB->getTerminator();
    BasicBlock* successorBB = nullptr;
    if (const BranchInst* brInst = dyn_cast<const BranchInst>(terminatorInst)) {
      unsigned int SUCCESSOR_NUM = brInst->getNumSuccessors();
      for (unsigned int i=0; i<SUCCESSOR_NUM; i++) {
        successorBB = brInst->getSuccessor(i);
        if (BBVisitedMap.find(successorBB) != BBVisitedMap.end()) {
          BBVisitedMap[successorBB] += 1;
          // if (TracedBBs.find(BB) != TracedBBs.end()) {
          //   // start from the traced BB to the rest of other traced BBs
          //   for (auto bbIter=TracedBBs.find(BB), endIter=TracedBBs.end(); bbIter!=endIter; bbIter++) {
          //     LoopPathBBs.push_back(*bbIter);
          //   }
          //   BBLoopPaths.push_back(LoopPathBBs);
          //   TracedBBs.pop_back();
          // }
          return true;
        } else {
          BBVisitedMap.insert(std::pair<BasicBlock*, int>(successorBB, 1));
          // TracedBBs.push_back(successorBB);
          // dfs continues
          return DFSCycleDetecting(successorBB, LoopLimit);
        }
      }
    }
    return false;
  }

  bool LoopPathScaning(const BasicBlock* BB) {
    bool ans = false;
    if (BB == nullptr) {
      return false;
    }
    if (BBVisitedMap.find(BB) != BBVisitedMap.end()) {    // current BB is visited
      // int existRet = std::count(TracedBBs.begin(), TracedBBs.end(), BB);
      unsigned int existRet = hasBB(TracedBBs, BB);
      if (existRet != -1) {
        PathBBs = new std::vector<const BasicBlock*>();
        while (existRet < TracedBBs.size()) {
          PathBBs->push_back(TracedBBs[existRet]);
          ++existRet;
        }
        BBLoopPaths.push_back(PathBBs);
      }
      ans = true;
    } else {    // current BB has not been visited
      BBVisitedMap.insert(std::pair<const BasicBlock*, int>(BB, 1));
      TracedBBs.push_back(BB);
      const Instruction* terminatorInst = BB->getTerminator();
      BasicBlock* successorBB = nullptr;
      if (const BranchInst* brInst = dyn_cast<const BranchInst>(terminatorInst)) {
        unsigned int SUCCESSOR_NUM = brInst->getNumSuccessors();
        for (unsigned int i=0; i<SUCCESSOR_NUM; i++) {
          successorBB = brInst->getSuccessor(i);
          ans = LoopPathScaning(successorBB);
        }
      }
      TracedBBs.pop_back();
    }
    return ans;
  }

  bool hasLoopBRInst(const Function & F) {
    std::map<const BasicBlock*, int>::iterator visitedIter = BBVisitedMap.begin();
    while (visitedIter != BBVisitedMap.end()) {
      if (visitedIter->second == 2) {
        errs() << "Caution: we found a BB which is visited twice!!\n";
        // errs() << "The BasicBlock name is: " << visitedIter->first->getName() << "\n";
        errs() << *(visitedIter->first) << "\n";
        return true;
      } 
      visitedIter++;
    }
    return false;
  }

  bool hasSpecialBrInst(const BasicBlock* BB) {
    const Instruction* terminatorInst = BB->getTerminator();
    if (const BranchInst* brInst = dyn_cast<const BranchInst>(terminatorInst)) {
      if (brInst->getNumSuccessors() == 2 && brInst->isConditional()) {
        BasicBlock* firstBB = brInst->getSuccessor(0);
        BasicBlock* secondBB = brInst->getSuccessor(1);
        int firstCount = BBVisitedMap.count(firstBB);
        int secondCount = BBVisitedMap.count(secondBB);
        if (firstCount==1 || secondCount==1) {
          return true;
        }
      }
    }
    return false;
  }

  virtual bool doFinalization(Module &M) {
    // errs() << "We have entered the doFinalization process.\n";
    if (hasGPUKernel) {
      errs() << "The former Module has GPU kernel.\n";
      if (isReseted)
        errs() << "The src file is safe.\n";
      else
        errs() << "The src file is not safe!!!\n";
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