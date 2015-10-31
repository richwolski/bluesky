#!/usr/bin/perl -w
#
# Copyright (c) 2002-2003
#      The President and Fellows of Harvard College.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $Id: nfsscan,v 1.18 2003/07/28 14:27:16 ellard Exp $

$ProgDir = $0;
$ProgDir =~ /(^.*)\//;
$ProgDir = $1;
if (!$ProgDir) {
	$ProgDir = ".";
}

require "$ProgDir/nfsdump.pl";
require "$ProgDir/userUtils.pl";
require "$ProgDir/hier.pl";
require "$ProgDir/counts.pl";
require "$ProgDir/latency.pl";
require "$ProgDir/key.pl";
require "$ProgDir/common.pl";

use Getopt::Std;

$INTERVAL	= 5 * 60;		# in seconds (5 minutes)

%KeysSeen	= ();

@ADD_USERS	= ();
@DEL_USERS	= ();
@ADD_GROUPS	= ();
@DEL_GROUPS	= ();
@ADD_CLIENTS	= ();
@DEL_CLIENTS	= ();

$DO_COUNTS	= 1;
$DO_LATENCY	= 0;
$DO_FILES	= 0;
$DO_PATHS	= 0;
$DO_SQUEEZE	= 0;

$FH_TYPE	= 'unknown';

$END_TIME	= -1;
$START_TIME	= -1;
$NOW		= -1;
$UseClient	= 0;
$UseFH		= 0;
$UseUID		= 0;
$UseGID		= 0;
$OMIT_ZEROS	= 0;

$OutFileBaseName	= undef;

$nextPruneTime		= -1;
$PRUNE_INTERVAL		= 1 * 60;	# One minute.

# &&&
# Is this really the right default set of operations?

$DEF_OPLIST	= 'read,write,lookup,getattr,access,create,remove';
@OPLIST		= ('TOTAL', 'INTERESTING', 
			split (/,/, $DEF_OPLIST));
%OPARRAY	= ();

$Usage =<< ".";

Usage: $0 [options] [trace1 [trace2 ...]]

If no trace files are specified, then the trace is read from stdin.

Command line options:

-h		Print usage message and exit.

-B [CFUG]	Compute per-Client, per-File, per-User, or per-Group info.

-c c1[,c2]*	Include only activity performed by the specified clients.

-C c1[,c2]*	Exclude activity performed by the specified clients.

-d		Compute per-directory statistics.  This implicitly
		enables -BF so that per-file info is computed.

-f		Do file info tracking.  This implicitly enables -BF so
		that per-File info is computed.

-F fhtype	Specify the file handle type used by the server.
		(advfs or netapp)

-g g1[,g2]*	Include only activity performed by the specified groups.

-G g1[,g2]*	Exclude activity performed by the specified groups.

-l		Record average operation latency.

-o basename	Write output to files starting with the specified
		basename.  The "Count" table goes to basename.cnt,
		"Latency" to basename.lat, and "File" to basename.fil.
		The default is to write all output to stdout.

-O op[,op]*	Specify the list of "interesting" operations.
		The default list is:

		read,write,lookup,getattr,access,create,remove

		If the first op starts with +, then the specified list
		of ops is appended to the default list.  The special
		pseudo-ops readM and writeM represent the number of
		bytes read and written, expressed in MB.

-t interval	Time interval for cummulative statistics (such as
		operation count).  The default is $INTERVAL seconds. 
		If set to 0, then the entire trace is processed.  By
		default, time is specified in seconds, but if the last
		character of the interval is any of s, m, h, or d,
		then the interval is interpreted as seconds, minutes,
		hours, or days.

-u u1[,u2]*	Include only activity performed by the specified users.

-U u1[,u2]*	Exclude activity performed by the specified users.

-Z		Omit count and latency lines that have a zero total
		operation count.
.


main ();

sub main {

	parseArgs ();

	if ($DO_COUNTS) {
		counts::printTitle (*OUT_COUNTS);
	}

	if ($DO_LATENCY) {
		latency::printTitle (*OUT_LATENCY);
	}

	counts::resetOpCounts ();

	my $cmdbuf = 'rm -f noattrdirdiscard noattrdir-root';
	system($cmdbuf);

	readTrace ();
}

