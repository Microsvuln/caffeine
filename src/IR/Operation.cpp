#include "caffeine/IR/Operation.h"
#include "Operation.h"

#include <boost/container_hash/hash.hpp>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <llvm/ADT/Hashing.h>
#include <llvm/Support/raw_ostream.h>

namespace caffeine {

Operation::Operation() : opcode_(Invalid), type_(Type::void_ty()) {}

Operation::Operation(Opcode op, Type t, const Inner& inner)
    : opcode_(static_cast<uint16_t>(op)), type_(t), inner_(inner) {}
Operation::Operation(Opcode op, Type t, Inner&& inner)
    : opcode_(static_cast<uint16_t>(op)), type_(t), inner_(std::move(inner)) {}

Operation::Operation(Opcode op, Type t)
    : opcode_(static_cast<uint16_t>(op)), type_(t) {}

Operation::Operation(Opcode op, Type t, ref<Operation>* operands)
    : opcode_(static_cast<uint16_t>(op)), type_(t),
      inner_(OpVec(operands, operands + (opcode_ & 0x3))) {
  CAFFEINE_ASSERT((opcode_ >> 6) != 1,
                  "Tried to create a constant with operands");
  // Don't use this constructor to create an invalid opcode.
  // It'll mess up constructors and destructors.
  CAFFEINE_ASSERT(op != Invalid);
  // No opcodes have > 3 operands
  CAFFEINE_ASSERT(num_operands() <= 3, "Invalid opcode");
}

Operation::Operation(Opcode op, const llvm::APInt& iconst)
    : opcode_(op), type_(Type::type_of(iconst)), inner_(iconst) {
  // Currently only ConstantInt is valid here
  CAFFEINE_ASSERT(op == ConstantInt);
}
Operation::Operation(Opcode op, llvm::APInt&& iconst)
    : opcode_(op), type_(Type::type_of(iconst)), inner_(iconst) {
  // Currently only ConstantInt is valid here
  CAFFEINE_ASSERT(op == ConstantInt);
}

Operation::Operation(Opcode op, const llvm::APFloat& fconst)
    : opcode_(op), type_(Type::type_of(fconst)), inner_(fconst) {
  CAFFEINE_ASSERT(op == ConstantFloat);
}
Operation::Operation(Opcode op, llvm::APFloat&& fconst)
    : opcode_(op), type_(Type::type_of(fconst)), inner_(fconst) {
  CAFFEINE_ASSERT(op == ConstantFloat);
}

Operation::Operation(Opcode op, Type t, const std::string& name)
    : opcode_(op), type_(t), inner_(name) {
  CAFFEINE_ASSERT(op == ConstantNamed);
}
Operation::Operation(Opcode op, Type t, uint64_t number)
    : opcode_(op), type_(t), inner_(number) {
  CAFFEINE_ASSERT(op == ConstantNumbered);
}

Operation::Operation(Opcode op, Type t, const ref<Operation>& op0)
    : opcode_(static_cast<uint16_t>(op)), type_(t), inner_(OpVec{op0}) {
  CAFFEINE_ASSERT((opcode_ >> 6) != 1,
                  "Tried to create a constant with operands");
  CAFFEINE_ASSERT(num_operands() == 1);
}
Operation::Operation(Opcode op, Type t, const ref<Operation>& op0,
                     const ref<Operation>& op1)
    : opcode_(static_cast<uint16_t>(op)), type_(t), inner_(OpVec{op0, op1}) {
  CAFFEINE_ASSERT((opcode_ >> 6) != 1,
                  "Tried to create a constant with operands");
  CAFFEINE_ASSERT(num_operands() == 2);
}
Operation::Operation(Opcode op, Type t, const ref<Operation>& op0,
                     const ref<Operation>& op1, const ref<Operation>& op2)
    : opcode_(static_cast<uint16_t>(op)), type_(t),
      inner_(OpVec{op0, op1, op2}) {
  CAFFEINE_ASSERT((opcode_ >> 6) != 1,
                  "Tried to create a constant with operands");
  CAFFEINE_ASSERT(num_operands() == 3);
}

bool Operation::operator==(const Operation& op) const {
  if (opcode_ != op.opcode_ || type_ != op.type_)
    return false;

  return std::visit(
      [](const auto& a, const auto& b) {
        if constexpr (!std::is_same_v<std::decay_t<decltype(a)>,
                                      std::decay_t<decltype(b)>>) {
          return false;
        } else if constexpr (std::is_same_v<std::decay_t<decltype(a)>,
                                            llvm::APFloat>) {
          return a.bitwiseIsEqual(b);
        } else {
          return a == b;
        }
      },
      inner_, op.inner_);
}
bool Operation::operator!=(const Operation& op) const {
  return !(*this == op);
}

const char* Operation::opcode_name() const {
  return opcode_name(static_cast<Opcode>(opcode()));
}
const char* Operation::opcode_name(Opcode op) {
  // clang-format off
  switch (op) {
  case Invalid:       return "Invalid";
  case ConstantNumbered:
  case ConstantNamed: return "Constant";
  case ConstantInt:   return "ConstantInt";
  case ConstantFloat: return "ConstantFloat";
  case ConstantArray: return "ConstantArray";
  case Undef:         return "Undef";

  case Add:   return "Add";
  case Sub:   return "Sub";
  case Mul:   return "Mul";
  case UDiv:  return "UDiv";
  case SDiv:  return "SDiv";
  case URem:  return "URem";
  case SRem:  return "SRem";
  case And:   return "And";
  case Or:    return "Or";
  case Xor:   return "Xor";
  case Shl:   return "Shl";
  case LShr:  return "LShr";
  case AShr:  return "AShr";
  case Not:   return "Not";

  case FAdd:  return "FAdd";
  case FSub:  return "FSub";
  case FMul:  return "FMul";
  case FDiv:  return "FDiv";
  case FRem:  return "FRem";
  case FNeg:  return "FNeg";

  case Trunc:   return "Trunc";
  case SExt:    return "SExt";
  case ZExt:    return "ZExt";
  case FpTrunc: return "FpTrunc";
  case FpExt:   return "FpExt";
  case FpToUI:  return "FpToUI";
  case FpToSI:  return "FpToSI";
  case UIToFp:  return "UIToFp";
  case SIToFp:  return "SIToFp";
  case Bitcast: return "Bitcast";

  case ICmpEq:
  case ICmpNe:
  case ICmpUgt:
  case ICmpUge:
  case ICmpUlt:
  case ICmpUle:
  case ICmpSgt:
  case ICmpSge:
  case ICmpSlt:
  case ICmpSle:
    return "ICmp";

  case FCmpOeq:
  case FCmpOgt:
  case FCmpOge:
  case FCmpOlt:
  case FCmpOle:
  case FCmpOne:
  case FCmpOrd:
  case FCmpUno:
  case FCmpUeq:
  case FCmpUgt:
  case FCmpUge:
  case FCmpUlt:
  case FCmpUle:
  case FCmpUne:
    return "FCmp";

  case Select: return "Select";

  case Alloc: return "Alloc";
  case Load:  return "Load";
  case Store: return "Store";

  // Silence warnings here
  case UnaryOpLast:
  case BinaryOpLast:
    break;
  }
  // clang-format on
  return "Unknown";
}

template <typename T, typename... Ts>
static std::ostream& print_spaced(std::ostream& os, const T& first,
                                  const Ts&... values) {
  os << first;
  (..., (os << ' ' << values));
  return os;
}

template <typename... Ts>
static std::ostream& print_cons(std::ostream& os, const Ts&... values) {
  static_assert(sizeof...(Ts) != 0);

  if constexpr (sizeof...(Ts) == 1) {
    (os << ... << values);
    return os;
  } else {
    return print_spaced(os << '(', values...) << ')';
  }
}

std::ostream& operator<<(std::ostream& os, const Operation& op) {
  if (const auto* constant = llvm::dyn_cast<Constant>(&op)) {
    if (constant->is_named())
      return print_cons(os, "const", constant->name());
    return print_cons(os, "const", constant->number());
  }

  if (const auto* constant = llvm::dyn_cast<ConstantInt>(&op)) {
    return print_cons(os, op.type(), constant->value().toString(10, false));
  }

  if (const auto* constant = llvm::dyn_cast<ConstantFloat>(&op)) {
    std::string s;
    llvm::raw_string_ostream o(s);
    constant->value().print(o);
    return print_cons(os, op.type(), s);
  }

  std::string name = op.opcode_name();
  std::transform(name.begin(), name.end(), name.begin(),
                 [](char c) { return std::tolower(c); });

  if (const auto* icmp = llvm::dyn_cast<ICmpOp>(&op)) {
    const char* cmp = "<unknown>";
    switch (icmp->comparison()) {
    // clang-format off
    case ICmpOpcode::EQ:  cmp = "eq";  break;
    case ICmpOpcode::NE:  cmp = "ne";  break;
    case ICmpOpcode::UGT: cmp = "ugt"; break;
    case ICmpOpcode::UGE: cmp = "uge"; break;
    case ICmpOpcode::ULT: cmp = "ult"; break;
    case ICmpOpcode::ULE: cmp = "ule"; break;
    case ICmpOpcode::SGT: cmp = "sgt"; break;
    case ICmpOpcode::SGE: cmp = "sge"; break;
    case ICmpOpcode::SLT: cmp = "slt"; break;
    case ICmpOpcode::SLE: cmp = "sle"; break;
      // clang-format on
    }

    name.push_back('.');
    name += cmp;
  }

  if (const auto* fcmp = llvm::dyn_cast<FCmpOp>(&op)) {
    const char* cmp = "<unknown>";
    switch (fcmp->comparison()) {
    // clang-format off
    case FCmpOpcode::OEQ: cmp = "oeq"; break;
    case FCmpOpcode::OGT: cmp = "ogt"; break;
    case FCmpOpcode::OGE: cmp = "oge"; break;
    case FCmpOpcode::OLT: cmp = "olt"; break;
    case FCmpOpcode::OLE: cmp = "ole"; break;
    case FCmpOpcode::ONE: cmp = "one"; break;
    case FCmpOpcode::ORD: cmp = "ord"; break;
    case FCmpOpcode::UNO: cmp = "uno"; break;
    case FCmpOpcode::UEQ: cmp = "ueq"; break;
    case FCmpOpcode::UGT: cmp = "ugt"; break;
    case FCmpOpcode::UGE: cmp = "uge"; break;
    case FCmpOpcode::ULT: cmp = "ult"; break;
    case FCmpOpcode::ULE: cmp = "ule"; break;
    case FCmpOpcode::UNE: cmp = "une"; break;
      // clang-format on
    }

    name.push_back('.');
    name += cmp;
  }

  switch (op.num_operands()) {
  case 0:
    return print_cons(os, name);
  case 1:
    return print_cons(os, name, op[0]);
  case 2:
    return print_cons(os, name, op[0], op[1]);
  case 3:
    return print_cons(os, name, op[0], op[1], op[2]);
  }

  CAFFEINE_UNREACHABLE();
}

/***************************************************
 * Constant                                        *
 ***************************************************/
Constant::Constant(Type t, const std::string& name)
    : Operation(ConstantNamed, t, name) {}
Constant::Constant(Type t, std::string&& name)
    : Operation(ConstantNamed, t, std::move(name)) {}
Constant::Constant(Type t, uint64_t number)
    : Operation(ConstantNumbered, t, number) {}

ref<Operation> Constant::Create(Type t, const std::string& name) {
  CAFFEINE_ASSERT(!name.empty(), "cannot create constant with empty name");

  return ref<Operation>(new Constant(t, name));
}
ref<Operation> Constant::Create(Type t, std::string&& name) {
  CAFFEINE_ASSERT(!name.empty(), "cannot create constant with empty name");

  return ref<Operation>(new Constant(t, std::move(name)));
}
ref<Operation> Constant::Create(Type t, uint64_t number) {
  return ref<Operation>(new Constant(t, number));
}

/***************************************************
 * ConstantInt                                     *
 ***************************************************/
ConstantInt::ConstantInt(const llvm::APInt& iconst)
    : Operation(Operation::ConstantInt, iconst) {}
ConstantInt::ConstantInt(llvm::APInt&& iconst)
    : Operation(Operation::ConstantInt, iconst) {}

ref<Operation> ConstantInt::Create(const llvm::APInt& iconst) {
  return ref<Operation>(new ConstantInt(iconst));
}
ref<Operation> ConstantInt::Create(llvm::APInt&& iconst) {
  return ref<Operation>(new ConstantInt(iconst));
}
ref<Operation> ConstantInt::Create(bool value) {
  return ConstantInt::Create(llvm::APInt(1, static_cast<uint64_t>(value)));
}

/***************************************************
 * ConstantFloat                                   *
 ***************************************************/
ConstantFloat::ConstantFloat(const llvm::APFloat& fconst)
    : Operation(Operation::ConstantFloat, fconst) {}
ConstantFloat::ConstantFloat(llvm::APFloat&& fconst)
    : Operation(Operation::ConstantFloat, fconst) {}

ref<Operation> ConstantFloat::Create(const llvm::APFloat& fconst) {
  return ref<Operation>(new ConstantFloat(fconst));
}
ref<Operation> ConstantFloat::Create(llvm::APFloat&& fconst) {
  return ref<Operation>(new ConstantFloat(fconst));
}

/***************************************************
 * ConstantArray                                   *
 ***************************************************/
ConstantArray::ConstantArray(Type t, const char* data, size_t size)
    : Operation(Opcode::ConstantArray, t,
                Inner(std::string(data, data + size))) {}

ref<Operation> ConstantArray::Create(Type index_ty, const char* data,
                                     size_t size) {
  CAFFEINE_ASSERT(index_ty.is_int(),
                  "Arrays cannot be indexed by non-integer types");
  CAFFEINE_ASSERT(
      index_ty.bitwidth() >= ilog2(size),
      "Index bitwidth is not large enough to address entire constant array");

  return ref<Operation>(
      new ConstantArray(Type::array_ty(index_ty.bitwidth()), data, size));
}

/***************************************************
 * BinaryOp                                        *
 ***************************************************/
BinaryOp::BinaryOp(Opcode op, Type t, const ref<Operation>& lhs,
                   const ref<Operation>& rhs)
    : Operation(op, t, lhs, rhs) {}

ref<Operation> BinaryOp::Create(Opcode op, const ref<Operation>& lhs,
                                const ref<Operation>& rhs) {
  CAFFEINE_ASSERT((op & 0x3) == 2, "Opcode doesn't have 2 operands");
  CAFFEINE_ASSERT(lhs, "lhs was null");
  CAFFEINE_ASSERT(rhs, "rhs was null");
  CAFFEINE_ASSERT(
      lhs->type() == rhs->type(),
      fmt::format(
          FMT_STRING(
              "BinaryOp created from operands with different types: {} != {}"),
          lhs->type(), rhs->type()));

  return ref<Operation>(new BinaryOp(op, lhs->type(), lhs, rhs));
}

#define ASSERT_INT(op) CAFFEINE_ASSERT((op)->type().is_int())
#define ASSERT_FP(op) CAFFEINE_ASSERT((op)->type().is_float())

// There's a lot of these so template them out using a macro
#define DECL_BINOP_CREATE(opcode, assert)                                      \
  ref<Operation> BinaryOp::Create##opcode(const ref<Operation>& lhs,           \
                                          const ref<Operation>& rhs) {         \
    CAFFEINE_ASSERT(lhs, "lhs was null");                                      \
    CAFFEINE_ASSERT(rhs, "rhs was null");                                      \
    assert(lhs);                                                               \
    assert(rhs);                                                               \
                                                                               \
    return Create(Opcode::opcode, lhs, rhs);                                   \
  }                                                                            \
  static_assert(true)

