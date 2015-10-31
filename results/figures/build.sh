#!/bin/bash

for f in *.gnuplot; do
    [ "$f" = "common.gnuplot" ] && continue
    echo "gnuplot: $f"
    gnuplot $f
done

for f in *.eps; do
    echo "epstopdf: $f"

    # Strip out PDF metadata which includes identifying usernames
    sed -e $'/^SDict begin/,/^end/ {\n  /^  \/.*(/d\n}' \
        -e '/^%%\(Title\|CreationDate\)/d' <$f >$f.tmp

    epstopdf --noembed --outfile=${f%.eps}.pdf $f.tmp
    rm $f $f.tmp
done
