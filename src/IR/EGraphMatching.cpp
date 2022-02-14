#include "caffeine/IR/EGraphMatching.h"
#include "caffeine/IR/EGraph.h"

namespace caffeine::ematching {

bool SubClauseFilter::operator==(const SubClauseFilter& filter) const {
  if (type != filter.type)
    return false;

  return is_equal(filter);
}
bool SubClauseFilter::operator!=(const SubClauseFilter& filter) const {
  return !(*this == filter);
}

llvm::hash_code hash_value(const SubClauseFilter& filter) {
  return llvm::hash_combine(filter.type, filter.hash());
}

bool SubClause::is_potential_match(const ENode& node) const {
  if (opcode != node.opcode() && opcode != Operation::Invalid)
    return false;

  if (filter && !filter->matches(node))
    return false;
  return true;
}

bool SubClause::operator==(const SubClause& clause) const {
  if (opcode != clause.opcode)
    return false;
  if (submatchers != clause.submatchers)
    return false;
  if (filter != clause.filter)
    return false;
  return true;
}
bool SubClause::operator!=(const SubClause& clause) const {
  return !(*this == clause);
}

llvm::hash_code hash_value(const SubClause& subclause) {
  using llvm::hash_value;

  size_t filter_hash =
      !subclause.filter ? 0 : (size_t)hash_value(*subclause.filter);

  return llvm::hash_combine(subclause.opcode,
                            (llvm::ArrayRef<size_t>)subclause.submatchers,
                            filter_hash);
}

bool MatchData::contains_match(size_t subclause, size_t eclass) const {
  if (subclause >= matches_.size())
    return false;

  return matches_[subclause].find(eclass) != matches_[subclause].end();
}

const MatchData::ClauseData& MatchData::matches(size_t subclause) const {
  return matches_.at(subclause);
}
llvm::ArrayRef<size_t> MatchData::matches(size_t subclause,
                                          size_t eclass) const {
  const auto& cdata = matches_.at(subclause);
  auto it = cdata.find(eclass);
  if (it == cdata.end())
    return {};
  return it->second;
}

size_t EMatcherBuilder::add_clause(Operation::Opcode opcode,
                                   llvm::ArrayRef<size_t> submatchers,
                                   std::unique_ptr<SubClauseFilter>&& filter) {
  for (size_t subclause : submatchers)
    CAFFEINE_ASSERT(subclause < subclause_id);

  SubClause clause{
      opcode, llvm::SmallVector<size_t>(submatchers.begin(), submatchers.end()),
      std::move(filter)};

  auto [it, inserted] = subclauses.emplace(std::move(clause), subclause_id);
  if (!inserted)
    return it->second;

  return subclause_id++;
}

void EMatcherBuilder::add_matcher(size_t clause, EMatcherUpdater update,
                                  std::optional<EMatcherFilter> filter) {
  clauses.push_back(Clause{clause, filter, update});
}

EMatcher EMatcherBuilder::build() {
  EMatcher matcher;

  std::vector<std::pair<size_t, SubClause>> clausetmp;
  clausetmp.reserve(subclauses.size());
  for (auto it = subclauses.begin(); it != subclauses.end(); ++it) {
    clausetmp.emplace_back(it->second,
                           std::move(*const_cast<SubClause*>(&it->first)));
  }

  std::sort(clausetmp.begin(), clausetmp.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  matcher.subclauses.reserve(clausetmp.size());
  for (auto& val : clausetmp)
    matcher.subclauses.push_back(std::move(val.second));

  subclauses.clear();
  subclause_id = 0;

  matcher.clauses = std::move(clauses);

  for (size_t i = 0; i < matcher.subclauses.size(); ++i) {
    const SubClause& clause = matcher.subclauses[i];
    matcher.subindex[clause.opcode].push_back(i);
  }

  return matcher;
}

EMatcherBuilder EMatcher::builder() {
  return EMatcherBuilder();
}

} // namespace caffeine::ematching
