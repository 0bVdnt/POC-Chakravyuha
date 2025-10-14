#pragma once
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"

struct StringEncryptionPass : public llvm::PassInfoMixin<StringEncryptionPass> {
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM);
};
