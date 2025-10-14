#include "StringEncryptionPass.h"
#include "ChakravyuhaReport.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

#include <algorithm>
#include <numeric>
#include <random>
#include <set>
#include <string>
#include <vector>

using namespace llvm;

// Define the different encryption schemes.
enum class EncryptionScheme {
  XOR_WITH_INDEX = 0,
  ADD_WITH_INDEX = 1,
  SUB_FROM_CONSTANT = 2,
  SBOX = 3,
};
const int NUM_ENCRYPTION_SCHEMES = 4;

const unsigned int XOR_KEY_LENGTH = 16;
const unsigned int SBOX_SIZE = 256;

// Helper to generate a random S-Box and its inverse for decryption.
void generateSBox(std::vector<uint8_t> &sbox, std::vector<uint8_t> &invSbox,
                  std::mt19937 &gen) {
  sbox.resize(SBOX_SIZE);
  invSbox.resize(SBOX_SIZE);
  std::iota(sbox.begin(), sbox.end(), 0);
  std::shuffle(sbox.begin(), sbox.end(), gen);
  for (unsigned i = 0; i < SBOX_SIZE; ++i) {
    invSbox[sbox[i]] = i;
  }
}

static std::vector<char> encryptStringWithXOR(StringRef S,
                                              const std::vector<uint8_t> &key) {
  std::vector<char> Encrypted(S.begin(), S.end());
  for (size_t i = 0; i < Encrypted.size(); ++i) {
    Encrypted[i] ^= key[i % XOR_KEY_LENGTH];
  }
  return Encrypted;
}

// An encryption function for the ADD_WITH_INDEX scheme.
static std::vector<char> encryptStringWithAdd(StringRef S,
                                              const std::vector<uint8_t> &key) {
  std::vector<char> Encrypted(S.begin(), S.end());
  for (size_t i = 0; i < Encrypted.size(); ++i) {
    Encrypted[i] += key[i % XOR_KEY_LENGTH];
  }
  return Encrypted;
}

static std::vector<char>
encryptStringWithSBox(StringRef S, const std::vector<uint8_t> &sbox) {
  std::vector<char> Encrypted;
  Encrypted.reserve(S.size());
  for (unsigned char C : S) {
    Encrypted.push_back(sbox[C]);
  }
  return Encrypted;
}

