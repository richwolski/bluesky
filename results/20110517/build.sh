#!/bin/bash

for c in 1 4; do for b in 32 128 1024; do (for s in 0 4 8 12 16 20 24 28 32; do echo -ne $s'\t'; ~/local/bluesky.git/nfs3/parse-synread.py $(($b * 1024)) ~/local/bluesky-data/20110517-synread/20110517-syntest-${s}G-$b-c$c/*; done) >${b}k-c$c.data; done; done
