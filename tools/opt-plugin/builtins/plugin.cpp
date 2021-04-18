
#include "builtins.h"
#include "caffeine/Support/Assert.h"
#include "plugins.h"
#include <fmt/format.h>
#include <llvm/Pass.h>
#include <llvm/Passes/PassPlugin.h>

using namespace llvm;

namespace caffeine {
bool GenBuiltinsPass::runOnModule(Module& module) {
  bool updated = false;

  for (llvm::Function& function : module.functions()) {
    if (!function.isDeclaration())
      continue;

    auto name = function.getName();
    if (name.startswith("llvm.memset.")) {
      caffeine::generateMemset(&module, &function);
      updated = true;
      continue;
    }

    if (name.startswith("llvm.memcpy.")) {
      caffeine::generateMemcpy(&module, &function);
      updated = true;
      continue;
    }
  }

  return updated;
}

PreservedAnalyses GenBuiltinsPass::run(Module& M, ModuleAnalysisManager&) {
  return runOnModule(M) ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

template <>
char LegacyModulePassAdapter<GenBuiltinsPass>::ID = 0;

namespace {
  static RegisterPass<LegacyModulePassAdapter<GenBuiltinsPass>>
      X(/* PassArg = */ GenBuiltinsPass::Name,
        /* Name    = */
        "Generate implementations of LLVM builtins suitable for execution by "
        "caffeine",
        /* CFGonly = */ false,
        /* isAnalysis = */ false);
}

} // namespace caffeine
