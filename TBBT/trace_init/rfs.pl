#!/usr/bin/perl -w

use Getopt::Std;
$Usage =<< "EOF";

Usage: $0 [options]

This program runs after nfsscan.pl. It outputs the file system hierarchy (RFSFS) as well as
the file system hierarchy map (stored in three files: fh-path-map, noattrdirdiscard, noattrdir-root).

This perl program do post processing based on this file

Command line options:

-h	Print Usage message and exit.

-S  The file in file system hierarchy are all of size 0. 
    Without -S, the files are written to it full length.


EOF

$writebuf = "R" x 8193;
$writeBlockSize = 8192;

main ();

sub rfsCreateSymlink()
{
        my ($path, $pathcnt, $sizeHexStr) = @_;
        $sizeHexStr = "0x".$sizeHexStr;

        my $dir = $path;
        my $name = '';
        my $cmdbuf;

        $dir =~ /(^.*)\/(.*)/;
        $dir = $1;
        $name = $2;
        if (!$dir) {$dir = ".";}
        die "name is empty\n" if (!$name);
        #print "path($path) dir($dir) name($name)\n";

        if (! -e $dir) {
                $cmdbuf = "mkdir -p $dir";
                system $cmdbuf;
                print "RFS: Warning: the directory should be created already: $path\n";
        } else {
                die "warning: directory name exist but not a directory: $path\n" if (!(-d $dir));
        }

        my $size = hex($sizeHexStr);
        #print "size($sizeHexStr) lp($lp) rem($remSize)\n";

        my $pathnamebuf = $sizeHexStr;
        symlink ($pathnamebuf, $path);
        #if (! symlink ($pathnamebuf, $path) ) {
        #	print STDERR "JIAWU: WARNING: failed to create symlink: $path -> $pathnamebuf\n";
        #}
}


sub  rfsCreateFile()
{
        my ($path, $pathcnt, $sizeHexStr) = @_;
        $sizeHexStr = "0x".$sizeHexStr;

        my $dir = $path;
        my $name = '';
        my $cmdbuf;

        $dir =~ /(^.*)\/(.*)/;
        $dir = $1;
        $name = $2;
        if (!$dir) {$dir = ".";}
        die "name is empty\n" if (!$name);
        #print "path($path) dir($dir) name($name)\n";

        if (! -e $dir) {
                $cmdbuf = "mkdir -p $dir";
                system $cmdbuf;
                print "RFS: Warning: the directory should be created already: $path\n";
        } else {
                die "warning: directory name exist but not a directory: $path\n" if (!(-d $dir));
        }

        my $size = hex($sizeHexStr);
        my $remSize = $size % $writeBlockSize;
        my $lp = ($size - $remSize) / $writeBlockSize;
        #print "size($sizeHexStr) lp($lp) rem($remSize)\n";

        open RFSTMPWRITE, ">$path" || die "RFS: can not open file for write";

	# the eight lines below should not be commented out 
		if (!defined $opt_S) {
        my $i = 0;
        for ($i = 0; $i < $lp; $i++) {
                syswrite(RFSTMPWRITE, $writebuf, $writeBlockSize);
        }
        if ($remSize) {
                syswrite(RFSTMPWRITE, $writebuf, $remSize);
                #print "write ($remSize) byte\n";
        }
        close RFSTMPWRITE;
		}
}

# Useful commands:
# sort -n -k a,b -c -u 
# 
# *** WARNING *** The locale specified by the  environment  affects  sort
#      order.  Set LC_ALL=C or LC_ALL=POSIX to get the traditional sort order that uses native
#      byte values.

sub parseArgs {
	my $Options = "S";
	if (! getopts($Options)) {
		print STDERR "$0: Incorrect usage.\n";
        print STDERR $Usage;
		exit (1);
	}
}

