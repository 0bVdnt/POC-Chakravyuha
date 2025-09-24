#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

using namespace llvm;

// The name our pass will be invoked by
#define PASS_NAME "string-encrypt"

// The simple XOR key for our POC
#define XOR_KEY 0x42

// The main pass logic
struct StringEncryptPass : public PassInfoMixin<StringEncryptPass> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    // Step 1: Create the decryption function
    Function *decryptFunc = createDecryptFunction(M);

    // Step 2: Find and encrypt all constant strings
    for (GlobalVariable &GV : M.globals()) {
      if (GV.isConstant() && GV.hasInitializer()) {
        if (ConstantDataSequential *CDS =
                dyn_cast<ConstantDataSequential>(GV.getInitializer())) {
          if (CDS->isString()) {
            StringRef originalStr = CDS->getAsString();
            // Don't encrypt empty strings
            if (originalStr.size() <= 1)
              continue;

            // Encrypt the string
            std::string encryptedStr;
            for (char c : originalStr) {
              encryptedStr += c ^ XOR_KEY;
            }

            // Replace the original string with the encrypted one
            GV.setInitializer(ConstantDataArray::getString(M.getContext(),
                                                           encryptedStr, true));

            // Step 3: Replace all uses of the string with a call to our decrypt
            // function We need to copy uses because replaceAllUsesWith
            // invalidates the iterator
            SmallVector<Value *, 16> uses;
            for (User *U : GV.users()) {
              uses.push_back(U);
            }

            for (Value *V : uses) {
              if (Instruction *I = dyn_cast<Instruction>(V)) {
                IRBuilder<> builder(I);
                Value *decryptedStrPtr = builder.CreateCall(decryptFunc, {&GV});
                I->replaceUsesOfWith(&GV, decryptedStrPtr);
              }
            }
          }
        }
      }
    }
    return PreservedAnalyses::all();
  }

  // Helper to create our decryption runtime function
  Function *createDecryptFunction(Module &M) {
    // Function signature: char* decrypt(char* str)
    LLVMContext &Ctx = M.getContext();
    Type *charPtrType = Type::getInt8PtrTy(Ctx);
    FunctionType *funcType =
        FunctionType::get(charPtrType, {charPtrType}, false);

    Function *func = Function::Create(funcType, Function::InternalLinkage,
                                      "decrypt_string", &M);

    // Create the function body
    BasicBlock *entry = BasicBlock::Create(Ctx, "entry", func);
    IRBuilder<> builder(entry);

    Argument *strArg = func->getArg(0);
    Value *i = builder.CreateAlloca(Type::getInt32Ty(Ctx), nullptr, "i");
    builder.CreateStore(ConstantInt::get(Type::getInt32Ty(Ctx), 0), i);

    // Loop condition
    BasicBlock *loopCond = BasicBlock::Create(Ctx, "loop_cond", func);
    builder.CreateBr(loopCond);
    builder.SetInsertPoint(loopCond);
    Value *i_val = builder.CreateLoad(Type::getInt32Ty(Ctx), i);
    Value *char_ptr = builder.CreateGEP(Type::getInt8Ty(Ctx), strArg, i_val);
    Value *char_val = builder.CreateLoad(Type::getInt8Ty(Ctx), char_ptr);
    Value *cond = builder.CreateICmpNE(
        char_val, ConstantInt::get(Type::getInt8Ty(Ctx), 0));

    // Loop body
    BasicBlock *loopBody = BasicBlock::Create(Ctx, "loop_body", func);
    BasicBlock *loopEnd = BasicBlock::Create(Ctx, "loop_end", func);
    builder.CreateCondBr(cond, loopBody, loopEnd);
    builder.SetInsertPoint(loopBody);

    // The XOR operation
    Value *decrypted_char = builder.CreateXor(
        char_val, ConstantInt::get(Type::getInt8Ty(Ctx), XOR_KEY));
    builder.CreateStore(decrypted_char, char_ptr);

    // Increment i
    Value *next_i =
        builder.CreateAdd(i_val, ConstantInt::get(Type::getInt32Ty(Ctx), 1));
    builder.CreateStore(next_i, i);
    builder.CreateBr(loopCond);

    // Return the original (now decrypted) pointer
    builder.SetInsertPoint(loopEnd);
    builder.CreateRet(strArg);

    return func;
  }
};

// This is the new way to register a pass in LLVM 15+
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {.APIVersion = LLVM_PLUGIN_API_VERSION,
          .PluginName = "PocObfuscator",
          .PluginVersion = "v0.1",
          .RegisterPasses = [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM, ...) {
                  if (Name == PASS_NAME) {
                    MPM.addPass(StringEncryptPass());
                    return true;
                  }
                  return false;
                });
          }};
}
