'''
Tool for renaming query terms
Input:
    argv[1]: A PISA query file
Output: 
    stdout: new queries one-per-line
'''



from tqdm import tqdm
import json 
mapping = {}
import sys

with open(sys.argv[1]) as f:
    for line in tqdm(f):
        id, text = line.strip().split(":", 1)
        terms = []
        for term in text.split(" "):
            terms.append(term+"_LOW")
            terms.append(term+"_HIGH")
        text = " ".join(terms)
        print(id + ":" + text)
    