// Injects a polymorphic decryption stub for a chosen cipher scheme.
static Function *
injectCipherStub(Module &M, EncryptionScheme scheme, int cloneId,
                 const std::vector<uint8_t> &obfuscatedKey = {},
                 GlobalVariable *invSboxGV = nullptr) {
  LLVMContext &Ctx = M.getContext();
  Type *Int8PtrTy = PointerType::get(Ctx, 0);
  Type *Int32Ty = Type::getInt32Ty(Ctx);
  FunctionType *DecryptFTy =
      FunctionType::get(Type::getVoidTy(Ctx), {Int8PtrTy, Int32Ty}, false);

  std::string funcName = "chakravyuha_decrypt_" + std::to_string(cloneId);
  Function *DecryptF = M.getFunction(funcName);
  if (DecryptF)
    return DecryptF;

  DecryptF =
      Function::Create(DecryptFTy, GlobalValue::PrivateLinkage, funcName, M);
  DecryptF->addFnAttr(Attribute::NoInline);
  DecryptF->addFnAttr(Attribute::OptimizeNone);
  DecryptF->setCallingConv(CallingConv::C);

  auto ArgIt = DecryptF->arg_begin();
  Argument *EncryptedStringPtr = &*ArgIt++;
  Argument *Length = &*ArgIt++;

  BasicBlock *EntryBB = BasicBlock::Create(Ctx, "entry", DecryptF);
  IRBuilder<> Builder(EntryBB);

  BasicBlock *LoopHeader = BasicBlock::Create(Ctx, "loop_header", DecryptF);
  BasicBlock *LoopBody = BasicBlock::Create(Ctx, "loop_body", DecryptF);
  BasicBlock *LoopExit = BasicBlock::Create(Ctx, "loop_exit", DecryptF);

  Value *DeobfuscatedKeyAlloca = nullptr;
  if (scheme != EncryptionScheme::SBOX) {
    ArrayType *KeyArrayTy =
        ArrayType::get(Type::getInt8Ty(Ctx), XOR_KEY_LENGTH);
    DeobfuscatedKeyAlloca =
        Builder.CreateAlloca(KeyArrayTy, nullptr, "dec_key");
    for (unsigned i = 0; i < XOR_KEY_LENGTH; ++i) {
      Value *ObfuscatedByte = Builder.getInt8(obfuscatedKey[i]);
      Value *DeobfuscatedByte;
      switch (scheme) {
      case EncryptionScheme::XOR_WITH_INDEX:
        DeobfuscatedByte =
            Builder.CreateXor(ObfuscatedByte, Builder.getInt8(i));
        break;
      case EncryptionScheme::ADD_WITH_INDEX:
        DeobfuscatedByte =
            Builder.CreateSub(ObfuscatedByte, Builder.getInt8(i));
        break;
      case EncryptionScheme::SUB_FROM_CONSTANT:
        DeobfuscatedByte =
            Builder.CreateSub(Builder.getInt8(0xFF), ObfuscatedByte);
        break;
      default:
        llvm_unreachable("Invalid scheme for key-based crypto");
      }
      Value *DestGEP =
          Builder.CreateInBoundsGEP(KeyArrayTy, DeobfuscatedKeyAlloca,
                                    {Builder.getInt32(0), Builder.getInt32(i)});
      Builder.CreateStore(DeobfuscatedByte, DestGEP);
    }
  }

  Builder.CreateBr(LoopHeader);
  Builder.SetInsertPoint(LoopHeader);
  PHINode *IndexPhi = Builder.CreatePHI(Int32Ty, 2, "index");
  IndexPhi->addIncoming(Builder.getInt32(0), EntryBB);
  Value *LoopCond = Builder.CreateICmpSLT(IndexPhi, Length, "loop_cond");
  Builder.CreateCondBr(LoopCond, LoopBody, LoopExit);
  Builder.SetInsertPoint(LoopBody);
  Value *SrcCharPtr = Builder.CreateGEP(
      Type::getInt8Ty(Ctx), EncryptedStringPtr, IndexPhi, "src_char_ptr");

  if (scheme == EncryptionScheme::SBOX) {
    Value *EncryptedByte = Builder.CreateLoad(Type::getInt8Ty(Ctx), SrcCharPtr);
    Value *Index = Builder.CreateZExt(EncryptedByte, Type::getInt64Ty(Ctx));
    Value *SboxLookupPtr = Builder.CreateInBoundsGEP(
        invSboxGV->getValueType(), invSboxGV, {Builder.getInt64(0), Index});
    Value *DecryptedByte =
        Builder.CreateLoad(Type::getInt8Ty(Ctx), SboxLookupPtr);
    Builder.CreateStore(DecryptedByte, SrcCharPtr);
  } else {
    Value *KeyIndex =
        Builder.CreateURem(IndexPhi, Builder.getInt32(XOR_KEY_LENGTH));
    Value *KeyByteGEP = Builder.CreateInBoundsGEP(
        cast<AllocaInst>(DeobfuscatedKeyAlloca)->getAllocatedType(),
        DeobfuscatedKeyAlloca, {Builder.getInt32(0), KeyIndex});
    Value *KeyByte = Builder.CreateLoad(Type::getInt8Ty(Ctx), KeyByteGEP);
    Value *LoadedByte = Builder.CreateLoad(Type::getInt8Ty(Ctx), SrcCharPtr);

    Value *DecryptedByte;
    switch (scheme) {
    case EncryptionScheme::ADD_WITH_INDEX:
      DecryptedByte = Builder.CreateSub(LoadedByte, KeyByte);
      break;
    case EncryptionScheme::XOR_WITH_INDEX:
    case EncryptionScheme::SUB_FROM_CONSTANT:
      DecryptedByte = Builder.CreateXor(LoadedByte, KeyByte);
      break;
    default:
      llvm_unreachable("Invalid key-based scheme in decryption loop");
    }
    Builder.CreateStore(DecryptedByte, SrcCharPtr);
  }

  Value *NextIndex =
      Builder.CreateAdd(IndexPhi, Builder.getInt32(1), "next_index");
  IndexPhi->addIncoming(NextIndex, LoopBody);
  Builder.CreateBr(LoopHeader);
  Builder.SetInsertPoint(LoopExit);
  Builder.CreateRetVoid();
  return DecryptF;
}

