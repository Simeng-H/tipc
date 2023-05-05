#include "MemorySafetyPass.h"
#include "PointsToAnalysis.h"
#include "CellStateAnalysis.h"

#include <deque>
#include <vector>
#include <set>
#include <map>
#include <utility>

using namespace llvm;

CellStateAnalysis::CsaResult CellStateAnalysis::runCellStateAnalysis(Function &F){

    // debug print
    errs() << "\nRunning cell state analysis...\n";

    // auto &variables = pointsToResult.variables;
    // auto &pointsToCells = pointsToResult.pointsToCells;
    // auto &equivalentCells = pointsToResult.equivalentCells;


    // construct simplified CFG that only contains relevant instructions
    std::map<Instruction *, std::set<Instruction *>> simplifiedCFG = getSimplifiedSuccCFG(F);

    // debug print: simplified CFG
    errs() << "\nSimplified CFG:\n";
    for (auto &B : F){
        for (auto &I : B){
            errs() << "\t[" << I << "]:\n";
            for (auto &succ : simplifiedCFG[&I]){
                errs() << "\t\t[" << *succ << "]\n";
            }
        }
    }

    // debug print
    errs() << "Collecting all instructions...\n";

    // collect all instructions
    std::set<Instruction *> allInstructions;
    for (auto &B : F){
        for (auto &I : B){
            allInstructions.insert(&I);
        }
    }



    // initialize the worklist to contain all instructions
    std::deque<Instruction *> worklist(allInstructions.begin(), allInstructions.end());

    // debug print
    errs() << "Initializing the state lattice...\n";

    // initialize the result
    auto state = AnalysisState();
    // for each instruction, initialize the state to default MapState

    // default MapState: all eligible cells are BOTTOM
    auto defaultMapState = MapState();
    for (auto &cell : eligibleCells){
        defaultMapState[cell] = BOTTOM;
    }


    for (auto &I : allInstructions){
        state[I] = defaultMapState;
    }

    // debug print
    errs() << "Running the worklist algorithm to analyze cell state...\n";

    // run until the worklist is empty
    while(!worklist.empty()){
        auto curr = worklist.front();
        worklist.pop_front();

        // debug print
        errs() << "Analyzing instruction: [" << *curr << "]\n";

        // lookup the instruction in the result
        auto &old = state[curr];

        // // debug print
        // errs() << "\t Old state:\n";
        // for (auto &cell : eligibleCells){
        //     errs() << "\t\t[" << *cell << "]: ";
        //     switch (old[cell]){
        //         case TOP:
        //             errs() << "TOP\n";
        //             break;
        //         case BOTTOM:
        //             errs() << "BOTTOM\n";
        //             break;
        //         case HEAP_ALLOCATED:
        //             errs() << "HEAP_ALLOCATED\n";
        //             break;
        //         case STACK_ALLOCATED:
        //             errs() << "STACK_ALLOCATED\n";
        //             break;
        //         case HEAP_FREED:
        //             errs() << "HEAP_FREED\n";
        //             break;
        //     }
        // }

        // get predecessor instructions
        auto predInsts = std::vector<Instruction *>();
        for (auto &I : allInstructions){
            if (simplifiedCFG[I].find(curr) != simplifiedCFG[I].end()){
                predInsts.push_back(I);
            }
        }

        // // debug print
        // errs() << "\t Predecessor instructions:\n";
        // for (auto &I : predInsts){
        //     errs() << "\t\t[" << *I << "]\n";
        // }

        // get predecessor states
        auto predStates = std::vector<MapState>();
        for (auto &pred : predInsts){
            predStates.push_back(state[pred]);
        }

        // use all bottom if no predecessor
        if (predStates.empty()){
            predStates.push_back(defaultMapState);
        }

        // merge the predecessor states
        auto iterPredStates = predStates.begin();
        auto endPredStates = predStates.end();
        auto mergedPredState = *iterPredStates;
        ++iterPredStates;
        for (;iterPredStates != endPredStates; ++iterPredStates){
            mergedPredState = mergeMapStates(mergedPredState, *iterPredStates);
        }

        auto updatedMapState = mergedPredState;

        // update based on current instruction，alloca/calloc/free changes the state of the cell
        if (auto allocaInst = dyn_cast<AllocaInst>(curr)){
            updatedMapState[allocaInst] = STACK_ALLOCATED;
        }
        else if (auto callocInst = dyn_cast<CallInst>(curr)){
            if (callocInst->getCalledFunction()->getName() == "calloc"){
                updatedMapState[callocInst] = HEAP_ALLOCATED;
            }
        }
        else if (auto freeInst = dyn_cast<CallInst>(curr)){
            if (freeInst->getCalledFunction()->getName() == "free"){
                updatedMapState[freeInst] = HEAP_FREED;
            }
        }

        // check if MapState has changed since last time
        auto changed = false;
        for (auto &cell : eligibleCells){
            if (updatedMapState[cell] != old[cell]){
                changed = true;
                break;
            }
        }

        // if MapState has changed, update the result and add all successors to the worklist
        if (changed){
            state[curr] = updatedMapState;
            for (auto &succ : simplifiedCFG[curr]){
                worklist.push_back(succ);
            }
        }
    }

    // debug print
    errs() << "Cell state analysis finished.\n";

    auto ret = CsaResult();
    ret.eligibleCells = eligibleCells;
    ret.inst2CellStates = state;
    ret.analyzedInstructions = allInstructions;

    // print the result
    printResults(ret);

    return ret;

}

