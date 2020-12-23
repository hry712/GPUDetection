#include "llvm/Pass.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <map>
#include <vector>
#include <algorithm>

using namespace llvm;

namespace {
struct InfiniteLoopDetection : public FunctionPass {
    static char ID;
    InfiniteLoopDetection() : FunctionPass(ID) {}

    int IndVarLimit = 0;
    int mIndVarLoadLayers = 0;
    std::vector<Value*> mEntryAllocaInstVector;
    std::vector<Value*> mLoadInstVector;
    Function* curFunc;

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
        AU.setPreservesCFG();
        AU.addRequired<LoopInfoWrapperPass>();
        AU.addRequired<TargetLibraryInfoWrapperPass>();
    }

    void InitAllocaInstVector(BasicBlock* EntryBB) {
        errs()<< "DEBUG INFO: Enter the InitAllocaInstVector() method...\n";
        mIndVarLoadLayers = 0;
        if (EntryBB != nullptr) {
            BasicBlock::iterator iiItr = EntryBB->begin();
            BasicBlock::iterator endItr = EntryBB->end();
            unsigned opcode = -1;
            mEntryAllocaInstVector.clear();
            while (iiItr != endItr) {
                opcode = (*iiItr).getOpcode();
                if (opcode == Instruction::Alloca) {
                    mEntryAllocaInstVector.push_back(&(*iiItr));
                }
                ++iiItr;
            }
        } else {
            errs()<< "WARNING: In InitAllocaInstVector() method, the argu EntryBB is NULL value.\n";
        }
    }
    
    Value* getInnerMostLoadOpnd(Instruction* LD, BasicBlock* ContainerBB) {
        errs()<< "DEBUG INFO: Enter the getInnerMostLoadOpnd() method...\n";
        Instruction* tmpInst = nullptr;
        std::vector<Value*>::iterator allocaInstItr;
        allocaInstItr = find(mEntryAllocaInstVector.begin(), mEntryAllocaInstVector.end(), LD->getOperand(0));
        if (allocaInstItr != mEntryAllocaInstVector.end()) {
            mIndVarLoadLayers += 1;
            return LD->getOperand(0);
        } else if ((tmpInst=dyn_cast<LoadInst>(LD->getOperand(0))) != nullptr) {
            if (tmpInst->getParent() == ContainerBB) {
                mIndVarLoadLayers += 1;
                allocaInstItr = find(mEntryAllocaInstVector.begin(), mEntryAllocaInstVector.end(), tmpInst->getOperand(0));
                if (allocaInstItr == mEntryAllocaInstVector.end()) {
                    return getInnerMostLoadOpnd(tmpInst, ContainerBB);
                } else {
                    return tmpInst->getOperand(0);
                }
            } else {
                errs()<< "WARNING: In getInnerMostLoadOpnd() method, unknown loop type may be found here...\n";
            }
        } else {
            errs()<< "WARNING: In getInnerMostLoadOpnd() method, the Inst from argu list is a NULL ptr.\n";
        }
        return nullptr;
    }

    int getLoopType(Loop* LP) {
        errs()<<"DEBUG INFO: Enter the getLoopType() method...\n";
        BasicBlock* headerBB = LP->getHeader();
        BasicBlock* exitBB = LP->getExitingBlock();
        // std::vector<BasicBlock*> exitBBs = LP->getExitBlocks();
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
        errs()<<"DEBUG INFO: In getLoopType() method, the header, latch, exiting BBs do not match the detection strategies.\n";
        errs()<<"DEBUG INFO: In getLoopType() method, print the processed Loop obj info as following...\n" << *LP << "\n";
        errs()<<"DEBUG INFO: In getLoopType() method, print the header as following...\n" << *headerBB << "\n";
        errs()<<"DEBUG INFO: In getLoopType() method, print the latch as following...\n" << *latchBB << "\n";
        errs()<<"DEBUG INFO: In getLoopType() method, print the exiting as following...\n" << *exitBB << "\n";
        return -1;
    }

    int getValueFromConstInt(ConstantInt* CI) {
        if (CI->getBitWidth() <= 32) {
            return CI->getSExtValue();
        }
        return -10086;
    }

