#pragma once

#include <algorithm>
#include <numeric>
#include <vector>

#include "query/queries.hpp"
#include "topk_queue.hpp"
#include "util/compiler_attribute.hpp"

namespace pisa {

struct maxscore_query {
    explicit maxscore_query(topk_queue& topk) : m_topk(topk) {}

    template <typename Cursors>
    [[nodiscard]] PISA_ALWAYSINLINE auto sorted_by_bound(Cursors&& cursors)
        -> std::vector<typename std::decay_t<Cursors>::value_type>
    {
        std::vector<std::size_t> term_positions(cursors.size());
        std::iota(term_positions.begin(), term_positions.end(), 0);
        std::sort(term_positions.begin(), term_positions.end(), [&](auto&& lhs, auto&& rhs) {
            return cursors[lhs].max_score() > cursors[rhs].max_score();
        });
        std::vector<typename std::decay_t<Cursors>::value_type> sorted;
        for (auto pos: term_positions) {
            sorted.push_back(std::move(cursors[pos]));
        };
        return sorted;
    }

    template <typename Cursors>
    [[nodiscard]] PISA_ALWAYSINLINE auto sorted_by_length(Cursors&& cursors)
        -> std::vector<typename std::decay_t<Cursors>::value_type>
    {
        std::vector<std::size_t> term_positions(cursors.size());
        std::iota(term_positions.begin(), term_positions.end(), 0);
        std::sort(term_positions.begin(), term_positions.end(), [&](auto&& lhs, auto&& rhs) {
            return cursors[lhs].size() < cursors[rhs].size();
        });
        std::vector<typename std::decay_t<Cursors>::value_type> sorted;
        for (auto pos: term_positions) {
            sorted.push_back(std::move(cursors[pos]));
        };
        return sorted;
    }



    template <typename Cursors>
    [[nodiscard]] PISA_ALWAYSINLINE auto calc_upper_bounds(Cursors&& cursors) -> std::vector<float>
    {
        std::vector<float> upper_bounds(cursors.size());
        auto out = upper_bounds.rbegin();
        float bound = 0.0;
        for (auto pos = cursors.rbegin(); pos != cursors.rend(); ++pos) {
            bound += pos->max_score();
            *out++ = bound;
        }
        return upper_bounds;
    }

    template <typename Cursors>
    [[nodiscard]] PISA_ALWAYSINLINE auto min_docid(Cursors&& cursors) -> std::uint32_t
    {
        return std::min_element(
                   cursors.begin(),
                   cursors.end(),
                   [](auto&& lhs, auto&& rhs) { return lhs.docid() < rhs.docid(); })
            ->docid();
    }

    enum class UpdateResult : bool { Continue, ShortCircuit };
    enum class DocumentStatus : bool { Insert, Skip };

    template <typename Cursors>
    PISA_ALWAYSINLINE void run_sorted(Cursors&& cursors, uint64_t max_docid)
    {
        auto upper_bounds = calc_upper_bounds(cursors);
        auto above_threshold = [&](auto score) { return m_topk.would_enter(score); };

        auto first_upper_bound = upper_bounds.end();
        auto first_lookup = cursors.end();
        auto next_docid = min_docid(cursors);
#ifdef PROFILE
size_t profile_docs = 0;
size_t profile_postings = 0;
#endif


        auto update_non_essential_lists = [&] {
            while (first_lookup != cursors.begin()
                   && !above_threshold(*std::prev(first_upper_bound))) {
                --first_lookup;
                --first_upper_bound;
                if (first_lookup == cursors.begin()) {
                    return UpdateResult::ShortCircuit;
                }
            }
            return UpdateResult::Continue;
        };

        if (update_non_essential_lists() == UpdateResult::ShortCircuit) {
#ifdef PROFILE
std::cout << profile_docs << " " << profile_postings << "\n";
#endif
 
            return;
        }
        float current_score = 0;
        std::uint32_t current_docid = 0;

        while (current_docid < max_docid) {
            auto status = DocumentStatus::Skip;
            while (status == DocumentStatus::Skip) {
                if (PISA_UNLIKELY(next_docid >= max_docid)) {
#ifdef PROFILE
std::cout << profile_docs << " " << profile_postings << "\n";
#endif
 
                    return;
                }

                current_score = 0;
                current_docid = std::exchange(next_docid, max_docid);
#ifdef PROFILE
++profile_docs;
#endif
                std::for_each(cursors.begin(), first_lookup, [&](auto& cursor) {
                    if (cursor.docid() == current_docid) {
                        current_score += cursor.score();
                        cursor.next();
#ifdef PROFILE
++profile_postings;
#endif
                    }
                    if (auto docid = cursor.docid(); docid < next_docid) {
                        next_docid = docid;
                    }
                });

                status = DocumentStatus::Insert;
                auto lookup_bound = first_upper_bound;
                for (auto pos = first_lookup; pos != cursors.end(); ++pos, ++lookup_bound) {
                    auto& cursor = *pos;
                    if (not above_threshold(current_score + *lookup_bound)) {
                        status = DocumentStatus::Skip;
                        break;
                    }
                    cursor.next_geq(current_docid);
                    if (cursor.docid() == current_docid) {
                        current_score += cursor.score();
#ifdef PROFILE
++profile_postings;
#endif
                    }
                }
            }
            if (m_topk.insert(current_score, current_docid)
                && update_non_essential_lists() == UpdateResult::ShortCircuit) {
#ifdef PROFILE
std::cout << profile_docs << " " << profile_postings << "\n";
#endif
 
                return;
            }
        }
#ifdef PROFILE
std::cout << profile_docs << " " << profile_postings << "\n";
#endif
 
    }