sub parseArgs {

	my $cmdline = "$0 " . join (' ', @ARGV);

	my $Options = "B:dfF:g:G:hlO:o:t:u:U:SR:Z";
	if (! getopts ($Options)) {
		print STDERR "$0: Incorrect usage.\n";
		print STDERR $Usage;
	exit (1);
	}
	if (defined $opt_h) {
		print $Usage;
		exit (0);
	}

	#RFS: neednot input arguments
	$opt_o = "test";
	$opt_f = 1;
	$opt_t = 0;
	#$opt_F = 'RFSNN'; # advfs or netapp

	if (defined $opt_B) {
		$UseClient = ($opt_B =~ /C/);
		$UseFH = ($opt_B =~ /F/);
		$UseUID = ($opt_B =~ /U/);
		$UseGID = ($opt_B =~ /G/);
	}

	if (defined $opt_o) {
		$OutFileBaseName = $opt_o;
	}

	if (defined $opt_O) {
		if ($opt_O =~ /^\+(.*)/) {
			@OPLIST = (@OPLIST, split (/,/, $1));
		}
		else {
			@OPLIST = ('TOTAL', 'INTERESTING', split (/,/, $opt_O));
		}
		# Error checking?
	}

	if (defined $opt_l) {
		$DO_LATENCY = 1;
	}

	if (defined $opt_t) {
		if ($INTERVAL =~ /([0-9]*)([smhd])/) {
			my $n = $1;
			my $unit = $2;

			if ($unit eq 's') {
				$INTERVAL = $opt_t;
			}
			elsif ($unit eq 'm') {
				$INTERVAL = $opt_t * 60;
			}
			elsif ($unit eq 'h') {
				$INTERVAL = $opt_t * 60 * 60;
			}
			elsif ($unit eq 'd') {
				$INTERVAL = $opt_t * 24 * 60 * 60;
			}
		}
		else {
			$INTERVAL = $opt_t;
		}
	}

	$DO_PATHS = (defined $opt_d);
	$DO_FILES = (defined $opt_f);
	$DO_SQUEEZE = (defined $opt_S);
	$OMIT_ZEROS = (defined $opt_Z);

	$TIME_ROUNDING = (defined $opt_R) ? $opt_R : 0;

	if (defined $opt_F) {
		$FH_TYPE = $opt_F;
	}

	if (defined $opt_c) {
		@ADD_CLIENTS = split (/,/, $opt_c);
	}
	if (defined $opt_C) {
		@DEL_CLIENTS = split (/,/, $opt_c);
	}

	if (defined $opt_g) {
		@ADD_GROUPS = groups2gids (split (/,/, $opt_g));
	}
	if (defined $opt_G) {
		@DEL_GROUPS = groups2gids (split (/,/, $opt_G));
	}

	if (defined $opt_u) {
		@ADD_USERS = logins2uids (split (/,/, $opt_u));
	}
	if (defined $opt_U) {
		@DEL_USERS = logins2uids (split (/,/, $opt_U));
	}


	# Now that we know what options the user asked for, initialize
	# things accordingly.

	if ($DO_PATHS || $DO_FILES) {
		$UseFH = 1;
	}

	if ($DO_LATENCY) {
		latency::init (@OPLIST);
	}

	if ($DO_COUNTS) {
		counts::init (@OPLIST);
	}

	if (defined $OutFileBaseName) {
		if ($DO_COUNTS) {
			open (OUT_COUNTS, ">$OutFileBaseName.cnt") ||
				die "Can't create $OutFileBaseName.cnt.";
			print OUT_COUNTS "#cmdline $cmdline\n";
		}
		if ($DO_LATENCY) {
			open (OUT_LATENCY, ">$OutFileBaseName.lat") ||
				die "Can't create $OutFileBaseName.lat.";
			print OUT_LATENCY "#cmdline $cmdline\n";
		}
		if ($DO_FILES) {
			open (OUT_FILES, ">$OutFileBaseName.fil") ||
				die "Can't create $OutFileBaseName.fil.";
			print OUT_FILES "#cmdline $cmdline\n";
		}
		if ($DO_PATHS) {
			open (OUT_PATHS, ">$OutFileBaseName.pat") ||
				die "Can't create $OutFileBaseName.pat.";
			print OUT_PATHS "#cmdline $cmdline\n";
		}
	}
	else {
		*OUT_COUNTS = STDOUT;
		*OUT_LATENCY = STDOUT;
		*OUT_FILES = STDOUT;
		*OUT_PATHS = STDOUT;

		print STDOUT "#cmdline $cmdline\n";
	}

	foreach my $op ( @OPLIST ) {
		$OPARRAY{$op} = 1;
	}

	return ;
}

