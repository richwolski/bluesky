load "common.gnuplot"

set grid
set title "Storage Used: Writes Running Concurrently with Cleaner"
set xlabel "Cleaner Pass Number"
set ylabel "Cloud Storage Consumed (MB)"
set key top right
set yrange [0:200]

set output "cleaner.eps"
set xtics 1
set xrange [0.5:14.5]
set grid noxtics
plot "../20110925/cleaner.data" using ($0+1):(($1+$2+$5)/1024**2) with boxes fill solid 0.0 lt 1 title "Reclaimed", \
     "../20110925/cleaner.data" using ($0+1):(($1+$2-$3+$5)/1024**2) with boxes fill solid 0.2 lt 1 title "Wasted", \
     "../20110925/cleaner.data" using ($0+1):(($1)/1024**2) with boxes fill solid 0.4 lt 1 title "Rewritten", \
     "../20110925/cleaner.data" using ($0+1):(($1-$5)/1024**2) with boxes fill solid 0.6 lt 1 title "Used/Unaltered"

set title "Data Written by Cleaner"
set ylabel "Writes (MB)"
set output "cleaner-rw.eps"
plot "../20110925/cleaner.data" using 0:($5/1024**2) with boxes fill solid 0.0 lt 1 title "Bytes Written"
