#!/bin/bash

do_run() {
    LOGDIR=$(mktemp -d ./logdir.XXXXXXXX)
    sync; sleep 0.5
    echo Running: "$@"
    (cd "$LOGDIR"; ../logbench "$@")
    rm -rf "$LOGDIR"
}

for s in 256 1024 4096 16384 65536; do
    #do_run -B -s $s
    true
done

for b in 1 2 4 8 16 32; do
    for s in 1024 4096 32768; do
        n=$(((1 << 26) / $s))
        if [ $n -gt 16384 ]; then
            n=16384
        fi
        if [ $n -lt 4096 ]; then
            n=4096
        fi

        do_run -B -s $s -b $b -n $n
        do_run -B -a -s $s -b $b -n $n
        do_run -F -s $s -b $b -n $n
        do_run -D -s $s -b $b -n $n
        echo "========"
    done
done