// Creates the dispatch functions for the self-modifying check.
static Function *createDispatchFunctions(Module &M, GlobalVariable *EncryptedGV,
                                         Function *DecryptFunc,
                                         GlobalVariable *DispatchPtrGV,
                                         int stringId) {
  LLVMContext &Ctx = M.getContext();
  Type *Int8PtrTy = PointerType::get(Ctx, 0);
  FunctionType *DispatchFTy = FunctionType::get(Int8PtrTy, false);

  Function *JustDispatchF =
      Function::Create(DispatchFTy, GlobalValue::PrivateLinkage,
                       "dispatch_fast_" + std::to_string(stringId), M);
  BasicBlock *JDEntry = BasicBlock::Create(Ctx, "entry", JustDispatchF);
  IRBuilder<> JDBuilder(JDEntry);
  Value *JD_GEP = JDBuilder.CreateInBoundsGEP(
      EncryptedGV->getValueType(), EncryptedGV,
      {JDBuilder.getInt32(0), JDBuilder.getInt32(0)});
  JDBuilder.CreateRet(JD_GEP);

  Function *DecryptAndDispatchF =
      Function::Create(DispatchFTy, GlobalValue::PrivateLinkage,
                       "dispatch_slow_" + std::to_string(stringId), M);
  BasicBlock *DADEntry = BasicBlock::Create(Ctx, "entry", DecryptAndDispatchF);
  IRBuilder<> DADBuilder(DADEntry);

  uint64_t stringSize =
      cast<ArrayType>(EncryptedGV->getValueType())->getNumElements();
  Value *DAD_GEP = DADBuilder.CreateInBoundsGEP(
      EncryptedGV->getValueType(), EncryptedGV,
      {DADBuilder.getInt32(0), DADBuilder.getInt32(0)});
  DADBuilder.CreateCall(DecryptFunc,
                        {DAD_GEP, DADBuilder.getInt32(stringSize)});
  DADBuilder.CreateStore(JustDispatchF, DispatchPtrGV)
      ->setOrdering(AtomicOrdering::Monotonic);
  DADBuilder.CreateRet(DAD_GEP);
  return DecryptAndDispatchF;
}

