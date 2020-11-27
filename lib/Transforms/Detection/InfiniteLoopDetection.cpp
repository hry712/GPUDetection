#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <map>


using namespace llvm;

namespace {
struct InfiniteLoopDetection : public FunctionPass {
    static char ID;
    InfiniteLoopDetection() : FunctionPass(ID) {}

    Value* IndVarLimit = nullptr;

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
        AU.setPreservesCFG();
        AU.addRequired<LoopInfoWrapperPass>();
        AU.addRequired<TargetLibraryInfoWrapperPass>();
    }

    int getLoopType(Loop* LP) {
        BasicBlock* headerBB = LP->getHeader();
        BasicBlock* exitBB = LP->getExitBlock();
        BasicBlock* latchBB = LP->getLoopLatch();
        // for(){...}, while(){...}
        if (headerBB == exitBB) {
            errs()<< "In getLoopType() method, we judge current LoopObj is for/while type.\n";
            return 0;
        }
        // do{...} while()
        if (latchBB == exitBB) {
            errs()<< "In getLoopType() method, we judge current LoopObj is do...while type.\n";
            return 1;
        }
        return -1;
    }

    Value* getIndVarFromHS(Value* lhs, Value* rhs) {
        // check which one is the variable and another one is a constant value
        // Supposing that one of the operands is of constant type
        ConstantInt* constIntVar = nullptr;
        ConstantFP* constFpVar = nullptr;
        IndVarLimit = nullptr;
        // check the left operand first
        if ((constIntVar=dyn_cast<ConstantInt>(lhs)) != nullptr ||
            (constFpVar=dyn_cast<ConstantFP>(lhs)) != nullptr) {
            if ((constIntVar=dyn_cast<ConstantInt>(rhs)) == nullptr ) {
                IndVarLimit = lhs;
                return rhs;
            }
            errs()<< "WARNING: In method getIndVarFromHS(), both operands are judged as constant value.\n";
        } else if ((constIntVar=dyn_cast<ConstantInt>)(rhs) != nullptr ||
            (constFpVar=dyn_cast<ConstantFP>)(rhs) != nullptr) {
            if ((constIntVar=dyn_cast<ConstantInt>(lhs)) == nullptr ) {
                IndVarLimit = rhs;
                return lhs;
            }
            errs()<< "WARNING: In method getIndVarFromHS(), both operands are judged as constant value.\n";
        } else {
            errs()<< "WARNING: both operands in the BR condition area are variables, whick is hard to detect the induction variable.\n";
        }
        return nullptr;
    }

    Value* getCondVarFromBrInst(BranchInst* BR) {
        Value* cond = brInst->getCondition();
        if (cond != nullptr) {
            unsigned opcode = cond->getOpcode();
            Value* lhs = nullptr;
            Value* rhs = nullptr;
            if (opcode == Instruction::ICMP) {
                ICmpInst* icmpInst = dyn_cast<ICmpInst>(cond);
                lhs = icmpInst->getOperand(0);
                rhs = icmpInst->getOperand(1);
                return getIndVarFromHS(lhs, rhs);
            } else if (opcode == Instruction::FCMP) {
                FCmpInst* fcmpInst = dyn_cast<FCmpInst>(cond);
                lhs = icmpInst->getOperand(0);
                rhs = icmpInst->getOperand(1);
                return getIndVarFromHS(lhs, rhs);
            } else if ((auto* CI=dyn_cast<ConstantInt>(cond)) != nullptr) { // TO-DO: check if the cond part is a constant value
                errs()<< "WARNING: In getCondVarFromBrInst() method, the condition part of BR inst is a constant value and shouldn't be handled in getForOrWhileInductionVar() method.\n";
                return nullptr;
            }
        } else {
            errs()<< "In getCondVarFromBrInst() method, the BR inst does not have a valid condition part in the LoopBB.\n";
            return nullptr;
        }
        return nullptr;
        
    }

    Value* getForOrWhileInductionVar(Loop* LP) {
        BasicBlock* headerBB = LP->getHeader();
        BasicBlock* latchBB = LP->getLoopLatch();
        BasicBlock* exitBB = LP->getExitBlock();
        if (headerBB == exitBB) {
            Instruction* termInst = headerBB->getTerminator();
            BranchInst* brInst = nullptr;
            if ((brInst=dyn_cast<BranchInst>(termInst)) != nullptr) {
                return getCondVarFromBrInst(brInst);
            }
        } else {
            errs()<< "WARNING: In function getForOrWhileInductionVar(), the header BB does not match the exit BB.\n";
            errs()<< "--Maybe the getLoopTye() method returned a wrong value for the current Loop.\n";
        }
        return nullptr;
    }

    Value* getDoWhileInductionVar(Loop* LP) {
        BasicBlock* headerBB = LP->getHeader();
        BasicBlock* latchBB = LP->getLoopLatch();
        BasicBlock* exitBB = LP->getExitBlock();
        if (latchBB == exitBB) {
            Instruction* termInst = latchBB->getTerminator();
            BranchInst* brInst = nullptr;
            if ((brInst=dyn_cast<BranchInst>(termInst)) != nullptr) {
                return getCondVarFromBrInst(brInst);
            }
        } else {
            errs()<< "WARNING: In function getDoWhileInductionVar(), the latch BB does not match the exit BB.\n";
            errs()<< "--Maybe the getLoopTye() method returned a wrong value for the current Loop.\n";
        }
        return nullptr;
    }