ref<Operation> BinaryOp::CreateAdd(const ref<Operation>& lhs,
                                   const ref<Operation>& rhs) {
  CAFFEINE_ASSERT(lhs, "lhs was null");
  CAFFEINE_ASSERT(rhs, "rhs was null");
  ASSERT_INT(lhs);
  ASSERT_INT(rhs);

  if (lhs->is<caffeine::Undef>() || rhs->is<caffeine::Undef>())
    return Undef::Create(lhs->type());

  if (is_constant_int(*lhs, 0))
    return rhs;
  if (is_constant_int(*rhs, 0))
    return lhs;

  const auto* lhs_int = llvm::dyn_cast<caffeine::ConstantInt>(lhs.get());
  const auto* rhs_int = llvm::dyn_cast<caffeine::ConstantInt>(rhs.get());
  if (lhs_int && rhs_int)
    return ConstantInt::Create(lhs_int->value() + rhs_int->value());

  return Create(Opcode::Add, lhs, rhs);
}
ref<Operation> BinaryOp::CreateSub(const ref<Operation>& lhs,
                                   const ref<Operation>& rhs) {
  CAFFEINE_ASSERT(lhs, "lhs was null");
  CAFFEINE_ASSERT(rhs, "rhs was null");
  ASSERT_INT(lhs);
  ASSERT_INT(rhs);

  if (lhs->is<caffeine::Undef>() || rhs->is<caffeine::Undef>())
    return Undef::Create(lhs->type());

  if (is_constant_int(*rhs, 0))
    return lhs;

  const auto* lhs_int = llvm::dyn_cast<caffeine::ConstantInt>(lhs.get());
  const auto* rhs_int = llvm::dyn_cast<caffeine::ConstantInt>(rhs.get());
  if (lhs_int && rhs_int)
    return ConstantInt::Create(lhs_int->value() - rhs_int->value());

  return Create(Opcode::Sub, lhs, rhs);
}
ref<Operation> BinaryOp::CreateMul(const ref<Operation>& lhs,
                                   const ref<Operation>& rhs) {
  CAFFEINE_ASSERT(lhs, "lhs was null");
  CAFFEINE_ASSERT(rhs, "rhs was null");
  ASSERT_INT(lhs);
  ASSERT_INT(rhs);

  if (is_constant_int(*lhs, 0))
    return lhs;
  if (is_constant_int(*rhs, 0))
    return rhs;

  const auto* lhs_int = llvm::dyn_cast<caffeine::ConstantInt>(lhs.get());
  const auto* rhs_int = llvm::dyn_cast<caffeine::ConstantInt>(rhs.get());
  if (lhs_int && rhs_int)
    return ConstantInt::Create(lhs_int->value() * rhs_int->value());

  return Create(Opcode::Mul, lhs, rhs);
}
ref<Operation> BinaryOp::CreateUDiv(const ref<Operation>& lhs,
                                    const ref<Operation>& rhs) {
  CAFFEINE_ASSERT(lhs, "lhs was null");
  CAFFEINE_ASSERT(rhs, "rhs was null");
  ASSERT_INT(lhs);
  ASSERT_INT(rhs);

  if (is_constant_int(*lhs, 0) || is_constant_int(*rhs, 1))
    return lhs;

  const auto* lhs_int = llvm::dyn_cast<caffeine::ConstantInt>(lhs.get());
  const auto* rhs_int = llvm::dyn_cast<caffeine::ConstantInt>(rhs.get());
  if (lhs_int && rhs_int)
    return ConstantInt::Create(lhs_int->value().udiv(rhs_int->value()));

  return Create(Opcode::UDiv, lhs, rhs);
}
ref<Operation> BinaryOp::CreateSDiv(const ref<Operation>& lhs,
                                    const ref<Operation>& rhs) {
  CAFFEINE_ASSERT(lhs, "lhs was null");
  CAFFEINE_ASSERT(rhs, "rhs was null");
  ASSERT_INT(lhs);
  ASSERT_INT(rhs);

  if (is_constant_int(*lhs, 0))
    return lhs;
  if (is_constant_int(*rhs, 1) && rhs->type().bitwidth() > 1)
    return lhs;

  const auto* lhs_int = llvm::dyn_cast<caffeine::ConstantInt>(lhs.get());
  const auto* rhs_int = llvm::dyn_cast<caffeine::ConstantInt>(rhs.get());
  if (lhs_int && rhs_int)
    return ConstantInt::Create(lhs_int->value().sdiv(rhs_int->value()));

  return Create(Opcode::SDiv, lhs, rhs);
}
ref<Operation> BinaryOp::CreateURem(const ref<Operation>& lhs,
                                    const ref<Operation>& rhs) {
  CAFFEINE_ASSERT(lhs, "lhs was null");
  CAFFEINE_ASSERT(rhs, "rhs was null");
  ASSERT_INT(lhs);
  ASSERT_INT(rhs);

  if (is_constant_int(*lhs, 0))
    return lhs;
  if (is_constant_int(*rhs, 1))
    return ConstantInt::Create(llvm::APInt(lhs->type().bitwidth(), 0));

  const auto* lhs_int = llvm::dyn_cast<caffeine::ConstantInt>(lhs.get());
  const auto* rhs_int = llvm::dyn_cast<caffeine::ConstantInt>(rhs.get());
  if (lhs_int && rhs_int)
    return ConstantInt::Create(lhs_int->value().urem(rhs_int->value()));

  return Create(Opcode::URem, lhs, rhs);
}
ref<Operation> BinaryOp::CreateSRem(const ref<Operation>& lhs,
                                    const ref<Operation>& rhs) {
  CAFFEINE_ASSERT(lhs, "lhs was null");
  CAFFEINE_ASSERT(rhs, "rhs was null");
  ASSERT_INT(lhs);
  ASSERT_INT(rhs);

  if (is_constant_int(*lhs, 0))
    return lhs;
  if (is_constant_int(*rhs, 1) && rhs->type().bitwidth() > 1)
    return ConstantInt::Create(llvm::APInt(lhs->type().bitwidth(), 0));

  const auto* lhs_int = llvm::dyn_cast<caffeine::ConstantInt>(lhs.get());
  const auto* rhs_int = llvm::dyn_cast<caffeine::ConstantInt>(rhs.get());
  if (lhs_int && rhs_int)
    return ConstantInt::Create(lhs_int->value().srem(rhs_int->value()));

  return Create(Opcode::SRem, lhs, rhs);
}

