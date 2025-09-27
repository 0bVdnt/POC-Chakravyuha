#include "llvm/IR/Constants.h"       // For ConstantDataArray, GlobalVariable
#include "llvm/IR/Function.h"        // For Function, FunctionType
#include "llvm/IR/GlobalVariable.h"  // For GlobalVariable
#include "llvm/IR/IRBuilder.h"       // To easily build LLVM instructions
#include "llvm/IR/Instructions.h"    // For Instruction, CallInst, etc.
#include "llvm/IR/Module.h"          // For Module, the top-level container
#include "llvm/IR/PassManager.h"     // For PassInfoMixin, ModuleAnalysisManager
#include "llvm/Passes/PassBuilder.h" // For PassBuilder and plugin registration
#include "llvm/Passes/PassPlugin.h"  // For PassPluginLibraryInfo
#include "llvm/Support/raw_ostream.h"          // For errs(), raw_string_ostream
#include "llvm/Transforms/Utils/ModuleUtils.h" // For appendToCompilerUsed

#include <chrono>  // For generating a timestamp for the report
#include <iomanip> // For formatting the timestamp
#include <sstream> // For formatting doubles in the report
#include <string>
#include <vector>

using namespace llvm;

// Define a simple, fixed XOR key for encryption/decryption.
// In a real product, this would be more dynamic or complex.
static const char XOR_KEY = 0xAB;

// Encrypts the input string data using a simple XOR cipher.
// Returns a vector of characters containing the encrypted data.
std::vector<char> encryptString(StringRef S) {
  std::vector<char> Encrypted(S.begin(), S.end());
  for (char &C : Encrypted) {
    C ^= XOR_KEY;
  }
  return Encrypted;
}

// This is the main struct for the LLVM pass.
// It inherits from PassInfoMixin to integrate with the new Pass Manager.
struct StringEncryptionPass : public PassInfoMixin<StringEncryptionPass> {
  // The main entry point for the pass. It is executed for each module.
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    bool Changed = false; // Flag to track if the module's IR is modified.
    std::vector<GlobalVariable *> StringGlobalsToEncrypt;

    // --- Metrics for the final report ---
    unsigned int encryptedStringsCount = 0;
    unsigned int originalStringDataSize = 0;
    unsigned int encryptedStringDataSize = 0;

    // --- 1. Find all global string constants to encrypt ---
    // Iterate over all global variables in the module.
    for (GlobalVariable &GV : M.globals()) {
      // Considering global variables that are constant and have an initializer.
      if (GV.isConstant() && GV.hasInitializer() &&
          GV.getInitializer()->getType()->isAggregateType()) {
        // Check if the initializer is a ConstantDataArray (e.g., a string
        // literal).
        if (ConstantDataArray *CDA =
                dyn_cast<ConstantDataArray>(GV.getInitializer())) {
          // LLVM identifies string literals as ConstantDataArrays of integers.
          if (CDA->isString()) {
            // Target strings created by Clang, which are often named ".str.".
            if (GV.getName().starts_with(".str.") ||
                GV.getName().starts_with(".str")) {
              StringGlobalsToEncrypt.push_back(&GV);
            }
          }
        }
      }
    }

    // If there are no strings to encrypt, the process completes. Generate a
    // report and exit.
    if (StringGlobalsToEncrypt.empty()) {
      generateReport(M.getSourceFileName(), "obfuscated.ll",
                     encryptedStringsCount, originalStringDataSize,
                     encryptedStringDataSize);
      return PreservedAnalyses::all(); // No changes were made.
    }

    // --- 2. Inject the decryption function into the module ---
    FunctionCallee DecryptFunc = injectDecryptionStub(M);
    LLVMContext &Ctx = M.getContext();
    // Get pointers to common types that will be needed later.
    PointerType *Int8PtrTy = PointerType::get(Ctx, 0); // i8* type
    Type *Int32Ty = Type::getInt32Ty(Ctx);             // i32 type

