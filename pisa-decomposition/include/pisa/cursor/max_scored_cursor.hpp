#pragma once

#include <vector>

#include "cursor/scored_cursor.hpp"
#include "query/queries.hpp"
#include "wand_data.hpp"

namespace pisa {

template <typename Cursor>
class MaxScoredCursor: public ScoredCursor<Cursor> {
  public:
    using base_cursor_type = Cursor;

    MaxScoredCursor(Cursor cursor, TermScorer term_scorer, float query_weight, float max_score, float paired_max_score = 0.0f, float low_max_score = 0.0f, size_t short_list_len = 0, size_t list_id = 0, bool is_dup = false)
        : ScoredCursor<Cursor>(std::move(cursor), std::move(term_scorer), query_weight),
          m_max_score(max_score), m_paired_max_score(paired_max_score), m_low_max_score(low_max_score), m_high_list_len(short_list_len), m_list_id(list_id), m_duplicate(is_dup)
    {
        // Ensure no threshold used if lists are the same
        if (m_duplicate) {
            m_high_list_len = 0; 
        }
    }
    MaxScoredCursor(MaxScoredCursor const&) = delete;
    MaxScoredCursor(MaxScoredCursor&&) = default;
    MaxScoredCursor& operator=(MaxScoredCursor const&) = delete;
    MaxScoredCursor& operator=(MaxScoredCursor&&) = default;
    ~MaxScoredCursor() = default;

    [[nodiscard]] PISA_ALWAYSINLINE auto max_score() const noexcept -> float { return m_max_score; }

    [[nodiscard]] PISA_ALWAYSINLINE auto paired_max_score() const noexcept -> float { return m_paired_max_score; }

    [[nodiscard]] PISA_ALWAYSINLINE auto low_max_score() const noexcept -> float { return m_low_max_score; }

    [[nodiscard]] PISA_ALWAYSINLINE auto list_id() const noexcept -> size_t { return m_list_id; }

    [[nodiscard]] PISA_ALWAYSINLINE auto safe_threshold(size_t k) const noexcept -> float {
       
        // We know there are k things with a higher score
        if (k <= m_high_list_len) {
            return m_low_max_score;
        } 
        return 0.0f;
    }

  private:
    float m_max_score;
    float m_paired_max_score;
    float m_low_max_score;
    float m_query_weight = 1.0;
    size_t m_high_list_len;
    size_t m_list_id; // a one-time ID used to uniquely identify the same (high vs low) lists for a term (they will have the same m_list_id)
    bool m_duplicate;
};

template <typename Index, typename WandType, typename Scorer>
[[nodiscard]] auto
make_max_scored_cursors(Index const& index, WandType const& wdata, Scorer const& scorer, Query query)
{
    auto terms = query.terms;
    auto query_term_freqs = query_freqs(terms);

    std::unordered_map<uint32_t, uint64_t> idx_to_uniqid;
    std::unordered_map<uint32_t, float> idx_to_maxscore;
    std::unordered_map<uint32_t, uint32_t> paired_list;
    std::unordered_map<uint32_t, bool> is_duplicate;
    std::unordered_map<uint32_t, float> low_max_score;
   
    // Naive way of building a map of term_id -> unique_term
    // This allows us to keep track of high/low pairs via an id
    for (const auto term_pair : query.paired_terms) {
        size_t uniqid = std::get<3>(term_pair);
        uint32_t idx_one = std::get<0>(term_pair);
        uint32_t idx_two = std::get<1>(term_pair);
        paired_list[idx_one] = idx_two;
        paired_list[idx_two] = idx_one;

        idx_to_uniqid[idx_one] = uniqid;
        idx_to_uniqid[idx_two] = uniqid;

        float query_weight = std::get<2>(term_pair);
        auto max_weight_one = query_weight * wdata.max_term_weight(idx_one);
        auto max_weight_two = query_weight * wdata.max_term_weight(idx_two);
        idx_to_maxscore[idx_one] = max_weight_one;
        idx_to_maxscore[idx_two] = max_weight_two;

        if (idx_one == idx_two) {
            is_duplicate[idx_one] = true;
            low_max_score[idx_one] = 0.0f; // No priming
        } else { 
            // The low_max_score is the one corresponding to the longer list
            if (index[idx_one].size() < index[idx_two].size()) {
                low_max_score[idx_to_uniqid[idx_one]] = max_weight_two; 
            } else {
                low_max_score[idx_to_uniqid[idx_one]] = max_weight_one;
            }
           // std::cerr << idx_one << " " << idx_two << " " << index[idx_one].size() << " " << index[idx_two].size() << " " << max_weight_one << " " << max_weight_two << "\n";
        }
    }

    std::vector<MaxScoredCursor<typename Index::document_enumerator>> cursors;
    cursors.reserve(query_term_freqs.size());
    std::transform(
        query_term_freqs.begin(), query_term_freqs.end(), std::back_inserter(cursors), [&](auto&& term) {
            return MaxScoredCursor<typename Index::document_enumerator>(
                index[term.first], 
                [scorer = scorer.term_scorer(term.first), weight = term.second](uint32_t doc, uint32_t freq) { return weight * scorer(doc, freq); }, // weighted sums
                term.second, idx_to_maxscore[term.first], idx_to_maxscore[paired_list[term.first]], low_max_score[idx_to_uniqid[term.first]], std::min(index[term.first].size(), index[paired_list[term.first]].size()), idx_to_uniqid[term.first], is_duplicate[term.first]);
        });
    return cursors;
}

}  // namespace pisa
 
