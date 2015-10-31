#!/bin/bash

for i in $(seq $1); do
    for j in $(seq 8); do
        dd if=/dev/zero of=block-$i-$j bs=1M count=128 conv=fsync
    done
done