    // --- 3. Process each identified string ---
    for (GlobalVariable *GV : StringGlobalsToEncrypt) {
      // Get the string data from the global variable's initializer.
      StringRef OriginalStringRef =
          cast<ConstantDataArray>(GV->getInitializer())->getAsString();

      // Skip empty strings.
      if (OriginalStringRef.empty())
        continue;

      // Update metrics for the report.
      encryptedStringsCount++;
      originalStringDataSize += OriginalStringRef.size();

      // Log to stderr that the current string is being encrypted.
      errs() << "Chakravyuha StringEncrypt: Encrypting string -> "
             << OriginalStringRef << "\n";

      // Encrypt the string data.
      std::vector<char> EncryptedBytes = encryptString(OriginalStringRef);
      // The original string has a null terminator (\0). It must also be
      // "encrypted" to avoid having a plaintext \0 in the encrypted data, which
      // might prematurely terminate string operations.
      EncryptedBytes.back() = '\0' ^ XOR_KEY;

      encryptedStringDataSize += EncryptedBytes.size();

      // --- 4. Create a new global variable with the encrypted data ---
      // Define the LLVM array type for the encrypted data.
      ArrayType *ArrTy =
          ArrayType::get(Type::getInt8Ty(Ctx), EncryptedBytes.size());
      // Create a constant array with the encrypted byte values.
      Constant *EncryptedConst =
          ConstantDataArray::get(Ctx, ArrayRef<char>(EncryptedBytes));

      // Create a new global variable to hold this encrypted constant.
      // It has private linkage, meaning it's only visible within this module.
      GlobalVariable *EncryptedGV =
          new GlobalVariable(M, ArrTy, true, GlobalValue::PrivateLinkage,
                             EncryptedConst, GV->getName() + ".enc");
      // Add the new global to the `llvm.compiler.used` list to prevent the
      // optimizer from removing it if it thinks it's unused.
      appendToCompilerUsed(M, {EncryptedGV});

      // --- 5. Replace all uses of the original string with a call to the
      // decryption function ---
      std::vector<Use *> UsesToReplace;
      // Collect all uses of the original global variable. Collecting them first
      // because modifying uses while iterating over them can invalidate
      // iterators.
      for (Use &U : GV->uses()) {
        UsesToReplace.push_back(&U);
      }

      for (Use *U : UsesToReplace) {
        User *CurrentUser = U->getUser();
        Instruction *InsertionPoint = nullptr;

        // The user of the global variable must be an instruction, so decryption
        // logic can be inserted before it.
        if (Instruction *Inst = dyn_cast<Instruction>(CurrentUser)) {
          InsertionPoint = Inst;
        } else if (Constant *C = dyn_cast<Constant>(CurrentUser)) {
          // Handling constant expressions that use the string address is
          // complex. For this PoC, just printing a warning and skipping it.
          errs() << "Chakravyuha StringEncrypt: Warning - Constant user of GV "
                    "not handled, skipping: "
                 << *C << "\n";
          continue;
        } else {
          errs() << "Chakravyuha StringEncrypt: Warning - Unexpected user type "
                    "of GV, skipping: "
                 << *CurrentUser << "\n";
          continue;
        }

        if (!InsertionPoint)
          continue;

        // --- 6. Build the decryption call instructions ---
        IRBuilder<> Builder(InsertionPoint);

        // Get a pointer to the first element of new encrypted global
        // variable.
        Value *Zero = Builder.getInt64(0);
        Value *EncryptedBasePtr = Builder.CreateInBoundsGEP(
            ArrTy, EncryptedGV, {Zero, Zero}, "encryptedPtr");

        // The decryption stub expects an i8*, so cast the pointer if necessary.
        Value *EncryptedArgPtr = EncryptedBasePtr;
        if (EncryptedArgPtr->getType() != Int8PtrTy) {
          EncryptedArgPtr = Builder.CreateBitCast(EncryptedArgPtr, Int8PtrTy,
                                                  "encryptedPtrCast");
        }

        // Allocate space on the stack to hold the decrypted string at runtime.
        Value *DecryptedStringAlloca =
            Builder.CreateAlloca(ArrTy, nullptr, GV->getName() + ".dec.alloca");

        // Cast the alloca pointer to i8* for the function call.
        Value *DecryptedAllocaPtr = DecryptedStringAlloca;
        if (DecryptedAllocaPtr->getType() != Int8PtrTy) {
          DecryptedAllocaPtr = Builder.CreateBitCast(
              DecryptedAllocaPtr, Int8PtrTy, "decryptedAllocaPtrCast");
        }

        // Create the call to the above defined decryption function.
        // void chakravyuha_decrypt_string(i8* dest, i8* src, i32 len)
        Builder.CreateCall(DecryptFunc,
                           {DecryptedAllocaPtr, // Destination
                            EncryptedArgPtr,    // Source (encrypted)
                            Builder.getInt32(EncryptedBytes.size())}, // Length
                           "");

        // This is the key step: replace the original use of the plaintext
        // string with the pointer to the stack space where the decrypted string
        // now resides.
        U->set(DecryptedAllocaPtr);
        Changed = true; // Mark that the IR has been modified.
      }

      // --- 7. Clean up the original global variable ---
      // After replacing all uses, the original GV should have no users.
      // If it does, something went wrong.
      if (!GV->user_empty()) {
        errs() << "Chakravyuha StringEncrypt: ERROR - Original GV still has "
                  "users after replacement: "
               << *GV << "\n";
        for (User *U_dbg : GV->users()) {
          errs() << "  Remaining user: " << *U_dbg << "\n";
        }
      }
      // Erase the now-unused original global variable from the module.
      GV->eraseFromParent();
    }

