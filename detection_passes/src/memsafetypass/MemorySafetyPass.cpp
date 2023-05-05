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
    errs() << "Running memory safety pass on function: " << F.getName() << "\n";

    // Run points to analysis
    PointsToResult pointsToResult = runPointsToAnalysis(F);

    // Run Cell State Analysis
    CellStateAnalysis cellStateAnalysis = CellStateAnalysis(pointsToResult);
    cellStateAnalysis.runCellStateAnalysis(F);


    return false; 
}

// Register the pass with llvm, so that we can call it with fvpass
char MemorySafetyPass::ID = 0;
static RegisterPass<MemorySafetyPass> X("mspass", "Prints out each potentially unsafe memory access");


PointsToResult MemorySafetyPass::runPointsToAnalysis(Function &F) {
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
    PointsToResult result = solver.solve();

    return result;
}

PointsToResult PointsToSolver::solve(){

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

// class CellStateAnalysis {
// private:
//     enum CellState {
//         TOP,
//         BOTTOM,
//         HEAP_ALLOCATED,
//         STACK_ALLOCATED,
//         HEAP_FREED,
//     };

//     typedef std::map<Value *, CellState> MapState;
//     typedef std::map<Instruction *, MapState> Result;

// public:
//     Result runCellStateAnalysis(Function &F);
//     static void printResults(Result &result);





// };

CellStateAnalysis::Result CellStateAnalysis::runCellStateAnalysis(Function &F){

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

    // collect all instructions
    std::set<Instruction *> allInstructions;
    for (auto &B : F){
        for (auto &I : B){
            allInstructions.insert(&I);
        }
    }

    // initialize the worklist to contain all instructions
    std::deque<Instruction *> worklist(allInstructions.begin(), allInstructions.end());

    // initialize the result
    auto state = Result();

    // for each instruction, initialize the state to default MapState

    // default MapState: all eligible cells are BOTTOM
    auto defaultMapState = MapState();
    for (auto &cell : eligibleCells){
        defaultMapState[cell] = BOTTOM;
    }


    for (auto &I : allInstructions){
        state[I] = defaultMapState;
    }


    // run until the worklist is empty
    while(!worklist.empty()){
        auto &curr = worklist.front();
        worklist.pop_front();

        // lookup the instruction in the result
        auto &old = state[curr];

        // get predecessor instructions
        auto predInsts = std::vector<Instruction *>();
        for (auto &I : allInstructions){
            if (simplifiedCFG[I].find(curr) != simplifiedCFG[I].end()){
                predInsts.push_back(I);
            }
        }

        // get predecessor states
        auto predStates = std::vector<MapState>();
        for (auto &pred : predInsts){
            predStates.push_back(state[pred]);
        }

        // merge the predecessor states
        auto iterPredStates = predStates.begin();
        auto endPredStates = predStates.end();
        auto mergedPredState = *iterPredStates;
        ++iterPredStates;
        for (;iterPredStates != endPredStates; ++iterPredStates){
            mergedPredState = mergeMapStates(mergedPredState, *iterPredStates);
        }

        // TODO: update based on current instruction

        


        

        
    }

    return Result();


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
