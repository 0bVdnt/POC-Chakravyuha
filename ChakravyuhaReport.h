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

  // Total IR size metrics
  uint64_t originalIRSize = 0;
  uint64_t obfuscatedIRSize = 0;

  // String encryption metrics
  unsigned stringsEncrypted = 0;
  uint64_t originalIRStringDataSize = 0;
  uint64_t obfuscatedIRStringDataSize = 0;
  std::string stringMethod;

  // Control flow flattening metrics
  unsigned flattenedFunctions = 0;
  unsigned flattenedBlocks = 0;
  unsigned skippedFunctions = 0;

  // Fake code insertion metrics
  unsigned fakeCodeBlocksInserted = 0;

  std::vector<std::string> passesRun;

  static ReportData &get() {
    static ReportData R;
    return R;
  }
};

// Centralized function to detect unsafe constructs.
inline bool shouldSkipFunction(llvm::Function &F) {
  for (llvm::BasicBlock &BB : F) {
    for (llvm::Instruction &I : BB) {
      if (auto *CI = llvm::dyn_cast<llvm::CallInst>(&I)) {
        if (CI->isInlineAsm()) {
          return true;
        }
        llvm::Function *CalledFunc = CI->getCalledFunction();
        if (CalledFunc && (CalledFunc->getName() == "setjmp" ||
                           CalledFunc->getName() == "_setjmp" ||
                           CalledFunc->getName() == "longjmp")) {
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

// Helper function to calculate the size of the Module's textual IR
// representation.
inline uint64_t getModuleIRSize(llvm::Module &M) {
  std::string str;
  llvm::raw_string_ostream os(str);
  M.print(os, nullptr);
  os.flush(); // Ensure all data is written to the string
  return str.size();
}

inline void emitReportJSON(llvm::Module &M) {
  finalizeDefaults(M);
  auto &R = ReportData::get();

  // Calculate final IR size at the last possible moment.
  R.obfuscatedIRSize = getModuleIRSize(M);

  // --- Percentage Change Calculations ---
  double strChangePct = 0.0;
  if (R.originalIRStringDataSize != 0) {
    strChangePct =
        (double)(R.obfuscatedIRStringDataSize - R.originalIRStringDataSize) /
        (double)R.originalIRStringDataSize * 100.0;
  }
  std::stringstream ssString;
  ssString << std::fixed << std::setprecision(2) << strChangePct;

  double totalChangePct = 0.0;
  if (R.originalIRSize != 0) {
    totalChangePct = ((double)R.obfuscatedIRSize - (double)R.originalIRSize) /
                     (double)R.originalIRSize * 100.0;
  }
  std::stringstream ssTotal;
  ssTotal << std::fixed << std::setprecision(2) << totalChangePct;

  // --- JSON Output to stderr ---
  llvm::errs() << "{\n";
  llvm::errs() << "  \"inputFile\": \"" << esc(R.inputFile) << "\",\n";
  llvm::errs() << "  \"outputFile\": \"" << esc(R.outputFile) << "\",\n";
  llvm::errs() << "  \"timestamp\": \"" << nowUtcIso8601() << "\",\n";
  llvm::errs() << "  \"inputParameters\": {\n";
  llvm::errs() << "    \"obfuscationLevel\": \"" << esc(R.obfuscationLevel)
               << "\",\n";
  llvm::errs() << "    \"targetPlatform\": \"" << esc(R.targetPlatform)
               << "\",\n";
  llvm::errs() << "    \"enableStringEncryption\": "
               << (R.enableStringEncryption ? "true" : "false") << ",\n";
  llvm::errs() << "    \"enableControlFlowFlattening\": "
               << (R.enableControlFlowFlattening ? "true" : "false") << ",\n";
  llvm::errs() << "    \"enableFakeCodeInsertion\": "
               << (R.enableFakeCodeInsertion ? "true" : "false") << "\n";
  llvm::errs() << "  },\n";
  llvm::errs() << "  \"outputAttributes\": {\n";
  llvm::errs() << "    \"originalIRSize\": \"" << R.originalIRSize
               << " bytes\",\n";
  llvm::errs() << "    \"obfuscatedIRSize\": \"" << R.obfuscatedIRSize
               << " bytes\",\n";
  llvm::errs() << "    \"totalIRSizeChange\": \"" << ssTotal.str() << "%\",\n";
  llvm::errs() << "    \"originalIRStringDataSize\": \""
               << R.originalIRStringDataSize << " bytes\",\n";
  llvm::errs() << "    \"obfuscatedIRStringDataSize\": \""
               << R.obfuscatedIRStringDataSize << " bytes\",\n";
  llvm::errs() << "    \"stringDataSizeChange\": \"" << ssString.str()
               << "%\"\n";
  llvm::errs() << "  },\n";
  llvm::errs() << "  \"obfuscationMetrics\": {\n";
  llvm::errs() << "    \"cyclesCompleted\": " << R.cyclesCompleted << ",\n";
  llvm::errs() << "    \"passesRun\": [";
  for (size_t i = 0; i < R.passesRun.size(); ++i) {
    llvm::errs() << "\"" << esc(R.passesRun[i]) << "\"";
    if (i + 1 < R.passesRun.size())
      llvm::errs() << ", ";
  }
  llvm::errs() << "],\n";
  llvm::errs() << "    \"stringEncryption\": {\n";
  llvm::errs() << "      \"count\": " << R.stringsEncrypted << ",\n";
  llvm::errs() << "      \"method\": \""
               << esc(R.stringMethod.empty() ? "N/A" : R.stringMethod)
               << "\"\n";
  llvm::errs() << "    },\n";
  llvm::errs() << "    \"controlFlowFlattening\": {\n";
  llvm::errs() << "      \"flattenedFunctions\": " << R.flattenedFunctions
               << ",\n";
  llvm::errs() << "      \"flattenedBlocks\": " << R.flattenedBlocks << ",\n";
  llvm::errs() << "      \"skippedFunctions\": " << R.skippedFunctions << "\n";
  llvm::errs() << "    },\n";
  llvm::errs() << "    \"fakeCodeInsertion\": {\n";
  llvm::errs() << "      \"insertedBlocks\": " << R.fakeCodeBlocksInserted
               << "\n";
  llvm::errs() << "    }\n";
  llvm::errs() << "  }\n";
  llvm::errs() << "}\n";
}

} // namespace chakravyuha
