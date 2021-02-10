#include "llvm/Pass.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
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
    int mLoopCtrlBBTrendCode = 0;
    int mLoopArithInstTrendCode = 0;
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
        mLoopCtrlBBTrendCode = 0;
        mLoopArithInstTrendCode = 0;
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
        if (LP != nullptr) {
            // for(){...}, while(){...}
            if (headerBB == exitBB) {
                errs()<< "DEBUG INFO: In getLoopType() method, we judge current LoopObj is for/while type.\n";
                return 0;
            }
            // do{...} while()
            if (latchBB == exitBB) {
                errs()<< "DEBUG INFO: In getLoopType() method, we judge current LoopObj is do...while type.\n";
                return 1;
            }
            errs()<< "DEBUG INFO: In getLoopType() method, the header, latch, exiting BBs do not match the detection strategies.\n";
            errs()<< "DEBUG INFO: In getLoopType() method, print the processed Loop obj info as following...\n" << *LP << "\n";
            if (headerBB != nullptr)
                errs()<< "DEBUG INFO: In getLoopType() method, print the header as following...\n" << *headerBB << "\n";
            if (latchBB != nullptr)
                errs()<< "DEBUG INFO: In getLoopType() method, print the latch as following...\n" << *latchBB << "\n";
            if (exitBB != nullptr)
                errs()<< "DEBUG INFO: In getLoopType() method, print the exiting as following...\n" << *exitBB << "\n";
        } else {
            errs()<< "DEBUG INFO: In getLoopType() method, the argu ptr *LP is NULL value!\n";
        }
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
        Type* flt = Type::getFloatTy(curFunc->getParent()->getContext());
        Type* dbl = Type::getDoubleTy(curFunc->getParent()->getContext());
        Instruction* tmpInst = nullptr;
        
        // check the left operand first
        // Binary Int Opnd
        if ((constIntVar=dyn_cast<ConstantInt>(lhs)) != nullptr) {         // suppose the left opnd is a constant value
                IndVarLimit = getValueFromConstInt(constIntVar);
            if ((constIntVar=dyn_cast<ConstantInt>(rhs)) != nullptr) {
                errs()<< "WARNING: In getIndVarFromHS() method, both lhs and rhs are constant values!\n";
                IndVarLimit = -10010;   // the special value with nullptr return value denote that condition part may be a constant value
                return nullptr;
            } else if (rhs->getType()==i32 || rhs->getType()==i64) {
                // check the Inst Type -- LoadInst or AllocaInst is expected
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
                errs()<< "WARNING: In getIndVarFromHS() method, both lhs and rhs are constant values!\n";
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
        } else {                                                        // suppose both lhs and rhs are variables
            if (lhs->getType()==i32 || lhs->getType()==i64) {           // lhs is an int type variable
                if (rhs->getType()==i32 || rhs->getType()==i64) {       // rhs is also an int variable
                    if ((tmpInst=dyn_cast<LoadInst>(lhs)) != nullptr) {
                        return getInnerMostLoadOpnd(tmpInst, tmpInst->getParent());
                    } else if ((tmpInst=dyn_cast<AllocaInst>(lhs)) != nullptr) {
                        return tmpInst;
                    } else {
                        errs()<< "DEBUG INFO: In getIndVarFromHS() method, the lhs and rhs are not int type.\n";
                    }
                }
            }
        }
        // Binary Float Opnd
        if ((constFpVar=dyn_cast<ConstantFP>(lhs)) != nullptr) {         // suppose the left opnd is a constant float value
            // IndVarLimit = getValueFromConstInt(constIntVar);
            if ((constIntVar=dyn_cast<ConstantInt>(rhs)) != nullptr) {
                // IndVarLimit = -10010; 
                errs()<< "WARNING: In getIndVarFromHS() method, both operands are judged as constant int values.\n";
                return nullptr;
            } else if (rhs->getType()==flt || rhs->getType()==dbl) {
                // check the Inst Type -- LoadInst or AllocaInst is expected
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
            // IndVarLimit = getValueFromConstInt(constIntVar);
            if ((constIntVar=dyn_cast<ConstantInt>(lhs)) != nullptr) {
                // IndVarLimit = -10010;
                errs()<< "WARNING: In getIndVarFromHS() method, both operands are judged as constant int values.\n";
                return nullptr;
            } else if (lhs->getType()==flt || lhs->getType()==dbl) {
                // check the Inst Type -- LoadInst or AllocaInst is expected
                if ((tmpInst=dyn_cast<LoadInst>(lhs)) != nullptr)
                    return getInnerMostLoadOpnd(tmpInst, tmpInst->getParent());
                else if ((tmpInst=dyn_cast<AllocaInst>(lhs)) != nullptr)
                    return tmpInst;
                else {
                    errs()<< "DEBUG INFO: In getIndVarFromHS() method, the potential IndVar inst is not Load or Alloca.\n";
                    return nullptr;
                }
            }
        } else {                    // suppose both lhs and rhs are variables
            if (lhs->getType()==flt || lhs->getType()==dbl) {
                if (rhs->getType()==flt || rhs->getType()==dbl) {
                    if ((tmpInst=dyn_cast<LoadInst>(lhs)) != nullptr) {
                        return getInnerMostLoadOpnd(tmpInst, tmpInst->getParent());
                    } else if ((tmpInst=dyn_cast<AllocaInst>(lhs)) != nullptr) {
                        return tmpInst;
                    } else {
                        errs()<< "DEBUG INFO: In getIndVarFromHS() method, the lhs and rhs are not float/double type.\n";
                    }
                }
            }
        }
        errs()<< "WARNING: In getIndVarFromHS() method, both operands are not clear, which is hard to detect the induction variable.\n";
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

    // TO-DO: Add the shl and shr to the opcode types
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
            opcode == Instruction::SDiv ||
            opcode == Instruction::AShr ||
            opcode == Instruction::LShr) {
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
                mLoopArithInstTrendCode = getTrendCodeFromArithInst(arithInst, IndVar);
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
                errs()<< "DEBUG INFO: In checkPatternLAS() method, no arithmetic inst exists behind the current LoadInst.\n";
            }
        } else {
            errs()<< "DEBUG INFO: In checkPatternLAS() method, a NULL ptr is passed into the argu list!\n";
        }
        return false;
    }

    // If the IndVar is used in the arithmetic instruction, this method will deem whether
    // the loop has been set a valid IndVar to reach an end.
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
        mIndVarLoadLayers = 0;
        int lpTy = getLoopType(LP);
        Value* lpIndVar = getInductionVarFrom(LP, lpTy);
        if (lpIndVar != nullptr) {
            errs()<< "DEBUG INFO: In isInfiniteLoop() method, found induction variable in the loop: " << *lpIndVar << "\n";
            mLoopCtrlBBTrendCode = getTrendCodeFromCtrlBB(LP->getExitingBlock(), lpIndVar);
            errs()<< "DEBUG INFO: In isInfiniteLoop() method, the value of the trend code from ctrl BB is " << mLoopCtrlBBTrendCode <<"\n";
            if (isChangedByLP(LP, lpIndVar)) {
                return true;
            } else {
                return false;
            }
        } else {
            errs()<< "WARNING: In isInfiniteLoop() method, fail to fetch the induction variable from the current Loop.\n\n";
        }
        return false;
    }

    bool isChangedByLP(Loop* LP, Value* IndVar) {
        errs()<< "DEBUG INFO: Enter the isChangedByLP() method...\n";
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
                    if (checkBasicArithmetic(&(*iiItr), IndVar)){
                        errs()<< "DEBUG INFO: In isChangedByLP() method, the value of the arith inst trend code is " << mLoopArithInstTrendCode << "\n";
                        if (mLoopCtrlBBTrendCode*mLoopArithInstTrendCode == 1)
                            return true;
                    }
                    ++iiItr;
                }
                ++bbItr;
            }
        }
        errs()<< "WARNING: In isChangedByLP() method, the NULL value exists in the argus.\n";
        return false;
    }

    // TO-DO: lhs and rhs may be not the direct IndVar
    int getSequenceTypeFromHS(Value* lhs, Value* rhs, Value* IndVar) {
        errs()<< "DEBUG INFO: Enter the getSequenceTypeFromHS() method...\n";
        ConstantInt* constIntVar = nullptr;
        LoadInst* tmpLDInst = nullptr;
        IntegerType* i32 = IntegerType::get(curFunc->getParent()->getContext(), 32);
        IntegerType* i64 = IntegerType::get(curFunc->getParent()->getContext(), 64);
        if (lhs==nullptr || rhs==nullptr) {
            errs()<< "WARNING: In getSequenceTypeFromHS() method, the argu list contains NULL value.\n";
        } else {
            if ((tmpLDInst=dyn_cast<LoadInst>(lhs)) != nullptr) {
                if (getInnerMostLoadOpnd(tmpLDInst, tmpLDInst->getParent()) == IndVar) {    // Suppose the left opnd is the IndVar value
                    if (rhs->getType()==i32 || rhs->getType()==i64 || (constIntVar=dyn_cast<ConstantInt>(rhs))!=nullptr) {
                        return 1;
                    } else {
                        errs()<< "WARNING: In getSequenceTypeFromHS() method, the argu rhs contains unknown content.\n";
                    }
                }
            } else if ((tmpLDInst=dyn_cast<LoadInst>(rhs)) != nullptr) {        // Suppose the right opnd is the IndVar value
                if (getInnerMostLoadOpnd(tmpLDInst, tmpLDInst->getParent()) == IndVar) {
                    if (lhs->getType()==i32 || lhs->getType()==i64 || (constIntVar=dyn_cast<ConstantInt>(lhs))!=nullptr) {
                        return 2;
                    } else {
                        errs()<< "WARNING: In getSequenceTypeFromHS() method, the argu lhs contains unknown content.\n";
                    }
                }
            } else {
                errs()<< "WARNING: In getSequenceTypeFromHS() method, something unknown is passed into the lhs and the rhs. Return the default state code 2.\n";
                return 2;
            }
        }
        return 0;
    }

    //TO-DO: This method is going to detect the type of operand pairs.
    int getPairTypeFromHS(Value* lhs, Value* rhs) {
        errs()<< "DEBUG INFO: Enter the getPairTypeFromHS() method...\n";
        ConstantInt* constIntVar = nullptr;
        ConstantFP* constFpVar = nullptr;
        IntegerType* i32 = IntegerType::get(curFunc->getParent()->getContext(), 32);
        IntegerType* i64 = IntegerType::get(curFunc->getParent()->getContext(), 64);
        Type* fltTy =  Type::getFloatTy(curFunc->getParent()->getContext());
        Type* dblTy =  Type::getDoubleTy(curFunc->getParent()->getContext());
        Instruction* tmpInst = nullptr;
        // int operand
        if ((constIntVar=dyn_cast<ConstantInt>(lhs)) != nullptr) {              // suppose the left opnd is a constant value
            if ((constIntVar=dyn_cast<ConstantInt>(rhs)) != nullptr) {          // both the two opnds are constant values 
                errs()<< "WARNING: In getPairTypeFromHS() method, the two operands of the CMPInst are constant, which can give a constant value to the CMPInst.\n";
                return 0;
            } else if (rhs->getType()==i32 || rhs->getType()==i64) {            // the right opnd is varying
                if ((tmpInst=dyn_cast<LoadInst>(rhs)) != nullptr) {
                    if (getInnerMostLoadOpnd(tmpInst, tmpInst->getParent()) != nullptr) {   // cheers! maybe a formative comparing inst exists
                        return 2;
                    } else {
                        errs()<< "WARNING: In getPairTypeFromHS() method, the current CMPInst may NOT be the flow control condition.\n";
                    }
                } else {
                    errs()<< "WARNING: In getPairTypeFromHS() method, an unknown opnd type is met at the right int opnd site.\n";
                }
            } else {
                errs()<< "WARNING: In getPairTypeFromHS() method, an unknown opnd data type is met at the right int opnd site.\n";
            }
        } else if ((constIntVar=dyn_cast<ConstantInt>(rhs)) != nullptr) {       // suppose the right opnd is a constant value
            if ((constIntVar=dyn_cast<ConstantInt>(lhs)) != nullptr) {          // both the two opnds are constant values 
                errs()<< "WARNING: In getPairTypeFromHS() method, the two operands of the CMPInst are constant, which can give a constant value to the CMPInst.\n";
                return 0;
            } else if (lhs->getType()==i32 || lhs->getType()==i64) {
                if ((tmpInst=dyn_cast<LoadInst>(lhs)) != nullptr) {
                    if (getInnerMostLoadOpnd(tmpInst, tmpInst->getParent()) != nullptr) {
                        return 1;
                    } else {
                        errs()<< "WARNING: In getPairTypeFromHS() method, the current CMPInst may NOT be the flow control condition.\n";
                    }
                } else {
                    errs()<< "WARNING: In getPairTypeFromHS() method, an unknown opnd type is met at the left opnd site.\n";
                }
            } else {
                errs()<< "WARNING: In getPairTypeFromHS() method, an unknown opnd data type is met at the left opnd site.\n";
            }
        }
        // float operand
        if ((constFpVar=dyn_cast<ConstantFP>(lhs)) != nullptr) {            // suppose the left opnd is a constant float value
            if ((constIntVar=dyn_cast<ConstantInt>(rhs)) != nullptr) {      // both the two opnds are constant values 
                errs()<< "WARNING: In getPairTypeFromHS() method, the two operands of the CMPInst are constant, which can give a constant value to the CMPInst.\n";
                return 0;
            } else if (rhs->getType()==fltTy || rhs->getType()==dblTy) {
                if ((tmpInst=dyn_cast<LoadInst>(rhs)) != nullptr) {
                    if (getInnerMostLoadOpnd(tmpInst, tmpInst->getParent()) != nullptr) {
                        return 2;
                    } else {
                        errs()<< "WARNING: In getPairTypeFromHS() method, the current CMPInst may NOT be the flow control condition.\n";
                    }
                } else {
                    errs()<< "WARNING: In getPairTypeFromHS() method, an unknown opnd type is met at the right float opnd site.\n";
                }
            } else {
                errs()<< "WARNING: In getPairTypeFromHS() method, an unknown opnd type is met at the right float opnd site.\n";
            }
        } else if ((constIntVar=dyn_cast<ConstantInt>(rhs)) != nullptr) {
            if ((constIntVar=dyn_cast<ConstantInt>(lhs)) != nullptr) {
                errs()<< "WARNING: In getPairTypeFromHS() method, the two operands of the CMPInst are constant, which can give a constant value to the CMPInst.\n";
                return 0;
            } else if (lhs->getType()==fltTy || lhs->getType()==dblTy) {
                if ((tmpInst=dyn_cast<LoadInst>(lhs)) != nullptr) {
                    if (getInnerMostLoadOpnd(tmpInst, tmpInst->getParent()) != nullptr) {
                        return 1;
                    } else {
                        errs()<< "WARNING: In getPairTypeFromHS() method, the current CMPInst may NOT be the flow control condition.\n";
                    }
                } else {
                    errs()<< "WARNING: In getPairTypeFromHS() method, an unknown opnd type is met at the left float opnd site.\n";
                }
            } else {
                errs()<< "WARNING: In getPairTypeFromHS() method, an unknown opnd date type is met at the left float opnd site.\n";
            }
        }
        errs()<< "WARNING: In getPairTypeFromHS() method, the current two opnds from CMPInst belongs to certain unknown types and can not be recognized.\n";
        return 0;
    }

    //TO-DO: we need a method to detect the trend type of ctrlBB
    int getTrendCodeFromCtrlBB(BasicBlock* CtrlBB, Value* IndVar) {
        errs()<< "DEBUG INFO: Enter the getTrendCodeFromCtrlBB() method...\n";
        Instruction* termInst = CtrlBB->getTerminator();
        BranchInst* brInst = nullptr;
        if (CtrlBB==nullptr || IndVar==nullptr) {
            errs()<< "WARNING: In getTrendCodeFromCtrlBB() method, the argu list contains NULL values.\n";
            return 0;
        }
        if ((brInst=dyn_cast<BranchInst>(termInst)) != nullptr) {
            Value* cond = brInst->getCondition();
            CmpInst* condInst = nullptr;
            if ((condInst=dyn_cast<CmpInst>(cond)) != nullptr) {
                unsigned opcode = condInst->getPredicate();
                // ConstantInt* lhs = dyn_cast<ConstantInt>(condInst->getOperand(0));
                // ConstantInt* rhs = dyn_cast<ConstantInt>(condInst->getOperand(1));
                Value* lhs = condInst->getOperand(0);
                Value* rhs = condInst->getOperand(1);
                int opndPairSeq = getSequenceTypeFromHS(lhs, rhs, IndVar);
                // detect if it's positive or negative
                if (opcode==CmpInst::ICMP_SLT ||
                    opcode==CmpInst::ICMP_SLE ||
                    opcode==CmpInst::ICMP_ULT ||
                    opcode==CmpInst::ICMP_ULE) {
                    if (opndPairSeq == 1) {                 // suppose positive trend
                        return 1;
                    } else if (opndPairSeq == 2) {          // suppose negative trend
                        return -1;
                    } else {
                        errs()<< "WARNING: In getTrendCodeFromCtrlBB() method, an unknown pair seq is met in the getSequenceTypeFromHS() method.\n";
                    }
                } else if (opcode==CmpInst::ICMP_UGT ||
                    opcode==CmpInst::ICMP_UGE ||
                    opcode==CmpInst::ICMP_SGT ||
                    opcode==CmpInst::ICMP_SGE) {
                    if (opndPairSeq == 1) {                 // suppose negative trend
                        return -1;
                    } else if (opndPairSeq == 2) {          // suppose positive trend
                        return 1;
                    } else {
                        errs()<< "WARNING: In getTrendCodeFromCtrlBB() method, an unknown pair seq is met in the getSequenceTypeFromHS() method.\n";
                    }
                } else if (opcode==CmpInst::FCMP_OGT ||
                    opcode==CmpInst::FCMP_OGE ||
                    opcode==CmpInst::FCMP_UGT ||
                    opcode==CmpInst::FCMP_UGE) {
                    if (opndPairSeq == 1) {                 // suppose negative trend
                        return -1;
                    } else if (opndPairSeq == 2) {          // suppose positive trend
                        return 1;
                    } else {
                        errs()<< "WARNING: In getTrendCodeFromCtrlBB() method, an unknown pair seq is met in the getSequenceTypeFromHS() method.\n";
                    }
                } else if (opcode==CmpInst::FCMP_OLT ||
                    opcode==CmpInst::FCMP_OLE ||
                    opcode==CmpInst::FCMP_ULT ||
                    opcode==CmpInst::FCMP_ULE) {
                    if (opndPairSeq == 1) {                 // suppose positive trend
                        return 1;
                    } else if (opndPairSeq == 2) {          // suppose negative trend
                        return -1;
                    } else {
                        errs()<< "WARNING: In getTrendCodeFromCtrlBB() method, an unknown pair seq is met in the getSequenceTypeFromHS() method.\n";
                    }
                } else {
                    errs()<<"WARNING: In getTrendCodeFromCtrlBB() method, an unknown ICMP opcode type is passed inside.\n";
                }
            } else {
                errs()<< "WARNING: In getTrendCodeFromCtrlBB() method, an unknown calculation type of BR Inst condition is met.\n";
            }
        } else {
            errs()<< "WARNING: In getTrendCodeFromCtrlBB() method, the terminator of argu CtrlBB is not the BRInst.\n";
            if (termInst == nullptr) {
                errs()<< "WARNING: In getTrendCodeFromCtrlBB() method, fail to fetch the terminator inst from the ctrl BB.\n";
            } else {
                errs()<< "WARNING: In getTrendCodeFromCtrlBB() method, the content of terminator inst is " << *termInst << "\n";
            }
        }
        return 0;
    }

    int getAddInstTrendCode(int PairCode, Value* LHS, Value* RHS) {
        errs()<< "DEBUG INFO: Enter the getAddInstTrendCode() method...\n";
        ConstantInt* constVal = nullptr;
        if (LHS==nullptr || RHS==nullptr) {
            errs()<< "WARNING: In getAddInstTrendCode() method, the argu list contains NULL ptr.\n";
            return 0;
        }
        if (PairCode == 1) {
            if ((constVal=dyn_cast<ConstantInt>(RHS)) != nullptr) {
                int64_t limitValue = constVal->getSExtValue();
                return (limitValue > 0)?1:-1;
            }
        } else if (PairCode == 2) {
            if ((constVal=dyn_cast<ConstantInt>(LHS)) != nullptr) {
                int64_t limitValue = constVal->getSExtValue();
                return (limitValue > 0)?1:-1;
            }
        }
        return 0;
    }

    int getSubInstTrendCode(int PairCode, Value* LHS, Value* RHS) {
        errs()<< "DEBUG INFO: Enter the getSubInstTrendCode() method...\n";
        ConstantInt* constVal = nullptr;
        if (LHS==nullptr || RHS==nullptr) {
            errs()<< "WARNING: In getSubInstTrendCode() method, the argu list contains NULL ptr.\n";
            return 0;
        }
        if (PairCode == 1) {
            if ((constVal=dyn_cast<ConstantInt>(RHS)) != nullptr) {
                int64_t limitValue = constVal->getSExtValue();
                return (limitValue > 0)?-1:1;
            }
        } else if (PairCode == 2) {
            errs()<< "WARNING: In getSubInstTrendCode() method, the var-const pair is met here.\n";
        }
        return 0;
    }

    int getMulInstTrendCode(int PairCode, Value* LHS, Value* RHS) {
        errs()<< "DEBUG INFO: Enter the getMulInstTrendCode() method...\n";
        ConstantInt* constVal = nullptr;
        if (LHS==nullptr || RHS==nullptr) {
            errs()<< "WARNING: In getMulInstTrendCode() method, the argu list contains NULL ptr.\n";
            return 0;
        }
        if (PairCode == 1) {
            if ((constVal=dyn_cast<ConstantInt>(RHS)) != nullptr) {
                int64_t limitValue = constVal->getSExtValue();
                if (limitValue>0 && limitValue <1)
                    return -1;
                else if (limitValue > 1)
                    return 1;
            }
        } else if (PairCode == 2) {
            if ((constVal=dyn_cast<ConstantInt>(LHS)) != nullptr) {
                int64_t limitValue = constVal->getSExtValue();
                if (limitValue>0 && limitValue <1)
                    return -1;
                else if (limitValue > 1)
                    return 1;
            }
        }
        return 0;
    }

    int getDivInstTrendCode(int PairCode, Value* LHS, Value* RHS) {
        errs()<< "DEBUG INFO: Enter the getDivInstTrendCode() method...\n";
        ConstantInt* constVal = nullptr;
        if (LHS==nullptr || RHS==nullptr) {
            errs()<< "WARNING: In getDivInstTrendCode() method, the argu list contains NULL ptr.\n";
            return 0;
        }
        if (PairCode == 1) {
            if ((constVal=dyn_cast<ConstantInt>(RHS)) != nullptr) {
                int64_t limitValue = constVal->getSExtValue();
                if (limitValue>0 && limitValue <1)
                    return 1;
                else if (limitValue > 1)
                    return -1;
            }
        } else if (PairCode == 2) {
            errs()<< "WARNING: In getDivInstTrendCode() method, the const-var pair can not be handled under current strategies.\n";
        }
        return 0;
    }

    // TO-DO: Add some to handle the shl and shr opcode types
    // TO-DO: the getPairTypeFromHS() method needs some improvement.
    int getTrendCodeFromArithInst(Instruction* ArithInst, Value* IndVar) {
        errs()<< "DEBUG INFO: Enter the getTrendCodeFromArithInst() method...\n";
        if (ArithInst != nullptr) {
            unsigned opcode = ArithInst->getOpcode();
            Value* lhs = ArithInst->getOperand(0);
            Value* rhs = ArithInst->getOperand(1);
            // int opndPairTy = getPairTypeFromHS(lhs, rhs);
            int opndPairSeq = getSequenceTypeFromHS(lhs, rhs, IndVar);
            if (opcode == Instruction::Add) {
                return getAddInstTrendCode(opndPairSeq, lhs, rhs);
            } else if (opcode == Instruction::Sub) {
                return getSubInstTrendCode(opndPairSeq, lhs, rhs);
            } else if (opcode == Instruction::Mul) {
                return getMulInstTrendCode(opndPairSeq, lhs, rhs);
            } else if (opcode==Instruction::UDiv || opcode==Instruction::SDiv) {
                return getDivInstTrendCode(opndPairSeq, lhs, rhs);
            } else {
                errs()<< "WARNING: In getTrendCodeFromArithInst() method, an unknown arith opcode is met here.\n";
            }
        } else {
            errs()<< "WARNING: In getTrendCodeFromArithInst() method, the argu list contains NULL ptr.\n";
        }
        return 0;
    }

    bool detectLoop(Loop* LP) {
        errs()<< "DEBUG INFO: Enter the detectLoop() method...\n";
        bool ans = true;
        bool tmp = false;
        if (LP != nullptr) {
            ans = isFiniteLoop(LP);
            errs()<< "DEBUG INFO: In detectLoop() method, isFiniteLoop() is finished.\n";
            std::vector<Loop*> subLoops = LP->getSubLoops();
            if (subLoops.empty() == false) {
                Loop::iterator i = subLoops.begin();
                Loop::iterator e = subLoops.end();
                errs()<< "DEBUG INFO: In detectLoop() method, the number of sub loops is -- " << subLoops.size() << "\n";
                while (i!=e) {
                    errs()<< "DEBUG INFO: In detectLoop() method, start to call the detectLoop() method for subloops' detection.\n";
                    tmp = detectLoop(*i);
                    ans = ans && tmp;
                    errs()<< "DEBUG INFO: In detectLoop() method, a sub loop detection has finished.\n";
                    ++i;
                }
            }
        }
        return ans;
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
                for (auto* lp : LI) {
                    // TO-DO: Check if a BB in the LoopObj is a subloop's header
                    errs()<< "=====-----------Infinite Loop Check Report-----------=====\n";
                    if (detectLoop(lp)) {
                        errs()<< "Under current checking strategies, this loop is considered as a finite type ----- SAFE\n";
                    } else {
                        errs()<< "Under current checking strategies, this loop is considered as an infinite type ----- UNSAFE\n";
                    }
                    errs()<< "=====-----------------------End----------------------=====\n";
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