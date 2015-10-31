load "common.gnuplot"

OPS_WSS_SCALE=36/1024.0

set grid
set xlabel "Requested Operations per Second"
set x2label "Working Set Size (GB)"

set xrange [0:1000]
set x2range [0:1000*OPS_WSS_SCALE]
set x2tics auto

set grid

set output "specsample1.eps"
set ylabel "Achieved Operations per Second"
plot "../20110226/sfssum.20110225-native" with linespoints title "Local NFS Server", \
     "../20110307/sfssum.20110307-ec2-west2" with linespoints title "EC2 NFS Server", \
     "../20110306/sfssum.20110306-s3-west-4GB" with linespoints title "BlueSky (4 GB cache, S3-west)"

set output "specsample2.eps"
set ylabel "Operation Latency (ms)"
plot "../20110226/sfssum.20110225-native" using 1:3 with linespoints title "Local NFS Server", \
     "../20110307/sfssum.20110307-ec2-west2" using 1:3 with linespoints title "EC2 NFS Server", \
     "../20110306/sfssum.20110306-s3-west-4GB" using 1:3 with linespoints title "BlueSky (4 GB cache, S3-west)"