PreservedAnalyses StringEncryptionPass::run(Module &M,
                                            ModuleAnalysisManager &) {
  bool Changed = false;
  auto &R = chakravyuha::ReportData::get();
  R.enableStringEncryption = true;
  R.passesRun.push_back("StringEncrypt");

  std::vector<GlobalVariable *> StringGlobalsToEncrypt;
  for (GlobalVariable &GV : M.globals()) {
    if (!GV.isConstant() || !GV.hasInitializer())
      continue;
    if (auto *CDA = dyn_cast<ConstantDataArray>(GV.getInitializer())) {
      if (CDA->isString()) {
        StringGlobalsToEncrypt.push_back(&GV);
      }
    }
  }
  if (StringGlobalsToEncrypt.empty())
    return PreservedAnalyses::all();

  std::set<Function *> unsafeFunctions;
  for (Function &F : M) {
    for (Instruction &I : instructions(F)) {
      if (auto *CI = dyn_cast<CallInst>(&I)) {
        if (CI->isInlineAsm()) {
          unsafeFunctions.insert(&F);
          break;
        }
        Function *CalledFunc = CI->getCalledFunction();
        if (CalledFunc && (CalledFunc->getName() == "setjmp" ||
                           CalledFunc->getName() == "_setjmp" ||
                           CalledFunc->getName() == "longjmp")) {
          unsafeFunctions.insert(&F);
          break;
        }
      }
    }
  }
  size_t lastSize;
  do {
    lastSize = unsafeFunctions.size();
    for (Function &F : M) {
      if (unsafeFunctions.count(&F))
        continue;
      for (Instruction &I : instructions(F)) {
        if (auto *CI = dyn_cast<CallInst>(&I)) {
          Function *CalledFunc = CI->getCalledFunction();
          if (CalledFunc && unsafeFunctions.count(CalledFunc)) {
            unsafeFunctions.insert(&F);
            break;
          }
        }
      }
    }
  } while (unsafeFunctions.size() > lastSize);

  LLVMContext &Ctx = M.getContext();
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> distrib(0, 255);

  int stringId = 0;
  for (GlobalVariable *GV : StringGlobalsToEncrypt) {
    StringRef OriginalString =
        cast<ConstantDataArray>(GV->getInitializer())->getAsString();
    if (OriginalString.empty())
      continue;

    bool usedInUnsafe = false;
    for (const Use &U : GV->uses()) {
      if (const auto *I = dyn_cast<Instruction>(U.getUser())) {
        if (unsafeFunctions.count(const_cast<Function *>(I->getFunction()))) {
          usedInUnsafe = true;
          break;
        }
      }
    }
    if (usedInUnsafe)
      continue;

    R.stringsEncrypted++;
    R.originalIRStringDataSize += OriginalString.size();

    int schemeIdx = distrib(gen) % NUM_ENCRYPTION_SCHEMES;
    EncryptionScheme scheme = static_cast<EncryptionScheme>(schemeIdx);
    std::vector<char> EncryptedBytes;
    Function *chosenDecryptFunc;

    if (scheme == EncryptionScheme::SBOX) {
      std::vector<uint8_t> sbox, invSbox;
      generateSBox(sbox, invSbox, gen);
      EncryptedBytes = encryptStringWithSBox(OriginalString, sbox);

      ArrayType *SboxArrayTy = ArrayType::get(Type::getInt8Ty(Ctx), SBOX_SIZE);
      Constant *InvSboxConst = ConstantDataArray::get(Ctx, invSbox);
      GlobalVariable *InvSboxGV = new GlobalVariable(
          M, SboxArrayTy, true, GlobalValue::PrivateLinkage, InvSboxConst,
          "inv_sbox_" + std::to_string(stringId));
      chosenDecryptFunc = injectCipherStub(M, scheme, stringId, {}, InvSboxGV);
    } else {
      std::vector<uint8_t> moduleKey;
      for (unsigned j = 0; j < XOR_KEY_LENGTH; ++j) {
        moduleKey.push_back((uint8_t)distrib(gen));
      }
      std::vector<uint8_t> obfuscatedModuleKey;
      for (unsigned j = 0; j < XOR_KEY_LENGTH; ++j) {
        uint8_t obfuscatedByte;
        switch (scheme) {
        case EncryptionScheme::XOR_WITH_INDEX:
          obfuscatedByte = moduleKey[j] ^ j;
          break;
        case EncryptionScheme::ADD_WITH_INDEX:
          obfuscatedByte = moduleKey[j] + j;
          break;
        case EncryptionScheme::SUB_FROM_CONSTANT:
          obfuscatedByte = 0xFF - moduleKey[j];
          break;
        default:
          llvm_unreachable("Invalid arithmetic scheme");
        }
        obfuscatedModuleKey.push_back(obfuscatedByte);
      }

      // Call the correct encryption function based on the chosen scheme.
      switch (scheme) {
      case EncryptionScheme::ADD_WITH_INDEX:
        EncryptedBytes = encryptStringWithAdd(OriginalString, moduleKey);
        break;
      case EncryptionScheme::XOR_WITH_INDEX:
      case EncryptionScheme::SUB_FROM_CONSTANT:
        EncryptedBytes = encryptStringWithXOR(OriginalString, moduleKey);
        break;
      default:
        llvm_unreachable("Invalid key-based scheme for encryption");
      }

      chosenDecryptFunc =
          injectCipherStub(M, scheme, stringId, obfuscatedModuleKey);
    }

    R.obfuscatedIRStringDataSize += EncryptedBytes.size();
    ArrayType *ArrTy =
        ArrayType::get(Type::getInt8Ty(Ctx), EncryptedBytes.size());
    Constant *EncryptedConst =
        ConstantDataArray::get(Ctx, ArrayRef<char>(EncryptedBytes));
    GlobalVariable *EncryptedGV =
        new GlobalVariable(M, ArrTy, false, GV->getLinkage(), EncryptedConst,
                           GV->getName() + ".enc");
    EncryptedGV->setAlignment(GV->getAlign());

    Type *Int8PtrTy = PointerType::get(Ctx, 0);
    FunctionType *DispatchFTy = FunctionType::get(Int8PtrTy, false);
    //
    // THIS IS THE FINAL FIX USING THE OPAQUE POINTER API
    // The type of a function pointer is now just a generic "ptr".
    //
    Type *DispatchPtrTy = PointerType::get(Ctx, 0);
    //
    //
    //
    std::string ptrName = "dispatch_ptr_" + std::to_string(stringId);
    GlobalVariable *DispatchPtrGV = new GlobalVariable(
        M, DispatchPtrTy, false, GlobalValue::PrivateLinkage, nullptr, ptrName);

    Function *InitialDispatchFunc = createDispatchFunctions(
        M, EncryptedGV, chosenDecryptFunc, DispatchPtrGV, stringId);

    // Since types are opaque, we must cast the initializer to the generic ptr
    // type.
    Constant *Initializer =
        ConstantExpr::getBitCast(InitialDispatchFunc, DispatchPtrTy);
    DispatchPtrGV->setInitializer(Initializer);

    std::vector<Use *> uses;
    for (auto &U : GV->uses()) {
      uses.push_back(&U);
    }

    for (Use *U : uses) {
      Instruction *UserInst = dyn_cast<Instruction>(U->getUser());
      if (!UserInst)
        continue;
      IRBuilder<> Builder(UserInst);
      Value *LoadedFuncPtr = Builder.CreateLoad(DispatchPtrTy, DispatchPtrGV);
      // The CreateCall instruction is smart enough to handle a generic pointer
      // as long as we provide the correct FunctionType.
      Value *DecryptedStringPtr =
          Builder.CreateCall(DispatchFTy, LoadedFuncPtr, {});
      Value *CastedPtr =
          Builder.CreateBitCast(DecryptedStringPtr, U->get()->getType());
      U->set(CastedPtr);
    }

    if (GV->use_empty()) {
      GV->eraseFromParent();
      Changed = true;
    }
    stringId++;
  }

  R.stringMethod = "Fully Polymorphic On-Demand Decryption via Self-Modifying "
                   "Pointers and Data-in-Code Stubs";

  if (Changed)
    return PreservedAnalyses::none();
  return PreservedAnalyses::all();
}
