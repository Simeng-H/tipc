#include "MemorySafetyPass.h"
#include "PointsToAnalysis.h"
#include "CellStateAnalysis.h"

#include <deque>
#include <vector>
#include <set>
#include <map>
#include <utility>

using namespace llvm;

PointsToSolver::PointsToResult PointsToSolver::solve(){

    // debug print
    errs() << "Solving points to constraints:\n";

    for (auto &constraint : constraints){
        auto &src = constraint->src;
        auto &dest = constraint->dest;
        Cell *x, *y, *z;
        Instruction *inst;

        // debug print
        errs() << "Processing constraint:" << "\n" << "\t" << *constraint << "\n";

        switch (constraint->type){
            case PointsToConstraint::Type::ALLOC:
                // inst = dyn_cast<Instruction>(src);
                // addToken(inst, dest);
                // propogate();
                break;
            case PointsToConstraint::Type::STORE:
                x = dest;
                y = src;

                addToken(y, x);
                propogate();

                for (auto &c : cells){
                    if(sol[x].find(c) != sol[x].end()){
                        addEdge(y, c);
                        propogate();
                    } else {
                        auto condKey = std::make_pair(x, c);
                        auto newCond = std::make_pair(y, c);
                        cond[condKey].insert(newCond);
                    }
                }
                break;
            case PointsToConstraint::Type::LOAD:
                x = src;
                z = dest;
                for (auto &c : cells){
                    if (sol[x].find(c) != sol[x].end()){
                        addToken(c, z);
                        propogate();
                    } else {
                        auto condKey = std::make_pair(x, c);
                        auto newCond = std::make_pair(c, z);
                        cond[condKey].insert(newCond);
                    }
                }
                
                break;
            case PointsToConstraint::Type::ASSIGN:
                addEdge(src, dest);

                // debug print
                errs() << "Adding equivalence: " << *src << " = " << *dest << "\n";

                // equivalentCells[src].insert(dest);
                equivalentCells[dest].insert(src);
                propogate();
                break;
        }
    }


    // debug print
    errs() << "\nPoints to solution:\n\n";

    PointsToResult result;
    result.variables = cells;
    result.pointsToCells = sol;
    result.equivalentCells = equivalentCells;

    printResults(result);

    return result;

}

void PointsToSolver::printResults(PointsToResult &result){

    auto &variables = result.variables;
    auto &sol = result.pointsToCells;
    auto &equiv = result.equivalentCells;

    for(auto &var: variables){
        errs() << "Variable: " << *var;
        errs() << "\n\tPoints to set:\n";
        for (auto &cell : sol[var]){
            errs() << "\t\t" << *cell << "\n";
        }
        errs() << "\n\tEquivalent cells:\n";
        for (auto &equiv : equiv[var]){
            errs() << "\t\t" << *equiv << "\n";
        }
    }

}

void PointsToSolver::addToken(Token *t, Cell *x){
    auto &curr_sol = sol[x];
    // check if the token is already in the set
    if (curr_sol.find(t) == curr_sol.end()){
        curr_sol.insert(t);

        errs() << "Adding token: " << *t << " to cell: [" << *x << "]\n";
        // add the token to the worklist
        worklist.push_back(std::make_pair(t, x));

        // add equivalent cells as well
        for (auto &equiv : equivalentCells[t]){
            addToken(equiv, x);
        }
    }
}

void PointsToSolver::addEdge(Cell *x, Cell *y){

    // debug print
    errs() << "Trying to add edge: [" << *x << "] -> [" << *y << "]\n";
    // check if x and y are the same
    if (x == y){
        return;
    }
    auto &x_succ = succ[x];

    // check if y is already in x's succ
    if (!(x_succ.empty())){
        if (x_succ.find(y) != x_succ.end()){
            errs() << "Edge already exists: [" << *x << "] -> [" << *y << "]\n";
            errs() << "Succs of [" << *x << "]:\n";
            for (auto &s : x_succ){
                errs() << "\t[" << *s << "]\n";
            }
            return;
        }
    }

    // add y to x's succ
    x_succ.insert(y);

    // debug print
    errs() << "Added edge: [" << *x << "] -> [" << *y << "]\n";
    
    for (auto &t : sol[x]){
        addToken(t, y);
    }
}

void PointsToSolver::propogate(){

    // debug print
    errs() << "Propogating...\n";

    while (!worklist.empty()){
        auto curr = worklist.front();
        auto &t = curr.first;
        auto &x = curr.second;
        worklist.pop_front();

        auto condKey = std::make_pair(x,t);
        for(auto &thisCond: cond[condKey]){
            auto &y = thisCond.first;
            auto &z = thisCond.second;
            addEdge(y, z);
        }
        for (auto &y : succ[x])
        {
            addToken(t, y);
        }
    }
}