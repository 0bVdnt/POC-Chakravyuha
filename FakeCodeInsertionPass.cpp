#include "FakeCodeInsertionPass.h"
#include "ChakravyuhaReport.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Transforms/Utils/Local.h"

#include <random>
#include <vector>

using namespace llvm;

const unsigned MAX_FAKE_BLOCKS_PER_FUNCTION = 15;
const unsigned MAX_FAKE_INSTRUCTIONS_PER_BLOCK = 30;

static std::mt19937 gen(std::random_device{}());

// Populates a block with junk, creates a side effect, and terminates it.
static void populateAndTerminateBlockWithJunk(BasicBlock *block,
                                              BasicBlock *successor,
                                              AllocaInst *dummyVar) {
  IRBuilder<> builder(block);
  LLVMContext &Ctx = block->getContext();
  Type *Int32Ty = Type::getInt32Ty(Ctx);

  std::uniform_int_distribution<> numInstrsDistrib(
      2, MAX_FAKE_INSTRUCTIONS_PER_BLOCK);
  unsigned numInstructions = numInstrsDistrib(gen);

  SmallVector<Value *, 8> availableValues;
  availableValues.push_back(ConstantInt::get(Int32Ty, 42));

  std::uniform_int_distribution<> opcodeDistrib(0, 4);
  std::uniform_int_distribution<uint32_t> valueDistrib;

  Value *lastVal = nullptr;
  for (unsigned i = 0; i < numInstructions; ++i) {
    std::uniform_int_distribution<size_t> operandDistrib(
        0, availableValues.size() - 1);
    Value *op1 = availableValues[operandDistrib(gen)];
    Value *op2 = availableValues[operandDistrib(gen)];

    availableValues.push_back(ConstantInt::get(Int32Ty, valueDistrib(gen)));

    Value *newVal = nullptr;
    switch (opcodeDistrib(gen)) {
    case 0:
      newVal = builder.CreateAdd(op1, op2, "fake.add");
      break;
    case 1:
      newVal = builder.CreateSub(op1, op2, "fake.sub");
      break;
    case 2:
      newVal = builder.CreateMul(op1, op2, "fake.mul");
      break;
    case 3:
      newVal = builder.CreateXor(op1, op2, "fake.xor");
      break;
    case 4:
      newVal = builder.CreateShl(op1, op2, "fake.shl");
      break;
    }
    if (newVal) {
      availableValues.push_back(newVal);
      lastVal = newVal;
    }
  }

  if (lastVal) {
    builder.CreateStore(lastVal, dummyVar, true);
  }

  builder.CreateBr(successor);
}

// Main function to add obfuscation.
static bool addFakeCodeToFunction(Function &F) {
  // A shared safety check at the very beginning.
  if (F.isDeclaration() || F.hasAvailableExternallyLinkage() || F.empty() ||
      chakravyuha::shouldSkipFunction(F)) {
    return false;
  }

  IRBuilder<> entryBuilder(&F.getEntryBlock(),
                           F.getEntryBlock().getFirstInsertionPt());
  AllocaInst *dummyVar = entryBuilder.CreateAlloca(
      Type::getInt32Ty(F.getContext()), nullptr, "dummy.var");

  std::vector<BasicBlock *> originalBlocks;
  for (BasicBlock &BB : F) {
    if (BB.getTerminator()->getNumSuccessors() == 1) {
      BasicBlock *Successor = BB.getTerminator()->getSuccessor(0);
      if (!isa<PHINode>(Successor->front())) {
        originalBlocks.push_back(&BB);
      }
    }
  }

  if (originalBlocks.empty()) {
    return false;
  }

  auto &R = chakravyuha::ReportData::get();

  std::uniform_int_distribution<> numBlocksDistrib(
      1, MAX_FAKE_BLOCKS_PER_FUNCTION);
  unsigned numBlocksToInsert = numBlocksDistrib(gen);
  bool changed = false;

  for (unsigned i = 0; i < numBlocksToInsert; ++i) {
    if (originalBlocks.empty())
      break;

    std::uniform_int_distribution<size_t> blockPicker(0, originalBlocks.size() -
                                                             1);
    size_t blockIdx = blockPicker(gen);
    BasicBlock *parentBlock = originalBlocks[blockIdx];

    BasicBlock *originalSuccessor =
        parentBlock->getTerminator()->getSuccessor(0);

    std::string blockName =
        "fake.block." + std::to_string(R.fakeCodeBlocksInserted);
    BasicBlock *fakeBlock =
        BasicBlock::Create(F.getContext(), blockName, &F, originalSuccessor);

    populateAndTerminateBlockWithJunk(fakeBlock, originalSuccessor, dummyVar);

    parentBlock->getTerminator()->eraseFromParent();
    Value *falseCond = ConstantInt::get(Type::getInt1Ty(F.getContext()), 0);
    IRBuilder<>(parentBlock)
        .CreateCondBr(falseCond, fakeBlock, originalSuccessor);

    R.fakeCodeBlocksInserted++;
    changed = true;
    originalBlocks.erase(originalBlocks.begin() + blockIdx);
  }

  return changed;
}

PreservedAnalyses FakeCodeInsertionPass::run(Module &M,
                                             ModuleAnalysisManager &) {
  bool Changed = false;

  auto &R = chakravyuha::ReportData::get();
  R.passesRun.push_back("FakeCodeInsertion");
  R.enableFakeCodeInsertion = true;

  for (Function &F : M) {
    if (addFakeCodeToFunction(F)) {
      Changed = true;
    }
  }

  if (Changed) {
    return PreservedAnalyses::none();
  }
  return PreservedAnalyses::all();
}
