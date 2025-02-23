#pragma once

#include <vector>

#include "query/queries.hpp"
#include "topk_queue.hpp"

namespace pisa {

struct wand_pair_query {
    explicit wand_pair_query(topk_queue& topk) : m_topk(topk) {}

    template <typename CursorRange>
    void operator()(CursorRange&& cursors, uint64_t max_docid)
    {
        using Cursor = typename std::decay_t<CursorRange>::value_type;
        if (cursors.empty()) {
            return;
        }

        std::vector<Cursor*> ordered_cursors;
        ordered_cursors.reserve(cursors.size());
        for (auto& en: cursors) {
            ordered_cursors.push_back(&en);
        }

        auto sort_enums = [&]() {
            // sort enumerators by increasing docid
            std::sort(ordered_cursors.begin(), ordered_cursors.end(), [](Cursor* lhs, Cursor* rhs) {
                return lhs->docid() < rhs->docid();
            });
        };

        sort_enums();
        while (true) {
            // find pivot
            float upper_bound = 0;
            size_t pivot;
            bool found_pivot = false;
            uint32_t min_high_non_considered = std::numeric_limits<uint32_t>::max();
            for (pivot = 0; pivot < ordered_cursors.size(); ++pivot) {
                if (ordered_cursors[pivot]->docid() >= max_docid) {
                    break;
                }
                upper_bound += ordered_cursors[pivot]->max_score();
                min_high_non_considered = std::min(
                    min_high_non_considered, ordered_cursors[pivot]->non_considered_high_docid());
                if (m_topk.would_enter(upper_bound)) {
                    found_pivot = true;
                    break;
                }
            }

            // no pivot found, we can stop the search
            if (!found_pivot) {
                if (min_high_non_considered >= max_docid) {
                    break;
                } else{
                    // We didn't find a pivot, but we might still be interested in some highs
                    for (size_t i = 0; i < ordered_cursors.size(); ++i) {
                        if (ordered_cursors[i]->docid() < min_high_non_considered)
                            ordered_cursors[i]->next_geq(min_high_non_considered);
                    }
                    sort_enums();
                    continue;
                }
            }

            // check if pivot is a possible match
            uint64_t pivot_id = ordered_cursors[pivot]->docid();

            if (pivot_id > min_high_non_considered) {
                // printf(
                //     "pivot_id %lu min_high_non_considered %u\n", pivot_id,
                //     min_high_non_considered);
                // fflush(stdout);
                for (size_t i = 0; i < pivot; ++i) {
                    if (ordered_cursors[i]->docid() < min_high_non_considered)
                        ordered_cursors[i]->next_geq(min_high_non_considered);
                }
                sort_enums();
                continue;
            }

            if (pivot_id == ordered_cursors[0]->docid()) {
                float score = 0;
                for (Cursor* en: ordered_cursors) {
                    if (en->docid() != pivot_id) {
                        break;
                    }
                    score += en->score();
                    en->next();
                }

                m_topk.insert(score, pivot_id);
                // resort by docid
                sort_enums();
            } else {
                // no match, move farthest list up to the pivot
                uint64_t next_list = pivot;
                for (; ordered_cursors[next_list]->docid() == pivot_id; --next_list) {
                }
                ordered_cursors[next_list]->next_geq(pivot_id);
                // // bubble down the advanced list
                for (size_t i = next_list + 1; i < ordered_cursors.size(); ++i) {
                    if (ordered_cursors[i]->docid() < ordered_cursors[i - 1]->docid()) {
                        std::swap(ordered_cursors[i], ordered_cursors[i - 1]);
                    } else {
                        break;
                    }
                }
            }
        }
    }

    std::vector<std::pair<float, uint64_t>> const& topk() const { return m_topk.topk(); }

  private:
    topk_queue& m_topk;
};

}  // namespace pisa
