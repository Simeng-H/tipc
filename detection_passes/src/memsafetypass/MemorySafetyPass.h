#pragma once

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"

#include "PointsToAnalysis.h"
#include "CellStateAnalysis.h"

#include <deque>
#include <vector>
#include <set>
#include <map>
#include <utility>

using namespace llvm;

class MemorySafetyPass : public FunctionPass
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
    PointsToSolver::PointsToResult runPointsToAnalysis(Function &F);
    std::vector<MsViolation> check_legality(
        Function &F, 
        PointsToSolver::PointsToResult &pointsToResult, 
        CellStateAnalysis::CsaResult &csaResult
    );
};