sub readTrace {
	my (@args) = @_;

	while (my $line = <>) {

		$hier::rfsLineNum++;
		if ( ($hier::rfsLineNum % 1000) eq 0) {
			print STDERR "$hier::rfsLineNum\n";
		}


		if ($line =~ /SHORT\ PACKET/) {
			next;
		}

		my ($proto, $op, $xid, $client, $now, $response) =
				nfsd::nfsDumpParseLineHeader ($line);
		$NOW = $now;

		# NOTE:  This next bit of logic requires a little
		# extra attention.  We want to discard lines as
		# quickly as we can if they're not "interesting". 
		# However, different lines are interesting in
		# different contexts, so the order of the tests and
		# the manner in which they are interspersed with
		# subroutine calls to pluck info from the lines is
		# very important.

		# Check whether it is a line that we should prune and
		# ignore, because of the filters.
		
		next if (($op eq 'C3' || $op eq 'C2') &&
				! pruneCall ($line, $client));

		if ($DO_PATHS || $DO_FILES) {
			hier::processLine ($line,
					$proto, $op, $xid, $client,
					$now, $response, $FH_TYPE);
		}

		my $key = key::makeKey ($line, $proto, $op,
				$xid, $client, $now,
				$UseClient, $UseFH, $UseUID, $UseGID,
				$FH_TYPE);
		if (! defined $key) {
			next ;
		}
		$KeysSeen{$key} = 1;

		# Count everything towards the total, but only
		# do the rest of the processing for things
		# that are "interesting".

		if ($proto eq 'C3' || $proto eq 'C2') {
			$counts::OpCounts{"$key,TOTAL"}++;
			$counts::KeysSeen{$key} = 1;

			next if (! exists $OPARRAY{$op});

			$counts::OpCounts{"$key,$op"}++;
			$counts::OpCounts{"$key,INTERESTING"}++;
		}

		if ($op eq 'read' && exists $OPARRAY{'readM'}) {
			doReadSize ($line, $proto, $op, $key, $client, $xid, $response, $now);
		}

		if ($op eq 'write' && exists $OPARRAY{'writeM'}) {
			doWriteSize ($line, $proto, $op, $key, $client, $xid, $response, $now);
		}

		if ($DO_LATENCY) {
			latency::update ($key, $proto, $op,
					$xid, $client, $now);
		}

		if ($END_TIME < 0) {
			$START_TIME = findStartTime ($NOW, $TIME_ROUNDING);
			$END_TIME = $START_TIME + $INTERVAL;
		}

		# Note that this is a loop, because if the interval is
		# short enough, or the system is very idle (or there's
		# a filter in place that makes it look idle), entire
		# intervals can go by without anything happening at
		# all.  Some tools can get confused if intervals are
		# missing from the table, so we emit them anyway.

		while (($INTERVAL > 0) && ($NOW >= $END_TIME)) {
			printAll ($START_TIME);

			counts::resetOpCounts ();
			latency::resetOpCounts ();

			$START_TIME += $INTERVAL;
			$END_TIME = $START_TIME + $INTERVAL;
		}

		if ($now > $nextPruneTime) {
			key::prunePending ($now - $PRUNE_INTERVAL);
			latency::prunePending ($now - $PRUNE_INTERVAL);

			prunePending ($now - $PRUNE_INTERVAL);

			$nextPruneTime = $now + $PRUNE_INTERVAL;
		}
	}

	# Squeeze out the last little bit, if there's anything that we
	# counted but did not emit.  If DO_SQUEEZE is true, then
	# always do this.  Otherwise, only squeeze out the results of
	# the last interval if the interval is "almost" complete (ie
	# within 10 seconds of the end).

	if (($NOW > $START_TIME) && ($DO_SQUEEZE || (($END_TIME - $NOW) < 10))) {
		printAll ($START_TIME);
		counts::resetOpCounts ();
	}

	print "#T endtime = $NOW\n";

}

sub printAll {
	my ($start_time) = @_;

	if ($DO_COUNTS) {
		counts::printOps ($start_time, *OUT_COUNTS);
	}

	if ($DO_LATENCY) {
		latency::printOps ($start_time, *OUT_LATENCY);
	}

	if ($DO_FILES) {
		hier::printAll ($start_time, *OUT_FILES);
	}

	if ($DO_PATHS) {
		printPaths ($start_time, *OUT_PATHS);
	}
}

sub pruneCall {
	my ($line, $client) = @_;

	if (@ADD_USERS > 0 || @DEL_USERS > 0) {
		my $c_uid = nfsd::nfsDumpParseLineField ($line, 'euid');
		if (! defined ($c_uid)) {
			return 0;
		}
		$c_uid = hex ($c_uid);

		if (@ADD_USERS && !grep (/^$c_uid$/, @ADD_USERS)) {
			return 0;
		}
		if (@DEL_USERS && grep (/^$c_uid$/, @DEL_USERS)) {
			return 0;
		}
	}

	if (@ADD_GROUPS > 0 || @DEL_GROUPS > 0) {
		my $c_gid = nfsd::nfsDumpParseLineField ($line, 'egid');
		if (! defined ($c_gid)) {
			return 0;
		}
		$c_gid = hex ($c_gid);

		if (@ADD_GROUPS && !grep (/^$c_gid$/, @ADD_GROUPS)) {
			return 0;
		}
		if (@DEL_GROUPS && grep (/^$c_gid$/, @DEL_GROUPS)) {
			return 0;
		}
	}

	if (@ADD_CLIENTS > 0 || @DEL_CLIENTS > 0) {
		if (@ADD_CLIENTS && !grep (/^$client$/, @ADD_CLIENTS)) {
			return 0;
		}
		if (@DEL_CLIENTS && grep (/^$client$/, @DEL_CLIENTS)) {
			return 0;
		}
	}

	return 1;
}