    // Run exhaustive over the high lists
    template <typename Cursors>
    PISA_ALWAYSINLINE void high_then_low_internal(Cursors&& high_cursors, Cursors&& low_cursors, uint64_t max_docid)
    {

        std::unordered_set<uint32_t> scored_documents;

        uint64_t cur_doc = min_docid(high_cursors);

        // Exhaustive OR over high lists
        while (cur_doc < max_docid) {
            float score = 0;
            uint64_t next_doc = max_docid;
            for (size_t i = 0; i < high_cursors.size(); ++i) {
                if (high_cursors[i].docid() == cur_doc) {
                    score += high_cursors[i].score();
                    high_cursors[i].next();
                }
                if (high_cursors[i].docid() < next_doc) {
                    next_doc = high_cursors[i].docid();
                }
            }

            // Now update scores for low lists if need be
            for (size_t i = 0; i < low_cursors.size(); ++i) {
                low_cursors[i].next_geq(cur_doc);
                if (low_cursors[i].docid() == cur_doc) {
                    score += low_cursors[i].score();
                }
            }
            m_topk.insert(score, cur_doc);
            // Keep the scored document for later
            scored_documents.insert(cur_doc);

            cur_doc = next_doc;
        }

        // Now we run a modified maxscore on the low lists

        // Reset the low lists and move up to the first non-scored doc
        for (size_t i = 0; i < low_cursors.size(); ++i) {
            low_cursors[i].reset();
            while (scored_documents.find(low_cursors[i].docid()) != scored_documents.end()) {
                low_cursors[i].next();
            }
        }
 
        auto upper_bounds = calc_upper_bounds(low_cursors);
        auto above_threshold = [&](auto score) { return m_topk.would_enter(score); };

        auto first_upper_bound = upper_bounds.end();
        auto first_lookup = low_cursors.end();
        auto next_docid = min_docid(low_cursors);

        auto update_non_essential_lists = [&] {
            while (first_lookup != low_cursors.begin()
                   && !above_threshold(*std::prev(first_upper_bound))) {
                --first_lookup;
                --first_upper_bound;
                if (first_lookup == low_cursors.begin()) {
                    return UpdateResult::ShortCircuit;
                }
            }
            return UpdateResult::Continue;
        };

        if (update_non_essential_lists() == UpdateResult::ShortCircuit) {
            return;
        }

        float current_score = 0;
        std::uint32_t current_docid = 0;

        while (current_docid < max_docid) {
            auto status = DocumentStatus::Skip;
            while (status == DocumentStatus::Skip) {
                if (PISA_UNLIKELY(next_docid >= max_docid)) {
                    return;
                }

                current_score = 0;
                current_docid = std::exchange(next_docid, max_docid);

                std::for_each(low_cursors.begin(), first_lookup, [&](auto& cursor) {
                    if (cursor.docid() == current_docid) {
                        current_score += cursor.score();
                        cursor.next();
                    }

                    // Move up until we find something of potential interest (not yet scored)
                    auto potential_next = cursor.docid();
                    while (scored_documents.find(potential_next) != scored_documents.end()) {
                        cursor.next();
                        potential_next = cursor.docid();
                    }
 
                    if (potential_next < next_docid) {
                        next_docid = potential_next;
                    }
                });

                status = DocumentStatus::Insert;
                auto lookup_bound = first_upper_bound;
                for (auto pos = first_lookup; pos != low_cursors.end(); ++pos, ++lookup_bound) {
                    auto& cursor = *pos;
                    if (not above_threshold(current_score + *lookup_bound)) {
                        status = DocumentStatus::Skip;
                        break;
                    }
                    cursor.next_geq(current_docid);
                    if (cursor.docid() == current_docid) {
                        current_score += cursor.score();
                    }
                }
            }
            if (m_topk.insert(current_score, current_docid)
                && update_non_essential_lists() == UpdateResult::ShortCircuit) {
                return;
            }
        }
    }

