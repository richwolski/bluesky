#!/bin/bash
#
# Gather statistics out of a cleaner.log log file and group into plottable
# form.

DATAFILE="$1"

extract() {
    grep "$1" "$DATAFILE" | cut -f2 -d' '
}

echo "#extract bytes_used extract bytes_wasted extract bytes_freed extract s3_get extract s3_put"
paste <(extract bytes_used) <(extract bytes_wasted) <(extract bytes_freed) <(extract s3_get) <(extract s3_put)
