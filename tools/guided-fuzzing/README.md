# Using Caffeine with AFL++

[AFL++](https://github.com/AFLplusplus/AFLplusplus) is a fuzzer which supports custom mutators.
This means that when AFL++ is fuzzing a given program with some test case, we can inspect the
test case and modify it. This enables Caffeine to symbolically execute the given program with
a test case, and discover which other branches are nearby in the search space.

# Usage

## User program setup

In order for this custom mutator to work, we need two artifacts:
1. The binary for the program that is being tested
2. The LLVM assembly instructions for the program that is being tested

The first of those can be obtained by using AFL++ to compile your program, the second one must
be generated by you. This can be done by using one of:
1. The `-emit-llvm` clang flag
2. [gllvm](https://github.com/SRI-CSL/gllvm)
3. [wllvm](https://github.com/SRI-CSL/whole-program-llvm)

## Operation

In order to start fuzzing, set the following environment variables:
* `AFL_CUSTOM_MUTATOR_LIBRARY="libafl-guided-mutator.so"` (or the full path to the shared
library)
* `CAFFEINE_FUZZ_TARGET=input.ll` where `input.ll` is the file that contains the LLVM assembly
mentioned in _User program setup_. This is necessary because Caffeine only operated on LLVM
assembly and not machine code.

Finally, run the fuzzer: `afl-fuzz -i inputs -o findings ./a.out` (if for example, the seed
test cases exist in `inputs/`, the results are placed in `findings/` and the binary name
is `a.out`).

## Pitfalls

Since the binary is compiled twice, a significant difference between
the two binaries may make Caffeine less effective. For example, if
Caffeine is operating on an unoptimized binary and AFL++ is running an
optimized binary, it is possible that Caffeine will find additional branches
which do not increase coverage in the optimized binary. As a result, AFL++ will always throw out these new testcases, and
if Caffeine keeps submitting them, no progress will be make.