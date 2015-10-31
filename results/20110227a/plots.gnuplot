set xlabel "Requests Operations per Second"
set ylabel "Achieved Operations per Second"

set term wxt 0
plot "sfssum.20110227-samba" with linespoints title "Samba 3", \
     "sfssum.20110227-cifs-bluesky-smallcache" with linespoints title "BlueSky Samba 4 (1 GB Cache)"

set term wxt 1
set ylabel "Latency (ms)"
plot "sfssum.20110227-samba" using 1:3 with linespoints title "Samba 3", \
     "sfssum.20110227-cifs-bluesky-smallcache" using 1:3 with linespoints title "BlueSky Samba 4 (1 GB Cache)"
