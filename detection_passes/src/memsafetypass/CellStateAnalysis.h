#pragma once

#include "MemorySafetyPass.h"

#include <deque>
#include <vector>
#include <set>
#include <map>
#include <utility>

class CellStateAnalysis
{
public:
    enum CellState
    {
        TOP,
        BOTTOM,
        HEAP_ALLOCATED,
        STACK_ALLOCATED,
        HEAP_FREED,
    };
    typedef std::map<Value *, CellState> MapState;
    typedef std::map<Instruction *, MapState> AnalysisState;
    typedef struct CsaResult
    {
        std::set<Instruction *> analyzedInstructions;
        std::set<Value *> eligibleCells;
        AnalysisState inst2CellStates;
    } CsaResult;
private:

    PointsToSolver::PointsToResult pointsToResult;
    std::set<Value *> eligibleCells;

    std::map<Instruction *, std::set<Instruction *>> getSimplifiedSuccCFG(Function &F);
    bool isEligibleInstruction(Instruction *I)
    {

        // stack allocation
        if (isa<AllocaInst>(I))
        {
            return true;
        }
        // heap allocation
        if (isa<CallInst>(I))
        {
            if (I->getOperand(0)->getName() == "calloc")
            {
                return true;
            }
        }

        // load and store
        if (isa<LoadInst>(I))
        {
            return true;
        }
        if (isa<StoreInst>(I))
        {
            return true;
        }

        // ptr casts
        if (isa<BitCastInst>(I))
        {
            return true;
        }

        // int-ptr casts
        if (isa<IntToPtrInst>(I))
        {
            return true;
        }
        if (isa<PtrToIntInst>(I))
        {
            return true;
        }

        return false;
    }
    MapState mergeMapStates(MapState &state1, MapState &state2);
    CellState lub(CellState &state1, CellState &state2);

public:

    CellStateAnalysis(PointsToSolver::PointsToResult pointsToResult) : pointsToResult(pointsToResult)
    {
        auto &variables = pointsToResult.variables;
        auto &pointsToCells = pointsToResult.pointsToCells;
        auto &equivalentCells = pointsToResult.equivalentCells;

        // filter out cells that are not heap/stack allocations
        for (auto &var : variables)
        {

            // stack allocated cells are eligible
            if (isa<AllocaInst>(var))
            {
                eligibleCells.insert(var);
            }

            // heap allocated cells are eligible (i.e. calloc calls)
            if (isa<CallInst>(var))
            {
                auto call = dyn_cast<CallInst>(var);
                auto calledFunction = call->getCalledFunction();
                if (calledFunction->getName() == "calloc")
                {
                    eligibleCells.insert(var);
                }
            }
        }

        // debug print: all eligible cells
        errs() << "\nEligible cells:\n";
        for (auto &cell : eligibleCells)
        {
            errs() << "\t[" << *cell << "]\n";
        }
    }
    CsaResult runCellStateAnalysis(Function &F);
    static void printResults(CsaResult &result);
};