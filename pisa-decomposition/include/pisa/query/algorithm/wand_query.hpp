#pragma once

#include <vector>

#include "query/queries.hpp"
#include "topk_queue.hpp"

namespace pisa {

struct wand_query {
    explicit wand_query(topk_queue& topk) : m_topk(topk) {}

    template <typename CursorRange>
    void operator()(CursorRange&& cursors, uint64_t max_docid, bool prime = false)
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

        if (prime) {
            // There *has to be* at least k docs with a score > initial_threshold based on our
            // precomputation
            float initial_threshold = 0.0f;
            size_t top_k_value = m_topk.capacity();
            //std::cerr << "k = " << top_k_value << "\n";
            for (size_t i = 0; i < ordered_cursors.size(); ++i) {
                initial_threshold = std::max(initial_threshold, ordered_cursors[i]->safe_threshold(top_k_value));
            }
            //std::cerr << "init = " << initial_threshold << "\n";
            m_topk.set_threshold(initial_threshold);
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
            for (pivot = 0; pivot < ordered_cursors.size(); ++pivot) {
                if (ordered_cursors[pivot]->docid() >= max_docid) {
                    break;
                }
                upper_bound += ordered_cursors[pivot]->max_score();
                if (m_topk.would_enter(upper_bound)) {
                    found_pivot = true;
                    break;
                }
            }

            // no pivot found, we can stop the search
            if (!found_pivot) {
                break;
            }

            // check if pivot is a possible match
            uint64_t pivot_id = ordered_cursors[pivot]->docid();
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
                // bubble down the advanced list
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

    template <typename CursorRange>
    void pair_aware_wand(CursorRange&& cursors, uint64_t max_docid, bool prime = false)
    {
        using Cursor = typename std::decay_t<CursorRange>::value_type;
        if (cursors.empty()) {
            return;
        }


        std::vector<Cursor*> ordered_cursors;
        ordered_cursors.reserve(cursors.size());
        size_t max_id = 0;
        for (auto& en: cursors) {
            ordered_cursors.push_back(&en);
            max_id = std::max(max_id, en.list_id());
        }
        max_id += 1;

        if (prime) {
            // There *has to be* at least k docs with a score > initial_threshold based on our
            // precomputation
            float initial_threshold = 0.0f;
            size_t top_k_value = m_topk.capacity();
            //std::cerr << "k = " << top_k_value << "\n";
            for (size_t i = 0; i < ordered_cursors.size(); ++i) {
                initial_threshold = std::max(initial_threshold, ordered_cursors[i]->safe_threshold(top_k_value));
            }
            //std::cerr << "init = " << initial_threshold << "\n";
            m_topk.set_threshold(initial_threshold);
        }

        auto sort_enums = [&]() {
            // sort enumerators by increasing docid
            std::sort(ordered_cursors.begin(), ordered_cursors.end(), [](Cursor* lhs, Cursor* rhs) {
                return lhs->docid() < rhs->docid();
            });
        };

        sort_enums();

        bit_vec_64 seen_term;

        while (true) {
            // find pivot
            float upper_bound = 0;
            size_t pivot;
            bool found_pivot = false;

            seen_term.clear();

            // Pivoting is aware of paired lists; removes redundant contributions on the fly
            for (pivot = 0; pivot < ordered_cursors.size(); ++pivot) {
                if (ordered_cursors[pivot]->docid() >= max_docid) {
                    break;
                }

                // We have seen this term already, so we have used its bound in the UB sum
                // If the list we already saw was the "HIGH" list, then we don't want to add anything new
                // If the list we already saw was the "LOW" list, we want to remove that UB and instate the "HIGH" one
                // Both of these conditions == "remove the LOW UB"
                // So, let's add the current bound first
                upper_bound += ordered_cursors[pivot]->max_score();
                
                if (seen_term[ordered_cursors[pivot]->list_id()]) {
                    upper_bound -= ordered_cursors[pivot]->low_max_score();
                } else {
                    seen_term.set(ordered_cursors[pivot]->list_id(), true);
                }

                if (m_topk.would_enter(upper_bound)) {
                    found_pivot = true;
                    break;
                }
            }

            // no pivot found, we can stop the search
            if (!found_pivot) {
                break;
            }

            // check if pivot is a possible match
            uint64_t pivot_id = ordered_cursors[pivot]->docid();
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
                // bubble down the advanced list
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
