#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "MemorySafetyPass.h"
//include std list library
#include <set>

#define DEBUG_TYPE "memory-safety"
std::set<llvm::Instruction*> w;

bool MemorySafetyPass::runOnFunction(Function &F) {
    errs() << "Examining function: " << F.getName() << "\n";

    // identify all heap allocations and their instructions. Heal allocations are calls to calloc.
    for (auto &B: F) {
        for (auto &I: B) {
//            errs() << "Found store call: " << I << "\n";
            if (auto *callInst = dyn_cast<StoreInst>(&I)) {
                // get operands of the callInst
                Value *op1 = callInst->getOperand(0);
                Value *op2 = callInst->getOperand(1);
                // if op1 is a ptrtoint instruction
                errs() << "Found store call: " << *callInst << "\n";

                if (auto *ptrToIntInst = dyn_cast<PtrToIntInst>(op1)) {
//                    errs() << "Found store call: " << *callInst << "\n";
//                    errs() << "Found op1: " << *op1 << "\n";
//                    errs() << "Found op2: " << *op2 << "\n";
                    //if op2 is a alloc instruction, push it into the list w
                    if (auto *allocInst = dyn_cast<AllocaInst>(op2) ) {
                        errs() << "Found alloc: " << *allocInst << "\n";
                        w.insert(allocInst);
                    }
                    else if (auto *allocInst = dyn_cast<PtrToIntInst>(op2) ) {
                        errs() << "Found alloc: " << *allocInst << "\n";
                        w.insert(allocInst);
                    }
                    // find the variable that is being stored to
                    Value *var = ptrToIntInst->getOperand(0);
                    if (auto *allocInst = dyn_cast<AllocaInst>(var)) {
                        errs() << "Found alloc: " << *allocInst << "\n";
                        w.insert(allocInst);
                    }
//                    errs() << "Found var: " << *var << "\n";
                }
            }
        }
    }

    //  print the size of list w
    errs() << "Size of set w: " << w.size() << "\n";
}

// Register the pass with llvm, so that we can call it with fvpass
char MemorySafetyPass::ID = 0;
static RegisterPass<MemorySafetyPass> X("mspass", "Prints out each potentially unsafe memory access");
