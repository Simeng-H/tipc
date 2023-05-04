#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "MemorySafetyPass.h"

#include <vector>
#include <set>
#include <map>
#include <utility>

bool MemorySafetyPass::runOnFunction(Function &F) {

    runPointsToAnalysis(F);

    // errs() << "Examining function: " << F.getName() << "\n";

    // // identify all heap allocations and their instructions. Heal allocations are calls to calloc.
    // for (auto &B : F) {
    //     for (auto &I : B) {
    //         if (auto *callInst = dyn_cast<CallInst>(&I)) {
    //             if (Function *calledFunction = callInst->getCalledFunction()) {
    //                 if (calledFunction->getName() == "calloc") {
    //                     errs() << "Found calloc call: " << *callInst << "\n";
    //                 }
    //             }
                
    //         }
    //     }
    // }
    return false; 
}

// Register the pass with llvm, so that we can call it with fvpass
char MemorySafetyPass::ID = 0;
static RegisterPass<MemorySafetyPass> X("mspass", "Prints out each potentially unsafe memory access");


void MemorySafetyPass::runPointsToAnalysis(Function &F) {
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
    std::map<Value *, std::set<Value *>> pointsToMap = solver.solve();
}

std::map<Value *, std::set<Value *>> PointsToSolver::solve(){

    // debug print
    errs() << "Solving points to constraints:\n";

    for (auto &constraint : constraints){
        auto &src = constraint->src;
        auto &dest = constraint->dest;
        Cell *x, *y, *z;
        Instruction *inst;

        switch (constraint->type){
            case PointsToConstraint::Type::ALLOC:
                inst = dyn_cast<Instruction>(src);
                addToken(inst, dest);
                propogate();
                break;
            case PointsToConstraint::Type::STORE:
                x = dest;
                y = src;
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
                propogate();
                break;
        }
    }


    // debug print
    errs() << "\nPoints to solution:\n";

    std::map<Value *, std::set<Value *>> pointsToSolution;
    for(auto &var : variables){
        pointsToSolution[var] = sol[var];
        errs() << "Variable: " << *var << " points to \n";
        for (auto &cell : sol[var]){
            errs() << "\t" << *cell << "\n";
        }

    }
    return pointsToSolution;

}

void PointsToSolver::addToken(Token *t, Cell *x){
    auto &curr_sol = sol[x];
    // check if the token is already in the set
    if (curr_sol.find(t) == curr_sol.end()){
        curr_sol.insert(t);

        errs() << "Adding token: " << *t << " to cell: []" << *x << "]\n";
        // add the token to the worklist
        worklist.push_back(std::make_pair(t, x));
    }
}

void PointsToSolver::addEdge(Cell *x, Cell *y){

    // check if x and y are the same
    if (x == y){
        return;
    }
    auto &x_succ = succ[x];

    // check if y is already in x's succ
    if (x_succ.find(y) != x_succ.end()){
        return;
    }

    // add y to x's succ
    x_succ.insert(y);

    // debug print
    errs() << "Adding edge: [" << *x << "] -> [" << *y << "]\n";
    
    for (auto &t : sol[x]){
        addToken(t, y);
    }
}

void PointsToSolver::propogate(){
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