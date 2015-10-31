#!/bin/bash

BASEDIR=$(dirname $(which $0))
. $BASEDIR/run-common.sh

BLUESKY_CACHE_SIZE=$((1024 * 1024))

sizes="128 512 1024 2048 8192"
ops_list="1000 500 200 100 50 20 10"

run_one() {
    PREFIX=$1

    BLUESKY_RUN_NAME=$PREFIX
    $HOME/bin/s3-cleanup.py mvrable-bluesky
    $HOME/bin/s3-cleanup.py mvrable-bluesky-west
    run_cmd $PROXY cleanup-proxy
    run_cmd $PROXY run-proxy >$PREFIX.proxy 2>&1 &
    proxy_pid=$!
    sleep 10
    run_cmd $BENCHER prepare-benchmark

    for BENCH_OPS in $ops_list; do
        sleep 10
        BLUESKY_RUN_NAME=$PREFIX-$BENCH_OPS
        echo "SETTINGS:" $(get_params)
        get_params >$BLUESKY_RUN_NAME.settings
        (date; run_cmd $BENCHER run-benchmark) | tee $BLUESKY_RUN_NAME.results
    done

    run_cmd $PROXY stop-proxy
    echo "Waiting for proxy to stop..."
    wait $proxy_pid
}

run_experiments() {
for size in $sizes; do
    BENCH_FILECOUNT=$(($size * 1024 / ($BENCH_FILESIZE / 1024)))

    BENCH_BLOCKSIZE=32768
    NFS_BLOCKSIZE=32768
    run_one s3-$BENCH_WRITERATIO-${size}M-32k

    BENCH_BLOCKSIZE=0
    NFS_BLOCKSIZE=1048576
    run_one s3-$BENCH_WRITERATIO-${size}M-1024k
done
}

BLUESKY_TARGET=s3:mvrable-bluesky-west
BENCH_THREADS=4

for BENCH_WRITERATIO in 0.0 1.0 0.5; do
    run_experiments
done
