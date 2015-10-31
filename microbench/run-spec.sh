#!/bin/bash

BASEDIR=$(dirname $(which $0))
. $BASEDIR/run-common.sh

run_spec() {
    BLUESKY_RUN_NAME=$(date +%Y%m%d)-$1
    SPEC_CONF=$2

    $HOME/bin/s3-cleanup.py mvrable-bluesky
    $HOME/bin/s3-cleanup.py mvrable-bluesky-west
    run_cmd $PROXY cleanup-proxy
    run_cmd $PROXY run-proxy >$BLUESKY_RUN_NAME.proxy 2>&1 &
    proxy_pid=$!
    sleep 10

    run_cmd $BENCHER run-specsfs

    run_cmd $PROXY stop-proxy
    echo "Waiting for proxy to stop..."
    wait $proxy_pid
}

# Test of using full segment fetches instead of range requests
#BLUESKY_TARGET=s3:mvrable-bluesky-west
#BLUESKY_OPT_FULL_SEGMENTS=1
#run_spec s3-west-fullseg sfs_bluesky

# Run a simulation of not using a log-structured system at all
#BLUESKY_TARGET=s3:mvrable-bluesky-west
#BLUESKY_EXTRA_OPTS="BLUESKY_OPT_FULL_SEGMENTS=1 BLUESKY_OPT_NO_AGGREGATION=1"
#run_spec s3-west-noseg sfs_bluesky

# Standard test with Azure instead of Amazon S3 for storage
#BLUESKY_TARGET=azure
#run_spec azure sfs_bluesky

# BlueSky, testing with a greater degree of parallelism
#BLUESKY_TARGET=s3:mvrable-bluesky-west
#BLUESKY_CACHE_SIZE=$((32 * 1024 * 1024))
#BLUESKY_EXTRA_OPTS=""
#run_spec s3-west-hi16-crypt sfs_bluesky-hi
#
#BLUESKY_TARGET=azure
#BLUESKY_CACHE_SIZE=$((32 * 1024 * 1024))
#BLUESKY_EXTRA_OPTS="BLUESKY_OPT_NO_CRYPTO=1"
#run_spec azure-hi16 sfs_bluesky-hi

#BLUESKY_TARGET=s3:mvrable-bluesky-west
#BLUESKY_CACHE_SIZE=$((32 * 1024 * 1024))
#BLUESKY_EXTRA_OPTS="BLUESKY_OPT_NO_CRYPTO=1 BLUESKY_OPT_FULL_SEGMENTS=1"
#run_spec s3-west-hi16-fullfetch sfs_bluesky-hi

#BLUESKY_TARGET=s3:mvrable-bluesky-west
#BLUESKY_CACHE_SIZE=$((32 * 1024 * 1024))
#BLUESKY_EXTRA_OPTS="BLUESKY_OPT_NO_CRYPTO=1 BLUESKY_OPT_NO_GROUP_READS=1"
#run_spec s3-west-hi16-noreadagg sfs_bluesky-hi

#BLUESKY_TARGET=s3:mvrable-bluesky-west
#BLUESKY_CACHE_SIZE=$((32 * 1024 * 1024))
#BLUESKY_EXTRA_OPTS="BLUESKY_OPT_NO_CRYPTO=1 BLUESKY_OPT_NO_AGGREGATION=1"
#run_spec s3-west-hi16-nosegments sfs_bluesky-hi

#BLUESKY_TARGET=native
#run_spec native-hi8 sfs_bluesky-hi

#BLUESKY_TARGET=s3:mvrable-bluesky-west
#BLUESKY_EXTRA_OPTS="BLUESKY_OPT_NO_GROUP_READS=1"
#run_spec s3-west-noagg sfs_bluesky

#BLUESKY_EXTRA_OPTS="BLUESKY_OPT_NO_CRYPTO=1"
#run_spec s3-west-nocrypt sfs_bluesky

#BLUESKY_EXTRA_OPTS="BLUESKY_OPT_NO_GROUP_READS=1 BLUESKY_OPT_NO_CRYPTO=1"
#run_spec s3-west-noagg-nocrypt sfs_bluesky

BLUESKY_TARGET=s3:mvrable-bluesky-west
BLUESKY_CACHE_SIZE=$((32 * 1024 * 1024))
BLUESKY_EXTRA_OPTS="BLUESKY_OPT_NO_CRYPTO=1"
run_spec s3-west-hi16-lowbandwidth sfs_bluesky-hi

