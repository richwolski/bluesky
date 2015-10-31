# Memory usage for a SPECsfs run.  Proxy running on niniel, with a 32 GB
# disk cache.  Values are summed Private_Dirty and Swap values out of
# /proc/<pid>/smaps.
#
# Ops-per-second range was 40..320 (8 steps total).
#
# Total cloud log entries tracked at end: 1438460

# Proxy statistics:
# Store[s3]: GETS: count=128 sum=3881355
# Store[s3]: PUTS: count=11968 sum=47969053879
# NFS RPC Messages In: count=2772619 sum=44487306816
# NFS RPC Messages Out: count=2772619 sum=3712517128

plot "memusage-niniel.data" using 0:($1/1024) with lines notitle

# Second run, this time using c09-45 and a 16 GB cache.
#
# Ops-per-second: 40..1000 (25 steps)
#
# Cloudlog cache: 0 dirty, 0 writeback, 0 journal, 4278627 cloud
# All segments have been flushed, journal < 35010 is clean
#
# Ending cache size: 16775684 kB
# Proxy statistics:
# Store[s3:mvrable-bluesky-west]: GETS: count=99683 sum=2448946675
# Store[s3:mvrable-bluesky-west]: PUTS: count=34670 sum=142264311995
# NFS RPC Messages In: count=10875062 sum=132270395664
# NFS RPC Messages Out: count=10875062 sum=12727291416
plot "memusage-s3-16G.data" using 0:($1/1024) with lines notitle

# Like above, but 1 GB cache and 40..800 ops per second
#
# Cloudlog cache: 0 dirty, 0 writeback, 0 journal, 3473807 cloud
# All segments have been flushed, journal < 28643 is clean
#
# Ending cache size: 1046960 kB
# Proxy statistics:
# Store[s3:mvrable-bluesky-west]: GETS: count=402565 sum=9032379883
# Store[s3:mvrable-bluesky-west]: PUTS: count=30075 sum=121942281049
# NFS RPC Messages In: count=8874706 sum=108355385832
# NFS RPC Messages Out: count=8874706 sum=12175326008
plot "memusage-s3-1G.data" using 0:($1/1024) with lines notitle
