#!/bin/bash

# -------------------------------------------------------------------
#  ____ ___  _   _ _____ ___ ____
# / ___/ _ \| \ | |  ___|_ _/ ___|
#| |  | | | |  \| | |_   | | |  _
#| |__| |_| | |\  |  _|  | | |_| |
# \____\___/|_| \_|_|   |___\____|
#
# -------------------------------------------------------------------

# Anserini2CIFF: https://github.com/osirrc/ciff
# Please get the repo and build it following the local instructions (see the README on GitHub)
# Recommend cloning as `anserini-ciff` to avoid confusion
anserini_to_ciff=anserini-ciff/target/appassembler/ExportAnseriniLuceneIndex 

# Reordering: https://github.com/mpetri/faster-graph-bisection
# Use GAIN=approx_1 when building the codebase (see the README on GitHub)
reorder=faster-graph-bisection/target/release/create-rgb

# Path to the existing DeepImpact CIFF File
# Currently not hosted for anonymity; The file will be made available once the anonymity period is over
di_path=/path/to/msmarcov2-deepimpact-0516.ciff

# -------------------------------------------------------------------

echo "Caution: Running this script will consume up to 2TiB of disk space due to the size of the collections"


# Assumes we already have the DeepImpact CIFF file. Make sure you set up the path.
run_deepimpact() {
    
    echo "DeepImpact Index (Augmented)"
    $reorder --input $di_path --min-len 128 --output-ciff bp-deepimpact-v2.ciff --loggap

}

# See: https://github.com/castorini/pyserini/blob/master/pyserini/prebuilt_index_info.py

run_original() {

    echo "Original Index (Augmented)"
    wget https://rgw.cs.uwaterloo.ca/JIMMYLIN-bucket0/pyserini-indexes/lucene-index.msmarco-v2-passage-augmented-slim.20220111.06fb4f.tar.gz
    md5sum lucene-index.msmarco-v2-passage-augmented-slim.20220111.06fb4f.tar.gz
    echo "Expected: af893e56d050a98b6646ce2ca063d3f4"

    tar xzvf lucene-index.msmarco-v2-passage-augmented-slim.20220111.06fb4f.tar.gz
    mv lucene-index.msmarco-v2-passage-augmented-slim.20220111.06fb4f anserini-original-v2

    $anserini_to_ciff -index anserini-original-v2 -output original-v2.ciff

    $reorder --input original-v2.ciff --min-len 128 --output-ciff bp-original-v2.ciff --loggap
}

run_t5() {

    echo "DocT5Query Index (Augmented)"
    wget https://rgw.cs.uwaterloo.ca/JIMMYLIN-bucket0/pyserini-indexes/lucene-index.msmarco-v2-passage-augmented-d2q-t5.20220201.9ea315.tar.gz
    md5sum lucene-index.msmarco-v2-passage-augmented-d2q-t5.20220201.9ea315.tar.gz
    echo "Expected: f248becbe3ef3fffc39680cff417791d"

    tar xzvf lucene-index.msmarco-v2-passage-augmented-d2q-t5.20220201.9ea315.tar.gz
    mv lucene-index.msmarco-v2-passage-augmented-d2q-t5.20220201.9ea315 anserini-doct5query-v2

    $anserini_to_ciff -index anserini-doct5query-v2 -output doct5query-v2.ciff

    $reorder --input doct5query-v2.ciff --min-len 128 --output-ciff bp-doct5query-v2.ciff --loggap

}

run_unicoil-tilde() {
    
    echo "UniCOIL-TILDE Index (Augmentation unknown)"
    wget https://rgw.cs.uwaterloo.ca/JIMMYLIN-bucket0/pyserini-indexes/lucene-index.msmarco-v2-passage.unicoil-tilde.20211012.58d286.tar.gz
    md5sum lucene-index.msmarco-v2-passage.unicoil-tilde.20211012.58d286.tar.gz
    echo "Expected: 562f9534eefe04ab8c07beb304074d41"
    
    tar xzvf lucene-index.msmarco-v2-passage.unicoil-tilde.20211012.58d286.tar.gz
    mv lucene-index.msmarco-v2-passage.unicoil-tilde.20211012.58d286 anserini-unicoil-tilde-v2

    $anserini_to_ciff -index anserini-unicoil-tilde-v2 -output unicoil-tilde-v2.ciff

    $reorder --input unicoil-tilde-v2.ciff --min-len 128 --output-ciff bp-unicoil-tilde-v2.ciff --loggap

}

run_unicoil-t5() {
    
    echo "uniCOIL-DocT5Query Index (Augmentation unknown)"
    wget https://rgw.cs.uwaterloo.ca/JIMMYLIN-bucket0/pyserini-indexes/lucene-index.msmarco-v2-passage-unicoil-0shot.20220219.6a7080.tar.gz
    md5sum lucene-index.msmarco-v2-passage-unicoil-0shot.20220219.6a7080.tar.gz
    echo "Expected: ea024b0dd43a574deb65942e14d32630"

    tar xzvf lucene-index.msmarco-v2-passage-unicoil-0shot.20220219.6a7080
    mv lucene-index.msmarco-v2-passage-unicoil-0shot.20220219.6a7080 anserini-unicoil-doct5query-v2

    $anserini_to_ciff -index anserini-unicoil-doct5query-v2 -output unicoil-doct5query-v2.ciff

    $reorder --input unicoil-doct5query-v2.ciff --min-len 128 --output-ciff bp-unicoil-doct5query-v2.ciff --loggap

}