ref<Operation> BinaryOp::CreateAnd(const ref<Operation>& lhs,
                                   const ref<Operation>& rhs) {
  CAFFEINE_ASSERT(lhs, "lhs was null");
  CAFFEINE_ASSERT(rhs, "rhs was null");
  ASSERT_INT(lhs);
  ASSERT_INT(rhs);

  if (is_constant_int(*lhs, 0))
    return lhs;
  if (is_constant_int(*rhs, 0))
    return rhs;

  const auto* lhs_int = llvm::dyn_cast<caffeine::ConstantInt>(lhs.get());
  const auto* rhs_int = llvm::dyn_cast<caffeine::ConstantInt>(rhs.get());
  if (lhs_int && rhs_int)
    return ConstantInt::Create(lhs_int->value() & rhs_int->value());

  return Create(Opcode::And, lhs, rhs);
}
ref<Operation> BinaryOp::CreateOr(const ref<Operation>& lhs,
                                  const ref<Operation>& rhs) {
  CAFFEINE_ASSERT(lhs, "lhs was null");
  CAFFEINE_ASSERT(rhs, "rhs was null");
  ASSERT_INT(lhs);
  ASSERT_INT(rhs);

  if (is_constant_int(*lhs, 0))
    return rhs;
  if (is_constant_int(*rhs, 0))
    return lhs;

  const auto* lhs_int = llvm::dyn_cast<caffeine::ConstantInt>(lhs.get());
  const auto* rhs_int = llvm::dyn_cast<caffeine::ConstantInt>(rhs.get());
  if (lhs_int && rhs_int)
    return ConstantInt::Create(lhs_int->value() | rhs_int->value());

  return Create(Opcode::Or, lhs, rhs);
}
ref<Operation> BinaryOp::CreateXor(const ref<Operation>& lhs,
                                   const ref<Operation>& rhs) {
  CAFFEINE_ASSERT(lhs, "lhs was null");
  CAFFEINE_ASSERT(rhs, "rhs was null");
  ASSERT_INT(lhs);
  ASSERT_INT(rhs);

  if (lhs->is<caffeine::Undef>() || rhs->is<caffeine::Undef>())
    return Undef::Create(lhs->type());

  if (is_constant_int(*lhs, 0))
    return rhs;
  if (is_constant_int(*rhs, 0))
    return lhs;

  const auto* lhs_int = llvm::dyn_cast<caffeine::ConstantInt>(lhs.get());
  const auto* rhs_int = llvm::dyn_cast<caffeine::ConstantInt>(rhs.get());
  if (lhs_int && rhs_int)
    return ConstantInt::Create(lhs_int->value() ^ rhs_int->value());

  return Create(Opcode::Xor, lhs, rhs);
}
ref<Operation> BinaryOp::CreateShl(const ref<Operation>& lhs,
                                   const ref<Operation>& rhs) {
  CAFFEINE_ASSERT(lhs, "lhs was null");
  CAFFEINE_ASSERT(rhs, "rhs was null");
  ASSERT_INT(lhs);
  ASSERT_INT(rhs);

  if (is_constant_int(*lhs, 0) || is_constant_int(*rhs, 0))
    return lhs;

  const auto* lhs_int = llvm::dyn_cast<caffeine::ConstantInt>(lhs.get());
  const auto* rhs_int = llvm::dyn_cast<caffeine::ConstantInt>(rhs.get());
  if (lhs_int && rhs_int)
    return ConstantInt::Create(lhs_int->value() << rhs_int->value());

  return Create(Opcode::Shl, lhs, rhs);
}
ref<Operation> BinaryOp::CreateLShr(const ref<Operation>& lhs,
                                    const ref<Operation>& rhs) {
  CAFFEINE_ASSERT(lhs, "lhs was null");
  CAFFEINE_ASSERT(rhs, "rhs was null");
  ASSERT_INT(lhs);
  ASSERT_INT(rhs);

  if (is_constant_int(*lhs, 0) || is_constant_int(*rhs, 0))
    return lhs;

  const auto* lhs_int = llvm::dyn_cast<caffeine::ConstantInt>(lhs.get());
  const auto* rhs_int = llvm::dyn_cast<caffeine::ConstantInt>(rhs.get());
  if (lhs_int && rhs_int)
    return ConstantInt::Create(lhs_int->value().lshr(rhs_int->value()));

  return Create(Opcode::LShr, lhs, rhs);
}
ref<Operation> BinaryOp::CreateAShr(const ref<Operation>& lhs,
                                    const ref<Operation>& rhs) {
  CAFFEINE_ASSERT(lhs, "lhs was null");
  CAFFEINE_ASSERT(rhs, "rhs was null");
  ASSERT_INT(lhs);
  ASSERT_INT(rhs);

  if (is_constant_int(*lhs, 0) || is_constant_int(*rhs, 0))
    return lhs;

  const auto* lhs_int = llvm::dyn_cast<caffeine::ConstantInt>(lhs.get());
  const auto* rhs_int = llvm::dyn_cast<caffeine::ConstantInt>(rhs.get());
  if (lhs_int && rhs_int)
    return ConstantInt::Create(lhs_int->value().ashr(rhs_int->value()));

  return Create(Opcode::AShr, lhs, rhs);
}

