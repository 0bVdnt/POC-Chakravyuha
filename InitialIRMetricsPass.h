#pragma once
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"

struct InitialIRMetricsPass : public llvm::PassInfoMixin<InitialIRMetricsPass> {
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM);
};