    // check which one is the variable and another one is a constant value
    // Supposing that one of the operands is of constant type
    Value* getIndVarFromHS(Value* lhs, Value* rhs) {
        errs()<<"DEBUG INFO: Enter the getIndVarFromHS() method...\n";
        ConstantInt* constIntVar = nullptr;
        ConstantFP* constFpVar = nullptr;
        IntegerType* i32 = IntegerType::get(curFunc->getParent()->getContext(), 32);
        IntegerType* i64 = IntegerType::get(curFunc->getParent()->getContext(), 64);
        Type* flt =  Type::getFloatTy(curFunc->getParent()->getContext());
        Type* dbl =  Type::getDoubleTy(curFunc->getParent()->getContext());
        Instruction* tmpInst = nullptr;
        
        // check the left operand first
        // Binary Int Opnd
        if ((constIntVar=dyn_cast<ConstantInt>(lhs)) != nullptr) {         // suppose the left opnd is a constant value
                IndVarLimit = getValueFromConstInt(constIntVar);
            if ((constIntVar=dyn_cast<ConstantInt>(rhs)) != nullptr) {
                IndVarLimit = -10010;   // the special value with nullptr return value denote that condition part may be a constant value
                return nullptr;
            } else if (rhs->getType()==i32 || rhs->getType()==i64) {
                //TO-DO: check the Inst Type -- LoadInst
                if ((tmpInst=dyn_cast<LoadInst>(rhs)) != nullptr)
                    return getInnerMostLoadOpnd(tmpInst, tmpInst->getParent());
                else if ((tmpInst=dyn_cast<AllocaInst>(rhs)) != nullptr)
                    return tmpInst;
                else {
                    errs()<< "DEBUG INFO: In getIndVarFromHS() method, the potential IndVar inst is not Load or Alloca.\n";
                    return nullptr;
                }
            }
        } else if ((constIntVar=dyn_cast<ConstantInt>(rhs)) != nullptr) { // suppose the right opnd is a constant value
            IndVarLimit = getValueFromConstInt(constIntVar);
            if ((constIntVar=dyn_cast<ConstantInt>(lhs)) != nullptr) {
                IndVarLimit = -10011;   // Give the member IndVarLimit a special int value to indicate the detecting result
                return nullptr;
            } else if (lhs->getType()==i32 || lhs->getType()==i64) {
                //TO-DO: check the Inst Type -- LoadInst
                if ((tmpInst=dyn_cast<LoadInst>(lhs)) != nullptr)
                    return getInnerMostLoadOpnd(tmpInst, tmpInst->getParent());
                else if ((tmpInst=dyn_cast<AllocaInst>(lhs)) != nullptr)
                    return tmpInst;
                else {
                    errs()<< "DEBUG INFO: In getIndVarFromHS() method, the potential IndVar inst is not Load or Alloca.\n";
                    return nullptr;
                }
            }
        }
        // Binary Float Opnd
        if ((constFpVar=dyn_cast<ConstantFP>(lhs)) != nullptr) {         // suppose the left opnd is a constant float value
            // IndVarLimit = getValueFromConstInt(constIntVar);
            if ((constIntVar=dyn_cast<ConstantInt>(rhs)) != nullptr) {
                // IndVarLimit = -10010; 
                errs()<< "WARNING: In method getIndVarFromHS(), both operands are judged as constant int values.\n";
                return nullptr;
            } else if (rhs->getType()==flt || rhs->getType()==dbl) {
                return rhs;
            }
        } else if ((constIntVar=dyn_cast<ConstantInt>(rhs)) != nullptr) { // suppose the right opnd is a constant value
            // IndVarLimit = getValueFromConstInt(constIntVar);
            if ((constIntVar=dyn_cast<ConstantInt>(lhs)) != nullptr) {
                // IndVarLimit = -10010;
                errs()<< "WARNING: In method getIndVarFromHS(), both operands are judged as constant int values.\n";
                return nullptr;
            } else if (lhs->getType()==flt || lhs->getType()==dbl) {
                // IndVarLimit = rhs;
                return lhs;
            }
        }
        errs()<< "WARNING: both operands in the BR condition area are variables, whick is hard to detect the induction variable.\n";
        return nullptr;
    }

