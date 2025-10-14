#pragma once
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"

struct EmitChakravyuhaReportPass
    : public llvm::PassInfoMixin<EmitChakravyuhaReportPass> {
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM);
};
