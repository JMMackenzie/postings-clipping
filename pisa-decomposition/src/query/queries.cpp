#include "query/queries.hpp"

#include <boost/algorithm/string.hpp>
#include <range/v3/view/enumerate.hpp>
#include <spdlog/spdlog.h>
#include <iterator>
#include "index_types.hpp"
#include "tokenizer.hpp"
#include "topk_queue.hpp"
#include "util/util.hpp"
#include <boost/algorithm/string/classification.hpp> 
#include <boost/algorithm/string/split.hpp> 

namespace pisa {

auto split_query_at_colon(std::string const& query_string)
    -> std::pair<std::optional<std::string>, std::string_view>
{
    // query id : terms (or ids)
    auto colon = std::find(query_string.begin(), query_string.end(), ':');
    std::optional<std::string> id;
    if (colon != query_string.end()) {
        id = std::string(query_string.begin(), colon);
    }
    auto pos = colon == query_string.end() ? query_string.begin() : std::next(colon);
    auto raw_query = std::string_view(&*pos, std::distance(pos, query_string.end()));
    return {std::move(id), raw_query};
}

bool ends_with(std::string const& value, std::string const& ending)
{
    if (ending.size() > value.size())
        return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

std::string extract_term(std::string const& value, std::string const& ending)
{
    auto term_len = value.size() - ending.size();
    return value.substr(0, term_len);
}

auto parse_query_terms(std::string const& query_string, TermProcessor term_processor) -> Query
{
    auto [id, raw_query] = split_query_at_colon(query_string);

    std::vector<std::string> tokenizer;
    boost::split(tokenizer, raw_query, boost::is_any_of(" "), boost::token_compress_on);
    /*
    std::stringstream ss(std::string(raw_query));
    std::istream_iterator<std::string> begin(ss);
    std::istream_iterator<std::string> end;
    std::vector<std::string> tokenizer(begin, end);
    //TermTokenizer tokenizer(raw_query);
    */
    std::vector<term_id_type> parsed_query;
    std::unordered_map<std::string, std::vector<term_id_type>> pair_collector;

    std::vector<bool> high_terms;
    for (auto term_iter = tokenizer.begin(); term_iter != tokenizer.end(); ++term_iter) {
        auto raw_term = *term_iter;
        // Check if HIGH or LOW
        bool ends_with_high = false;
        ends_with_high = ends_with(raw_term, "_HIGH");
        bool ends_with_low = false;
        ends_with_low = ends_with(raw_term, "_LOW");

        if (!ends_with_high && !ends_with_low) {
            std::cerr << "ERROR: Term [" << raw_term << "] is neither HIGH nor LOW. Exiting...\n";
            exit(EXIT_FAILURE);
        }

        high_terms.push_back(ends_with_high);

        //std::cerr << "TERM: |" << raw_term << "|\n";
        auto term = term_processor(raw_term);
        //std::cerr << "Processed: |" << *term << "|\n";
        if (term) {
            if (!term_processor.is_stopword(*term)) {
                parsed_query.push_back(*term);
                if (ends_with_high) {
                    auto term_prefix = extract_term(raw_term, "_HIGH");
                    pair_collector[term_prefix].push_back(*term);
                }
                if (ends_with_low) {
                    auto term_prefix = extract_term(raw_term, "_LOW");
                    pair_collector[term_prefix].push_back(*term);
                }
            } else {
                spdlog::warn("Term `{}` is a stopword and will be ignored", raw_term);
            }
        } else {
            spdlog::warn("Term `{}` not found and will be ignored", raw_term);
        }
    }

    std::vector<high_low_tuple> parsed_pairs;
    size_t collected_pairs = 0;
    for (auto term: pair_collector) {
        auto term_vec = term.second;
       // std::cerr << term_vec.size() << " " << term.first << "\n";
        if (term_vec.size() % 2 != 0) {
            std::cerr << "WARN: Term needs to have HIGH and LOW...\n";
            std::cerr << "I am duplicating the non-empty list...\n";
            parsed_pairs.push_back({term_vec[0], term_vec[0], term_vec.size(), collected_pairs++});
            continue;
        }
        
        // Sanity check the terms and get a frequency
        std::unordered_map<uint32_t, size_t> term_counts;
        for (size_t i = 0; i < term_vec.size(); ++i) {
            term_counts[term_vec[i]] += 1;
        }

        // Case: We have just one list (eg, there is no corresponding paired list)
        if (term_counts.size() == 1) {
            parsed_pairs.push_back({term_vec[0], term_vec[0], term_vec.size(), collected_pairs++});
        } else if (term_counts.size() == 2) {

            // We need to find both term identifiers;
            uint32_t term_id_one = term_vec[0];
            uint32_t term_id_two = term_vec[0];

            // Guaranteed we have two, so don't check bounds
            size_t i = 0;
            while (term_id_one == term_id_two) {
                term_id_two = term_vec[++i];
            }
            //std::cerr << "Found a pair: " << term_id_one << " " << term_id_two << " \n";

            parsed_pairs.push_back({term_id_one, term_id_two, term_vec.size()/2, collected_pairs++});
 
        } else {
            std::cerr << "ERROR: UNREACHABLE?\n";
            exit(EXIT_FAILURE);
        }

    }

    return {
        std::move(id), std::move(parsed_query), {}, std::move(high_terms), std::move(parsed_pairs)};
}

auto parse_query_ids(std::string const& query_string) -> Query
{
    auto [id, raw_query] = split_query_at_colon(query_string);
    std::vector<term_id_type> parsed_query;
    std::vector<std::string> term_ids;
    boost::split(term_ids, raw_query, boost::is_any_of("\t, ,\v,\f,\r,\n"));

    auto is_empty = [](const std::string& val) { return val.empty(); };
    // remove_if move matching elements to the end, preparing them for erase.
    term_ids.erase(std::remove_if(term_ids.begin(), term_ids.end(), is_empty), term_ids.end());

    try {
        auto to_int = [](const std::string& val) { return std::stoi(val); };
        std::transform(term_ids.begin(), term_ids.end(), std::back_inserter(parsed_query), to_int);
    } catch (std::invalid_argument& err) {
        spdlog::error("Could not parse term identifiers of query `{}`", raw_query);
        exit(1);
    }
    return {std::move(id), std::move(parsed_query), {}, {}, {}};
}

std::function<void(const std::string)> resolve_query_parser(
    std::vector<Query>& queries,
    std::optional<std::string> const& terms_file,
    std::optional<std::string> const& stopwords_filename,
    std::optional<std::string> const& stemmer_type)
{
    if (terms_file) {
        auto term_processor = TermProcessor(terms_file, stopwords_filename, stemmer_type);
        return [&queries, term_processor = std::move(term_processor)](std::string const& query_line) {
            queries.push_back(parse_query_terms(query_line, term_processor));
        };
    }
    return [&queries](std::string const& query_line) {
        queries.push_back(parse_query_ids(query_line));
    };
}

bool read_query(term_id_vec& ret, std::istream& is)
{
    ret.clear();
    std::string line;
    if (!std::getline(is, line)) {
        return false;
    }
    ret = parse_query_ids(line).terms;
    return true;
}

void remove_duplicate_terms(term_id_vec& terms)
{
    std::sort(terms.begin(), terms.end());
    terms.erase(std::unique(terms.begin(), terms.end()), terms.end());
}

term_freq_vec query_freqs(term_id_vec terms)
{
    term_freq_vec query_term_freqs;
    std::sort(terms.begin(), terms.end());
    // count query term frequencies
    for (size_t i = 0; i < terms.size(); ++i) {
        if (i == 0 || terms[i] != terms[i - 1]) {
            query_term_freqs.emplace_back(terms[i], 1);
        } else {
            query_term_freqs.back().second += 1;
        }
    }
    return query_term_freqs;
}

[[nodiscard]] auto get_high_query(Query q) -> Query
{
    Query high_query;
    high_query.id = q.id;
    for (size_t i = 0; i < q.terms.size(); ++i) {
        if (q.is_high[i]) {
            high_query.terms.push_back(q.terms[i]);
        }
    }
    return high_query;
}

[[nodiscard]] auto get_low_query(Query q) -> Query
{
    Query low_query;
    low_query.id = q.id;
    for (size_t i = 0; i < q.terms.size(); ++i) {
        if (!q.is_high[i]) {
            low_query.terms.push_back(q.terms[i]);
        }
    }
    return low_query;
}

[[nodiscard]] auto get_high_low_pairs(Query q) -> Query
{
    Query low_query;
    low_query.id = q.id;
    for (size_t i = 0; i < q.terms.size(); ++i) {
        if (!q.is_high[i]) {
            low_query.terms.push_back(q.terms[i]);
        }
    }
    return low_query;
}

}  // namespace pisa
