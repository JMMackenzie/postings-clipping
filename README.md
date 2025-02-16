# Postings Clipping: Accelerating Learned Sparse Indexes Via Term Impact Decomposition

## Publication
This repository contains tools related to our EMNLP 2022 findings paper **Accelerating Learned Sparse Indexes Via Term Impact Decomposition** by Joel Mackenzie, Antonio Mallia, Alistair Moffat, and Matthias Petri.

If you use our code in any of your experiments, please consider citing our work:
```
@inproceedings{mmmp22-emnlp,
  author = {J. Mackenzie and A. Mallia and A. Moffat and M. Petri},
  title = {Accelerating Learned Sparse Indexes Via Term Impact Decomposition},
  booktitle = {Findings of the Association for Computational Linguistics: EMNLP 2022},
  year = {2022},
}
```


This repo is a snapshot of what was submitted to EMNLP in 2022. Unfortunately, the 20TB disk array storing the experimental artefacts crashed, so no data is available; our goal is to mainline this code into PISA -- please see https://github.com/pisa-engine/pisa/issues/565 -- if you need any support, feel free to make an issue.

## Notes

This README is outlined according to the directory structure.

```
├── data
│   ├── decompress-queries.sh
│   ├── get-data.sh
│   └── queries
├── example
│   ├── index.sh
│   ├── run-baselines.sh
│   ├── run-improvement-sequence.sh
│   └── runs
├── pisa-decomposition
│   ├── benchmarks
│   ├── bump.json
│   ├── CHANGELOG.md
│   ├── clang.cmake
│   ├── CMakeLists.txt
│   ├── docs
│   ├── include
│   ├── LICENSE
│   ├── README.md
│   ├── script
│   ├── src
│   ├── test
│   └── tools
├── README.md
└── tools
    ├── clip_index.cpp
    ├── compute_splits.py
    ├── Makefile
    ├── modify_queries.py
    ├── README.md
    ├── split_index.cpp
    └── tokenize_queries.py
```

data
----
This directory contains all of the scripts required to collect the basic data,
and to build an initial "CIFF" representation of the data (preprocessing).

get-data.sh is the tool to do that; please see the script for more information
on tooling required to collect and preprocess the data.

queries are xz compressed and need to be decompressed via decompress-queries.sh

The queries directory contains all MSMARCO queries for all collections pre-generated.

example
-------

This is an example set of retrieval runs, the main experiments of the paper.
The example shows how to reproduce one system (with the ability to plug in the
other systems with minor changes). 

index.sh will build three indexes: a baseline index; a split index; and a clipped
index. Again, see the script for more information on requirements.

the run-baselines.sh and run-improvement-sequence.sh scripts can be used to execute
the experimental runs.

runs/ is a directory for storing the output runs.

pisa-baseline
-------------

This is the baseline C++ PISA repository as cloned from GitHub.
Please run:
git clone https://github.com/pisa-engine/pisa pisa-baseline

pisa-decomposition
------------------

This is the same as the above repo, but it also incorporates all of our
novel modifications. We aim to contribute these changes back into the
main PISA repo with the help of the PISA maintainers after the
anonymity period is over.

Note that the external repo needs to be linked from the other PISA
repository, as it was too large to be included in this supplementary
file. 

ln -s pisa-baseline/external pisa-decomposition/external

tools
-----

This directory contains some tooling required to generate the split or
clipped indexes for our work. Please see the local README and the
tools for more information.


Requirements
-------------

Our experimental setup is described in the paper. We assume a system
which is capable of building the baseline PISA and Anserini systems,
as well as the Rust tooling via Cargo. See, for example:
https://pisa.readthedocs.io/en/latest/getting_started.html#dependencies

All external tools and their requirements can be found below:
 - https://github.com/pisa-engine/ciff
 - https://github.com/pisa-engine/pisa/
 - https://github.com/mpetri/faster-graph-bisection
 - https://github.com/castorini/anserini
 - https://github.com/osirrc/ciff

Over 2TiB of disk space will be required to host all pre-processing data.

All experiments are ran in memory, so at least 64 GiB of RAM is recommended.

More details on our particular configuration can be found in the paper.
