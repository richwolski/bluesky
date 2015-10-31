load "common.gnuplot"

OPS_WSS_SCALE=36/1024.0

set grid
set xlabel "Requested Operations per Second"
set x2label "Working Set Size (GB)"
set xtics nomirror
set x2tics auto

XMAX=1600
set xrange [0:XMAX]
set x2range [0:XMAX*OPS_WSS_SCALE]

set output "spec1a.eps"     # Base comparison
set key off
set ylabel "Achieved Operations per Second"
plot "../20110409/sfssum.20110410-native-hi16" with linespoints title "Local NFS", \
     "../20110409/sfssum.20110409-s3-west-hi16" with linespoints title "BlueSky", \
     "../20110409/sfssum.20110411-s3-west-hi16-crypt" with linespoints title "BlueSky (crypto)", \
     "../20110409/sfssum.20110430-s3-west-hi16-nosegments" with linespoints title "BlueSky (noseg)", \
     "../20110409/sfssum.20110430-s3-west-hi16-fullfetch" with linespoints title "BlueSky (norange)", \
     "../20120109/sfssum.20120121-s3-west-hi16-lowbandwidth" with linespoints title "BlueSky (100 Mbps)"
     #"../20110409/sfssum.20110430-s3-west-hi16-noreadagg" with linespoints title "BlueSky (noreadagg)"

#set output "spec2a.eps"
#plot "../20110409/sfssum.20110409-s3-west-hi16" with linespoints title "BlueSky", \
#     "../20110409/sfssum.20110411-s3-west-hi16-crypt" with linespoints title "BlueSky (+crypto)", \
#     "../20110409/sfssum.20110430-s3-west-hi16-nosegments" with linespoints title "BlueSky (noseg)", \
#     "../20110409/sfssum.20110430-s3-west-hi16-fullfetch" with linespoints title "BlueSky (norange)", \
#     "../20110409/sfssum.20110430-s3-west-hi16-noreadagg" with linespoints title "BlueSky (noreadagg)"

set output "spec1b.eps"
set ylabel "Operation Latency (ms)"
set yrange [0:100]
set key at graph -0.06, 0.97 top left
plot "../20110409/sfssum.20110410-native-hi16" using 1:3 with linespoints title "Local NFS", \
     "../20110409/sfssum.20110409-s3-west-hi16" using 1:3 with linespoints title "BlueSky", \
     "../20110409/sfssum.20110411-s3-west-hi16-crypt" using 1:3 with linespoints title "BlueSky (crypto)", \
     "../20110409/sfssum.20110430-s3-west-hi16-nosegments" using 1:3 with linespoints title "BlueSky (noseg)", \
     "../20110409/sfssum.20110430-s3-west-hi16-fullfetch" using 1:3 with linespoints title "BlueSky (norange)", \
     "../20120109/sfssum.20120121-s3-west-hi16-lowbandwidth" using 1:3 with linespoints title "BlueSky (100 Mbps)"
     #"../20110409/sfssum.20110430-s3-west-hi16-noreadagg" using 1:3 with linespoints title "BlueSky (noreadagg)"

# Comparison with low parallelism
XMAX=1250
set xrange [0:XMAX]
set x2range [0:XMAX*OPS_WSS_SCALE]
set yrange [*:*]

set key off
set output "spec2a.eps"
set ylabel "Achieved Operations per Second"
plot "../20110311/sfssum.20110312-native" with linespoints title "Local NFS", \
     "../20110311/sfssum.20110311-s3-west" with linespoints title "BlueSky", \
     "../20110311/sfssum.20110311-s3-west-4kfs" with linespoints title "BlueSky (4K)", \
     "../20110311/sfssum.20110312-s3-west-noseg" with linespoints title "BlueSky (No Segments)", \
     "../20110311/sfssum.20110312-s3-west-fullseg" with linespoints title "BlueSky (full fetches)", \
     "../20110401/sfssum.20110401-azure" with linespoints title "Azure"

set key top right
set ylabel "Operation Latency (ms)"
set yrange [0:100]
set output "spec2b.eps"
plot "../20110311/sfssum.20110312-native" using 1:3 with linespoints title "Local NFS", \
     "../20110311/sfssum.20110311-s3-west" using 1:3 with linespoints title "BlueSky", \
     "../20110311/sfssum.20110311-s3-west-4kfs" using 1:3 with linespoints title "BlueSky (4K)", \
     "../20110311/sfssum.20110312-s3-west-noseg" using 1:3 with linespoints title "BlueSky (noseg)", \
     "../20110311/sfssum.20110312-s3-west-fullseg" using 1:3 with linespoints title "BlueSky (norange)", \
     "../20110401/sfssum.20110401-azure" using 1:3 with linespoints title "BlueSky (Azure)"

# CIFS results
XMAX=1000
set xrange [0:XMAX]
set x2range [0:XMAX*OPS_WSS_SCALE]
set xtics nomirror
set x2tics auto
set yrange [*:*]
set output "spec-cifs1.eps"
set ylabel "Achieved Operations per Second"
plot "../20110227a/sfssum.20110227-samba" with linespoints title "Samba", \
     "../20110317/sfssum.20110317-cifs" with linespoints title "BlueSky"

set output "spec-cifs2.eps"
set ylabel "Operation Latency (ms)"
plot "../20110227a/sfssum.20110227-samba" using 1:3 with linespoints title "Samba", \
     "../20110317/sfssum.20110317-cifs" using 1:3 with linespoints title "BlueSky"

# Four-way comparison of read latencies among native/BlueSky with NFS/CIFS
XMAX=750
set xrange [0:XMAX]
set x2range [0:XMAX*OPS_WSS_SCALE]
set xtics nomirror
set x2tics auto
set yrange [*:*]
set key top left

set output "spec-read-latencies.eps"
set ylabel "Operation Latency (ms)"
plot "../20110317/read-latencies.data" using 1:2 with linespoints title "Native NFS", \
     "../20110317/read-latencies.data" using 1:3 with linespoints title "BlueSky NFS", \
     "../20110317/read-latencies.data" using 1:4 with linespoints title "Samba (CIFS)", \
     "../20110317/read-latencies.data" using 1:5 with linespoints title "BlueSky CIFS"
