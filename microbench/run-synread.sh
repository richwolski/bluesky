#!/bin/bash

BASEDIR=$(dirname $(which $0))
. $BASEDIR/run-common.sh

BLUESKY_TARGET=s3:mvrable-readbench-west
BLUESKY_EXTRA_OPTS="BLUESKY_OPT_NO_CRYPTO=1"

sizes="1048576 131072 32768"

run_synbench() {
    basename=$(date +%Y%m%d)-$1
    BLUESKY_RUN_NAME=$basename

    run_cmd $PROXY cleanup-proxy
    run_cmd $PROXY run-proxy >$BLUESKY_RUN_NAME.proxy 2>&1 &
    proxy_pid=$!
    sleep 10

    SYNREAD_SIZE=$((1 << 24))
    run_cmd $BENCHER warmup-synread
    for s in $sizes; do
        SYNREAD_SIZE=$s

        SYNREAD_OUTSTANDING=1
        SYNREAD_PROCS=1
        BLUESKY_RUN_NAME=$basename-$(($s / 1024))-c1
        run_cmd $BENCHER run-synread

        SYNREAD_OUTSTANDING=2
        SYNREAD_PROCS=2
        BLUESKY_RUN_NAME=$basename-$(($s / 1024))-c4
        run_cmd $BENCHER run-synread
    done

    run_cmd $PROXY stop-proxy
    echo "Waiting for proxy to stop..."
    wait $proxy_pid
}

SYNREAD_DURATION=120
for cache in 0 4 8 12 16 20 24 28 32; do
    BLUESKY_CACHE_SIZE=$(($cache * 1024 * 1024))
    if [ $cache -eq 0 ]; then
        BLUESKY_CACHE_SIZE=$((64 * 1024))
    fi
    run_synbench "syntest-${cache}G"
done
