set xlabel "Requests Operations per Second"
set ylabel "Achieved Operations per Second"

set term wxt 0
plot "sfssum.20110225-native" with linespoints title "Linux knfsd", \
     "sfssum.20110226-s3-bigcache" with linespoints title "BlueSky (64 GB Cache)", \
     "sfssum.20110226-s3-smallcache" with linespoints title "BlueSky (1 GB Cache)", \
     "sfssum.20110228-azure-1GB" with linespoints title "BlueSky (Azure, 1 GB Cache)", \
     "sfssum.20110228-s3-west-1GB" with linespoints title "BlueSky (S3-US-West, 1 GB Cache)", \
     "../20110306/sfssum.20110306-s3-west-4GB" with linespoints title "BlueSky (S3-US-West, 4 GB Cache, re-run)", \
     "../20110307/sfssum.20110307-ec2-west2" with linespoints title "In-cloud NFS"

set term wxt 1
set ylabel "Latency (ms)"
plot "sfssum.20110225-native" using 1:3 with linespoints title "Linux knfsd", \
     "sfssum.20110226-s3-bigcache" using 1:3 with linespoints title "BlueSky (64 GB Cache)", \
     "sfssum.20110226-s3-smallcache" using 1:3 with linespoints title "BlueSky (1 GB Cache)", \
     "sfssum.20110228-azure-1GB" using 1:3 with linespoints title "BlueSky (Azure, 1 GB Cache)", \
     "sfssum.20110228-s3-west-1GB" using 1:3 with linespoints title "BlueSky (S3-US-West, 1 GB Cache)", \
     "../20110307/sfssum.20110306-s3-west-4GB" using 1:3 with linespoints title "BlueSky (S3-US-West, 4 GB Cache, re-run)", \
     "../20110307/sfssum.20110307-ec2-west2" using 1:3 with linespoints title "In-cloud NFS", \
     "../20110307/sfssum.20110307-ec2-west" using 1:3 with linespoints title "In-cloud NFS"

#set term wxt 2
#set ylabel "Working Set Size (GB)"
#plot "sfssum.20110225-native" using 1:($9*0.3/1024.0**2) with linespoints notitle