    // Generate the final JSON report with all the collected metrics.
    generateReport(M.getSourceFileName(), "obfuscated.ll",
                   encryptedStringsCount, originalStringDataSize,
                   encryptedStringDataSize);

    if (Changed) {
      // If changes were made, any analyses cannot be preserved.
      return PreservedAnalyses::none();
    }
    // If no changes were made, all analyses are still valid.
    return PreservedAnalyses::all();
  }

  // This function injects a C-style function into the module that can decrypt a
  // string. The function signature is: void chakravyuha_decrypt_string(char*
  // dest, const char* src, int len)
  FunctionCallee injectDecryptionStub(Module &M) {
    LLVMContext &Ctx = M.getContext();
    // Define the types for the function signature.
    Type *Int8PtrTy = PointerType::get(Ctx, 0);
    Type *Int32Ty = Type::getInt32Ty(Ctx);
    Type *VoidTy = Type::getVoidTy(Ctx);

    // Create the function type: void(i8*, i8*, i32)
    FunctionType *DecryptFTy =
        FunctionType::get(VoidTy, {Int8PtrTy, Int8PtrTy, Int32Ty}, false);

    // Check if the function has already been injected to avoid duplicates.
    Function *DecryptF = M.getFunction("chakravyuha_decrypt_string");
    if (!DecryptF) {
      // Create the function if it doesn't exist.
      DecryptF = Function::Create(DecryptFTy, GlobalValue::PrivateLinkage,
                                  "chakravyuha_decrypt_string", M);
      DecryptF->setCallingConv(
          CallingConv::C); // Use standard C calling convention.
      // Add attributes to prevent the optimizer from inlining or modifying it
      // in unexpected ways.
      DecryptF->addFnAttr(Attribute::NoInline);
      DecryptF->addFnAttr(Attribute::NoUnwind);

      // Name the function arguments for better readability in the IR.
      Function::arg_iterator ArgIt = DecryptF->arg_begin();
      Argument *DestPtr = ArgIt++;
      DestPtr->setName("dest_ptr");
      Argument *SrcPtr = ArgIt++;
      SrcPtr->setName("src_ptr");
      Argument *Length = ArgIt++;
      Length->setName("length");

      // --- Build the function body ---
      // Create the basic blocks: entry, loop header, loop body, and exit.
      BasicBlock *EntryBB = BasicBlock::Create(Ctx, "entry", DecryptF);
      IRBuilder<> Builder(EntryBB);

      BasicBlock *LoopHeader = BasicBlock::Create(Ctx, "loop_header", DecryptF);
      BasicBlock *LoopBody = BasicBlock::Create(Ctx, "loop_body", DecryptF);
      BasicBlock *LoopExit = BasicBlock::Create(Ctx, "loop_exit", DecryptF);

      // The entry block unconditionally jumps to the loop header.
      Builder.CreateBr(LoopHeader);

      // At the loop header, set up the loop's PHI node for the index.
      Builder.SetInsertPoint(LoopHeader);
      // PHI node: index starts at 0 from the entry block, and will be updated
      // from the loop body.
      PHINode *IndexPhi = Builder.CreatePHI(Int32Ty, 2, "index");
      IndexPhi->addIncoming(Builder.getInt32(0), EntryBB);

      // The loop condition: `index < length`.
      Value *LoopCondition =
          Builder.CreateICmpSLT(IndexPhi, Length, "loop_cond");
      // Branch to the body if true, otherwise exit.
      Builder.CreateCondBr(LoopCondition, LoopBody, LoopExit);

      // In the loop body, perform one iteration of decryption.
      Builder.SetInsertPoint(LoopBody);

      // Get the address of the source character: `src_ptr + index`.
      Value *SrcCharPtr = Builder.CreateGEP(Type::getInt8Ty(Ctx), SrcPtr,
                                            IndexPhi, "src_char_ptr");
      // Load the encrypted byte from that address.
      Value *LoadedByte =
          Builder.CreateLoad(Type::getInt8Ty(Ctx), SrcCharPtr, "loaded_byte");

      // Decrypt the byte by XORing it with the key.
      Value *DecryptedByte = Builder.CreateXor(
          LoadedByte, Builder.getInt8(XOR_KEY), "decrypted_byte");

      // Get the address of the destination character: `dest_ptr + index`.
      Value *DestCharPtr = Builder.CreateGEP(Type::getInt8Ty(Ctx), DestPtr,
                                             IndexPhi, "dest_char_ptr");
      // Store the decrypted byte in the destination.
      Builder.CreateStore(DecryptedByte, DestCharPtr);

      // Increment the index for the next iteration.
      Value *NextIndex =
          Builder.CreateAdd(IndexPhi, Builder.getInt32(1), "next_index");
      // Add the next index value to the PHI node for the back-edge from the
      // loop body.
      IndexPhi->addIncoming(NextIndex, LoopBody);
      // Jump back to the loop header to check the condition again.
      Builder.CreateBr(LoopHeader);

      // In the loop exit block, return.
      Builder.SetInsertPoint(LoopExit);
      Builder.CreateRetVoid();
    }
    // Return a FunctionCallee object, which is a convenient wrapper for the
    // function.
    return FunctionCallee(DecryptF->getFunctionType(), DecryptF);
  }

  // Generates a structured JSON report and prints it to standard error.
  void generateReport(StringRef inputFileName, StringRef outputFileName,
                      unsigned int encryptedStrings, unsigned int originalSize,
                      unsigned int encryptedSize) {
    std::string reportBuffer;
    raw_string_ostream S(reportBuffer);

    S << "{\n";
    S << "  \"inputFile\": \""
      << (inputFileName.empty() ? "<stdin>" : inputFileName.str()) << "\",\n";
    S << "  \"outputFile\": \"" << outputFileName.str() << "\",\n";

    // Get the current time and format it as an ISO 8601 timestamp.
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    struct tm buf;
    gmtime_r(&in_time_t, &buf); // Use thread-safe gmtime_r
    char time_str[24];
    strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%SZ", &buf);
    S << "  \"timestamp\": \"" << time_str << "\",\n";

    S << "  \"inputParameters\": {\n";
    S << "    \"obfuscationLevel\": \"medium\",\n";
    S << "    \"targetPlatform\": \"linux\",\n";
    S << "    \"enableStringEncryption\": true,\n";
    S << "    \"enableControlFlowFlattening\": false,\n";
    S << "    \"enableAntiDebug\": false\n";
    S << "  },\n";

    S << "  \"outputAttributes\": {\n";
    S << "    \"originalIRStringDataSize\": \"" << originalSize
      << " bytes\",\n";
    S << "    \"obfuscatedIRStringDataSize\": \"" << encryptedSize
      << " bytes\",\n";
    double sizeIncrease =
        (originalSize == 0)
            ? 0.0
            : (double)(encryptedSize - originalSize) / originalSize * 100.0;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2)
       << sizeIncrease; // Format to 2 decimal places.
    S << "    \"stringDataSizeChange\": \"" << ss.str() << "%\"\n";
    S << "  },\n";

    S << "  \"obfuscationMetrics\": {\n";
    S << "    \"cyclesCompleted\": 1,\n";
    S << "    \"passesRun\": [\"StringEncrypt\"],\n";
    S << "    \"stringEncryption\": {\n";
    S << "      \"count\": " << encryptedStrings << ",\n";
    S << "      \"method\": \"XOR with fixed key (0xAB)\"\n";
    S << "    }\n";
    S << "  }\n";
    S << "}\n";

    // Print the final JSON string to standard error.
    errs() << reportBuffer;
  }
};

// This is the entry point for the LLVM pass plugin.
// When `opt` loads this shared library, it will call this function to discover
// the passes.
extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "ChakravyuhaStringEncryptionPassPlugin",
          "v0.1", [](PassBuilder &PB) {
            // Register a callback that allows to add the custom pass to the
            // pipeline when the name "chakravyuha-string-encrypt" is used on
            // the command line.
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "chakravyuha-string-encrypt") {
                    MPM.addPass(StringEncryptionPass());
                    return true; // Handled this name.
                  }
                  return false; // Did not handle this name.
                });
          }};
}
