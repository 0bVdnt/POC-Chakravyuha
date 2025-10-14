#include "EmitReportPass.h"
#include "ChakravyuhaReport.h"

llvm::PreservedAnalyses
EmitChakravyuhaReportPass::run(llvm::Module &M, llvm::ModuleAnalysisManager &) {
  chakravyuha::emitReportJSON(M);
  return llvm::PreservedAnalyses::all();
}
