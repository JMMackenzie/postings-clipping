#!/bin/bash

# This file shows all the steps required to reproduce a single index run.
# This will need to be repeated for each index.

# -----------------------------------------------------------------------------
# Configure the following details.

# Grab the tool to convert CIFF to PISA
# see: https://github.com/pisa-engine/ciff for instructions if you are stuck
cargo install ciff

# Assumes you collected the data already
ciff=../data/bp-unicoil-tilde-v2.ciff
collection="bp-unicoil-tilde"

# Assumes you have build the modified PISA repository
pisa=../pisa/build/bin
# -----------------------------------------------------------------------------

# Prepare the directory for the BASE index
mkdir base-index
cd base-index

# 1. Convert the CIFF file into something PISA can understand
ciff2pisa --ciff-file $ciff --output bp-unicoil-tilde

# 2. Build PISA indexes

echo "Building PISA index with block_simdbp compression..."
$pisa/compress_inverted_index --encoding block_simdbp --collection $collection --output $collection.block_simdbp.idx

echo "Building WAND data for $collection..."

$pisa/create_wand_data --collection $collection \
                       --output $collection.fixed-40.bmw \
                       --block-size 40 \
                        --scorer quantized

echo "Building the term lexicon..."
$pisa/lexicon build bp-$collection.terms $collection.termlex
echo "Done."

echo "Building the document identifier map..."
$pisa/lexicon build bp-$collection.documents $collection.docmap
echo "Done."

cd ..

echo "Preparing the clipped index..."
# Prepare the clipped index
cp -r base-index clipped-index
cd clipped-index 
rm *idx *bmw

# Compute the split points based on 1/64th of list length
python3 ../tools/compute_splits.py ../base-index/$collection 64 > splits.txt

# Convert the index to a clipped index
../tools/clip-index ../base-index/$collection $collection splits.txt
echo "Building PISA index with block_simdbp compression..."
$pisa/compress_inverted_index --encoding block_simdbp --collection $collection --output $collection.block_simdbp.idx

echo "Building WAND data for $collection..."

$pisa/create_wand_data --collection $collection \
                       --output $collection.fixed-40.bmw \
                       --block-size 40 \
                        --scorer quantized

echo "Building the term lexicon..."
$pisa/lexicon build bp-$collection.terms $collection.termlex
echo "Done."

echo "Building the document identifier map..."
$pisa/lexicon build bp-$collection.documents $collection.docmap
echo "Done."

cd ..

echo "Preparing the split index..."
# Prepare the split index
cp -r base-index split-index
cd split-index 
rm *idx *bmw
cp ../clipped-index/splits.txt .

# Convert the index to a split index
../tools/split-index ../base-index/$collection $collection splits.txt
echo "Building PISA index with block_simdbp compression..."
$pisa/compress_inverted_index --encoding block_simdbp --collection $collection --output $collection.block_simdbp.idx

echo "Building WAND data for $collection..."

$pisa/create_wand_data --collection $collection \
                       --output $collection.fixed-40.bmw \
                       --block-size 40 \
                        --scorer quantized

echo "Building the term lexicon..."
$pisa/lexicon build bp-$collection.terms $collection.termlex
echo "Done."

echo "Building the document identifier map..."
$pisa/lexicon build bp-$collection.documents $collection.docmap
echo "Done."