    // Maxscore which has awareness of partner lists
    template <typename Cursors>
    PISA_ALWAYSINLINE void run_sorted_aware(Cursors&& cursors, uint64_t max_docid)
    {
        //auto upper_bounds = calc_upper_bounds(cursors);
       
        bit_vec_64 seen_term;

        // Stolen from calc_upper_bounds
        // Modify to only use the upper list at any step
        std::vector<float> upper_bounds(cursors.size());
        auto out = upper_bounds.rbegin();
        float bound = 0.0;
        size_t idx = cursors.size() - 1;
        for (auto pos = cursors.rbegin(); pos != cursors.rend(); ++pos) {
            bound += pos->max_score();
            if (seen_term[pos->list_id()]) {
               bound -= pos->low_max_score(); 
            } else {
                seen_term.set(pos->list_id(), true);
            }
            *out++ = bound;
        }

        auto above_threshold = [&](auto score) { return m_topk.would_enter(score); };

        auto first_upper_bound = upper_bounds.end();
        auto first_lookup = cursors.end();
        auto next_docid = min_docid(cursors);

        auto update_non_essential_lists = [&] {
            while (first_lookup != cursors.begin()
                   && !above_threshold(*std::prev(first_upper_bound))) {
                --first_lookup;
                --first_upper_bound;
                if (first_lookup == cursors.begin()) {
                    return UpdateResult::ShortCircuit;
                }
            }
            return UpdateResult::Continue;
        };

        if (update_non_essential_lists() == UpdateResult::ShortCircuit) {
            return;
        }

        float current_score = 0;
        std::uint32_t current_docid = 0;

        while (current_docid < max_docid) {
            auto status = DocumentStatus::Skip;
            while (status == DocumentStatus::Skip) {
                if (PISA_UNLIKELY(next_docid >= max_docid)) {
                    return;
                }

                current_score = 0;
                current_docid = std::exchange(next_docid, max_docid);

                std::for_each(cursors.begin(), first_lookup, [&](auto& cursor) {
                    if (cursor.docid() == current_docid) {
                        current_score += cursor.score();
                        cursor.next();
                    }
                    if (auto docid = cursor.docid(); docid < next_docid) {
                        next_docid = docid;
                    }
                });

                status = DocumentStatus::Insert;
                auto lookup_bound = first_upper_bound;
                for (auto pos = first_lookup; pos != cursors.end(); ++pos, ++lookup_bound) {
                    auto& cursor = *pos;
                    if (not above_threshold(current_score + *lookup_bound)) {
                        status = DocumentStatus::Skip;
                        break;
                    }
                    cursor.next_geq(current_docid);
                    if (cursor.docid() == current_docid) {
                        current_score += cursor.score();
                    }
                }
            }
            if (m_topk.insert(current_score, current_docid)
                && update_non_essential_lists() == UpdateResult::ShortCircuit) {
                return;
            }
        }
    }

 
    template <typename Cursors>
    PISA_ALWAYSINLINE void prime_heap(Cursors&& cursors)
    {

        // There *has to be* at least k docs with a score > initial_threshold based on our
        // precomputation
        float initial_threshold = 0.0f;
        size_t top_k_value = m_topk.capacity();
        //std::cerr << "k = " << top_k_value << "\n";
        for (size_t i = 0; i < cursors.size(); ++i) {
            initial_threshold = std::max(initial_threshold, cursors[i].safe_threshold(top_k_value));
        }
        //std::cerr << "init = " << initial_threshold << "\n";
        m_topk.set_threshold(initial_threshold);
    }


    template <typename Cursors>
    void operator()(Cursors&& cursors_, uint64_t max_docid, bool prime = false)
    {
        if (cursors_.empty()) {
            return;
        }
        if (prime) {
            prime_heap(cursors_);
        }
        auto cursors = sorted_by_bound(cursors_);
        run_sorted(cursors, max_docid);
        std::swap(cursors, cursors_);
    }

    template <typename Cursors>
    void length_sorted_maxscore(Cursors&& cursors_, uint64_t max_docid, bool prime = false)
    {
        if (cursors_.empty()) {
            return;
        }
        if (prime) {
            prime_heap(cursors_);
        }
        auto cursors = sorted_by_length(cursors_);
        run_sorted(cursors, max_docid);
        std::swap(cursors, cursors_);
    }



    template <typename Cursors>
    void high_then_low(Cursors&& high_cursors_, Cursors&& low_cursors_, uint64_t max_docid)
    {

        auto high_cursors = sorted_by_bound(high_cursors_);
        auto low_cursors = sorted_by_bound(low_cursors_);
        high_then_low_internal(high_cursors, low_cursors, max_docid);
        std::swap(high_cursors, high_cursors_);
        std::swap(low_cursors, low_cursors_);
    }

    template <typename Cursors>
    void pair_aware_maxscore(Cursors&& cursors_, uint64_t max_docid, bool prime = false)
    {

        if (cursors_.empty()) {
            return;
        }
        if (prime) {
            prime_heap(cursors_);
        }
        auto cursors = sorted_by_length(cursors_);
        run_sorted_aware(cursors, max_docid);
        std::swap(cursors, cursors_);
    
    }



    std::vector<std::pair<float, uint64_t>> const& topk() const { return m_topk.topk(); }

  private:
    topk_queue& m_topk;
};

}  // namespace pisa
