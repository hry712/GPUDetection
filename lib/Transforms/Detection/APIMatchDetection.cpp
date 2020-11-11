#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Pass.h"

#include <vector>
#include <map>
#include <string>

using namespace llvm;
using namespace std;

namespace {
struct APIMatchDetection : public FunctionPass {
    static char ID;
    APIMatchDetection() : FunctionPass(ID) {}

    Value* rawPtrVar = nullptr;
    std::vector<CallInst*> callInstVect;
    std::map<Value*, std::string> records;

    Value* getRawVarValue(Function &F, Value* FirstArgu) {
        if (BitCastInst* bitcastInst = dyn_cast<BitCastInst>(FirstArgu)) {
            return bitcastInst;
        }
        return nullptr;
    }

    void printCallInstVector(void) {
        errs()<< "Start to print out the CallInst vector...\n";
        if (!callInstVect.empty()) {
            for (auto inst : callInstVect)
                errs()<< *inst << "\n";
        }
        callInstVect.clear();
        records.clear();
        errs()<< "Finish printing.\n\n";
    }

    Value* getCudaMallocArguVar(CallInst* Inst) {
        BasicBlock* curBB = Inst->getParent();
        Context* curContext = curBB->getParent()->getParent()->getContext();
        Value* arg0 = Inst->getArgOperand(0);
        BitCastInst* bcInst = nullptr;
        // PointerType* i8_ptr = PointerType::getInt8PtrTy(curContext);
        // PointerType* i8_pptr = PointerType::get(i8_ptr, 0);

        if (arg0 != nullptr) {
            if (bcInst = dyn_cast<BitCastInst>(arg0)) {
                /// parsing the current BB to find out the bitcast inst
                BasicBlock::iterator instItr = curBB->begin();
                BasicBlock::const_iterator End = curBB->end();
                while (instItr != End) {
                    if ((&(*instItr)) == bcInst) { // find the target bitcast inst here
                        // withdraw the target argument of bitcast
                        return bcInst->getOperand(0);
                    }
                    ++instItr;
                }
            }
        }
        return nullptr;
    }

    Value* getCudaFreeArguVar(CallInst* Inst) {
        Value* arg0 = Inst->getArgOperand(0);
        return arg0;
    }

    /// This method returns the CallInst's first argu var or ret recording var to 
    /// withdraw the key content for records hashmap.
    Value* getRealMemVar(CallInst* Inst) {
        Function* calledFunc = Inst->getCalledFunction();
        std::string funcName = calledFunc->getName();
        if (funcName == "cudaMalloc") {
            return getCudaMallocArguVar(Inst);
        } else if (funcName == "cudaFree") {
            return getCudaFreeArguVar(Inst);
        } else {
            errs()<< "In the getRealMemVar() method, we met a new function named "
                    << funcName << " .\n";
            return nullptr;
        }
    }

    void cudaDeviceResetDetecting(void) {
        if (!callInstVect.empty()) {
            std::vector<CallInst*>::iterator callInstItr = callInstVect.begin();
            std::vector<CallInst*>::iterator endItr = callInstVect.end();
            std::vector<CallInst*>::iterator idxItr;
            CallInst* callInst = nullptr;
            Function* calledFunc = nullptr;
            std::string funcName;
            CallInst* idxCallInst = nullptr;
            std::string idxFuncName;
            while (callInstItr != endItr) {
                callInst = *callInstItr;
                funcName = callInst->getCalledFunction()->getName();
                if (funcName == "cudaDeviceReset") {
                    // start to find the interloop before the next cudaDeviceReset calling.
                    idxItr = (callInstItr+1);
                    while (idxItr != endItr) {
                        idxCallInst = *idxItr;
                        if (idxCallInst->getCalledFunction()->getName() == "cudaDeviceReset")
                            break;
                        ++idxItr;
                    }
                    // start to call the api matching detection method.
                    if (idxItr != endItr) {
                        apiMatchDetecting(callInstItr, idxItr);
                        callInstItr = idxItr;
                    } else {
                        apiMatchDetecting(callInstItr+1, endItr);
                    }
                }
                ++callInstItr;
            }
        } else {
            errs()<< "The member callInstVect is empty!\n";
        }
    }