DECL_BINOP_CREATE(FAdd, ASSERT_FP);
DECL_BINOP_CREATE(FSub, ASSERT_FP);
DECL_BINOP_CREATE(FMul, ASSERT_FP);
DECL_BINOP_CREATE(FDiv, ASSERT_FP);
DECL_BINOP_CREATE(FRem, ASSERT_FP);

/***************************************************
 * UnaryOp                                         *
 ***************************************************/
UnaryOp::UnaryOp(Opcode op, Type t, const ref<Operation>& operand)
    : Operation(op, t, operand) {}

ref<Operation> UnaryOp::Create(Opcode op, const ref<Operation>& operand) {
  CAFFEINE_ASSERT(operand, "operand was null");
  CAFFEINE_ASSERT((op & 0x3) == 1, "Opcode doesn't have 2 operands");

  return ref<Operation>(new UnaryOp(op, operand->type(), operand));
}

#define DECL_UNOP_CREATE(opcode, assert)                                       \
  ref<Operation> UnaryOp::Create##opcode(const ref<Operation>& operand) {      \
    assert(operand);                                                           \
                                                                               \
    return Create(Opcode::opcode, operand);                                    \
  }                                                                            \
  static_assert(true)

ref<Operation> UnaryOp::CreateNot(const ref<Operation>& operand) {
  ASSERT_INT(operand);

  if (const auto* op = llvm::dyn_cast<caffeine::ConstantInt>(operand.get()))
    return ConstantInt::Create(~op->value());

  return Create(Opcode::Not, operand);
}

