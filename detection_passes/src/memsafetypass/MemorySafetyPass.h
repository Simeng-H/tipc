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


namespace {
    struct MemorySafetyPass : public FunctionPass {
        public:
            static char ID;
            MemorySafetyPass() : FunctionPass(ID) {}
            virtual bool runOnFunction(Function &F) override;
            PointsToResult runPointsToAnalysis(Function &F);
    };
}



class PointsToConstraint {
public:
    enum Type {
        ALLOC,       // Initial Allocation (alloca / calloc)
        ASSIGN,      // Direct Conversion (casts, int to pointer, etc.)
        LOAD,        // * Operator (Load)
        STORE        // * Operator (Store)
    };

    Type type;
    Value *src;
    Value *dest;

    PointsToConstraint(Type type, Value *src, Value *dest) : type(type), src(src), dest(dest) {}

    friend raw_ostream &operator<<(raw_ostream &os, const PointsToConstraint &constraint) {
        switch (constraint.type) {
            case ALLOC:
                os << "ALLOC: " << *constraint.src; 
                break;
            case ASSIGN:
                os << "ASSIGN: " <<"[" << *constraint.dest << "] = [" << *constraint.src << "]";
                break;
            case LOAD:
                os << "LOAD: " <<"c ∈ [" << *constraint.src << "] -> [c] ⊆ [" << *constraint.dest << "] for each c";
                break;
            case STORE:
                os << "STORE: " <<"c ∈ [" << *constraint.dest << "] -> [" << *constraint.src << "] ⊆ [c] for each c";
                break;
        }
        return os;
    }
};

class PointsToSolver {
private:
    typedef Value Token;
    typedef Value Cell;

    std::vector<PointsToConstraint *> constraints;
    std::set<Value *> variables;
    std::vector<Instruction *> allocSites;
    std::set<Cell*> cells;
    std::map<Cell*, std::set<Cell*>> equivalentCells;

public:
    // Update the type of allocSites in the constructor as well
    PointsToSolver(std::vector<PointsToConstraint *> constraints, std::set<Value *> variables, std::vector<Instruction *> allocSites) : constraints(constraints), variables(variables), allocSites(allocSites) {
        for (auto *allocSite : allocSites) {
            cells.insert(allocSite);
        }
        for (auto *variable : variables) {
            cells.insert(variable);
        }

        // initialize equivalentCells
        for (auto *cell : cells) {
            equivalentCells[cell] = std::set<Cell*>();
            equivalentCells[cell].insert(cell);
        }
    }

    PointsToResult solve();
    static void printResults(PointsToResult &result);

private:
    std::map<Cell *, std::set<Token *>> sol;
    std::map<Cell *, std::set<Cell *>> succ;
    std::map<
        std::pair<Cell*, Token*>,
        std::set<
            std::pair<Cell*, Cell*>
        >
    > cond;

    std::deque<
        std::pair<Token *, Cell *>
    > worklist;

    void addToken(Token *t, Cell *x);
    void addEdge(Cell *x, Cell *y);
    void propogate();
};

