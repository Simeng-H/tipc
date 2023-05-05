#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "MemorySafetyPass.h"
#include "PointsToAnalysis.h"
#include "CellStateAnalysis.h"

#include <vector>
#include <set>
#include <map>
#include <utility>
#include <stack>

bool MemorySafetyPass::runOnFunction(Function &F) {
    errs() << "Running memory safety pass on function: " << F.getName() << "\n";

    // Run points to analysis
    PointsToSolver::PointsToResult pointsToResult = runPointsToAnalysis(F);

    // Run Cell State Analysis
    CellStateAnalysis cellStateAnalysis = CellStateAnalysis(pointsToResult);
    CellStateAnalysis::CsaResult csaResult = cellStateAnalysis.runCellStateAnalysis(F);

    // Run legality check
    MsaResult violations = checkLegality(F, pointsToResult, csaResult);

    // Print results
    printResults(violations);
    
    


    return false; 
}

// Register the pass with llvm, so that we can call it with fvpass
char MemorySafetyPass::ID = 0;
static RegisterPass<MemorySafetyPass> X("mspass", "Prints out each potentially unsafe memory access");


PointsToSolver::PointsToResult MemorySafetyPass::runPointsToAnalysis(Function &F) {
    errs() << "Running points to analysis on function: " << F.getName() << "\n";

    auto allocSites = std::vector<Instruction *>();
    auto variables = std::set<Value *>();
    auto constraints = std::vector<PointsToConstraint *>();

    for (auto &B : F) {
        for (auto &I : B) {

            // Collect all heap allocations.
            if (auto *callInst = dyn_cast<CallInst>(&I)) {
                if (Function *calledFunction = callInst->getCalledFunction()) {
                    if (calledFunction->getName() == "calloc") {
                        allocSites.push_back(callInst);
                        variables.insert(callInst);
                        constraints.push_back(new PointsToConstraint(PointsToConstraint::Type::ALLOC, callInst, callInst));

                        // Debug Print
                        errs() << "Found calloc call: " << *callInst << "\n";
                        errs() << "\t Generated constraint: " << *constraints.back() << "\n";
                    }
                }
            }

            // Collect all stack allocations.
            if (auto *allocaInst = dyn_cast<AllocaInst>(&I)) {
                allocSites.push_back(allocaInst);
                variables.insert(allocaInst);
                constraints.push_back(new PointsToConstraint(PointsToConstraint::Type::ALLOC, allocaInst, allocaInst));

                // Debug Print
                errs() << "Found alloca call: " << *allocaInst << "\n";
                errs() << "\t Generated constraint: " << *constraints.back() << "\n";
            }

            // Collect all store pointer assignments.
            if (auto *storeInst = dyn_cast<StoreInst>(&I)) {
                Value *src = storeInst->getValueOperand();

                // skip constant assignment stores
                if (isa<Constant>(src)) {
                    continue;
                }

                Value *dest = storeInst->getPointerOperand();
                variables.insert(src);
                variables.insert(dest);
                constraints.push_back(new PointsToConstraint(PointsToConstraint::Type::STORE, src, dest));

                // Debug Print
                errs() << "Found store instruction: " << *storeInst << "\n";
                errs() << "\t Generated constraint: " << *constraints.back() << "\n";
            }

            // Collect all load pointer assignments.
            if (auto *loadInst = dyn_cast<LoadInst>(&I)) {
                Value *src = loadInst->getPointerOperand();
                Value *dest = loadInst;
                variables.insert(src);
                variables.insert(dest);
                constraints.push_back(new PointsToConstraint(PointsToConstraint::Type::LOAD, src, dest));

                // Debug Print
                errs() << "Found load instruction: " << *loadInst << "\n";
                errs() << "\t Generated constraint: " << *constraints.back() << "\n";
            }

            // Collect all pointer casts.
            if (auto *castInst = dyn_cast<CastInst>(&I)) {
                Value *src = castInst->getOperand(0);
                variables.insert(src);
                variables.insert(castInst);
                constraints.push_back(new PointsToConstraint(PointsToConstraint::Type::ASSIGN, src, castInst));

                // Debug Print
                errs() << "Found cast instruction: " << *castInst << "\n";
                errs() << "\t Generated constraint: " << *constraints.back() << "\n";
            }

            // Collect all pointer-to-int and int-to-pointer casts.
            if (auto *intToPtrInst = dyn_cast<IntToPtrInst>(&I)) {
                Value *src = intToPtrInst->getOperand(0);
                variables.insert(src);
                variables.insert(intToPtrInst);
                constraints.push_back(new PointsToConstraint(PointsToConstraint::Type::ASSIGN, src, intToPtrInst));

                // Debug Print
                errs() << "Found intToPtr instruction: " << *intToPtrInst << "\n";
                errs() << "\t Generated constraint: " << *constraints.back() << "\n";

            } else if (auto *ptrToIntInst = dyn_cast<PtrToIntInst>(&I)) {
                Value *src = ptrToIntInst->getOperand(0);
                variables.insert(src);
                variables.insert(ptrToIntInst);
                constraints.push_back(new PointsToConstraint(PointsToConstraint::Type::ASSIGN, src, ptrToIntInst));

                // Debug Print
                errs() << "Found ptrToInt instruction: " << *ptrToIntInst << "\n";
                errs() << "\t Generated constraint: " << *constraints.back() << "\n";
            }
        }
    }

    // Run the actual cubic solver
    PointsToSolver solver = PointsToSolver(constraints, variables, allocSites);
    PointsToSolver::PointsToResult result = solver.solve();

    return result;
}

