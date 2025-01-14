#include "caffeine/Interpreter/Executor.h"
#include "caffeine/Interpreter/CaffeineContext.h"
#include "caffeine/Interpreter/ExprEval.h"
#include "caffeine/Interpreter/Interpreter.h"
#include "caffeine/Interpreter/Store.h"
#include "caffeine/Support/UnsupportedOperation.h"
#include <boost/range/algorithm/remove_if.hpp>
#include <thread>
#include <z3++.h>

namespace caffeine {

void Executor::run_worker() {
  auto solver = caffeine->build_solver();
  {
    std::lock_guard<std::mutex> guard(mutex_);
    solvers.push_back(solver);
  }
  InterpreterContext::BackingList queue;

  while (auto ctx = store->next_context()) {
    queue.clear();
    queue.push_back(std::make_unique<InterpreterContext::ContextQueueEntry>(
        std::move(ctx.value())));

    auto guard =
        UnsupportedOperation::SetCurrentContext(&queue.front()->context);

    while (!queue.empty()) {
      if (should_stop != nullptr && should_stop->load()) {
        queue.clear();
        break;
      };

      guard.update(&queue.front()->context);

      InterpreterContext ictx{&queue, 0, solver, caffeine};

      try {
        Interpreter interp{&ictx};
        interp.execute();
      } catch (ExprEvaluator::Unevaluatable& ex) {
        logger->log_failure(nullptr, ctx.value(),
                            Failure(Assertion(), ex.what()));
        break;
      } catch (UnsupportedOperationException&) {
        // The assert that threw this already printed an error message
        // TODO: We should have a better way to indicate that this failed to the
        //       parent program.

        logger->log_failure(
            nullptr, ctx.value(),
            Failure(Assertion(), "internal error: unsupported operation"));
        break;
      }

      auto it = boost::remove_if(queue,
                                 [](const auto& entry) { return entry->dead; });
      queue.erase(it, queue.end());

      while (queue.size() > 1) {
        store->add_context(std::move(queue.back()->context));
        queue.pop_back();
      }
    }
  }
}

Executor::Executor(CaffeineContext* caffeine, const ExecutorOptions& options,
                   std::shared_ptr<std::atomic_bool> should_stop)
    : caffeine(caffeine), policy(caffeine->policy()), store(caffeine->store()),
      logger(caffeine->logger()), options(options), should_stop(should_stop) {}

void Executor::run() {
  if (options.num_threads == 1) {
    run_worker();
    return;
  }

  std::vector<std::thread> threads;

  for (uint32_t i = 0; i < options.num_threads; i++) {
    threads.emplace_back([&] { run_worker(); });
  }

  for (auto& thread : threads) {
    thread.join();
  }
}

void Executor::interrupt() {
  // Solvers should stop accepting requests
  if (should_stop != nullptr) {
    should_stop->store(true);
  }

  // The queue should stop assigning work
  store->shutdown();

  // Currently executing solvers should stop what they're doing
  {
    std::lock_guard<std::mutex> guard(mutex_);
    for (auto& solver : solvers) {
      if (solver)
        solver->interrupt();
    }

    solvers.clear();
  }
}

} // namespace caffeine
