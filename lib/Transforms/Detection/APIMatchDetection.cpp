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

using namespace llvm;
using namespace std;

namespace {
struct APIMatchDetection : public FunctionPass {
    static char ID;
    APIMatchDetection() : FunctionPass(ID) {}

    Value* rawPtrVar = nullptr;
    std::vector<Value*> callInstVect;
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
        errs()<< "Finish printing.\n\n";
    }

    Value* getCudaMallocRetVar(CallInst* Inst) {
        BasicBlock* curBB = Inst->getParent();
        User::op_iterator argItr = Inst->arg_begin();
        Value* firstArgVar = &(*argItr);
        if (isa<BitCastInst>(firstArgVar)) {
            if (firstArgVar != nullptr) {
                /// parsing the current BB to find out the bitcast inst
                BasicBlock::const_iterator instItr = curBB->begin();
                BasicBlock::const_iterator End = curBB->end();
                BitCastInst* tmp = nullptr;
                while (&(*instItr) != Inst) {
                    if (tmp = dyn_cast<BitCastInst>(&(*instItr))) {
                        if (tmp == firstArgVar) {
                            Value* firstOpd = tmp->getOperand(0);
                            if (firstOpd != nullptr)
                                return firstOpd;
                            else
                                return nullptr;
                        }
                    }
                    ++instItr;
                }
            } else {
                return nullptr;
            }
        } else {
            return firstArgVar;
        }
    }

    Value* getCudaFreeArguVar(CallInst* Inst) {
        User::op_iterator argItr = Inst->arg_begin();
        Value* firstArgVar = &(*argItr);
        if (firstArgVar != nullptr)
            return firstArgVar;
        else
            return nullptr;
    }

    /// This method returns the CallInst's first argu var or ret recording var to 
    /// withdraw the key content for records hashmap.
    Value* getRealMemVar(CallInst* Inst) {

    }

    void apiMatchDetecting(std::vector<Value*>::const_iterator InstItr, std::vector<Value*>::const_iterator End) {
        int flag = 0;
        CallInst* tmp = nullptr;
        Function* calledFunc = nullptr;
        Value* tmpArguVar = nullptr;
        Value* lastArguVar = nullptr;
        std::string funcName;
        while (InstItr != End) {
            tmp = dyn_cast<CallInst>(&(*InstItr));
            calledFunc = tmp->getCalledFunction();
            funcName = calledFunc->getName();
            if (flag == 0) {
                if (funcName == "cudaMalloc") {
                    tmpArguVar = calledFunc->
                }
                flag = 1;
            } else if (flag == 1) {
                if (funcName == "cudaFree") {

                }
            }
            ++InstItr;
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
                                callInstVect.push_back(&(*IRItr));
                            }
                        }
                        
                    }
                    // if (callInstPtr = dyn_cast<CallInst> (IRItr)) {
                        // 1. identify the callee's function name
                        // 2. get the callee's first argu
                        // calledFunc = callInstPtr->getCalledFunction();
                        // calledFuncName = calledFunc->getName().str();
                        // if (calledFuncName == "cudaMalloc") {
                            // 1. first check the called function whether exists in the records.
                            //    and the token is the first argument value
                            // firstArgu = dyn_cast<ConstantInt>(calledFunc->arg_begin());
                            // 2. Try to find out the according bitcast raw variable behind first argu through use
                            // firstArgu->uses();
                            // errs() << "The first argu content is: " << *firstArgu << "\n";
                            // for (auto tmpU : firstArgu->users()) {
                                // if (Instruction* tmpI = dyn_cast<Instruction>(tmpU)) {
                                //     errs() << "The user of the first argu: " << *tmpI << "\n";
                                // }
                            // }
                            // realArguVal = 
                        // } else if (calledFuncName == "cudaFree") {

                        // }
                    // }
                }
            }
            //TO-DO: realize the method to detect the matching API calling inst
            // 1. cudaDeviceReset detection.
            for (std::vector<Value*>::const_iterator iter=callInstVect.cbegin(); iter != callInstVect.end(); iter++) {
                callInst = dyn_cast<CallInst>(iter);
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

    // virtual bool doFinalization() {

    //     return false;
    // }
};
}

char APIMatchDetection::ID = 0;
static RegisterPass<APIMatchDetection> APIMD("APIMatchDetection", 
                                            "The GPU Memory util API matching detection pass module.", 
                                            false, false);
static llvm::RegisterStandardPasses APIMDY2(PassManagerBuilder::EP_EarlyAsPossible,
                                      [](const PassManagerBuilder &Builder,
                                      legacy::PassManagerBase &PM) { PM.add(new APIMatchDetection()); });