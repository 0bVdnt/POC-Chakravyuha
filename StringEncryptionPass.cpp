#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

#include <algorithm>
#include <chrono>  // For timestamp
#include <iomanip> // For std::put_time
#include <sstream> // For std::stringstream, to format doubles more easily
#include <string>
#include <vector>

using namespace llvm;

static const char XOR_KEY = 0xAB;

std::vector<char> encryptString(StringRef S) {
  std::vector<char> Encrypted(S.begin(), S.end());
  for (char &C : Encrypted) {
    C ^= XOR_KEY;
  }
  return Encrypted;
}

struct StringEncryptionPass : public PassInfoMixin<StringEncryptionPass> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    bool Changed = false;
    std::vector<GlobalVariable *> StringGlobalsToEncrypt;

    unsigned int encryptedStringsCount = 0;
    unsigned int originalStringDataSize = 0;
    unsigned int encryptedStringDataSize = 0;

    for (GlobalVariable &GV : M.globals()) {
      if (GV.isConstant() && GV.hasInitializer() &&
          GV.getInitializer()->getType()->isAggregateType()) {
        if (ConstantDataArray *CDA =
                dyn_cast<ConstantDataArray>(GV.getInitializer())) {
          if (CDA->isString()) {
            if (GV.getName().starts_with(".str.") ||
                GV.getName().starts_with(".str")) {
              StringGlobalsToEncrypt.push_back(&GV);
            }
          }
        }
      }
    }

    if (StringGlobalsToEncrypt.empty()) {
      // Only generate report if some strings were actually processed or
      // attempted Use a more robust check for module name if needed, but for
      // stdin this is fine.
      generateReport(M.getSourceFileName(), "obfuscated.ll",
                     encryptedStringsCount, originalStringDataSize,
                     encryptedStringDataSize);
      return PreservedAnalyses::all();
    }

    FunctionCallee DecryptFunc = injectDecryptionStub(M);
    LLVMContext &Ctx = M.getContext();
    PointerType *Int8PtrTy = PointerType::get(Ctx, 0);
    Type *Int32Ty = Type::getInt32Ty(Ctx);

    for (GlobalVariable *GV : StringGlobalsToEncrypt) {
      StringRef OriginalStringRef =
          cast<ConstantDataArray>(GV->getInitializer())->getAsString();

      if (OriginalStringRef.empty())
        continue;

      encryptedStringsCount++;
      originalStringDataSize += OriginalStringRef.size();

      errs() << "Chakravyuha StringEncrypt: Encrypting string -> "
             << OriginalStringRef << "\n";

      std::vector<char> EncryptedBytes = encryptString(OriginalStringRef);
      EncryptedBytes.back() = '\0' ^ XOR_KEY;

      encryptedStringDataSize += EncryptedBytes.size();

      ArrayType *ArrTy =
          ArrayType::get(Type::getInt8Ty(Ctx), EncryptedBytes.size());
      Constant *EncryptedConst =
          ConstantDataArray::get(Ctx, ArrayRef<char>(EncryptedBytes));

      GlobalVariable *EncryptedGV =
          new GlobalVariable(M, ArrTy, true, GlobalValue::PrivateLinkage,
                             EncryptedConst, GV->getName() + ".enc");
      appendToCompilerUsed(M, {EncryptedGV});

      std::vector<Use *> UsesToReplace;
      for (Use &U : GV->uses()) {
        UsesToReplace.push_back(&U);
      }

      for (Use *U : UsesToReplace) {
        User *CurrentUser = U->getUser();
        Instruction *InsertionPoint = nullptr;

        if (Instruction *Inst = dyn_cast<Instruction>(CurrentUser)) {
          InsertionPoint = Inst;
        } else if (Constant *C = dyn_cast<Constant>(CurrentUser)) {
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

        IRBuilder<> Builder(InsertionPoint);

        Value *Zero = Builder.getInt64(0);
        Value *EncryptedBasePtr = Builder.CreateInBoundsGEP(
            ArrTy, EncryptedGV, {Zero, Zero}, "encryptedPtr");

        Value *EncryptedArgPtr = EncryptedBasePtr;
        if (EncryptedArgPtr->getType() != Int8PtrTy) {
          EncryptedArgPtr = Builder.CreateBitCast(EncryptedArgPtr, Int8PtrTy,
                                                  "encryptedPtrCast");
        }

        Value *DecryptedStringAlloca = Builder.CreateAlloca(
            ArrTy,   // Allocate array type matching the string's final size
            nullptr, // No explicit array size, ArrTy provides it
            GV->getName() + ".dec.alloca");

        Value *DecryptedAllocaPtr = DecryptedStringAlloca;
        if (DecryptedAllocaPtr->getType() != Int8PtrTy) {
          DecryptedAllocaPtr = Builder.CreateBitCast(
              DecryptedAllocaPtr, Int8PtrTy, "decryptedAllocaPtrCast");
        }

        Builder.CreateCall(
            DecryptFunc,
            {DecryptedAllocaPtr, // Destination
             EncryptedArgPtr,    // Source (encrypted)
             Builder.getInt32(
                 EncryptedBytes.size())}, // Full size including null
            "");

        U->set(DecryptedAllocaPtr);
        Changed = true;
      }

      if (!GV->user_empty()) {
        errs() << "Chakravyuha StringEncrypt: ERROR - Original GV still has "
                  "users after replacement: "
               << *GV << "\n";
        for (User *U_dbg : GV->users()) {
          errs() << "  Remaining user: " << *U_dbg << "\n";
        }
      }
      GV->eraseFromParent();
    }

    generateReport(M.getSourceFileName(), "obfuscated.ll",
                   encryptedStringsCount, originalStringDataSize,
                   encryptedStringDataSize);

    if (Changed) {
      return PreservedAnalyses::none();
    }
    return PreservedAnalyses::all();
  }

  FunctionCallee injectDecryptionStub(Module &M) {
    LLVMContext &Ctx = M.getContext();
    Type *Int8PtrTy = PointerType::get(Ctx, 0);
    Type *Int32Ty = Type::getInt32Ty(Ctx);
    Type *VoidTy = Type::getVoidTy(Ctx);

    FunctionType *DecryptFTy =
        FunctionType::get(VoidTy, {Int8PtrTy, Int8PtrTy, Int32Ty}, false);

    Function *DecryptF = M.getFunction("chakravyuha_decrypt_string");
    if (!DecryptF) {
      DecryptF = Function::Create(DecryptFTy, GlobalValue::PrivateLinkage,
                                  "chakravyuha_decrypt_string", M);
      DecryptF->setCallingConv(CallingConv::C);
      DecryptF->addFnAttr(Attribute::NoInline);
      DecryptF->addFnAttr(Attribute::NoUnwind);

      Function::arg_iterator ArgIt = DecryptF->arg_begin();
      Argument *DestPtr = ArgIt++;
      DestPtr->setName("dest_ptr");
      Argument *SrcPtr = ArgIt++;
      SrcPtr->setName("src_ptr");
      Argument *Length = ArgIt++;
      Length->setName("length");

      BasicBlock *EntryBB = BasicBlock::Create(Ctx, "entry", DecryptF);
      IRBuilder<> Builder(EntryBB);

      BasicBlock *LoopHeader = BasicBlock::Create(Ctx, "loop_header", DecryptF);
      BasicBlock *LoopBody = BasicBlock::Create(Ctx, "loop_body", DecryptF);
      BasicBlock *LoopExit = BasicBlock::Create(Ctx, "loop_exit", DecryptF);

      Builder.CreateBr(LoopHeader);

      Builder.SetInsertPoint(LoopHeader);
      PHINode *IndexPhi = Builder.CreatePHI(Int32Ty, 2, "index");
      IndexPhi->addIncoming(Builder.getInt32(0), EntryBB);

      Value *LoopCondition =
          Builder.CreateICmpSLT(IndexPhi, Length, "loop_cond");
      Builder.CreateCondBr(LoopCondition, LoopBody, LoopExit);

      Builder.SetInsertPoint(LoopBody);

      Value *SrcCharPtr = Builder.CreateGEP(Type::getInt8Ty(Ctx), SrcPtr,
                                            IndexPhi, "src_char_ptr");
      Value *LoadedByte =
          Builder.CreateLoad(Type::getInt8Ty(Ctx), SrcCharPtr, "loaded_byte");

      Value *DecryptedByte = Builder.CreateXor(
          LoadedByte, Builder.getInt8(XOR_KEY), "decrypted_byte");

      Value *DestCharPtr = Builder.CreateGEP(Type::getInt8Ty(Ctx), DestPtr,
                                             IndexPhi, "dest_char_ptr");
      Builder.CreateStore(DecryptedByte, DestCharPtr);

      Value *NextIndex =
          Builder.CreateAdd(IndexPhi, Builder.getInt32(1), "next_index");
      IndexPhi->addIncoming(NextIndex, LoopBody);
      Builder.CreateBr(LoopHeader);

      Builder.SetInsertPoint(LoopExit);
      Builder.CreateRetVoid();
    }
    return FunctionCallee(DecryptF->getFunctionType(), DecryptF);
  }

  void generateReport(StringRef inputFileName, StringRef outputFileName,
                      unsigned int encryptedStrings, unsigned int originalSize,
                      unsigned int encryptedSize) {
    std::string reportBuffer;
    raw_string_ostream S(reportBuffer);

    S << "{\n";
    // Ensure no trailing comma after the last item in a block
    S << "  \"inputFile\": \""
      << (inputFileName.empty() ? "<stdin>" : inputFileName.str())
      << "\",\n"; // Handle stdin
    S << "  \"outputFile\": \"" << outputFileName.str() << "\",\n";

    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    struct tm buf;
    gmtime_r(&in_time_t, &buf);
    char time_str[24];
    strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%SZ", &buf);
    S << "  \"timestamp\": \"" << time_str << "\",\n";

    S << "  \"inputParameters\": {\n";
    S << "    \"obfuscationLevel\": \"medium\",\n";
    S << "    \"targetPlatform\": \"linux\",\n";
    S << "    \"enableStringEncryption\": true,\n";
    S << "    \"enableControlFlowFlattening\": false,\n";
    S << "    \"enableAntiDebug\": false\n"; // Removed trailing comma here
    S << "  },\n"; // Kept comma here as 'inputParameters' is not the last
                   // member of the root object

    S << "  \"outputAttributes\": {\n";
    S << "    \"originalIRStringDataSize\": \"" << originalSize
      << " bytes\",\n";
    S << "    \"obfuscatedIRStringDataSize\": \"" << encryptedSize
      << " bytes\",\n";
    double sizeIncrease =
        (originalSize == 0)
            ? 0.0
            : (double)(encryptedSize - originalSize) / originalSize * 100.0;
    // Using std::stringstream for better control over double formatting in JSON
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << sizeIncrease;
    S << "    \"stringDataSizeChange\": \"" << ss.str()
      << "%\"\n";  // Removed trailing comma here
    S << "  },\n"; // Kept comma here

    S << "  \"obfuscationMetrics\": {\n";
    S << "    \"cyclesCompleted\": 1,\n";
    S << "    \"passesRun\": [\"StringEncrypt\"],\n";
    S << "    \"stringEncryption\": {\n";
    S << "      \"count\": " << encryptedStrings << ",\n";
    S << "      \"method\": \"XOR with fixed key (0xAB)\"\n"; // Removed
                                                              // trailing comma
                                                              // here
    S << "    }\n"; // Removed trailing comma here
    S << "  }\n";   // Removed trailing comma here
    S << "}\n";

    errs() << reportBuffer;
  }

  static StringRef name() { return "chakravyuha-string-encrypt"; }
  static bool isRequired() { return true; }
  bool skipFunction(const Function &F) const { return false; }
};

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "ChakravyuhaStringEncryptionPassPlugin",
          "v0.1", [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "chakravyuha-string-encrypt") {
                    MPM.addPass(StringEncryptionPass());
                    return true;
                  }
                  return false;
                });
          }};
}
