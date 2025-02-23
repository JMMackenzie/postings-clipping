#pragma once

#include <vector>

#include "cursor/max_scored_cursor.hpp"
#include "query/queries.hpp"
#include "scorer/index_scorer.hpp"
#include "wand_data.hpp"

namespace pisa {

template <typename Cursor, typename Wand>
class BlockMaxScoredCursor: public MaxScoredCursor<Cursor> {
  public:
    using base_cursor_type = Cursor;

    BlockMaxScoredCursor(
        Cursor cursor,
        TermScorer term_scorer,
        float weight,
        float max_score,
        typename Wand::wand_data_enumerator wdata,
        float paired_max_score = 0.0f,
        float low_max_score = 0.0f,
        size_t short_list_len = 0,
        size_t list_id = 0,
        bool is_dup = false)
        : MaxScoredCursor<Cursor>(std::move(cursor), std::move(term_scorer), weight, max_score, paired_max_score, low_max_score, short_list_len, list_id, is_dup),
          m_wdata(std::move(wdata))
    {}
    BlockMaxScoredCursor(BlockMaxScoredCursor const&) = delete;
    BlockMaxScoredCursor(BlockMaxScoredCursor&&) = default;
    BlockMaxScoredCursor& operator=(BlockMaxScoredCursor const&) = delete;
    BlockMaxScoredCursor& operator=(BlockMaxScoredCursor&&) = default;
    ~BlockMaxScoredCursor() = default;

    [[nodiscard]] PISA_ALWAYSINLINE auto block_max_score() -> float { return m_wdata.score(); }
    [[nodiscard]] PISA_ALWAYSINLINE auto block_max_docid() -> std::uint32_t
    {
        return m_wdata.docid();
    }
    PISA_ALWAYSINLINE void block_max_next_geq(std::uint32_t docid) { m_wdata.next_geq(docid); }
    PISA_ALWAYSINLINE void block_max_reset() { m_wdata.reset(); }

  private:
    typename Wand::wand_data_enumerator m_wdata;
};

template <typename Index, typename WandType, typename Scorer>
[[nodiscard]] auto
make_block_max_scored_cursors(Index const& index, WandType const& wdata, Scorer const& scorer, Query query)
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
        }
    }

    std::vector<BlockMaxScoredCursor<typename Index::document_enumerator, WandType>> cursors;
    cursors.reserve(query_term_freqs.size());
    std::transform(
        query_term_freqs.begin(), query_term_freqs.end(), std::back_inserter(cursors), [&](auto&& term) {
            return BlockMaxScoredCursor<typename Index::document_enumerator, WandType>(
                std::move(index[term.first]),
                [scorer = scorer.term_scorer(term.first), weight = term.second](uint32_t doc, uint32_t freq) { return weight * scorer(doc, freq); },
                term.second, idx_to_maxscore[term.first], wdata.getenum(term.first), idx_to_maxscore[paired_list[term.first]], low_max_score[idx_to_uniqid[term.first]], std::min(index[term.first].size(), index[paired_list[term.first]].size()), idx_to_uniqid[term.first], is_duplicate[term.first]);
        });
    return cursors;
}

}  // namespace pisa