void CellStateAnalysis::printResults(CsaResult &result) {
    // debug print
    errs() << "Printing Cell State Analysis results...\n";

    auto &eligibleCells = result.eligibleCells;
    auto &inst2CellStates = result.inst2CellStates;
    auto &analyzedInstructions = result.analyzedInstructions;

    // print the result
    for (auto &I : analyzedInstructions){
        errs() << "Instruction: [" << *I << "]\n";
        errs() << "\t Cell states:\n";
        for (auto &cell : eligibleCells){
            errs() << "\t\t[" << *cell << "]: ";
            switch (inst2CellStates[I][cell]){
                case TOP:
                    errs() << "TOP\n";
                    break;
                case BOTTOM:
                    errs() << "BOTTOM\n";
                    break;
                case HEAP_ALLOCATED:
                    errs() << "HEAP_ALLOCATED\n";
                    break;
                case STACK_ALLOCATED:
                    errs() << "STACK_ALLOCATED\n";
                    break;
                case HEAP_FREED:
                    errs() << "HEAP_FREED\n";
                    break;
            }
        }
    }

}


std::map<Instruction *, std::set<Instruction *>> CellStateAnalysis::getSimplifiedSuccCFG(Function &F) {
    std::set<Instruction *> allInstructions;

    // get all instructions
    for (auto &B : F) {
        for (auto &I : B) {
            allInstructions.insert(&I);
        }
    }

    // initialize the result
    std::map<Instruction *, std::set<Instruction *>> simplifiedCFG;

    // for each instruction that's not a block terminator, its successors are the instructions that follow it in the same basic block
    // for each instruction that's a block terminator, its successors are the starts of all the blocks it can branch to
    for (auto &B : F) {
        for (auto I = B.begin(), E = B.end(); I != E; ++I) {
            if (!I->isTerminator()) {
                // Get the next instruction in the same basic block using iterators
                auto nextInstr = std::next(I);
                if (nextInstr != E) {
                    simplifiedCFG[&*I].insert(&*nextInstr);
                }
            } else {
                // Get the successor blocks and insert the first instruction of each block
                for (unsigned i = 0; i < I->getNumSuccessors(); ++i) {
                    BasicBlock *succBlock = I->getSuccessor(i);
                    if (!succBlock->empty()) {
                        simplifiedCFG[&*I].insert(&succBlock->front());
                    }
                }
            }
        }
    }

    return simplifiedCFG;
}

CellStateAnalysis::MapState CellStateAnalysis::mergeMapStates(CellStateAnalysis::MapState &state1, CellStateAnalysis::MapState &state2){
    auto result = MapState();

    // key-wise CellState merge
    for (auto &cell : eligibleCells){
        auto &state1Cell = state1[cell];
        auto &state2Cell = state2[cell];
        result[cell] = lub(state1Cell, state2Cell);
    }


    return result;

}

CellStateAnalysis::CellState CellStateAnalysis::lub(CellStateAnalysis::CellState &state1, CellStateAnalysis::CellState &state2){
    if (state1 == state2){
        return state1;
    }
    if (state1 == TOP || state2 == TOP){
        return TOP;
    }
    if (state1 == BOTTOM){
        return state2;
    }
    if (state2 == BOTTOM){
        return state1;
    }

    if (state1 == STACK_ALLOCATED || state2 == STACK_ALLOCATED){
        return TOP;
    }

    // both are not equal and not TOP and not BOTTOM and not STACK_ALLOCATED: must be HEAP_ALLOCATED and HEAP_FREED
    // if it can be FREED, we consider it FREED so we never double free/use after free
    return HEAP_FREED;

}