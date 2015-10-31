set xlabel "Requests Operations per Second"
set ylabel "Achieved Operations per Second"

set term wxt 0
plot "sfssum.20110214-native" with linespoints title "Linux knfsd", \
     "sfssum.20110215-bluesky-512M" with linespoints title "BlueSky (512 MB Cache)", \
     "sfssum.20110214-bluesky-4G" with linespoints title "BlueSky (4 GB Cache)"

set term wxt 1
set ylabel "Latency (ms)"
plot "sfssum.20110214-native" using 1:3 with linespoints title "Linux knfsd", \
     "sfssum.20110215-bluesky-512M" using 1:3 with linespoints title "BlueSky (512 MB Cache)", \
     "sfssum.20110214-bluesky-4G" using 1:3 with linespoints title "BlueSky (4 GB Cache)"