DECL_UNOP_CREATE(FNeg, ASSERT_FP);

ref<Operation> UnaryOp::CreateTrunc(Type tgt, const ref<Operation>& operand) {
  CAFFEINE_ASSERT(tgt.is_int());
  CAFFEINE_ASSERT(operand->type().is_int());
  CAFFEINE_ASSERT(tgt.bitwidth() < operand->type().bitwidth());

  if (llvm::isa<caffeine::Undef>(operand.get()))
    return Undef::Create(tgt);

  if (const auto* op = llvm::dyn_cast<caffeine::ConstantInt>(operand.get()))
    return ConstantInt::Create(op->value().trunc(tgt.bitwidth()));

  return ref<Operation>(new UnaryOp(Opcode::Trunc, tgt, operand));
}
ref<Operation> UnaryOp::CreateZExt(Type tgt, const ref<Operation>& operand) {
  CAFFEINE_ASSERT(tgt.is_int());
  CAFFEINE_ASSERT(operand->type().is_int());
  CAFFEINE_ASSERT(tgt.bitwidth() > operand->type().bitwidth());

  if (const auto* op = llvm::dyn_cast<caffeine::ConstantInt>(operand.get()))
    return ConstantInt::Create(op->value().zext(tgt.bitwidth()));

  return ref<Operation>(new UnaryOp(Opcode::ZExt, tgt, operand));
}
ref<Operation> UnaryOp::CreateSExt(Type tgt, const ref<Operation>& operand) {
  CAFFEINE_ASSERT(tgt.is_int());
  CAFFEINE_ASSERT(operand->type().is_int());
  CAFFEINE_ASSERT(tgt.bitwidth() > operand->type().bitwidth());

  if (llvm::isa<caffeine::Undef>(operand.get()))
    return Undef::Create(tgt);

  if (const auto* op = llvm::dyn_cast<caffeine::ConstantInt>(operand.get()))
    return ConstantInt::Create(op->value().sext(tgt.bitwidth()));

  return ref<Operation>(new UnaryOp(Opcode::SExt, tgt, operand));
}
ref<Operation> UnaryOp::CreateFpTrunc(Type tgt, const ref<Operation>& operand) {
  CAFFEINE_ASSERT(tgt.is_float());
  CAFFEINE_ASSERT(operand->type().is_float());
  CAFFEINE_ASSERT(tgt.exponent_bits() < operand->type().exponent_bits() &&
                  tgt.mantissa_bits() < operand->type().mantissa_bits());

  return ref<Operation>(new UnaryOp(Opcode::FpTrunc, tgt, operand));
}
ref<Operation> UnaryOp::CreateFpExt(Type tgt, const ref<Operation>& operand) {
  CAFFEINE_ASSERT(tgt.is_float());
  CAFFEINE_ASSERT(operand->type().is_float());
  CAFFEINE_ASSERT(tgt.exponent_bits() > operand->type().exponent_bits() &&
                  tgt.mantissa_bits() > operand->type().mantissa_bits());

  return ref<Operation>(new UnaryOp(Opcode::FpExt, tgt, operand));
}
ref<Operation> UnaryOp::CreateFpToUI(Type tgt, const ref<Operation>& operand) {
  CAFFEINE_ASSERT(tgt.is_int());
  CAFFEINE_ASSERT(operand->type().is_float());

  return ref<Operation>(new UnaryOp(Opcode::FpToUI, tgt, operand));
}
ref<Operation> UnaryOp::CreateFpToSI(Type tgt, const ref<Operation>& operand) {
  CAFFEINE_ASSERT(tgt.is_int());
  CAFFEINE_ASSERT(operand->type().is_float());

  return ref<Operation>(new UnaryOp(Opcode::FpToSI, tgt, operand));
}
ref<Operation> UnaryOp::CreateUIToFp(Type tgt, const ref<Operation>& operand) {
  CAFFEINE_ASSERT(tgt.is_float());
  CAFFEINE_ASSERT(operand->type().is_int());

  return ref<Operation>(new UnaryOp(Opcode::UIToFp, tgt, operand));
}
ref<Operation> UnaryOp::CreateSIToFp(Type tgt, const ref<Operation>& operand) {
  CAFFEINE_ASSERT(tgt.is_float());
  CAFFEINE_ASSERT(operand->type().is_int());

  return ref<Operation>(new UnaryOp(Opcode::SIToFp, tgt, operand));
}
ref<Operation> UnaryOp::CreateBitcast(Type tgt, const ref<Operation>& operand) {
  // TODO: Validate sizes if possible.
  // CAFFEINE_ASSERT(tgt.byte_size() == operand->type().byte_size());

  return ref<Operation>(new UnaryOp(Opcode::Bitcast, tgt, operand));
}

