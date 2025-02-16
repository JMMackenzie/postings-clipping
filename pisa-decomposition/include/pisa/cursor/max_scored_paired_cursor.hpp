#pragma once

#include <vector>

#include "query/queries.hpp"
#include "scorer/index_scorer.hpp"
#include "wand_data.hpp"

namespace pisa {

template <typename Cursor>
class MaxScoredPairedCursor {
  public:
    using base_cursor_type = Cursor;

    MaxScoredPairedCursor(
        Cursor cursor_one,
        Cursor cursor_two,
        TermScorer term_scorer,
        float query_weight,
        float max_score_one,
        float max_score_two,
        size_t short_list,
        bool same = false)
        : m_base_cursors{std::move(cursor_one), std::move(cursor_two)},
          m_term_scorer(std::move(term_scorer)),
          m_query_weight(query_weight),
          m_max_scores{max_score_one, max_score_two},
          m_high_list_len(short_list),
          m_low_max_score(std::min(max_score_one, max_score_two)),
          m_same(same)
    {

        if (same) {
            m_high_list_len = 0; // Ensure no thresholds are used
        }

        m_current_list = 0;
        if (m_base_cursors[m_current_list].docid() > m_base_cursors[m_other_list].docid()) {
            std::swap(m_current_list, m_other_list);
        }
    }
    MaxScoredPairedCursor(MaxScoredPairedCursor const&) = delete;
    MaxScoredPairedCursor(MaxScoredPairedCursor&&) = default;
    MaxScoredPairedCursor& operator=(MaxScoredPairedCursor const&) = delete;
    MaxScoredPairedCursor& operator=(MaxScoredPairedCursor&&) = default;
    ~MaxScoredPairedCursor() = default;

    [[nodiscard]] PISA_ALWAYSINLINE auto query_weight() const noexcept -> float
    {
        return m_query_weight;
    }
    [[nodiscard]] PISA_ALWAYSINLINE auto docid() const -> std::uint32_t
    {
        return m_base_cursors[m_current_list].docid();
    }
    [[nodiscard]] PISA_ALWAYSINLINE auto freq() -> std::uint32_t
    {
        return m_base_cursors[m_current_list].freq();
    }
    [[nodiscard]] PISA_ALWAYSINLINE auto score() -> float { return m_term_scorer(docid(), freq()); }
    void PISA_ALWAYSINLINE next()
    {
        m_base_cursors[m_current_list].next();
        if (m_same) {
            return;
        }
        if (m_base_cursors[m_current_list].docid() > m_base_cursors[m_other_list].docid()) {
            std::swap(m_current_list, m_other_list);
        }
    }
    void PISA_ALWAYSINLINE next_geq(std::uint32_t docid)
    {
        m_base_cursors[m_current_list].next_geq(docid);
        if (m_same) {
            return;
        }
        // m_base_cursors[m_other_list].next_geq(docid);  // TODO can this be conditional?
        if (m_base_cursors[m_current_list].docid() > m_base_cursors[m_other_list].docid()) {
            std::swap(m_current_list, m_other_list);
        }
    }
    [[nodiscard]] PISA_ALWAYSINLINE auto size() -> std::size_t
    {
        if (m_same)
            return m_base_cursors[0].size();
        return m_base_cursors[0].size() + m_base_cursors[1].size();
    }

    [[nodiscard]] PISA_ALWAYSINLINE auto non_considered_high_docid() const -> std::uint32_t
    {
        if (m_same || m_current_list == 1) {
            return std::numeric_limits<uint32_t>::max();
        }
        return m_base_cursors[1].docid();
    }

    void PISA_ALWAYSINLINE reset()
    {
        m_base_cursors[0].reset();
        m_base_cursors[1].reset();
        m_current_list = 0;
        if (m_same) {
            return;
        }
        if (m_base_cursors[m_current_list].docid() > m_base_cursors[m_other_list].docid()) {
            std::swap(m_current_list, m_other_list);
        }
    }

    [[nodiscard]] PISA_ALWAYSINLINE auto max_score() const noexcept -> float
    {
        return m_max_scores[m_current_list];
    }

    [[nodiscard]] PISA_ALWAYSINLINE auto safe_threshold(size_t k) const noexcept -> float {
       
        // We know there are k things with a higher score
        if (k <= m_high_list_len) {
            return m_low_max_score;
        } 
        return 0.0f;
    }
 
  private:
    uint8_t m_current_list = 0;
    uint8_t m_other_list = 1;
    Cursor m_base_cursors[2];
    TermScorer m_term_scorer;
    float m_query_weight = 1.0;
    float m_max_scores[2];
    size_t m_high_list_len;
    float m_low_max_score;
    bool m_same;
};

template <typename Index, typename WandType, typename Scorer>
[[nodiscard]] auto
make_scored_paired_cursors(Index const& index, WandType const& wdata, Scorer const& scorer, Query query)
{
    auto paired_terms = query.paired_terms;

    std::vector<MaxScoredPairedCursor<typename Index::document_enumerator>> cursors;
    cursors.reserve(paired_terms.size());
    std::transform(
        paired_terms.begin(), paired_terms.end(), std::back_inserter(cursors), [&](auto&& term_pair) {
            float query_weight = std::get<2>(term_pair);
            auto max_weight_one = query_weight * wdata.max_term_weight(std::get<0>(term_pair));
            auto max_weight_two = query_weight * wdata.max_term_weight(std::get<1>(term_pair));
            bool same = false;
            if (std::get<0>(term_pair) == std::get<1>(term_pair)) {
                same = true;
            }
            return MaxScoredPairedCursor<typename Index::document_enumerator>(
                index[std::get<0>(term_pair)],
                index[std::get<1>(term_pair)],
                scorer.term_scorer(std::get<0>(term_pair)),
                query_weight,
                max_weight_one,
                max_weight_two,
                std::min(
                    index[std::get<0>(term_pair)].size(), 
                    index[std::get<1>(term_pair)].size()
                ), 
                same);
        });
    return cursors;
}

}  // namespace pisa
