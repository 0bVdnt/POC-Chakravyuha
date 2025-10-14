#include "InitialIRMetricsPass.h"
#include "ChakravyuhaReport.h"
#include "llvm/Support/raw_ostream.h"

static uint64_t getModuleIRSize(llvm::Module &M) {
  std::string str;
  llvm::raw_string_ostream os(str);
  M.print(os, nullptr);
  os.flush();
  return str.size();
}

llvm::PreservedAnalyses
InitialIRMetricsPass::run(llvm::Module &M, llvm::ModuleAnalysisManager &) {
  auto &R = chakravyuha::ReportData::get();
  R.originalIRSize = getModuleIRSize(M);

  return llvm::PreservedAnalyses::all();
}