/***************************************************
 * SelectOp                                        *
 ***************************************************/
SelectOp::SelectOp(Type t, const ref<Operation>& cond,
                   const ref<Operation>& true_val,
                   const ref<Operation>& false_val)
    : Operation(Opcode::Select, t, cond, true_val, false_val) {}

ref<Operation> SelectOp::Create(const ref<Operation>& cond,
                                const ref<Operation>& true_value,
                                const ref<Operation>& false_value) {
  CAFFEINE_ASSERT(cond, "cond was null");
  CAFFEINE_ASSERT(true_value, "true_value was null");
  CAFFEINE_ASSERT(false_value, "false_value was null");

  CAFFEINE_ASSERT(cond->type() == Type::int_ty(1),
                  "select condition was not an i1");
  CAFFEINE_ASSERT(true_value->type() == false_value->type(),
                  "select values had different types");

  if (const auto* vcond = llvm::dyn_cast<caffeine::ConstantInt>(cond.get()))
    return vcond->value() == 1 ? true_value : false_value;

  return ref<Operation>(
      new SelectOp(true_value->type(), cond, true_value, false_value));
}

/***************************************************
 * ICmpOp                                          *
 ***************************************************/
ICmpOp::ICmpOp(ICmpOpcode cmp, Type t, const ref<Operation>& lhs,
               const ref<Operation>& rhs)
    : BinaryOp(static_cast<Opcode>(
                   detail::opcode(icmp_base, 2, static_cast<uint16_t>(cmp))),
               t, lhs, rhs) {}

