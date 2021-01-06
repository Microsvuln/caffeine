
#ifndef CAFFEINE_IR_OPERATION_INL
#define CAFFEINE_IR_OPERATION_INL

#include <functional>
#include <type_traits>

#include "caffeine/IR/Operation.h"

/**
 * This file is meant to hold one-liners so that they can be inlined.
 * This way we still get the optimization advantages but they don't
 * obscure the interface of the operation classes.
 *
 * If a method is either
 *  - bigger than a few lines, or
 *  - would need an extra header
 * then it shouldn't go in this file and should instead be put within
 * Operation.cpp so as to minimize compile time.
 *
 * The exception here is any template methods. Those should be declared
 * in the header and defined here.
 */

namespace caffeine {

// All derived operation types should be the same size
static_assert(sizeof(ConstantInt) == sizeof(Operation));
static_assert(sizeof(ConstantFloat) == sizeof(Operation));
static_assert(sizeof(ConstantArray) == sizeof(Operation));
static_assert(sizeof(Constant) == sizeof(Operation));
static_assert(sizeof(BinaryOp) == sizeof(Operation));
static_assert(sizeof(UnaryOp) == sizeof(Operation));
static_assert(sizeof(SelectOp) == sizeof(Operation));
static_assert(sizeof(ICmpOp) == sizeof(Operation));
static_assert(sizeof(FCmpOp) == sizeof(Operation));
static_assert(sizeof(AllocOp) == sizeof(Operation));
static_assert(sizeof(LoadOp) == sizeof(Operation));
static_assert(sizeof(StoreOp) == sizeof(Operation));
static_assert(sizeof(Undef) == sizeof(Operation));

namespace detail {
  template <typename T>
  class double_deref_iterator {
  private:
    T* inner;

    using self = double_deref_iterator<T>;

  public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = std::remove_reference_t<decltype(*std::declval<T>())>;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    double_deref_iterator(T* inner) : inner(inner) {}

    reference operator*() const {
      return **inner;
    }
    pointer operator->() const {
      return std::addressof(**inner);
    }

    self& operator++() {
      inner++;
      return *this;
    }
    self& operator--() {
      inner--;
      return *this;
    }

    self operator++(int) {
      return self(inner++);
    }
    self operator--(int) {
      return self(inner--);
    }

    self operator+(difference_type n) const {
      return self(inner + n);
    }
    self operator-(difference_type n) const {
      return self(inner - n);
    }

    self& operator+=(difference_type n) {
      inner += n;
      return *this;
    }
    self& operator-=(difference_type n) {
      inner -= n;
      return *this;
    }

    difference_type operator-(self b) const {
      return self(inner - b.inner);
    }

    bool operator==(self b) const {
      return inner == b.inner;
    }
    bool operator!=(self b) const {
      return inner != b.inner;
    }

    bool operator<=(self b) const {
      return inner <= b.inner;
    }
    bool operator>=(self b) const {
      return inner >= b.inner;
    }
    bool operator<(self b) const {
      return inner < b.inner;
    }
    bool operator>(self b) const {
      return inner > b.inner;
    }

    reference operator[](difference_type n) const {
      return *(*this + n);
    }
  };