sub main {

	parseArgs ();

	my $cmdbuf;

	# format of lines in test.fil:
	# "F(1) $type(2) $state(3) $fh(4) $path(5) $pathcnt(6) $attrOrig(7-14) $attr()
	# attrOrig: ($size(7) $mode(8) $op(9) $atime(10) $mtime(11) $ctime(12) $nlink(13) $ts(14))
	# attrLast: ($size(15) $mode(16) $op(17) $atime(18) $mtime(19) $ctime(20) $nlink(21) $ts(22))

	# skip comment lines
	# if (path_count ($6) == 0 or original_op($9) == "create" or "mkdir" or "symlink") skip the file 

	$cmdbuf = 'gawk \' !/^[#]/ { if ($6 != "0") print $0 }\'  test.fil > all.fil';
	print "$cmdbuf\n";
	system $cmdbuf;
	
	# sort the test.fil according to path name
	$cmdbuf = 'export LC_ALL=C; sort -k 5,5 all.fil > all.fil.order; echo $LC_ALL';
	print "$cmdbuf\n";
	system $cmdbuf;

	# keep the interested field only
	# 2(D/F) 4(FH) 5(path) 6(count) 15(size_last) 21(nlink_last) 14(ts_s) 22(ts_e)
	$cmdbuf = 'gawk \' { print $14, $22, $4, $5, $15, $21}\'  all.fil.order > fh-path-map-all';
	print "$cmdbuf\n";
	system $cmdbuf;
	
	# skip comment lines
	# if (path_count ($6) == 0 or original_op($9) == "create" or "mkdir" or "symlink") skip the file 

	$cmdbuf = 'gawk \' !/^[#]/ { if ($6 != "0" && $9 != "create" && $9 != "mkdir" && $9 != "symlink") print $0 }\'  test.fil > active.fil';
	print "$cmdbuf\n";
	system $cmdbuf;
	
	# sort the active.fil according to path name
	$cmdbuf = 'export LC_ALL=C; sort -k 5,5 active.fil > active.fil.order; echo $LC_ALL';
	print "$cmdbuf\n";
	system $cmdbuf;

	# keep the interested field only
	# 2(D/F) 4(FH) 5(path) 6(count) 7(size) 8(mode) 13(nlink)
	$cmdbuf = 'gawk \' { print $2, $4, $5, $6, $7, $8 }\'  active.fil.order > active';
	print "$cmdbuf\n";
	system $cmdbuf;
	$cmdbuf = 'gawk \' { print $4, $5, $7, $13}\'  active.fil.order > fh-path-map';
	print "$cmdbuf\n";
	system $cmdbuf;

	# format output files
	$cmdbuf = 'export LC_ALL=C; mv noattrdirdiscard noattrdirdiscard-tmp; sort -u -k 1,1 noattrdirdiscard-tmp > noattrdirdiscard; echo $LC_ALL; rm -f noattrdirdiscard-tmp';
	print "$cmdbuf\n";
	system $cmdbuf;

	$cmdbuf = 'export LC_ALL=C; mv noattrdir-root noattrdir-root-tmp; sort -u -k 1,1 noattrdir-root-tmp > noattrdir-root; echo $LC_ALL; rm -f noattrdir-root-tmp';
	print "$cmdbuf\n";
	system $cmdbuf;

	#$cmdbuf = 'mv noattrdirdiscard noattrdirdiscard-tmp; cat  noattrdirdiscard-tmp missdiscardfh >noattrdirdiscard; rm -f noattrdirdiscard-tmp missdiscardfh';
	$cmdbuf = 'mv noattrdirdiscard noattrdirdiscard-tmp; cat  noattrdirdiscard-tmp missdiscardfh >noattrdirdiscard; rm -f noattrdirdiscard-tmp';
	print "$cmdbuf\n";
	system $cmdbuf;

	$cmdbuf = "cut -d ' ' -f '1 2' fh-path-map > fh-path-map-tmp";
	print "$cmdbuf\n";
	system $cmdbuf;

	$cmdbuf = 'cat noattrdirdiscard noattrdir-root fh-path-map-tmp > fh-path-map-play; rm -rf fh-path-map-tmp';
	print "$cmdbuf\n";
	system $cmdbuf;
	
	#exit(0);
	
	# so far, you got the following information
	# in active: all files/dirs active 
	# in noattrdir-root: a set of fhs pointing to RFSNN0
	# in rfsinfo: a set of dir fhs refer to RFSNNxxx(>0)
	#                a set of file fhs should be discard due to short of information

	# create the active fs
	# 1. BASEDIR/RFSNN0
	# 2. BASEDIR/active
	$cmdbuf = "mkdir -p RFSFS/RFSNN0";
	system $cmdbuf;
	open RFS_ACTIVE, "active" || die "open active failed\n";
	while (<RFS_ACTIVE>) {
		chop;
		my ($type, $fh, $path, $pathcnt, $sizeHexStr, $mode) = split (' ', $_, 7);
		if ($type==2) {
			$cmdbuf = "mkdir -p RFSFS$path";
			system $cmdbuf;
		}elsif ($type==1){
			&rfsCreateFile("RFSFS$path", $pathcnt, $sizeHexStr);
		}elsif ($type==5){
			#print STDERR "SYMLINK: $type fh: $fh path: $path\n";
			&rfsCreateSymlink("RFSFS$path", $pathcnt, $sizeHexStr);
		}else {
			print STDERR "special file: $type fh: $fh path: $path\n";
		}

		my $line_num=0;
		$line_num++;
		if ( ($line_num %100)==0 ) {
			print STDERR "$line_num\n";
		}
		
	}

	# create map table: key (fh), value (path/fn)

	# check whether there is fh that is not mapped to any path/fn in the trace

	# simulate a replay of trace

	return;
	
}

1;
