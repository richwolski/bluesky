load "common.gnuplot"

set output "s3bench.eps"

set logscale xy
set xlabel "Object Size (bytes)"
set ylabel "Effective Upload Bandwidth (Mbps)"
set key bottom right
set xrange [1:1e8]; set xtics 1, 100
set yrange [0.0001:1000]
set grid

plot "../s3test-old/t1.data" using 2:($7*8/10**6) with linespoints title "1", \
     "../s3test-old/t2.data" using 2:($7*8/10**6) with linespoints title "2", \
     "../s3test-old/t4.data" using 2:($7*8/10**6) with linespoints title "4", \
     "../s3test-old/t8.data" using 2:($7*8/10**6) with linespoints title "8", \
     "../s3test-old/t16.data" using 2:($7*8/10**6) with linespoints title "16", \
     "../s3test-old/t32.data" using 2:($7*8/10**6) with linespoints title "32", \
     "../s3test-old/t64.data" using 2:($7*8/10**6) with linespoints title "Threads: 64"
