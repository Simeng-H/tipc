#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "MemorySafetyPass.h"

bool MemorySafetyPass::runOnFunction(Function &F) {
    errs() << "Examining function: " << F.getName() << "\n";

    // identify all heap allocations and their instructions. Heal allocations are calls to calloc.
    for (auto &B : F) {
        for (auto &I : B) {
            if (auto *callInst = dyn_cast<CallInst>(&I)) {
                if (Function *calledFunction = callInst->getCalledFunction()) {
                    if (calledFunction->getName() == "calloc") {
                        errs() << "Found calloc call: " << *callInst << "\n";
                    }
                }
                
            }
        }
    }
    return false; 
}

// Register the pass with llvm, so that we can call it with fvpass
char MemorySafetyPass::ID = 0;
static RegisterPass<MemorySafetyPass> X("mspass", "Prints out each potentially unsafe memory access");
