'''
Tool for computing index split points.
Input:
    argv[1]: A PISA canonical index
Output: 
    one-split-point-per-line to stdout
'''


import os
import numpy as np
import statistics
from tqdm import tqdm
import math
import sys

class InvertedIndex:
    def __init__(self, index_name):
        index_dir = os.path.join(index_name)
        self.docs = np.memmap(index_name + ".docs", dtype=np.uint32,
              mode='r')
        self.freqs = np.memmap(index_name + ".freqs", dtype=np.uint32,
              mode='r')

    def __iter__(self):
        i = 2
        while i < len(self.docs):
            size = self.docs[i]
            yield (self.docs[i+1:size+i+1], self.freqs[i-1:size+i-1])
            i += size+1

    def __next__(self):
        return self

for  _, freqs in tqdm(InvertedIndex(sys.argv[1])):
    sorted_freqs = sorted(freqs, reverse=True) # Sort high to low
    split_point = int( (1.0 / int(sys.argv[2])) * len(sorted_freqs)) - 1 
    if len(sorted_freqs) < 256:
        print(10000000)
    else:
        print(sorted_freqs[split_point])
