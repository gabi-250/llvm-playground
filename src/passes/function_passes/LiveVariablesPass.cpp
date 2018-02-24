#include <vector>
#include <stdint.h>
#include <llvm/Pass.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

#define UNOPT_PREFIX "__unopt_"

using namespace llvm;
using std::vector;

namespace {

struct LiveVariablesPass: public FunctionPass {
  static char id;

  LiveVariablesPass() : FunctionPass(id) {}

  virtual bool runOnFunction(Function &fun) {
    Module *mod = fun.getParent();
    StringRef funName = fun.getName();
    outs() << "Running LiveVariablesPass on function: " << funName << '\n';
    auto stackmapIntrinsic = Intrinsic::getDeclaration(
        mod, Intrinsic::experimental_stackmap);
    auto patchpointIntrinsicVoid = Intrinsic::getDeclaration(
        mod, Intrinsic::experimental_patchpoint_void);
    auto patchpointIntrinsici64 = Intrinsic::getDeclaration(
        mod, Intrinsic::experimental_patchpoint_i64);
    for (auto &bb : fun) {
      for (BasicBlock::iterator it = bb.begin(); it != bb.end(); ++it) {
          if (isa<CallInst>(it)) {
            // Insert a stackmap call after each function call inside which a
            // guard may fail. This enables the runtime to work out the return
            // address of the function.
            CallInst &callInst = cast<CallInst>(*it);
            Function *calledFun = callInst.getCalledFunction();
            if (calledFun == stackmapIntrinsic
                || calledFun == patchpointIntrinsicVoid
                || calledFun == patchpointIntrinsici64) {
              auto liveVariables = getLiveRegisters(*it);
              vector<Value *> args(callInst.arg_begin(), callInst.arg_end());
              std::move(liveVariables.begin(), liveVariables.end(),
                        std::back_inserter(args));
              CallInst *newCall = CallInst::Create(calledFun, args);
              if (!callInst.use_empty()) {
                callInst.replaceAllUsesWith(newCall);
              }
              ReplaceInstWithInst(callInst.getParent()->getInstList(),
                                  it, newCall);
            }
        }
      }
    }
    return true;
  }

  /*
   * Return the list of registers which are live across the given instruction.
   */
  static vector<Value *> getLiveRegisters(Instruction &instr) {
    vector<Value *> args;
    Function *fun = instr.getFunction();
    Module *mod = fun->getParent();
    DataLayout dataLayout(mod);
    DominatorTree DT(*fun);
    // Iterarate over each instruction in the function.
    for (auto &bb : *fun) {
      IRBuilder<> builder(bb.getContext());
      for (BasicBlock::iterator it = bb.begin(); it != bb.end(); ++it) {
        if (!it->use_empty() && DT.dominates(&*it, &instr)) {
          for (Value::user_iterator user = (*it).user_begin();
               user != (*it).user_end(); ++user) {
            // This instruction is defined 'above' `instr` and used 'below'
            // `instr`, so it is live across `instr`.
            if (isa<Instruction>(*user)) {
              auto userInst = cast<Instruction>(*user);
              if (DT.dominates(&instr, userInst) ||
                  instr.isSameOperationAs(&*userInst)) {
                args.push_back(&*it);
                auto instSize = builder.getInt64(8); // XXX default size
                // The runtime may need to copy more than 8 bytes starting at
                // this location. We can store the size of the object being
                // allocated in the stack map.
                if (isa<AllocaInst>(it)) {
                  Type *t = cast<AllocaInst>(*it).getAllocatedType();
                  instSize = builder.getInt64(
                      dataLayout.getTypeAllocSize(t));
                }
                // Also insert the size of the recorded location to know how
                // many bytes to copy at runtime.
                args.push_back(instSize);
              }
            }
          }
        } else if (isa<ReturnInst>(it) && it->getNumOperands() > 0) {
            ReturnInst &retInst = cast<ReturnInst>(*it);
            auto retValue = retInst.getReturnValue();
            if (isa<Constant>(retValue)) {
              args.push_back(retValue);
              auto instSize = builder.getInt64(
                  dataLayout.getTypeAllocSize(retValue->getType()));
              args.push_back(instSize);
            }
        }
      }
    }
    return args;
  }

};

} // end anonymous namespace

char LiveVariablesPass::id = 0;

// Automatically enable the pass.
// http://adriansampson.net/blog/clangpass.html
static void registerPass(const PassManagerBuilder &,
                         legacy::PassManagerBase &PM) {
  PM.add(new LiveVariablesPass());
}
static RegisterStandardPasses RegisterPass(
    PassManagerBuilder::EP_EarlyAsPossible, registerPass);