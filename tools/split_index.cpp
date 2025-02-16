/* 
 * ../../script/split_index /path/to/canonical/deepimpact /path/to/output/canonical/deepimpact-new splits.txt
 * splits.txt: one integer per line which is the score at which to split
 * Will output three files: a new .docs and  .freqs file, and a new .terms file
 */

#include <iostream>
#include <vector>
#include <iterator>
#include <fstream>
#include <istream>
#include <sstream>
#include <ios>
#include <limits>
#include <stdexcept>
#include <x86intrin.h>
#include <algorithm>
#include <numeric>
#include <sys/time.h>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <string_view>
#include <algorithm>
#include <random>

struct postings_list {
    std::string term;
    std::vector<uint32_t> docids;
    std::vector<uint32_t> impacts;
};

void sort_and_dump(std::string output_basename, std::vector<postings_list>& my_index, uint32_t num_docs) {

    // Outputs
    std::ofstream docs_out (output_basename + ".docs", std::ios::binary);
    std::ofstream freqs_out (output_basename + ".freqs", std::ios::binary);
    std::ofstream lex_out (output_basename + ".terms");

    // Write the doc count header to the new docs file
    uint32_t one = 1;
    uint32_t tdocs = num_docs;
    docs_out.write(reinterpret_cast<char *>(&one), sizeof(uint32_t));
    docs_out.write(reinterpret_cast<char *>(&tdocs), sizeof(uint32_t));


    // (1) Sort the index lexicographically
    std::sort(my_index.begin(), my_index.end(), [](const auto& lhs, const auto& rhs) {
                    return lhs.term < rhs.term;
    });


    // (2) Write stuff
    for (size_t i = 0; i < my_index.size(); ++i) {
        // list sizes
        uint32_t list_size = my_index[i].docids.size();
        docs_out.write(reinterpret_cast<char *>(&list_size), sizeof(uint32_t));
        freqs_out.write(reinterpret_cast<char *>(&list_size), sizeof(uint32_t));

        // Lists
        for(size_t j = 0; j < list_size; ++j) {
                
            uint32_t docid = my_index[i].docids[j];
            uint32_t fdt = my_index[i].impacts[j];
            docs_out.write(reinterpret_cast<char *>(&docid), sizeof(uint32_t));
            freqs_out.write(reinterpret_cast<char *>(&fdt), sizeof(uint32_t));
       }
       // Lex entry
       lex_out <<  my_index[i].term << "\n";
    }
} 


int main(int argc, char **argv) {

    std::vector<postings_list> my_index;

    std::string ds2i_basename = argv[1];
    std::string output_basename = argv[2];
    std::string splits_file = argv[3];

    // Read the lexicon
    std::vector<std::string> lex;
    std::string term;
    std::ifstream in_lex(ds2i_basename + ".terms");
    while (in_lex >> term) {
        lex.push_back(term);
    }

    // Read the splits
    std::vector<uint64_t> splits;
    std::ifstream in_splits(splits_file);
    uint64_t split;
    while (in_splits >> split) {
       splits.push_back(split); 
    }
    std::cerr << "Read " << splits.size() << " splits...\n";

    // Read DS2i document file prefix
    std::ifstream docs (ds2i_basename + ".docs", std::ios::binary);
    std::ifstream freqs (ds2i_basename + ".freqs", std::ios::binary);


    // Read first sequence from docs
    uint32_t one;
    uint32_t tdocs;
    docs.read(reinterpret_cast<char *>(&one), sizeof(uint32_t));
    docs.read(reinterpret_cast<char *>(&tdocs), sizeof(uint32_t));

    uint32_t d_seq_len = 0;
    uint32_t f_seq_len = 0;
    uint32_t term_id = 0;

    // Sequences are now aligned. Walk them.        
    while(!docs.eof() && !freqs.eof()) {

        // Read sequence lengths and check alignment
        docs.read(reinterpret_cast<char *>(&d_seq_len), sizeof(uint32_t));
        freqs.read(reinterpret_cast<char *>(&f_seq_len), sizeof(uint32_t));
        if (d_seq_len != f_seq_len) {
            std::cerr << "ERROR: Freq and Doc sequences are not aligned. Exiting."
                      << std::endl;
            exit(EXIT_FAILURE);
        }
        
        uint32_t seq_count = 0;
        uint32_t docid = 0;
        uint32_t fdt = 0;

        std::vector<uint32_t> low_docs;
        std::vector<uint32_t> low_freqs;
        std::vector<uint32_t> high_docs;
        std::vector<uint32_t> high_freqs;

        size_t split_bound = splits[term_id];
        
        while (seq_count < d_seq_len) {
            docs.read(reinterpret_cast<char *>(&docid), sizeof(uint32_t));
            freqs.read(reinterpret_cast<char *>(&fdt), sizeof(uint32_t));
            
            // Decide which list...
            if (fdt <= split_bound) {
                low_docs.push_back(docid);
                low_freqs.push_back(fdt);
            } else{
                high_docs.push_back(docid);
                high_freqs.push_back(fdt);
            }
            
            ++seq_count;
        }

        // Write the new split lists, *high first*
        uint32_t list_size = high_docs.size();
        if (list_size > 0) {
            postings_list new_list;
            new_list.term = lex[term_id] + "_HIGH";
            new_list.docids = std::move(high_docs);
            new_list.impacts = std::move(high_freqs);
            my_index.push_back(std::move(new_list));
        }


        list_size = low_docs.size();
        if (list_size > 0) {
            postings_list new_list;
            new_list.term = lex[term_id] + "_LOW";
            new_list.docids = std::move(low_docs);
            new_list.impacts = std::move(low_freqs);
            my_index.push_back(std::move(new_list));
        }
        ++term_id;
    }
   
    sort_and_dump(output_basename, my_index, tdocs);

    if (term_id-1 != splits.size()) {
        std::cerr << "I had " << splits.size() << " splits, but processed " << term_id << " lists...\n";
    } 

} 


