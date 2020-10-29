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

using namespace llvm;
using namespace std;

namespace {
struct APIMatchDetection : public FunctionPass {
    static char ID;
    APIMatchDetection() : FunctionPass(ID) {}

    Value* rawPtrVar = nullptr;
    std::vector<Value*> callInstVect;

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
        errs()<< "Finish printing.\n";
    }
    
    virtual bool runOnFunction(Function &F) {
        // this pass module is prepared for host codes detection
        if ((F.getParent())->getTargetTriple().compare("x86_64-unknown-linux-gnu") == 0) {
            // unsigned opcode = 0;
            // CallInst* callInstPtr = nullptr;
            // Function* calledFunc = nullptr;
            // std::string calledFuncName;
            // Value* firstArgu = nullptr;
            // Value* realArguVal = nullptr;
            for (Function::iterator BBItr = F.begin(), EndBB = F.end(); BBItr != EndBB; BBItr++) {
                for (BasicBlock::iterator IRItr = (*BBItr).begin(), EndIR = (*BBItr).end(); IRItr != EndIR; IRItr++) {
                    if (isa<CallInst>(&(*IRItr))) {
                        CallInst* callInst = dyn_cast<CallInst>(&(*IRItr));
                        Function* calledFunc = callInst->getCalledFunction();
                        std::string calledFuncName = calledFunc->getName();
                        if (calledFuncName=="cudaMalloc" || 
                            calledFuncName=="cudaFree" ||
                            calledFuncName=="cudaDeviceReset") {
                            callInstVect.push_back(&(*IRItr));
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