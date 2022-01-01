#include "caffeine/Interpreter/ExternalFunction.h"

namespace caffeine {

class CaffeineAssertFunction : public ExternalFunction {
public:
  void call(InterpreterContext& ctx, Span<LLVMValue> args) const override;
};

} // namespace caffeine
