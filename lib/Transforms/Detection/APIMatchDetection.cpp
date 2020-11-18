#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/LLVMContext.h"
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
        // LLVMContext curContext = curBB->getParent()->getParent()->getContext();
        Value* arg0 = Inst->getArgOperand(0);
        BitCastInst* bcInst = nullptr;
        // PointerType* i8_ptr = PointerType::getInt8PtrTy(curContext);
        // PointerType* i8_pptr = PointerType::get(i8_ptr, 0);
        if (arg0 != nullptr) {
            if ((bcInst = dyn_cast<BitCastInst>(arg0)) != nullptr) {
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
        BasicBlock* curBB = Inst->getParent();
        Value* arg0 = Inst->getArgOperand(0);
        BitCastInst* bcInst = nullptr;
        LoadInst* ldInst = nullptr;
        if (arg0 != nullptr) {
            if ((bcInst = dyn_cast<BitCastInst>(arg0)) != nullptr) {
                BasicBlock::iterator instItr = curBB->begin();
                BasicBlock::const_iterator End = curBB->end();
                while (instItr != End) {
                    if ((&(*instItr)) == bcInst) { // find the target bitcast inst here
                        // withdraw the target argument of bitcast
                        // return bcInst->getOperand(0);
                        Value* valVar = bcInst->getOperand(0);
                        // start to find the target LoadInst for the value variable
                        if ((ldInst = dyn_cast<LoadInst>(valVar)) != nullptr) {
                            return ldInst->getOperand(0);
                        }
                    }
                    ++instItr;
                }
            }
        }
        return nullptr;
    }

    Value* getCudaMallocHostArguVar(CallInst* Inst) {
        BasicBlock* curBB = Inst->getParent();
        // LLVMContext curContext = curBB->getParent()->getParent()->getContext();
        Value* arg0 = Inst->getArgOperand(0);
        BitCastInst* bcInst = nullptr;
        // PointerType* i8_ptr = PointerType::getInt8PtrTy(curContext);
        // PointerType* i8_pptr = PointerType::get(i8_ptr, 0);

        if (arg0 != nullptr) {
            if ((bcInst = dyn_cast<BitCastInst>(arg0)) != nullptr) {
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

    Value* getCudaFreeHostArguVar(CallInst* Inst) {
        BasicBlock* curBB = Inst->getParent();
        Value* arg0 = Inst->getArgOperand(0);
        BitCastInst* bcInst = nullptr;
        LoadInst* ldInst = nullptr;
        if (arg0 != nullptr) {
            if ((bcInst = dyn_cast<BitCastInst>(arg0)) != nullptr) {
                BasicBlock::iterator instItr = curBB->begin();
                BasicBlock::const_iterator End = curBB->end();
                while (instItr != End) {
                    if ((&(*instItr)) == bcInst) { // find the target bitcast inst here
                        // withdraw the target argument of bitcast
                        // return bcInst->getOperand(0);
                        Value* valVar = bcInst->getOperand(0);
                        // start to find the target LoadInst for the value variable
                        if ((ldInst = dyn_cast<LoadInst>(valVar)) != nullptr) {
                            return ldInst->getOperand(0);
                        }
                    }
                    ++instItr;
                }
            }
        }
        return nullptr;
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
        } else if (funcName == "cudaMallocHost") {
            return getCudaMallocHostArguVar(Inst);
        } else if (funcName == "cudaFreeHost") {
            return getCudaFreeHostArguVar(Inst);
        } else {
            errs()<< "In the getRealMemVar() method, we met a new function named "
                    << funcName << " .\n";
            return nullptr;
        }
    }

    int cudaDeviceResetDetecting(void) {
        if (!callInstVect.empty()) {
            std::vector<CallInst*>::iterator callInstItr = callInstVect.begin();
            std::vector<CallInst*>::iterator endItr = callInstVect.end();
            std::vector<CallInst*>::iterator idxItr;
            CallInst* callInst = nullptr;
            std::string funcName;
            CallInst* idxCallInst = nullptr;
            std::string idxFuncName;
            while (callInstItr != endItr) {
                callInst = *callInstItr;
                funcName = callInst->getCalledFunction()->getName();
                if (funcName == "cudaDeviceReset") {
                    errs()<< "Find the cudaDeviceReset() calling in the codes.\n";
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
                        errs()<< "Find one more cudaDeviceReset() calling in the CallInst vector.\n";
                        errs()<< "Start the API matching detection process...\n";
                        if (apiMatchDetecting(callInstItr+1, idxItr))
                            callInstItr = idxItr;
                        else {
                            errs()<< "WARNING, the CallInsts after cudaDeviceReset() calling mismatched!\n";
                            return 0;
                        }
                    } else {
                        errs()<< "In the cudaDeviceReset detection, we reach the end of the CallInst vector.\n";
                        errs()<< "Check the rest part of the CallInst vector with API matching method...\n";
                        if (apiMatchDetecting(callInstItr+1, endItr))
                            return 1;
                        else {
                            errs()<< "WARNING, the CallInsts after cudaDeviceReset() calling mismatched!\n";
                            return 0;
                        }
                    }
                }
                ++callInstItr;
            }
        } else {
            errs()<< "The Pass Module APIMatchDetection member callInstVect is empty!\n";
            return -1;
        }
        return 0;
    }

    bool apiMatchDetecting(std::vector<CallInst*>::iterator beginItr, std::vector<CallInst*>::iterator endItr) {
        CallInst* callInst = nullptr;
        Function* calledFunc = nullptr;
        Value* arguVar = nullptr;
        std::string funcName;
        records.clear();
        while (beginItr != endItr) {
            callInst = *beginItr;
            calledFunc = callInst->getCalledFunction();
            funcName = calledFunc->getName();
            if (funcName == "cudaDeviceReset") {
                ++beginItr;
                continue;
            }

            arguVar = getRealMemVar(callInst);
            if (arguVar != nullptr) {
                // TO-DO: accomplish the rest part of the designing 
                std::map<Value*, std::string>::iterator arguMapItr = records.find(arguVar);
                if (arguMapItr != records.end()) {
                    if (arguMapItr->second == "cudaMalloc") {
                        if (funcName == "cudaMalloc") {
                            errs()<< "Successive cudaMalloc() calling, WARNING!!!\n";
                            return false;
                        } else if (funcName == "cudaFree") {
                            records.erase(arguVar);
                            errs()<< "Found a malloc-free pair, and this record is removed.\n";
                        }
                    } else if (arguMapItr->second == "cudaFree") {
                        if (funcName == "cudaMalloc") {
                            records[arguVar] = "cudaMalloc";
                            errs()<< "Start a new matching circle with cudaMalloc().\n";
                        } else if (funcName == "cudaFree") {
                            errs()<< "Repeat calling the cudaFree() API, WARNING!!!\n";
                            return false;
                        }
                    } else if (arguMapItr->second == "cudaMallocHost") {
                        if (funcName == "cudaMallocHost") {
                            errs()<< "Successive cudaMallocHost() calling, WARNING!!!\n";
                            return false;
                        } else if (funcName == "cudaFreeHost") {
                            records.erase(arguVar);
                            errs()<< "Found a malloc-free pair, and this record is removed.\n";
                        }
                    } else if (arguMapItr->second == "cudaFreeHost") {
                        if (funcName == "cudaMallocHost") {
                            records[arguVar] = "cudaMallocHost";
                            errs()<< "Start a new matching circle with cudaMallocHost().\n";
                        } else if (funcName == "cudaFreeHost") {
                            errs()<< "Repeat calling the cudaFreeHost() API, WARNING!!!\n";
                            return false;
                        }
                    } else {
                        errs()<< "Unknown error occured in the apiMatchDetecting() method...\n";
                    }
                } else {
                    if (funcName == "cudaMalloc") {
                        records.insert(make_pair(arguVar, funcName));
                        errs()<< "Added a new record of cudaMalloc() into the map.\n";
                    } else if (funcName == "cudaFree") {
                        errs()<< "Warning, the code try to free an unallocated variable with cudaFree().\n";
                        return false;
                    } else if (funcName == "cudaMallocHost") {
                        records.insert(make_pair(arguVar, funcName));
                        errs()<< "Added a new record of cudaMallocHost() into the map.\n";
                    } else if (funcName == "cudaFreeHost") {
                        errs()<< "Warning, the code try to free an unallocated variable with cudaFreeHost().\n";
                        return false;
                    }
                }
            } else {
                errs()<< "The CallInst contains invalid argument: " << *callInst << "\n";
                return false;
            }
            ++beginItr;
        }
        return true;
    }
    
    int InitCallInstVector(Function &F) {
        CallInst* callInst = nullptr;
        Function* calledFunc = nullptr;
        std::string calledFuncName;
        for (Function::iterator BBItr = F.begin(), EndBB = F.end(); BBItr != EndBB; BBItr++) {
            for (BasicBlock::iterator IRItr = (*BBItr).begin(), EndIR = (*BBItr).end(); IRItr != EndIR; IRItr++) {
                if ((callInst = dyn_cast<CallInst>(&(*IRItr))) != nullptr) {
                    // errs()<< "Find a CallInst: " << *callInst << "\n";
                    calledFunc = callInst->getCalledFunction();
                    if (calledFunc != nullptr) {
                        calledFuncName = calledFunc->getName();
                        // errs()<< "Called function name: " << calledFuncName << "\n";
                        if (calledFuncName=="cudaMalloc" || 
                            calledFuncName=="cudaFree" ||
                            calledFuncName=="cudaMallocHost" ||
                            calledFuncName=="cudaFreeHost" ||
                            calledFuncName=="cudaDeviceReset") {
                            callInstVect.push_back(callInst);
                        }
                    }
                }
            }
        }
        if (callInstVect.empty())
            return 0;
        else
            return 1;
    }
    
    virtual bool runOnFunction(Function &F) {
        // this pass module is prepared for host codes detection
        if ((F.getParent())->getTargetTriple().compare("x86_64-unknown-linux-gnu") == 0) {
            errs()<< "Enter the host function: " << F.getName() << "\n";
            // Initialize the class member: callInstVect
            InitCallInstVector(F);
            // 1. cudaDeviceReset detection.
            errs()<< "Start the cudaDeviceReset() detecting...\n";
            errs()<< "=====------- cudaDeviceResetDetecting Detection Report -------=====\n";
            switch (cudaDeviceResetDetecting())
            {
            case -1:
                errs()<< "No CallInst exists in the src codes....STATUS -- Safe.\n";
                break;
            case 0:
                errs()<< "WARNING: mismatch happened after the cudaDeviceReset() calling instruction...STATUS -- Unsafe\n";
                break;
            case 1:
                errs()<< "The CallInsts seem working well under current detecting rules...STATUS -- Safe.\n";
                break;
            default:
                break;
            }
            errs()<< "=====----------------------- End ---------------------------=====\n\n";
            // 2. malloc-free matching detection.
            errs()<< "Start the API match detecting on the whole code area.\n";
            errs()<< "=====--------------API match Detection Report --------------=====\n";
            if (apiMatchDetecting(callInstVect.begin(), callInstVect.end())) {
                errs()<< "The CallInsts seem working well under current detecting rules...STATUS -- Safe.\n";
            } else {
                errs()<< "WARNING: mismatch happened in the current function...STATUS -- Unsafe\n";
            }
            errs()<< "=====----------------------- End ---------------------------=====\n\n";
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