#pragma once

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"

#include <deque>
#include <vector>
#include <set>
#include <map>
#include <utility>

using namespace llvm;

typedef struct PointsToResult
{
    std::set<Value *> variables;
    std::map<Value *, std::set<Value *>> pointsToCells;
    std::map<Value *, std::set<Value *>> equivalentCells;
} PointsToResult;

namespace
{
    struct MemorySafetyPass : public FunctionPass
    {
    public:
        enum MsViolationType{
            DOUBLE_FREE,
            USE_AFTER_FREE,
            STACK_FREE,
        };

        typedef struct MsViolation{
            MsViolationType type;
            Instruction *inst;
            MsViolation(MsViolationType type, Instruction *inst) : type(type), inst(inst) {}
        } MsViolation;

        static char ID;
        MemorySafetyPass() : FunctionPass(ID) {}
        virtual bool runOnFunction(Function &F) override;
        PointsToResult runPointsToAnalysis(Function &F);

    };
}

class PointsToConstraint
{
public:
    enum Type
    {
        ALLOC,  // Initial Allocation (alloca / calloc)
        ASSIGN, // Direct Conversion (casts, int to pointer, etc.)
        LOAD,   // * Operator (Load)
        STORE   // * Operator (Store)
    };

    Type type;
    Value *src;
    Value *dest;

    PointsToConstraint(Type type, Value *src, Value *dest) : type(type), src(src), dest(dest) {}

    friend raw_ostream &operator<<(raw_ostream &os, const PointsToConstraint &constraint)
    {
        switch (constraint.type)
        {
        case ALLOC:
            os << "ALLOC: " << *constraint.src;
            break;
        case ASSIGN:
            os << "ASSIGN: "
               << "[" << *constraint.dest << "] = [" << *constraint.src << "]";
            break;
        case LOAD:
            os << "LOAD: "
               << "c ∈ [" << *constraint.src << "] -> [c] ⊆ [" << *constraint.dest << "] for each c";
            break;
        case STORE:
            os << "STORE: "
               << "c ∈ [" << *constraint.dest << "] -> [" << *constraint.src << "] ⊆ [c] for each c";
            break;
        }
        return os;
    }
};

class PointsToSolver
{
private:
    typedef Value Token;
    typedef Value Cell;

    std::vector<PointsToConstraint *> constraints;
    std::set<Value *> variables;
    std::vector<Instruction *> allocSites;
    std::set<Cell *> cells;
    std::map<Cell *, std::set<Cell *>> equivalentCells;

public:
    // Update the type of allocSites in the constructor as well
    PointsToSolver(std::vector<PointsToConstraint *> constraints, std::set<Value *> variables, std::vector<Instruction *> allocSites) : constraints(constraints), variables(variables), allocSites(allocSites)
    {
        for (auto *allocSite : allocSites)
        {
            cells.insert(allocSite);
        }
        for (auto *variable : variables)
        {
            cells.insert(variable);
        }

        // initialize equivalentCells
        for (auto *cell : cells)
        {
            equivalentCells[cell] = std::set<Cell *>();
            equivalentCells[cell].insert(cell);
        }
    }

    PointsToResult solve();
    static void printResults(PointsToResult &result);

private:
    std::map<Cell *, std::set<Token *>> sol;
    std::map<Cell *, std::set<Cell *>> succ;
    std::map<
        std::pair<Cell *, Token *>,
        std::set<
            std::pair<Cell *, Cell *>>>
        cond;

    std::deque<
        std::pair<Token *, Cell *>>
        worklist;

    void addToken(Token *t, Cell *x);
    void addEdge(Cell *x, Cell *y);
    void propogate();
};

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

    PointsToResult pointsToResult;
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

    CellStateAnalysis(PointsToResult pointsToResult) : pointsToResult(pointsToResult)
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