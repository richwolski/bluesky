# Common definitions and functions for the various run-* scripts.

. $HOME/bin/aws-keys

SCRIPT_PATH=/scratch/bluesky.git/microbench/setup

PARAMS="BLUESKY_RUN_NAME BLUESKY_CACHE_SIZE BLUESKY_TARGET BENCH_DURATION BENCH_FILESIZE BENCH_FILECOUNT BENCH_WRITERATIO BENCH_THREADS BENCH_OPS BENCH_INTERVALS BENCH_DIRSIZE BENCH_BLOCKSIZE NFS_BLOCKSIZE SPEC_CONF BLUESKY_OPT_FULL_SEGMENTS SYNREAD_OUTSTANDING SYNREAD_PROCS SYNREAD_SIZE SYNREAD_DURATION SYNWRITE_RATE"

BLUESKY_RUN_NAME=$(date +%Y%m%d)-$$
BENCHER=vrable1.sysnet.ucsd.edu
PROXY=vrable2.sysnet.ucsd.edu
BLUESKY_EXTRA_OPTS=""

SSH_ARGS="-i $HOME/.ssh/id_bluesky"

get_params() {
    for p in $PARAMS; do
        if [ -n "${!p}" ]; then
            echo "$p=${!p}"
        fi
    done
    for p in $BLUESKY_EXTRA_OPTS; do
        echo $p
    done
}

run_cmd() {
    host="$1"; shift
    cmd="$1"; shift
    echo "EXECUTE($host):" "$cmd" "$@" 1>&2
    ssh $SSH_ARGS -l root $host $SCRIPT_PATH/$cmd "$@" $(get_params)
}

BLUESKY_CACHE_SIZE=$((8 * 1024 * 1024))
BENCH_FILESIZE=$((1024 * 1024))
BENCH_BLOCKSIZE=0
BENCH_FILECOUNT=$((62))
BENCH_WRITERATIO=0.5
BENCH_THREADS=4
BENCH_DURATION=120
BENCH_INTERVALS=10
BENCH_DIRSIZE=128
NFS_BLOCKSIZE=1048576