  template <typename T>
  double_deref_iterator<T>
  operator+(typename double_deref_iterator<T>::difference_type n,
            double_deref_iterator<T> a) {
    return a + n;
  }
} // namespace detail

template <size_t N>
Operation::Operation(Opcode op, Type t, ref<Operation> (&operands)[N])
    : Operation(([&] {
                  // Need the lambda so that this is evaluated before the
                  // delegated constructor runs.
                  CAFFEINE_ASSERT(((uint16_t)op & 0x3) <= N);
                  return op;
                })(),
                t, (ref<Opcode>*)operands) {}

inline bool Operation::valid() const {
  return opcode_ != 0;
}

inline uint16_t Operation::opcode() const {
  return opcode_;
}

inline uint32_t Operation::refcnt() const {
  return refcount;
}

inline size_t Operation::num_operands() const {
  return opcode_ & 0x3;
}

inline ref<Operation> Operation::as_ref() {
  CAFFEINE_ASSERT(refcount != 0, "Unable to convert non-refcounted Operation "
                                 "instance to a refcounted one");
  return ref<Operation>(this);
}
inline ref<const Operation> Operation::as_ref() const {
  CAFFEINE_ASSERT(refcount != 0, "Unable to convert non-refcounted Operation "
                                 "instance to a refcounted one");
  return ref<const Operation>(this);
}

inline llvm::iterator_range<Operation::operand_iterator> Operation::operands() {
  if (auto* vec = std::get_if<OpVec>(&inner_))
    return llvm::iterator_range<Operation::operand_iterator>{
        operand_iterator(vec->data()),
        operand_iterator(vec->data() + num_operands())};

  return llvm::iterator_range<Operation::operand_iterator>{
      operand_iterator(nullptr), operand_iterator(nullptr)};
}
inline llvm::iterator_range<Operation::const_operand_iterator>
Operation::operands() const {
  if (const auto* vec = std::get_if<OpVec>(&inner_))
    return llvm::iterator_range<Operation::const_operand_iterator>{
        const_operand_iterator(vec->data()),
        const_operand_iterator(vec->data() + num_operands())};

  return llvm::iterator_range<Operation::const_operand_iterator>{
      const_operand_iterator(nullptr), const_operand_iterator(nullptr)};
}

inline uint16_t Operation::aux_data() const {
  return (opcode() >> 2) & 0xF;
}
inline Type Operation::type() const {
  return type_;
}

inline bool Operation::is_constant() const {
  // The opcodes of constant operations are currently represented as (1, 0,
  // <arbitrary aux data>). If this representation change then we'll have to
  // update this function.
  return (opcode_ >> 6) == 1;
}

inline Operation& Operation::operator[](size_t idx) {
  CAFFEINE_ASSERT(idx < num_operands(),
                  "Tried to access out-of-bounds operand");
  return *operand_at(idx);
}
inline const Operation& Operation::operator[](size_t idx) const {
  CAFFEINE_ASSERT(idx < num_operands(),
                  "Tried to access out-of-bounds operand");
  return *operand_at(idx);
}

inline ref<Operation>& Operation::operand_at(size_t idx) {
  return std::get<OpVec>(inner_)[idx];
}
inline const ref<Operation>& Operation::operand_at(size_t idx) const {
  return std::get<OpVec>(inner_)[idx];
}

/***************************************************
 * Constant                                        *
 ***************************************************/
inline std::string_view Constant::name() const {
  CAFFEINE_ASSERT(is_named(), "tried to access name of unnamed constant");
  return std::get<std::string>(inner_);
}
inline uint64_t Constant::number() const {
  CAFFEINE_ASSERT(is_numbered(), "tried to access number of named constant");
  return std::get<uint64_t>(inner_);
}

inline bool Constant::is_numbered() const {
  return opcode() == ConstantNumbered;
}
inline bool Constant::is_named() const {
  return opcode() == ConstantNamed;
}

/***************************************************
 * ConstantInt                                     *
 ***************************************************/
inline llvm::APInt& ConstantInt::value() {
  return std::get<llvm::APInt>(inner_);
}
inline const llvm::APInt& ConstantInt::value() const {
  return std::get<llvm::APInt>(inner_);
}

/***************************************************
 * ConstantFloat                                   *
 ***************************************************/
inline llvm::APFloat& ConstantFloat::value() {
  return std::get<llvm::APFloat>(inner_);
}
inline const llvm::APFloat& ConstantFloat::value() const {
  return std::get<llvm::APFloat>(inner_);
}

/***************************************************
 * ConstantArray                                   *
 ***************************************************/
inline llvm::ArrayRef<char> ConstantArray::data() const {
  const std::string& str = std::get<std::string>(inner_);
  return llvm::ArrayRef<char>(str.data(), str.size());
}

/***************************************************
 * BinaryOp                                        *
 ***************************************************/
inline const ref<Operation>& BinaryOp::lhs() const {
  return operand_at(0);
}
inline const ref<Operation>& BinaryOp::rhs() const {
  return operand_at(1);
}

inline ref<Operation>& BinaryOp::lhs() {
  return operand_at(0);
}
inline ref<Operation>& BinaryOp::rhs() {
  return operand_at(1);
}

/***************************************************
 * UnaryOp                                         *
 ***************************************************/
inline ref<Operation>& UnaryOp::operand() {
  return operand_at(0);
}
inline const ref<Operation>& UnaryOp::operand() const {
  return operand_at(0);
}

/***************************************************
 * SelectOp                                        *
 ***************************************************/
inline ref<Operation>& SelectOp::condition() {
  return operand_at(0);
}
inline ref<Operation>& SelectOp::true_value() {
  return operand_at(1);
}
inline ref<Operation>& SelectOp::false_value() {
  return operand_at(2);
}

inline const ref<Operation>& SelectOp::condition() const {
  return operand_at(0);
}
inline const ref<Operation>& SelectOp::true_value() const {
  return operand_at(1);
}
inline const ref<Operation>& SelectOp::false_value() const {
  return operand_at(2);
}

/***************************************************
 * ICmpOp                                          *
 ***************************************************/
inline ICmpOpcode ICmpOp::comparison() const {
  return static_cast<ICmpOpcode>(aux_data());
}

inline bool ICmpOp::is_signed() const {
  auto aux = aux_data();
  return aux < static_cast<uint16_t>(ICmpOpcode::EQ) && (aux & (1 << 2));
}
inline bool ICmpOp::is_unsigned() const {
  auto aux = aux_data();
  return aux < static_cast<uint16_t>(ICmpOpcode::EQ) && !(aux & (1 << 2));
}

/***************************************************
 * FCmpOp                                          *
 ***************************************************/
inline FCmpOpcode FCmpOp::comparison() const {
  return static_cast<FCmpOpcode>(aux_data());
}

inline bool FCmpOp::is_ordered() const {
  return aux_data() & 010;
}
inline bool FCmpOp::is_unordered() const {
  return !is_ordered();
}

/***************************************************
 * AllocOp                                         *
 ***************************************************/
inline ref<Operation>& AllocOp::size() {
  return operand_at(0);
}
inline const ref<Operation>& AllocOp::size() const {
  return operand_at(0);
}

inline ref<Operation>& AllocOp::default_value() {
  return operand_at(1);
}
inline const ref<Operation>& AllocOp::default_value() const {
  return operand_at(1);
}

/***************************************************
 * LoadOp                                          *
 ***************************************************/
inline ref<Operation>& LoadOp::data() {
  return operand_at(0);
}
inline const ref<Operation>& LoadOp::data() const {
  return operand_at(0);
}

inline ref<Operation>& LoadOp::offset() {
  return operand_at(1);
}
inline const ref<Operation>& LoadOp::offset() const {
  return operand_at(1);
}

/***************************************************
 * StoreOp                                         *
 ***************************************************/
inline ref<Operation>& StoreOp::data() {
  return operand_at(0);
}
inline const ref<Operation>& StoreOp::data() const {
  return operand_at(0);
}

inline ref<Operation>& StoreOp::offset() {
  return operand_at(1);
}
inline const ref<Operation>& StoreOp::offset() const {
  return operand_at(1);
}

inline ref<Operation>& StoreOp::value() {
  return operand_at(2);
}
inline const ref<Operation>& StoreOp::value() const {
  return operand_at(2);
}

/***************************************************
 * classof method function impls                   *
 ***************************************************/
#define CAFFEINE_OP_DECL_CLASSOF(derived, opcode_)                             \
  inline bool derived::classof(const Operation* op) {                          \
    return op->opcode() == Operation::opcode_;                                 \
  }                                                                            \
  static_assert(true)

CAFFEINE_OP_DECL_CLASSOF(ConstantInt, ConstantInt);
CAFFEINE_OP_DECL_CLASSOF(ConstantFloat, ConstantFloat);
CAFFEINE_OP_DECL_CLASSOF(ConstantArray, ConstantArray);
CAFFEINE_OP_DECL_CLASSOF(SelectOp, Select);
CAFFEINE_OP_DECL_CLASSOF(AllocOp, Alloc);
CAFFEINE_OP_DECL_CLASSOF(LoadOp, Load);
CAFFEINE_OP_DECL_CLASSOF(StoreOp, Store);
CAFFEINE_OP_DECL_CLASSOF(Undef, Undef);

inline bool Constant::classof(const Operation* op) {
  return op->opcode() == ConstantNamed || op->opcode() == ConstantNumbered;
}
inline bool BinaryOp::classof(const Operation* op) {
  return BinaryOpFirst <= op->opcode() && op->opcode() <= BinaryOpLast;
}
inline bool UnaryOp::classof(const Operation* op) {
  return UnaryOpFirst <= op->opcode() && op->opcode() <= UnaryOpLast;
}
inline bool ICmpOp::classof(const Operation* op) {
  return detail::opcode(icmp_base, 0, 0) <= op->opcode() &&
         op->opcode() <= detail::opcode(icmp_base, 3, 0xF);
}
inline bool FCmpOp::classof(const Operation* op) {
  return detail::opcode(fcmp_base, 0, 0) <= op->opcode() &&
         op->opcode() <= detail::opcode(fcmp_base, 3, 0xF);
}

#undef CAFFEINE_OP_DECL_CLASSOF

/***************************************************
 * hashing implementations                         *
 ***************************************************/
llvm::hash_code hash_value(const Operation& op);

} // namespace caffeine

namespace std {

template <>
struct hash<caffeine::Operation> {
  std::size_t operator()(const caffeine::Operation& op) const noexcept {
    return static_cast<std::size_t>(caffeine::hash_value(op));
  }
};

} // namespace std

#endif
