#pragma once

#include "query/queries.hpp"
#include "topk_queue.hpp"
#include <vector>
namespace pisa {

struct block_max_wand_query {
    explicit block_max_wand_query(topk_queue& topk) : m_topk(topk) {}

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


        auto sort_cursors = [&]() {
            // sort enumerators by increasing docid
            std::sort(ordered_cursors.begin(), ordered_cursors.end(), [](Cursor* lhs, Cursor* rhs) {
                return lhs->docid() < rhs->docid();
            });
        };


        
        sort_cursors();
 
        while (true) {
            // find pivot
            float upper_bound = 0.F;
            size_t pivot;
            bool found_pivot = false;
            uint64_t pivot_id = max_docid;

            for (pivot = 0; pivot < ordered_cursors.size(); ++pivot) {
                if (ordered_cursors[pivot]->docid() >= max_docid) {
                    break;
                }

                upper_bound += ordered_cursors[pivot]->max_score();


                if (m_topk.would_enter(upper_bound)) {
                    found_pivot = true;
                    pivot_id = ordered_cursors[pivot]->docid();
                    for (; pivot + 1 < ordered_cursors.size()
                         && ordered_cursors[pivot + 1]->docid() == pivot_id;
                         ++pivot) {
                    }
                    break;
                }
            }

            // no pivot found, we can stop the search
            if (!found_pivot) {
                break;
            }

            double block_upper_bound = 0;

            for (size_t i = 0; i < pivot + 1; ++i) {
                if (ordered_cursors[i]->block_max_docid() < pivot_id) {
                    ordered_cursors[i]->block_max_next_geq(pivot_id);
                }

                block_upper_bound +=
                    ordered_cursors[i]->block_max_score() * ordered_cursors[i]->query_weight();
            }

            if (m_topk.would_enter(block_upper_bound)) {
                // check if pivot is a possible match
                if (pivot_id == ordered_cursors[0]->docid()) {
                    float score = 0;
                    for (Cursor* en: ordered_cursors) {
                        if (en->docid() != pivot_id) {
                            break;
                        }
                        float part_score = en->score();
                        score += part_score;
                        block_upper_bound -= en->block_max_score() * en->query_weight() - part_score;
                        if (!m_topk.would_enter(block_upper_bound)) {
                            break;
                        }
                    }
                    for (Cursor* en: ordered_cursors) {
                        if (en->docid() != pivot_id) {
                            break;
                        }
                        en->next();
                    }

                    m_topk.insert(score, pivot_id);
                    // resort by docid
                    sort_cursors();

                } else {
                    uint64_t next_list = pivot;
                    for (; ordered_cursors[next_list]->docid() == pivot_id; --next_list) {
                    }
                    ordered_cursors[next_list]->next_geq(pivot_id);

                    // bubble down the advanced list
                    for (size_t i = next_list + 1; i < ordered_cursors.size(); ++i) {
                        if (ordered_cursors[i]->docid() <= ordered_cursors[i - 1]->docid()) {
                            std::swap(ordered_cursors[i], ordered_cursors[i - 1]);
                        } else {
                            break;
                        }
                    }
                }

            } else {
                uint64_t next;
                uint64_t next_list = pivot;

                float max_weight = ordered_cursors[next_list]->max_score();

                for (uint64_t i = 0; i < pivot; i++) {
                    if (ordered_cursors[i]->max_score() > max_weight) {
                        next_list = i;
                        max_weight = ordered_cursors[i]->max_score();
                    }
                }

                next = max_docid;

                for (size_t i = 0; i <= pivot; ++i) {
                    if (ordered_cursors[i]->block_max_docid() < next) {
                        next = ordered_cursors[i]->block_max_docid();
                    }
                }

                next = next + 1;
                if (pivot + 1 < ordered_cursors.size() && ordered_cursors[pivot + 1]->docid() < next) {
                    next = ordered_cursors[pivot + 1]->docid();
                }

                if (next <= pivot_id) {
                    next = pivot_id + 1;
                }

                ordered_cursors[next_list]->next_geq(next);

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
    void pair_aware_bmw(CursorRange&& cursors, uint64_t max_docid, bool prime = false)
 
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


        auto sort_cursors = [&]() {
            // sort enumerators by increasing docid
            std::sort(ordered_cursors.begin(), ordered_cursors.end(), [](Cursor* lhs, Cursor* rhs) {
                return lhs->docid() < rhs->docid();
            });
        };

        bit_vec_64 seen_term;
        sort_cursors();

        while (true) {
            // find pivot
            float upper_bound = 0.F;
            size_t pivot;
            bool found_pivot = false;
            uint64_t pivot_id = max_docid;
            seen_term.clear();

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
                    pivot_id = ordered_cursors[pivot]->docid();
                    for (; pivot + 1 < ordered_cursors.size()
                         && ordered_cursors[pivot + 1]->docid() == pivot_id;
                         ++pivot) {
                    }
                    break;
                }
            }

            // no pivot found, we can stop the search
            if (!found_pivot) {
                break;
            }

            double block_upper_bound = 0;

//#define _FANCY_
#ifdef _FANCY_
            seen_term.clear();
            // XXX this is probably bad
            std::vector<float> block_ub(cursors.size());
#endif

            for (size_t i = 0; i < pivot + 1; ++i) {
                if (ordered_cursors[i]->block_max_docid() < pivot_id) {
                    ordered_cursors[i]->block_max_next_geq(pivot_id);
                }

                float block =
                    ordered_cursors[i]->block_max_score() * ordered_cursors[i]->query_weight();

                block_upper_bound += block;
#ifdef _FANCY_
                if (seen_term[ordered_cursors[i]->list_id()]) {
                    block_upper_bound -= std::min(block, block_ub[ordered_cursors[i]->list_id()]);
                } else {
                    seen_term.set(ordered_cursors[i]->list_id(), true);
                    block_ub[ordered_cursors[i]->list_id()] = block;
                }
#endif
            }

            if (m_topk.would_enter(block_upper_bound)) {
                // check if pivot is a possible match
                if (pivot_id == ordered_cursors[0]->docid()) {
                    float score = 0;
                    for (Cursor* en: ordered_cursors) {
                        if (en->docid() != pivot_id) {
                            break;
                        }
                        float part_score = en->score();
                        score += part_score;
                        block_upper_bound -= en->block_max_score() * en->query_weight() - part_score;
                        if (!m_topk.would_enter(block_upper_bound)) {
                            break;
                        }
                    }
                    for (Cursor* en: ordered_cursors) {
                        if (en->docid() != pivot_id) {
                            break;
                        }
                        en->next();
                    }

                    m_topk.insert(score, pivot_id);
                    // resort by docid
                    sort_cursors();

                } else {
                    uint64_t next_list = pivot;
                    for (; ordered_cursors[next_list]->docid() == pivot_id; --next_list) {
                    }
                    ordered_cursors[next_list]->next_geq(pivot_id);

                    // bubble down the advanced list
                    for (size_t i = next_list + 1; i < ordered_cursors.size(); ++i) {
                        if (ordered_cursors[i]->docid() <= ordered_cursors[i - 1]->docid()) {
                            std::swap(ordered_cursors[i], ordered_cursors[i - 1]);
                        } else {
                            break;
                        }
                    }
                }

            } else {
                uint64_t next;
                uint64_t next_list = pivot;

                float max_weight = ordered_cursors[next_list]->max_score();

                for (uint64_t i = 0; i < pivot; i++) {
                    if (ordered_cursors[i]->max_score() > max_weight) {
                        next_list = i;
                        max_weight = ordered_cursors[i]->max_score();
                    }
                }

                next = max_docid;

                for (size_t i = 0; i <= pivot; ++i) {
                    if (ordered_cursors[i]->block_max_docid() < next) {
                        next = ordered_cursors[i]->block_max_docid();
                    }
                }

                next = next + 1;
                if (pivot + 1 < ordered_cursors.size() && ordered_cursors[pivot + 1]->docid() < next) {
                    next = ordered_cursors[pivot + 1]->docid();
                }

                if (next <= pivot_id) {
                    next = pivot_id + 1;
                }

                ordered_cursors[next_list]->next_geq(next);

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

    void clear_topk() { m_topk.clear(); }

    topk_queue const& get_topk() const { return m_topk; }

  private:
    topk_queue& m_topk;
};

}  // namespace pisa
