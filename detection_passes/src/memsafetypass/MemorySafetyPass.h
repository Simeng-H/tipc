#pragma once

#include <deque>
#include <vector>
#include <set>
#include <map>
#include <utility>

using namespace llvm;

namespace {
    struct MemorySafetyPass : public FunctionPass {
        public:
            static char ID;
            MemorySafetyPass() : FunctionPass(ID) {}
            virtual bool runOnFunction(Function &F) override;
            void runPointsToAnalysis(Function &F);
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

public:
    // Update the type of allocSites in the constructor as well
    PointsToSolver(std::vector<PointsToConstraint *> constraints, std::set<Value *> variables, std::vector<Instruction *> allocSites) : constraints(constraints), variables(variables), allocSites(allocSites) {
        for (auto *allocSite : allocSites) {
            cells.insert(allocSite);
        }
        for (auto *variable : variables) {
            cells.insert(variable);
        }
    }

    std::map<Cell *, std::set<Token *>> solve();
    void printResults();

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