    Value* getCondVarFromBrInst(BranchInst* BR) {
        errs()<<"DEBUG INFO: Enter the getCondVarFromBrInst() method...\n";
        Value* cond = BR->getCondition();
        Instruction* condInst = nullptr;
        if ((condInst=dyn_cast<Instruction>(cond)) != nullptr) {
            unsigned opcode = condInst->getOpcode();
            Value* lhs = nullptr;
            Value* rhs = nullptr;
            if (opcode == Instruction::ICmp) {
                ICmpInst* icmpInst = dyn_cast<ICmpInst>(condInst);
                lhs = icmpInst->getOperand(0);
                rhs = icmpInst->getOperand(1);
                return getIndVarFromHS(lhs, rhs);
            } else if (opcode == Instruction::FCmp) {
                FCmpInst* fcmpInst = dyn_cast<FCmpInst>(condInst);
                lhs = fcmpInst->getOperand(0);
                rhs = fcmpInst->getOperand(1);
                return getIndVarFromHS(lhs, rhs);
            } else if ((cond=dyn_cast<ConstantInt>(cond)) != nullptr) { // TO-DO: check if the cond part is a constant value
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
        errs()<<"DEBUG INFO: Enter the getForOrWhileInductionVar() method...\n";
        BasicBlock* headerBB = LP->getHeader();
        // BasicBlock* latchBB = LP->getLoopLatch();
        BasicBlock* exitBB = LP->getExitingBlock();
        if (headerBB == exitBB) {
            Instruction* termInst = headerBB->getTerminator();
            BranchInst* brInst = nullptr;
            if ((brInst=dyn_cast<BranchInst>(termInst)) != nullptr) {
                errs()<<"DEBUG INFO: Enter the getCondVarFromBrInst() method...\n";
                return getCondVarFromBrInst(brInst);
            } else {
                errs()<<"DEBUG INFO: In getForOrWhileInductionVar() method, fail to get a valid BR inst in the condition part.\n";
            }
        } else {
            errs()<< "WARNING: In function getForOrWhileInductionVar(), the header BB does not match the exit BB.\n";
            errs()<< "--Maybe the getLoopTye() method returned a wrong value for the current Loop.\n";
        }
        return nullptr;
    }

    Value* getDoWhileInductionVar(Loop* LP) {
        errs()<<"DEBUG INFO: Enter the getDoWhileInductionVar() method...\n";
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
        errs()<<"DEBUG INFO: Enter the getInductionVarFrom() method...\n";
        if (Type == 0) {
            errs()<<"DEBUG INFO: Enter the getForOrWhileInductionVar() method...\n";
            return getForOrWhileInductionVar(LP);
        } else if (Type == 1) {
            errs()<<"DEBUG INFO: Enter the getDoWhileInductionVar() method...\n";
            return getDoWhileInductionVar(LP);
        }
        errs()<< "WARNING: In getInductionVarFrom() method, an unknown loop type is given in the argus.\n";
        return nullptr;
    }

    bool isValidArithInst(Instruction* Inst, Value** Lhs, Value** Rhs) {
        unsigned opcode = Inst->getOpcode();
        if (Inst == nullptr) {
            errs()<< "DEBUG INDO: In isValidArithOp() method, the InstPtr is NULL!\n";
            *Lhs = nullptr;
            *Rhs = nullptr;
            return false;
        }
        if (opcode == Instruction::Add ||
            opcode == Instruction::Sub ||
            opcode == Instruction::Mul ||
            opcode == Instruction::UDiv ||
            opcode == Instruction::SDiv) {
            (*Lhs) = Inst->getOperand(0);
            (*Rhs) = Inst->getOperand(1);
            return true;
        } else {
            errs()<< "DEBUG INDO: In isValidArithInst() method, an unknown inst type is passed into argu list.\n";
            *Lhs = nullptr;
            *Rhs = nullptr;
        }
        return false;
    }

    Instruction* passIncessantLoadInst(Instruction* curInst, Value* lastInst) {
        errs()<< "DEBUG INFO: Enter the passIncessantLoadInst() method...\n";
        if (curInst!=nullptr && lastInst!=nullptr) {
            Instruction* tmpInst = nullptr;
            Value* firstOprd = nullptr;
            if ((tmpInst=dyn_cast<LoadInst>(curInst)) != nullptr) {
                mLoadInstVector.push_back(curInst);
                firstOprd = tmpInst->getOperand(0);
                if (firstOprd==lastInst) {
                    tmpInst = tmpInst->getNextNonDebugInstruction();
                    return passIncessantLoadInst(tmpInst, curInst);
                } else {
                    errs()<< "WARNING: In passIncessantLoadInst() method, the LoadInst's argu mismatched.\n";
                    return nullptr;
                }
            } else {
                return curInst;
            }
        } else {
            errs()<< "WARNING: In passIncessantLoadInst() method, the argu list contains NULL value.\n";
        }
        return nullptr;
    }

    bool checkPatternLAS(Instruction* Inst, Value* IndVar) {
        errs()<< "DEBUG INFO: Enter the checkPatternLAS() method...\n";
        if (Inst != nullptr && IndVar != nullptr) {
            errs()<< "DEBUG INFO: In checkPatternLAS() method, print out the LoadInst layer number -- " << mIndVarLoadLayers << "\n";
            unsigned opcode = 0;
            Value* lhs = nullptr;
            Value* rhs = nullptr;
            // First, check the incessant LoadInsts before checking the arithmetic ops
            mLoadInstVector.clear();
            Instruction* arithInst = passIncessantLoadInst(Inst, IndVar);
            Instruction* storeInst = nullptr;
            if (arithInst!=nullptr && isValidArithInst(arithInst, &lhs, &rhs)) {
                errs()<< "DEBUG INFO: In checkPatternLAS() method, the content of expected ArithmeticInst is : " << *arithInst << "\n";
                storeInst = arithInst->getNextNonDebugInstruction();
                opcode = storeInst->getOpcode();
                if (opcode == Instruction::Store) {
                    if (lhs!=nullptr && rhs!=nullptr) {
                        if(getIndVarFromHS(lhs, rhs)==IndVar) {
                            lhs = storeInst->getOperand(0);
                            rhs = storeInst->getOperand(1);
                            // TO-DO: We need a new design of the Opnd check mechanism
                            if (lhs==arithInst) {
                                std::vector<Value*>::iterator result = find(mLoadInstVector.begin(), mLoadInstVector.end(), rhs);
                                if (rhs==IndVar || result!=mLoadInstVector.end())
                                    return true;
                                else {
                                    errs()<< "DEBUG INFO: In checkPatternLAS() method, the operand(1) of StoreInst does NOT match the LoadInst vector or IndVar.\n";
                                }
                            }
                        } else {
                            errs()<< "DEBUG INFO: In checkPatternLAS() method, the getIndVarFromHS() method did not return a same Value* to the IndVar.\n";
                            errs()<< "IndVar info: " << *IndVar << "\n";
                            errs()<< "LHS info: " << *lhs << "\n";
                            errs()<< "RHS info: " << *rhs << "\n";
                        }
                    } else {
                        errs()<< "DEBUG INFO: In checkPatternLAS() method, LHS and RHS are set NULL after calling isValidArithInst() method.\n";
                    }
                } else {
                        errs()<< "DEBUG INFO: In checkPatternLAS() method, the instruction next to the arith inst is NOT store inst.\n";
                }
            } else {
                errs()<< "DEBUG INFO: In checkPatternLAS() method, no artichmetic inst exists behind the current LoadInst.\n";
            }
        } else {
            errs()<< "DEBUG INFO: In checkPatternLAS() method, a NULL ptr is passed into the argu list!\n";
        }
        return false;
    }

    // If the IndVar is used in the arithmetic instruction, this method will deem that
    // the loop has setted a valid IndVar to reach an end.
    bool checkBasicArithmetic(Instruction* Inst, Value* IndVar) {
        errs()<<"DEBUG INFO: Enter the checkBasicArithmetic() method...\n";
        if (Inst!=nullptr && IndVar!=nullptr) {
            unsigned opcode = Inst->getOpcode();
            if (opcode==Instruction::Load && Inst->getOperand(0)==IndVar) {
                // Start to check the pattern
                if (checkPatternLAS(Inst, IndVar)) {
                    mIndVarLoadLayers = 0;
                    return true;
                }
            }
        }
        errs()<< "WARNING: In checkBasicArithmetic() method, the argu Inst is tested with NULL value.\n";
        return false;
    }

    // Maybe we need a new method to process the LoopObj iteratively
    bool isFiniteLoop(Loop* LP) {
        errs()<< "DEBUG INFO: Enter the isInfiniteLoop() method...\n";
        if (LP == nullptr) {
            errs()<< "WARNING: In isInfiniteLoop() method, the argu LP is a NULL value.\n";
            return false;
        }
        std::vector<Loop*> subLoops = LP->getSubLoopsVector();
        if (!subLoops.empty()) {
            errs()<< "DEBUG INFO: Sub loops were found under current processing LoopObj...\n";
            for (auto* lp : subLoops) {
                mIndVarLoadLayers = 0;
                if (isFiniteLoop(lp) == false) 
                    return false;
            }
            errs()<< "DEBUG INFO: The sub loops are finite --- safe inner\n";
        }
        // Detect the current processing loop obj...
        mIndVarLoadLayers = 0;
        int lpTy = getLoopType(LP);
        Value* lpIndVar = getInductionVarFrom(LP, lpTy);
        if (lpIndVar != nullptr) {
            errs()<< "Found induction variable in the loop: " << *lpIndVar << "\n";
            if (isChangedByLP(LP, lpIndVar)) {
                return true;
            } else {
                return false;
            }
        } else {
            errs()<< "WARNING: Fail to fetch the induction variable from the current Loop.\n\n";
        }
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
                errs()<< "DEBUG INFO: In isChangedByLP() method, we are processing the BB as fllowing...\n" << *bb << "\n";
                while (iiItr != ieItr) {
                    if (checkBasicArithmetic(&(*iiItr), IndVar))
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
        if ((F.getParent())->getTargetTriple().compare("nvptx64-nvidia-cuda") == 0) {
            errs()<< "Now, this pass is dealing with a GPU kernel module...\n";
            curFunc = &F;
            LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
            if (LI.empty()) {
                errs()<< "No Loop Structure exists in the function: " << F.getName() << "\n\n";
            } else {
                errs()<< "Start analyzing the LoopInfo obj in the Function: " << F.getName() << "\n";
                errs()<< "LoopInfo obj content:\n";
                LI.print(errs());
                errs()<< "\n";
                InitAllocaInstVector(&(*(F.begin())));
                errs()<< "DEBUG INFO: In runOnFunction() method, use the begin()/end() itr instead of foreach parsing.\n";                std::vector<Loop*>::iterator lpItr = LI.begin();
                std::vector<Loop*> loops = LI.getLoopsInPreorder();
                // std::vector<Loop*>::iterator lpsItr = loops.begin();
                // std::vector<Loop*>::iterator lpsEnd = loops.end();

                LoopInfoBase<BasicBlock, Loop>::iterator lpItr = LI.begin();
                LoopInfoBase<BasicBlock, Loop>::iterator lpEnd = LI.end();
                while (lpItr != lpEnd) {
                    errs()<< "DEBUG INFO: In runOnFunction() method, try to print out the content of Loops' iterator... \n";
                    errs()<< *(*lpItr) << "\n";
                    ++lpItr;
                }
                for (auto* lp : LI) {
                    // TO-DO: Check if a BB in the LoopObj is a subloop's header
                    errs()<< "=====-----------Infinite Loop Check Report-----------=====\n";
                    if (isFiniteLoop(lp)) {
                        errs()<< "Under current checking strategies, this loop is considered as a finite type ----- SAFE\n";
                    } else {
                        errs()<< "Under current checking strategies, this loop is considered as an infinite type ----- UNSAFE\n";
                    }
                    errs()<< "=====-----------------------End----------------------=====\n";
                    // std::vector<Loop*> subLoops = lp->getSubLoopsVector();
                    // if (!subLoops.empty()) {
                    //     errs()<< "DEBUG INFO: After check report, sub loops were found under current processing LoopObj...\n";
                    // } else {
                    //     errs()<< "DEBUG INFO: No sub loop exists under current processing LoopObj...\n";
                    // }
                    // int lpTy = getLoopType(lp);
                    // Value* lpIndVar = getInductionVarFrom(lp, lpTy);
                    // if (lpIndVar != nullptr) {
                    //     errs()<< "Found induction variable in the loop: " << *lpIndVar << "\n";
                    //     errs()<< "=====-----------Infinite Loop Check Report-----------=====\n";
                    //     if (isChangedByLP(lp, lpIndVar)) {
                    //         //TO-DO: Print out the safety detection report
                    //         errs()<< "Under current checking strategies, this loop is considered as a finite type ----- SAFE\n";
                    //     } else {
                    //         errs()<< "Under current checking strategies, this loop is considered as an infinite type ----- UNSAFE\n";
                    //     }
                    //     errs()<< "=====-----------------------End----------------------=====\n";
                        
                    // } else {
                    //     errs()<< "WARNING: Fail to fetch the induction variable from the current Loop.\n";
                    // }
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