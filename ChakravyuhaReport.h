#pragma once
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Triple.h"
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace chakravyuha {

struct ReportData {
  std::string inputFile;
  std::string outputFile = "obfuscated.ll";
  std::string targetPlatform;
  std::string obfuscationLevel = "medium";
  bool enableStringEncryption = false;
  bool enableControlFlowFlattening = false;
  bool enableFakeCodeInsertion = false;
  unsigned cyclesCompleted = 1;

  // String encryption
  unsigned stringsEncrypted = 0;
  uint64_t originalIRStringDataSize = 0;
  uint64_t obfuscatedIRStringDataSize = 0;
  std::string stringMethod;

  // Control flow flattening
  unsigned flattenedFunctions = 0;
  unsigned flattenedBlocks = 0;
  unsigned skippedFunctions = 0;

  // Fake code insertion
  unsigned fakeCodeBlocksInserted = 0;

  std::vector<std::string> passesRun;

  static ReportData &get() {
    static ReportData R;
    return R;
  }
};

// Centralized function to detect unsafe constructs. Making it inline prevents
// linker errors.
inline bool shouldSkipFunction(llvm::Function &F) {
  for (llvm::BasicBlock &BB : F) {
    for (llvm::Instruction &I : BB) {
      if (auto *CI = llvm::dyn_cast<llvm::CallInst>(&I)) {
        if (CI->isInlineAsm()) {
          llvm::errs() << "INFO: Skipping function '" << F.getName()
                       << "' due to inline assembly.\n";
          return true;
        }
        llvm::Function *CalledFunc = CI->getCalledFunction();
        if (CalledFunc && (CalledFunc->getName() == "setjmp" ||
                           CalledFunc->getName() == "_setjmp" ||
                           CalledFunc->getName() == "longjmp")) {
          llvm::errs() << "INFO: Skipping function '" << F.getName()
                       << "' due to setjmp/longjmp.\n";
          return true;
        }
      }
    }
  }
  return false;
}

inline std::string esc(const std::string &S) {
  std::string T;
  T.reserve(S.size());
  for (char c : S) {
    if (c == '\\')
      T += "\\\\";
    else if (c == '"')
      T += "\\\"";
    else
      T += c;
  }
  return T;
}

inline std::string nowUtcIso8601() {
  std::time_t t = std::time(nullptr);
  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif
  char b[24];
  std::strftime(b, sizeof(b), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return b;
}

inline void finalizeDefaults(llvm::Module &M) {
  auto &R = ReportData::get();

  if (R.inputFile.empty()) {
    const std::string &SF = M.getSourceFileName();
    R.inputFile = SF.empty() ? std::string("<stdin>") : SF;
  }

  if (R.targetPlatform.empty()) {
    llvm::Triple T(M.getTargetTriple());
    R.targetPlatform = T.isOSWindows() ? "windows" : "linux";
  }
}

inline void emitReportJSON(llvm::Module &M) {
  finalizeDefaults(M);
  auto &R = ReportData::get();

  double changePct = 0.0;
  if (R.originalIRStringDataSize != 0) {
    changePct =
        (double)(R.obfuscatedIRStringDataSize - R.originalIRStringDataSize) /
        (double)R.originalIRStringDataSize * 100.0;
  }
  std::stringstream ss;
  ss << std::fixed << std::setprecision(2) << changePct;

  llvm::outs() << "{\n";
  llvm::outs() << "  \"inputFile\": \"" << esc(R.inputFile) << "\",\n";
  llvm::outs() << "  \"outputFile\": \"" << esc(R.outputFile) << "\",\n";
  llvm::outs() << "  \"timestamp\": \"" << nowUtcIso8601() << "\",\n";
  llvm::outs() << "  \"inputParameters\": {\n";
  llvm::outs() << "    \"obfuscationLevel\": \"" << esc(R.obfuscationLevel)
               << "\",\n";
  llvm::outs() << "    \"targetPlatform\": \"" << esc(R.targetPlatform)
               << "\",\n";
  llvm::outs() << "    \"enableStringEncryption\": "
               << (R.enableStringEncryption ? "true" : "false") << ",\n";
  llvm::outs() << "    \"enableControlFlowFlattening\": "
               << (R.enableControlFlowFlattening ? "true" : "false") << ",\n";
  llvm::outs() << "    \"enableFakeCodeInsertion\": "
               << (R.enableFakeCodeInsertion ? "true" : "false") << "\n";
  llvm::outs() << "  },\n";
  llvm::outs() << "  \"outputAttributes\": {\n";
  llvm::outs() << "    \"originalIRStringDataSize\": \""
               << R.originalIRStringDataSize << " bytes\",\n";
  llvm::outs() << "    \"obfuscatedIRStringDataSize\": \""
               << R.obfuscatedIRStringDataSize << " bytes\",\n";
  llvm::outs() << "    \"stringDataSizeChange\": \"" << ss.str() << "%\"\n";
  llvm::outs() << "  },\n";
  llvm::outs() << "  \"obfuscationMetrics\": {\n";
  llvm::outs() << "    \"cyclesCompleted\": " << R.cyclesCompleted << ",\n";
  llvm::outs() << "    \"passesRun\": [";
  for (size_t i = 0; i < R.passesRun.size(); ++i) {
    llvm::outs() << "\"" << esc(R.passesRun[i]) << "\"";
    if (i + 1 < R.passesRun.size())
      llvm::outs() << ", ";
  }
  llvm::outs() << "],\n";
  llvm::outs() << "    \"stringEncryption\": {\n";
  llvm::outs() << "      \"count\": " << R.stringsEncrypted << ",\n";
  llvm::outs() << "      \"method\": \""
               << esc(R.stringMethod.empty() ? "N/A" : R.stringMethod)
               << "\"\n";
  llvm::outs() << "    },\n";
  llvm::outs() << "    \"controlFlowFlattening\": {\n";
  llvm::outs() << "      \"flattenedFunctions\": " << R.flattenedFunctions
               << ",\n";
  llvm::outs() << "      \"flattenedBlocks\": " << R.flattenedBlocks << ",\n";
  llvm::outs() << "      \"skippedFunctions\": " << R.skippedFunctions << "\n";
  llvm::outs() << "    },\n";
  llvm::outs() << "    \"fakeCodeInsertion\": {\n";
  llvm::outs() << "      \"insertedBlocks\": " << R.fakeCodeBlocksInserted
               << "\n";
  llvm::outs() << "    }\n";
  llvm::outs() << "  }\n";
  llvm::outs() << "}\n";
}

} // namespace chakravyuha
