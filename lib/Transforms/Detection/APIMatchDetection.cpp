#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Use.h"
#include "llvm/Pass.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;

namespace {
struct APIMatchDetection : public FunctionPass {
    static char ID;
    APIMatchDetection() : FunctionPass(ID) {}

    Value* rawPtrVar = nullptr;

    Value* getRawVarValue(Function &F, Value* FirstArgu) {
        if (BitCastInst* bitcastInst = dyn_cast<BitCastInst>(FirstArgu)) {
            return bitcastInst;
        }
        return nullptr;
    }
    
    virtual bool runOnFunction(Function &F) {
        // unsigned opcode = 0;
        CallInst* callInstPtr = nullptr;
        Function* calledFunc = nullptr;
        std::string calledFuncName;
        Value* firstArgu = nullptr;
        Value* realArguVal = nullptr;
        for (Function::iterator BBItr = F.begin(), EndBB = F.end(); BBItr != EndBB; BBItr++) {
            for (BasicBlock::iterator IRItr = (*BBItr).begin(), EndIR = (*BBItr).end(); IRItr != EndIR; IRItr++) {
                if (callInstPtr = dyn_cast<CallInst> (IRItr)) {
                    // 1. identify the callee's function name
                    // 2. get the callee's first argu
                    calledFunc = callInstPtr->getCalledFunction();
                    calledFuncName = calledFunc->getName().str();
                    if (calledFuncName == "cudaMalloc") {
                        // 1. first check the called function whether exists in the records.
                        //    and the token is the first argument value
                        firstArgu = dyn_cast<ConstantInt>(calledFunc->arg_begin());
                        // 2. Try to find out the according bitcast raw variable behind first argu through use
                        // firstArgu->uses();
                        errs() << "The first argu content is: " << *firstArgu << "\n";
                        for (auto tmpU : firstArgu->users()) {
                            if (Instruction* tmpI = dyn_cast<Instruction>(tmpU)) {
                                errs() << "The user of the first argu: " << *tmpI << "\n";
                            }
                        }
                        // realArguVal = 
                    } else if (calledFuncName == "cudaFree") {

                    }
                }
            }
        }
        return false;
    }
};
}

char APIMatchDetection::ID = 0;
static RegisterPass<APIMatchDetection> DT3("APIMatchDetection", "The GPU Memory util API matching detection pass module.", false, false);
// static llvm::RegisterStandardPasses Y(llvm::PassManagerBuilder::EP_EarlyAsPossible,
//                                       [](const llvm::PassManagerBuilder &Builder,
//                                       llvm::legacy::PassManagerBase &PM) { PM.add(new APIMatchDetection()); });