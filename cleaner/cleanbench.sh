#!/bin/bash
#
# Simple script to perform various random writes to files in a directory to
# create a workload for the cleaner.

NUM_FILES=8
NUM_BLOCKS=16
BLOCKSIZE=512K

NUM_WRITES=128

for n in $(seq 0 $(($NUM_FILES - 1))); do
    [ -f file-$n ] || dd if=/dev/zero of=file-$n bs=$BLOCKSIZE count=$NUM_BLOCKS
done

for i in $(seq $NUM_WRITES); do
    f=$(($RANDOM % $NUM_FILES))
    b=$(($RANDOM % $NUM_BLOCKS))
    echo "Write $i: file $f block $b"
    dd if=/dev/zero of=file-$f bs=$BLOCKSIZE count=1 seek=$b conv=notrunc
    sleep 2
done