ref<Operation> ICmpOp::CreateICmp(ICmpOpcode cmp, const ref<Operation>& lhs,
                                  const ref<Operation>& rhs) {
  CAFFEINE_ASSERT(rhs, "rhs was null");
  CAFFEINE_ASSERT(lhs, "lhs was null");
  CAFFEINE_ASSERT(rhs->type() == lhs->type(),
                  "cannot compare icmp operands with different types");
  CAFFEINE_ASSERT(lhs->type().is_int(),
                  "icmp can only be created with integer operands");

  const auto* lhs_int = llvm::dyn_cast<caffeine::ConstantInt>(lhs.get());
  const auto* rhs_int = llvm::dyn_cast<caffeine::ConstantInt>(rhs.get());
  if (lhs_int && rhs_int)
    return ConstantInt::Create(
        constant_int_compare(cmp, lhs_int->value(), rhs_int->value()));

  return ref<Operation>(new ICmpOp(cmp, Type::int_ty(1), lhs, rhs));
}
ref<Operation> ICmpOp::CreateICmp(ICmpOpcode cmp, int64_t lhs,
                                  const ref<Operation>& rhs) {
  CAFFEINE_ASSERT(rhs, "rhs was null");
  CAFFEINE_ASSERT(rhs->type().is_int(),
                  "icmp can only be created with integer operands");

  auto literal = llvm::APInt(64, static_cast<uint64_t>(lhs));
  return ICmpOp::CreateICmp(
      cmp, ConstantInt::Create(literal.sextOrTrunc(rhs->type().bitwidth())),
      rhs);
}
ref<Operation> ICmpOp::CreateICmp(ICmpOpcode cmp, const ref<Operation>& lhs,
                                  int64_t rhs) {
  CAFFEINE_ASSERT(lhs, "rhs was null");
  CAFFEINE_ASSERT(lhs->type().is_int(),
                  "icmp can only be created with integer operands");

  auto literal = llvm::APInt(64, static_cast<uint64_t>(rhs));
  return ICmpOp::CreateICmp(
      cmp, lhs,
      ConstantInt::Create(literal.sextOrTrunc(lhs->type().bitwidth())));
}