%PathOpCounts	= ();
%PathsSeen	= ();

sub buildDirPath {
	my ($fh, $key) = @_;
	my $pfh;
	my $cnt;

	foreach my $op ( @OPLIST ) {
		if (exists $counts::OpCounts{"$key,$op"}) {
			$cnt = $counts::OpCounts{"$key,$op"};
		}
		else {
			$cnt = 0;
		}
		$PathOpCounts{"$fh,$op"} = $cnt;

		$PathsSeen{$fh} = 1;

		$pfh = $fh;
		my $len = 0;
		while (defined ($pfh = $hier::fh2Parent{$pfh})) {

			if ($len++ > 20) {
				print "Really long path ($fh)\n";
				last;
			}

			if (exists $PathOpCounts{"$pfh,$op"}) {
				$PathOpCounts{"$pfh,$op"} += $cnt;
			}
			else {
				$PathOpCounts{"$pfh,$op"} = $cnt;
			}
			$PathsSeen{$pfh} = 1;
		}
	}

	return ;
}

sub printPaths {
	my ($start_time, $out) = @_;

	my $str = "#D time Dir/File dircnt path fh";
	foreach my $op ( @OPLIST ) {
		$str .= " $op";
	}
	$str .= "\n";

	print $out $str;

	undef %PathsSeen;

	foreach my $key ( keys %KeysSeen ) {
		my ($client_id, $fh, $euid, $egid) = split (/,/, $key);

		buildDirPath ($fh, $key);
	}

	foreach my $fh ( keys %PathsSeen ) {
		my ($path, $cnt) = hier::findPath ($fh);

		if ($cnt == 0) {
			$path = ".";
		}

		my $type = (exists $hier::fhIsDir{$fh} && $hier::fhIsDir{$fh}==2) ? 'D' : 'F';

		my $str = "$cnt $type $path $fh ";

		foreach my $op ( @OPLIST ) {
			my $cnt;

			if (exists $PathOpCounts{"$fh,$op"}) {
				$cnt = $PathOpCounts{"$fh,$op"};
			}
			else {
				print "Missing $fh $op\n";
				$cnt = 0;
			}

			$str .= " $cnt";

			$PathOpCounts{"$fh,$op"} = 0;	# &&& reset
		}

		print $out "D $start_time $str\n";
	}
}

%uxid2key	= ();
%uxid2time	= ();

sub doReadSize {
	my ($line, $proto, $op, $key, $client, $xid, $response, $time) = @_;

	my $uxid = "$client-$xid";

	if ($proto eq 'C3' || $proto eq 'C2') {
		$uxid2time{$uxid} = $time;
		$uxid2key{$uxid} = $key;
	}
	else {
		if (! exists $uxid2key{$uxid}) {
			return ;
		}
		if ($response ne 'OK') {
			return ;
		}

		$key = $uxid2key{$uxid};
		my $count = nfsd::nfsDumpParseLineField ($line, 'count');
		$count = hex ($count);

		delete $uxid2key{$uxid};
		delete $uxid2time{$uxid};

		$counts::OpCounts{"$key,readM"} += $count;
	}
}

# Note that we always just assume that writes succeed, because on most
# systems they virtually always do.  If you're tracing a system where
# your users are constantly filling up the disk or exceeding their
# quotas, then you will need to fix this.

sub doWriteSize {
	my ($line, $proto, $op, $key, $client, $xid, $response, $time) = @_;

	if ($proto eq 'C3' || $proto eq 'C2') {

		my $tag = ($proto eq 'C3') ? 'count' : 'tcount';

		my $count = nfsd::nfsDumpParseLineField ($line, $tag);

		if (! $count) {
			printf "WEIRD count $line\n";
		}

		$count = hex ($count);

		$counts::OpCounts{"$key,writeM"} += $count;
	}
}


# Purge all the pending XID records dated earlier than $when (which is
# typically at least $PRUNE_INTERVAL seconds ago).  This is important
# because otherwise missing XID records can pile up, eating a lot of
# memory. 
  
sub prunePending {
	my ($when) = @_;

	foreach my $uxid ( keys %uxid2time ) {
		if ($uxid2time{$uxid} < $when) {
			delete $uxid2key{$uxid};
			delete $uxid2time{$uxid};
		}
	}

	return ;
}

