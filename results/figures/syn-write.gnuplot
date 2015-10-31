load "common.gnuplot"

set grid
set xlabel "Client Write Rate (MB/s): 2-Minute Burst"
set ylabel "Average Write Latency (ms/1 MB write)"
set key bottom right

set yrange [0:*]
set output "syn-write.eps"
set title "Latency vs. Write Rate with Constrained Upload"
plot "../20110405/synwrite-256M-summary.data" using 1:($2*1000):($3*1000) with yerrorbars title "128 MB Write Buffer", \
     "../20110405/synwrite-2048M-summary.data" using 1:($2*1000):($3*1000) with yerrorbars title "1 GB Write Buffer", \
     "../20110405/synwrite-256M-summary.data" using 1:($2*1000) with lines lt 1 notitle, \
     "../20110405/synwrite-2048M-summary.data" using 1:($2*1000) with lines lt 2 notitle

set yrange [0:*]
set output "syn-write10.eps"
set title "Latency vs. Write Rate with Constrained Upload"
plot "../20110815/synwrite-256M-summary.data" using 1:($2*1000) with linespoints title "128 MB Write Buffer"
     #"../20110815/synwrite-2048M-summary.data" using 1:($2*1000) with linespoints title "1 GB Write Buffer"