/***************************************************
 * FCmpOp                                          *
 ***************************************************/
FCmpOp::FCmpOp(FCmpOpcode cmp, Type t, const ref<Operation>& lhs,
               const ref<Operation>& rhs)
    : BinaryOp(static_cast<Opcode>(
                   detail::opcode(fcmp_base, 2, static_cast<uint16_t>(cmp))),
               t, lhs, rhs) {}

ref<Operation> FCmpOp::CreateFCmp(FCmpOpcode cmp, const ref<Operation>& lhs,
                                  const ref<Operation>& rhs) {
  CAFFEINE_ASSERT(rhs, "rhs was null");
  CAFFEINE_ASSERT(lhs, "lhs was null");
  CAFFEINE_ASSERT(rhs->type() == lhs->type(),
                  "cannot compare icmp operands with different types");
  CAFFEINE_ASSERT(lhs->type().is_float(),
                  "icmp can only be created with integer operands");

  return ref<Operation>(new FCmpOp(cmp, Type::type_of<bool>(), lhs, rhs));
}

/***************************************************
 * AllocOp                                         *
 ***************************************************/
AllocOp::AllocOp(const ref<Operation>& size, const ref<Operation>& defaultval)
    : Operation(Opcode::Alloc, Type::array_ty(size->type().bitwidth()), size,
                defaultval) {}

ref<Operation> AllocOp::Create(const ref<Operation>& size,
                               const ref<Operation>& defaultval) {
  CAFFEINE_ASSERT(size, "size was null");
  CAFFEINE_ASSERT(defaultval, "defaultval was null");
  // To be fully correct, this should be validating that the bitwidth of size
  // is the correct one for the architecture model. Unfortunately we don't
  // have enough information here to validate that so instead we just ensure
  // that size is an integer.
  CAFFEINE_ASSERT(size->type().is_int(), "Array size must be an integer type");
  CAFFEINE_ASSERT(defaultval->type() == Type::int_ty(8));
  return ref<Operation>(new AllocOp(size, defaultval));
}

/***************************************************
 * LoadOp                                          *
 ***************************************************/
LoadOp::LoadOp(const ref<Operation>& data, const ref<Operation>& offset)
    : Operation(Opcode::Load, Type::int_ty(8), data, offset) {}

ref<Operation> LoadOp::Create(const ref<Operation>& data,
                              const ref<Operation>& offset) {
  CAFFEINE_ASSERT(data, "data was null");
  CAFFEINE_ASSERT(offset, "offset was null");
  CAFFEINE_ASSERT(offset->type().is_int(),
                  "Load offset must be a pointer-sized integer type");
  return ref<Operation>(new LoadOp(data, offset));
}

/***************************************************
 * StoreOp                                         *
 ***************************************************/
StoreOp::StoreOp(const ref<Operation>& data, const ref<Operation>& offset,
                 const ref<Operation>& value)
    : Operation(Opcode::Store, data->type(), data, offset, value) {}

ref<Operation> StoreOp::Create(const ref<Operation>& data,
                               const ref<Operation>& offset,
                               const ref<Operation>& value) {
  CAFFEINE_ASSERT(data, "data was null");
  CAFFEINE_ASSERT(offset, "offset was null");
  CAFFEINE_ASSERT(value, "value was null");

  CAFFEINE_ASSERT(offset->type().is_int(),
                  "Store offset must be a pointer-size integer type");
  CAFFEINE_ASSERT(value->type() == Type::int_ty(8), "Value must be of type i8");

  return ref<Operation>(new StoreOp(data, offset, value));
}

/***************************************************
 * Undef                                           *
 ***************************************************/
Undef::Undef(const Type& t) : Operation(Opcode::Undef, t) {}

ref<Operation> Undef::Create(const Type& t) {
  return ref<Operation>(new Undef(t));
}

/***************************************************
 * hashing implementations                         *
 ***************************************************/
static llvm::hash_code hash_value(const ref<Operation>& op) {
  return std::hash<ref<Operation>>()(op);
}

llvm::hash_code hash_value(const Operation& op) {
  std::size_t hash = llvm::hash_combine(op.opcode(), op.type());

  return std::visit(
      [&](const auto& v) {
        using type = std::decay_t<decltype(v)>;

        if constexpr (std::is_same_v<type, Operation::OpVec>) {
          return llvm::hash_combine(
              hash, llvm::hash_combine_range(v.begin(), v.end()));
        } else if constexpr (std::is_same_v<type, std::monostate>) {
          return llvm::hash_combine(hash, std::hash<type>()(v));
        } else {
          return llvm::hash_combine(hash, v);
        }
      },
      op.inner_);
}

} // namespace caffeine
