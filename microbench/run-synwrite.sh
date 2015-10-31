#!/bin/bash

BASEDIR=$(dirname $(which $0))
. $BASEDIR/run-common.sh

BLUESKY_TARGET=s3:mvrable-bluesky-west
BLUESKY_EXTRA_OPTS="BLUESKY_OPT_NO_CRYPTO=1"

rates="2 4 6 8 10 12 14 16 18 20 24 28 32"

run_synbench() {
    basename=$(date +%Y%m%d)-$1
    BLUESKY_RUN_NAME=$basename

    $HOME/bin/s3-cleanup.py mvrable-bluesky-west
    run_cmd $PROXY cleanup-proxy
    run_cmd $PROXY run-proxy >$BLUESKY_RUN_NAME.proxy 2>&1 &
    proxy_pid=$!
    sleep 10

    for SYNWRITE_RATE in $rates; do
        run_cmd $BENCHER run-synwrite
        sleep 180
    done

    run_cmd $PROXY stop-proxy
    echo "Waiting for proxy to stop..."
    wait $proxy_pid
}

for BLUESKY_CACHE_SIZE in $((256 * 1024)) $((2048 * 1024)); do
    run_synbench "write100-$(($BLUESKY_CACHE_SIZE / 1024))M"
done
