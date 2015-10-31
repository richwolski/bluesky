load "common.gnuplot"

set grid
set xlabel "Proxy Cache Size (% Working Set)"
set ylabel "Read Latency (ms)"
set xrange [0:100]

set output "syn-read-1.eps"
set title "Single-Client Request Stream"
set key top right
plot "../20110517/32k-c1.data" using (100*$1/32):4 with linespoints title "32 KB", \
     "../20110517/128k-c1.data" using (100*$1/32):4 with linespoints title "128 KB", \
     "../20110517/1024k-c1.data" using (100*$1/32):4 with linespoints title "1024 KB"

# set output "syn-read-16.eps"
# set title "16 Concurrent Request Streams"
# set key top left
# plot "../20110316-synread/32k-c16.data" using (100*$1/32):4 with linespoints title "32 KB", \
#      "../20110316-synread/128k-c16.data" using (100*$1/32):4 with linespoints title "128 KB", \
#      "../20110316-synread/1024k-c16.data" using (100*$1/32):4 with linespoints title "1024 KB"

set ylabel "Read Bandwidth (MB/s)"
set output "syn-read-1b.eps"
set title "Single-Client Request Stream"
set key top left
plot "../20110517/32k-c1.data" using (100*$1/32):3 with linespoints title "32 KB", \
     "../20110517/128k-c1.data" using (100*$1/32):3 with linespoints title "128 KB", \
     "../20110517/1024k-c1.data" using (100*$1/32):3 with linespoints title "1024 KB"

# set output "syn-read-16b.eps"
# set title "16 Concurrent Request Streams"
# set key top right
# plot "../20110316-synread/32k-c16.data" using (100*$1/32):3 with linespoints title "32 KB", \
#      "../20110316-synread/128k-c16.data" using (100*$1/32):3 with linespoints title "128 KB", \
#      "../20110316-synread/1024k-c16.data" using (100*$1/32):3 with linespoints title "1024 KB"