    Value* getInductionVarFrom(Loop* LP, int Type) {
        if (Type == 0) {
            return getForOrWhileInductionVar(LP);
        } else if (Type == 1) {
            return getDoWhileInductionVar(LP);
        }
        return nullptr;
    }

    // If the IndVar is used in the arithmetic instruction, this method will deem that
    // the loop has setted a valid IndVar to reach an end.
    bool checkBasicArithmetic(Instruction* Inst, Value* IndVar) {
        Value* lhs = nullptr;
        Value* rhs = nullptr;
        ConstantInt* stepIntLen = nullptr;
        ConstantFP* stepFPLen = nullptr;
        if (Inst != nullptr) {
            if (Inst->isBinaryOp()) {
                unsigned opcode = Inst->getOpcode();
                // Binary Int Opcode
                // check if the operands contains the IndVar and a constant value
                if (opcode == Instruction::Add ||
                    opcode == Instruction::Sub ||
                    opcode == Instruction::Mul ||
                    opcode == Instruction::UDiv ||
                    opcode == Instruction::SDiv) {
                    lhs = Inst->getOperand(0);
                    rhs = Inst->getOperand(1);
                    if (lhs==IndVar && (stepIntLen=dyn_cast<ConstantInt>(rhs)!=nullptr)) {
                        return true;
                    }
                    if (rhs==IndVar && (stepIntLen=dyn_cast<ConstantInt>(lhs)!=nullptr)) {
                        return true;
                    }
                }
                // Binary Float Opcode
                if (opcode == Instruction::FAdd ||
                    opcode == Instruction::FSub ||
                    opcode == Instruction::FMul ||
                    opcode == Instruction::FDiv) {
                    lhs = Inst->getOperand(0);
                    rhs = Inst->getOperand(1);
                    // errs()<< "In checkBasicArithmetic() method, the target "
                    if (lhs==IndVar && (stepFPLen=dyn_cast<ConstantFP>(rhs)!=nullptr)) {
                        return true;
                    }
                    if (rhs==IndVar && (stepFPLen=dyn_cast<ConstantFP>(lhs)!=nullptr)) {
                        return true;
                    }
                }
            } else {
                return false;
            }
        }
        errs()<< "WARNING: In checkBasicArithmetic() method, the argu Inst is tested with NULL value.\n";
        return false;
    }

    bool isChangedByLP(Loop* LP, Value* IndVar) {
        if (LP != nullptr && IndVar != nullptr) {
            std::vector<BasicBlock*> BBs = LP->getBlocksVector();
            std::vector<BasicBlock*>::iterator bbItr = BBs.begin();
            std::vector<BasicBlock*>::iterator endItr = BBs.end();
            BasicBlock::iterator iiItr;
            BasicBlock::iterator ieItr;
            BasicBlock* bb = nullptr;
            while (bbItr != endItr) {
                bb = *bbItr;
                iiItr = bb->begin();
                ieItr = bb->end();
                while (iiItr != ieItr) {
                    if (checkBasicArithmetic(*iiItr, IndVar))
                        return true;
                    ++iiItr;
                }
                ++bbItr;
            }
        }
        errs()<< "WARNING: In isChangedByLP() method, the NULL value exists in the argus.\n";
        return false;
    }

    virtual bool runOnFunction(Function &F) {
        Loop* LP = nullptr;
        PHINode* indctVar = nullptr;
        if ((F.getParent())->getTargetTriple().compare("nvptx64-nvidia-cuda") == 0) {
            errs()<< "Now, this pass is dealing with a GPU kernel module...\n";
            LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
            if (LI.empty()) {
                errs()<< "No Loop Structure exists in the function: " << F.getName() << "\n\n";
            } else {
                errs()<< "Start analyzing the LoopInfo obj in the Function: " << F.getName() << "\n";
                errs()<< "LoopInfo obj content:\n";
                LI.print(errs());
                errs()<< "\n";
                for (auto* lp : LI) {
                    int lpTy = getLoopType(lp);
                    Value* lpIndVar = getInductionVarFrom(lp. lpTy);
                    if (lpIndVar != nullptr) {
                        errs()<< "Found induction variable in the loop: " << *lpIndVar << "\n";
                        errs()<< "=====-----------Infinite Loop Check Report-----------=====\n";
                        if (isChangedByLP(lp, lpIndVar)) {
                            //TO-DO: Print out the safety detection report
                            errs()<< "Under current checking strategies, this loop is considered as a finite type ----- SAFE\n";
                        } else {
                            errs()<< "Under current checking strategies, this loop is considered as an infinite type ----- UNSAFE\n";
                        }
                        errs()<< "=====-----------------------End----------------------=====\n";
                    } else {
                        errs()<< "WARNING: Fail to fetch the induction variable from the current Loop.\n";
                    }
                }
                errs()<< "\n";
            }
        }
        return false;
    }
};
}

char InfiniteLoopDetection::ID = 0;
static RegisterPass<InfiniteLoopDetection> ILD1("InfiniteLoopDetection", "GPU kernel infinite loop detection Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);
static llvm::RegisterStandardPasses ILDY2(PassManagerBuilder::EP_EarlyAsPossible,
        [](const PassManagerBuilder &Builder,
        legacy::PassManagerBase &PM) { PM.add(new InfiniteLoopDetection()); });