#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

using namespace llvm;

#define PASS_NAME "string-encrypt"
#define XOR_KEY 0x42

struct StringEncryptPass : public PassInfoMixin<StringEncryptPass> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    Function *decryptFunc = M.getFunction("decrypt_string");
    if (!decryptFunc) {
      decryptFunc = createDecryptFunction(M);
    }

    std::vector<GlobalVariable *> stringGlobals;
    for (GlobalVariable &GV : M.globals()) {
      if (GV.isConstant() && GV.hasInitializer()) {
        if (auto *CDS = dyn_cast<ConstantDataSequential>(GV.getInitializer())) {
          if (CDS->isString() && CDS->getAsString().size() > 1) {
            stringGlobals.push_back(&GV);
          }
        }
      }
    }

    for (GlobalVariable *GV : stringGlobals) {
      ConstantDataSequential *CDS =
          cast<ConstantDataSequential>(GV->getInitializer());
      StringRef originalStr = CDS->getAsString();

      std::string encryptedStr;
      for (char c : originalStr) {
        encryptedStr += c ^ XOR_KEY;
      }

      Constant *encryptedConst =
          ConstantDataArray::getString(M.getContext(), encryptedStr, false);
      GlobalVariable *newGV = new GlobalVariable(
          M, encryptedConst->getType(), true, GlobalValue::PrivateLinkage,
          encryptedConst, GV->getName() + "_encrypted");

      SmallVector<User *, 16> users(GV->users());
      for (User *U : users) {
        if (Instruction *I = dyn_cast<Instruction>(U)) {
          IRBuilder<> builder(I);
          Value *decryptedStrPtr = builder.CreateCall(decryptFunc, {newGV});
          U->replaceUsesOfWith(GV, decryptedStrPtr);
        } else {
          errs() << "Warning: Found non-instruction use of string global. Not "
                    "handled in this POC.\n";
        }
      }
      GV->eraseFromParent();
    }

    return stringGlobals.empty() ? PreservedAnalyses::all()
                                 : PreservedAnalyses::none();
  }

private:
  Function *createDecryptFunction(Module &M) {
    LLVMContext &Ctx = M.getContext();

    // --- FIX 2: Use PointerType::get for modern LLVM API ---
    Type *charPtrType = PointerType::get(Type::getInt8Ty(Ctx), 0);
    // --------------------------------------------------------

    FunctionType *funcType =
        FunctionType::get(charPtrType, {charPtrType}, false);
    Function *func = Function::Create(funcType, Function::InternalLinkage,
                                      "decrypt_string", &M);

    BasicBlock *entry = BasicBlock::Create(Ctx, "entry", func);
    IRBuilder<> builder(entry);

    Argument *strArg = func->getArg(0);
    strArg->setName("str");

    Value *i = builder.CreateAlloca(Type::getInt32Ty(Ctx), nullptr, "i");
    builder.CreateStore(ConstantInt::get(Type::getInt32Ty(Ctx), 0), i);

    BasicBlock *loopCond = BasicBlock::Create(Ctx, "loop_cond", func);
    BasicBlock *loopBody = BasicBlock::Create(Ctx, "loop_body", func);
    BasicBlock *loopEnd = BasicBlock::Create(Ctx, "loop_end", func);

    builder.CreateBr(loopCond);
    builder.SetInsertPoint(loopCond);

    Value *i_val = builder.CreateLoad(Type::getInt32Ty(Ctx), i, "i_val");
    Value *char_ptr =
        builder.CreateGEP(Type::getInt8Ty(Ctx), strArg, i_val, "char_ptr");
    Value *char_val =
        builder.CreateLoad(Type::getInt8Ty(Ctx), char_ptr, "char_val");
    Value *cond = builder.CreateICmpNE(
        char_val, ConstantInt::get(Type::getInt8Ty(Ctx), 0), "cond");
    builder.CreateCondBr(cond, loopBody, loopEnd);

    builder.SetInsertPoint(loopBody);
    Value *decrypted_char = builder.CreateXor(
        char_val, ConstantInt::get(Type::getInt8Ty(Ctx), XOR_KEY));
    builder.CreateStore(decrypted_char, char_ptr);
    Value *next_i =
        builder.CreateAdd(i_val, ConstantInt::get(Type::getInt32Ty(Ctx), 1));
    builder.CreateStore(next_i, i);
    builder.CreateBr(loopCond);

    builder.SetInsertPoint(loopEnd);
    builder.CreateRet(strArg);

    return func;
  }
};

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