    void apiMatchDetecting(std::vector<CallInst*>::iterator beginItr, std::vector<CallInst*>::iterator endItr) {
        CallInst* callInst = nullptr;
        Function* calledFunc = nullptr;
        Value* arguVar = nullptr;
        Value* lastArguVar = nullptr;
        std::string funcName;
        while (beginItr != End) {
            callInst = *beginItr;
            calledFunc = callInst->getCalledFunction();
            funcName = calledFunc->getName();
            arguVar = getRealMemVar(callInst);
            if (arguVar != nullptr) {
                // TO-DO: accomplish the rest part of the designing 
                std::map<Value*, std::string>::iterator arguMapItr = records.find(arguVar);
                if (arguMapItr != records.end()) {
                    if (arguMapItr->second == "cudaMalloc") {
                        if (funcName == "cudaMalloc") {
                            errs()<< "Successive cudaMalloc() calling, be cautious!!!\n";
                        } else if (funcName == "cudaFree") {
                            records.erase(arguVar);
                            errs()<< "Found a malloc-free pair, and this record is removed.\n";
                        }
                    } else if (arguMapItr->second == "cudaFree") {
                        if (funcName == "cudaMalloc") {
                            records[arguVar] = "cudaMalloc";
                            errs()<< "Start a new matching circle with cudaMalloc().\n";
                        } else if (funcName == "cudaFree") {
                            errs()<< "Repeat calling the cudaFree() API.\n";
                        }
                    } else if (arguMapItr->second == records.end()) {
                        if (funcName == "cudaMalloc") {
                            records.insert(make_pair(arguVar, funcName));
                            errs()<< "Added a new record of cudaMalloc() into the map.\n";
                        } else if (funcName == "cudaFree") {
                            errs()<< "Warning, the code try to free an unallocated variable with cudaFree().\n";
                        }
                    } else {
                        errs()<< "Unknown error occured in the apiMatchDetecting() method...\n";
                    }
                }
            } else {
                errs()<< "The CallInst contains invalid argument: " << *callInst << "\n";
            }
            ++beginItr;
        }
    }
    
    virtual bool runOnFunction(Function &F) {
        // this pass module is prepared for host codes detection
        if ((F.getParent())->getTargetTriple().compare("x86_64-unknown-linux-gnu") == 0) {
            errs()<< "Enter the host function: " << F.getName() << "\n";
            CallInst* callInst = nullptr;
            Function* calledFunc = nullptr;
            std::string calledFuncName;
            for (Function::iterator BBItr = F.begin(), EndBB = F.end(); BBItr != EndBB; BBItr++) {
                for (BasicBlock::iterator IRItr = (*BBItr).begin(), EndIR = (*BBItr).end(); IRItr != EndIR; IRItr++) {
                    // errs()<< "Inst: " << (*IRItr) << "\n";
                    // if (isa<CallInst>(&(*IRItr))) {
                    if (callInst = dyn_cast<CallInst>(&(*IRItr))) {
                        errs()<< "Find a CallInst: " << *callInst << "\n";
                        calledFunc = callInst->getCalledFunction();
                        if (calledFunc != nullptr) {
                            calledFuncName = calledFunc->getName();
                            errs()<< "Called function name: " << calledFuncName << "\n";
                            if (calledFuncName=="cudaMalloc" || 
                                calledFuncName=="cudaFree" ||
                                calledFuncName=="cudaDeviceReset") {
                                callInstVect.push_back(callInst);
                            }
                        }
                        
                    }
                }
            }
            //TO-DO: realize the method to detect the matching API calling inst
            // 1. cudaDeviceReset detection.
            for (std::vector<CallInst*>::iterator iter=callInstVect.begin(); iter != callInstVect.end(); iter++) {
                callInst = *iter;
                calledFunc = callInst->getCalledFunction();
                calledFuncName = calledFunc->getName();
                if (calledFuncName == "cudaDeviceReset") {  // match the cudaDeviceReset() calling
                    while (iter != callInstVect.end()) {    // to filter the reset part

                    }
                }
            }
            // 2. malloc-free matching detection.
            
            printCallInstVector();
        }
        return false;
    }
};
}

char APIMatchDetection::ID = 0;
static RegisterPass<APIMatchDetection> APIMD("APIMatchDetection", 
                                            "The GPU Memory util API matching detection pass module.", 
                                            false, false);
static llvm::RegisterStandardPasses APIMDY2(PassManagerBuilder::EP_EarlyAsPossible,
                                      [](const PassManagerBuilder &Builder,
                                      legacy::PassManagerBase &PM) { PM.add(new APIMatchDetection()); });