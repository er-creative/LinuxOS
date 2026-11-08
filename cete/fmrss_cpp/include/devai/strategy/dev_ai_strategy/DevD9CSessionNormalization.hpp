#pragma once
// D9C session normalization: include after Row, Session, Sessions, Key declarations.
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace d9c_session_fix {
struct Diagnostics {
 std::size_t source_days=0, accepted=0, rejected=0, ignored_after_cutoff_rows=0;
 std::size_t missing_1515=0, missing_stock=0, duplicate_stock=0;
 std::size_t missing_minute=0, invalid_top5=0, invalid_time=0;
};

template<typename Row, typename Key>
auto normalize(const std::vector<Row>& rows, Diagnostics& d)
    -> std::map<Key, std::vector<Row>> {
 using Session = std::vector<Row>;
 std::map<Key, Session> all, accepted;
 // The exporter uses completion-time keys. -1 ensures midnight completion
 // belongs to the previous calendar session.
 for (const auto& r : rows) all[(r.obs.decision_key - 1) / 1440].push_back(r);
 d.source_days = all.size();
 for (auto& [day, rs] : all) {
  bool bad_stock=false, bad_duplicate=false, bad_gap=false, bad_top5=false, bad_time=false;
  bool has_1515=false;
  Key previous=-1;
  Session trading;
  trading.reserve(rs.size());
  for (std::size_t i=0; i<rs.size();) {
   const Key key=rs[i].obs.decision_key;
   const Key minute=key-day*1440;
   std::set<std::string> symbols;
   std::size_t selected=0;
   std::size_t j=i;
   while (j<rs.size() && rs[j].obs.decision_key==key) {
    if (minute>=555 && minute<=915) {
     if (!symbols.insert(rs[j].obs.symbol).second) bad_duplicate=true;
     selected+=rs[j].selected ? 1U : 0U;
     trading.push_back(rs[j]);
    } else if (minute>915) {
     ++d.ignored_after_cutoff_rows;
    }
    ++j;
   }
   if (minute>=555 && minute<=915) {
    if (symbols.size()!=10) bad_stock=true;
    if (selected!=5) bad_top5=true;
    if (previous!=-1 && key!=previous+1) bad_gap=true;
    previous=key;
    if (minute==915) has_1515=true;
   } else if (minute<555) {
    // Pre-open rows are allowed in source, but not part of D9C replay.
   } else if (minute>1440) {
    bad_time=true;
   }
   i=j;
  }
  if (!has_1515) ++d.missing_1515;
  if (bad_stock) ++d.missing_stock;
  if (bad_duplicate) ++d.duplicate_stock;
  if (bad_gap) ++d.missing_minute;
  if (bad_top5) ++d.invalid_top5;
  if (bad_time) ++d.invalid_time;
  if (trading.empty() || !has_1515 || bad_stock || bad_duplicate || bad_gap || bad_top5 || bad_time) {
   ++d.rejected;
  } else {
   accepted.emplace(day, std::move(trading));
   ++d.accepted;
  }
 }
 return accepted;
}
inline void print(const Diagnostics& d) {
 std::cout << "D9C_SESSION_DIAGNOSTICS source_days=" << d.source_days
           << " accepted=" << d.accepted << " rejected=" << d.rejected
           << " ignored_after_15_15_rows=" << d.ignored_after_cutoff_rows << '\n';
 std::cout << "D9C_REJECTION_REASONS missing_15_15=" << d.missing_1515
           << " missing_stock=" << d.missing_stock
           << " duplicate_stock=" << d.duplicate_stock
           << " missing_minute=" << d.missing_minute
           << " invalid_top5=" << d.invalid_top5
           << " invalid_time=" << d.invalid_time << '\n';
}
} // namespace d9c_session_fix
