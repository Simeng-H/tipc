#pragma once

using namespace llvm;

namespace {
    struct MemorySafetyPass : public FunctionPass {
        public:
            static char ID;
            MemorySafetyPass() : FunctionPass(ID) {}
            virtual bool runOnFunction(Function &F) override;
    };
}
