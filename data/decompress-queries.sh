#!/bin/bash

cd queries
for f in `find . -type f`; do 
  xz -d $f; 
done