std::vector<MemorySafetyPass::MsViolation> MemorySafetyPass::checkLegality(
        Function &F, 
        PointsToSolver::PointsToResult &pointsToResult, 
        CellStateAnalysis::CsaResult &csaResult
    )
{
    auto &allInsts = csaResult.analyzedInstructions;
    auto &allCells = csaResult.eligibleCells;
    auto &inst2CellStates = csaResult.inst2CellStates;
    auto &pointsToSets = pointsToResult.pointsToCells;
    auto &equivalentCells = pointsToResult.equivalentCells;

    // initialize result vector
    auto result = std::vector<MsViolation>();

    // Iterate over all instructions and check if they are load/store/free
    for (auto &inst : allInsts) {
        
        errs() << "Checking instruction: " << *inst << "\n";

        auto referencedMemoryCells = std::vector<Value *>();
        auto &cellStates = inst2CellStates[inst];

        if (auto *loadInst = dyn_cast<LoadInst>(inst)) {
            Value *src = loadInst->getPointerOperand();
            referencedMemoryCells.push_back(src);
        } else if (auto *storeInst = dyn_cast<StoreInst>(inst)) {
            Value *dest = storeInst->getPointerOperand();
            referencedMemoryCells.push_back(dest);
        } else if (auto *freeInst = dyn_cast<CallInst>(inst)) {
            if (Function *calledFunction = freeInst->getCalledFunction()) {
                if (calledFunction->getName() == "free") {
                    Value *src = freeInst->getArgOperand(0);
                    referencedMemoryCells.push_back(src);
                }
            }
        }

        // add all direct and transitive equivalent cells to the referenced cells via DFS
        for (auto *cell : referencedMemoryCells) {
            std::stack<Value *> dfsStack;
            dfsStack.push(cell);
            while (!dfsStack.empty()) {
                Value *currentCell = dfsStack.top();
                dfsStack.pop();
                if (equivalentCells.find(currentCell) != equivalentCells.end()) {
                    for (auto *equivalentCell : equivalentCells[currentCell]) {
                        if (std::find(referencedMemoryCells.begin(), referencedMemoryCells.end(), equivalentCell) == referencedMemoryCells.end()) {
                            dfsStack.push(equivalentCell);
                        }
                    }
                }
                if (std::find(referencedMemoryCells.begin(), referencedMemoryCells.end(), currentCell) == referencedMemoryCells.end()) {
                    referencedMemoryCells.push_back(currentCell);
                }
            }
        }

        // debug print
        errs() << "\t Referenced cells: \n";
        for (auto *cell : referencedMemoryCells) {
            errs() << "\t\t" << *cell << "\n";
        }

        // check if all referenced cells are safe
        if (auto *loadInst = dyn_cast<LoadInst>(inst)){
            for (auto *cell : referencedMemoryCells) {
                if(cellStates.count(cell) == 0) {
                    continue;
                }
                auto &cellState = cellStates[cell];
                if (cellState == CellStateAnalysis::CellState::HEAP_FREED) {
                    auto violation = MsViolation(MsViolationType::USE_AFTER_FREE, inst);
                    result.push_back(violation);
                }
            }
        } else if (auto *storeInst = dyn_cast<StoreInst>(inst)) {
            for (auto *cell : referencedMemoryCells) {
                if(cellStates.count(cell) == 0) {
                    continue;
                }
                auto &cellState = cellStates[cell];
                if (cellState == CellStateAnalysis::CellState::HEAP_FREED) {
                    auto violation = MsViolation(MsViolationType::USE_AFTER_FREE, inst);
                    result.push_back(violation);
                }
            }
        } else if (auto *freeInst = dyn_cast<CallInst>(inst)) {
            if (Function *calledFunction = freeInst->getCalledFunction()) {
                if (calledFunction->getName() == "free") {
                    for (auto *cell : referencedMemoryCells) {
                        if(cellStates.count(cell) == 0) {
                            continue;
                        }
                        auto &cellState = cellStates[cell];
                        if (cellState == CellStateAnalysis::CellState::HEAP_FREED) {
                            auto violation = MsViolation(MsViolationType::DOUBLE_FREE, inst);
                            result.push_back(violation);
                        } else if (cellState == CellStateAnalysis::CellState::HEAP_ALLOCATED) {
                            auto violation = MsViolation(MsViolationType::STACK_FREE, inst);
                            result.push_back(violation);
                        }
                    }
                }
            }
        }

    }

    return result;
}

void MemorySafetyPass::printResults(MsaResult &msaResult){
    errs() << "Memory Safety Analysis Results:\n";
    for(auto resultItem : msaResult){
        auto type = resultItem.type;
        auto inst = resultItem.inst;
        auto typeString = "";
        switch(type){
            case MsViolationType::USE_AFTER_FREE:
                typeString = "Use after free";
                break;
            case MsViolationType::DOUBLE_FREE:
                typeString = "Double free";
                break;
            case MsViolationType::STACK_FREE:
                typeString = "Freeing non-heap memory";
                break;
        }

        errs() << "\t" << typeString << " in " << *inst << "\n";
    }


}