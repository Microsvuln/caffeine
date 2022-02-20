#include "caffeine/IR/EGraph.h"
#include "caffeine/IR/EGraphMatching.h"
#include "caffeine/IR/Operation.h"
#include <gtest/gtest.h>

using namespace caffeine;
namespace r = caffeine::ematching::reductions;

using caffeine::ematching::EMatcherBuilder;

class EMatchingTests : public ::testing::Test {
public:
  EGraph egraph;
  EMatcherBuilder builder = EMatcher::builder();

  OpRef add(const OpRef& op) {
    return EGraphNode::Create(op->type(), egraph.add(*op));
  }
};

TEST_F(EMatchingTests, commutativity) {
  r::commutativity(builder, Operation::Add);
  auto matcher = builder.build();

  auto a = add(Constant::Create(Type::int_ty(32), "a"));
  auto b = add(Constant::Create(Type::int_ty(32), "b"));
  auto c = add(BinaryOp::CreateAdd(a, b));
  auto d = add(BinaryOp::CreateAdd(b, a));

  size_t aid = egraph.add(*a);
  size_t bid = egraph.add(*b);
  size_t cid = egraph.add(*c);
  size_t did = egraph.add(*d);

  egraph.simplify(matcher);

  ASSERT_EQ(egraph.find(cid), egraph.find(did));
  ASSERT_NE(egraph.find(aid), egraph.find(bid));
}

TEST_F(EMatchingTests, associativity) {
  r::associativity(builder, Operation::Add);
  auto matcher = builder.build();

  auto a = add(Constant::Create(Type::int_ty(32), "a"));
  auto b = add(Constant::Create(Type::int_ty(32), "b"));
  auto c = add(Constant::Create(Type::int_ty(32), "c"));

  auto t1 = add(BinaryOp::CreateAdd(a, BinaryOp::CreateAdd(b, c)));
  auto t2 = add(BinaryOp::CreateAdd(BinaryOp::CreateAdd(a, b), c));

  size_t id1 = egraph.add(*t1);
  size_t id2 = egraph.add(*t2);

  egraph.simplify(matcher);

  ASSERT_EQ(egraph.find(id1), egraph.find(id2));
}