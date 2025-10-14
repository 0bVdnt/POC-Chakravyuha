#pragma once
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"

struct FakeCodeInsertionPass
    : public llvm::PassInfoMixin<FakeCodeInsertionPass> {
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM);
};
