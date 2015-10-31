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
# $Id: ns_timeagg,v 1.7 2003/07/28 14:27:17 ellard Exp $

use Getopt::Std;

$ProgDir = $0;
$ProgDir =~ /(^.*)\//;
$ProgDir = $1;
if (!$ProgDir) {
	$ProgDir = ".";
}

require "$ProgDir/common.pl";

$UseClient	= 0;
$UseFH		= 0;
$UseUID		= 0;
$UseGID		= 0;
$NEW_INTERVAL	= 300;
$OMIT_ZEROS	= 0;

$Usage =<< ".";

Usage: $0 [options] [table1.ns [table2.ns ... ]]

If no table files are specified, then the input is read from stdin.

Command line options:

-h		Print usage message and exit.

-B flags	Choose fields to NOT aggregate.  The fields are:
		C - client host ID
		U - Effective user ID
		G - Effective group ID
		F - File handle

-R secs		Round the start time to the closest multiple of
		the given number of seconds.  This is useful for
		dealing with small amounts of clock drift.  For example,
		if the trace starts at 12:00:01 instead of 12:00:00,
		it is probably convenient to pretend that it started
		at 12:00:00.

-t secs		Aggregate over the given number of seconds.   The default
		is $NEW_INTERVAL.  If the current interval does not
		divide the new interval, warning messages are printed
		and the output might not be meaningful.

		If secs is 0, then the total for the entire table is
		computed.

-Z		Omit count lines that have a zero total operation count.
.

$cmdline = "$0 " . join (' ', @ARGV);
$Options = "B:hR:t:Z";
if (! getopts ($Options)) {
	print STDERR "$0: Incorrect usage.\n";
	print STDERR $Usage;
	exit (1);
}
if (defined $opt_h) {
	print $Usage;
	exit (0);
}

if (defined $opt_t) {
	if ($opt_t <= 0) {
		$EndTime = 0;
	}
	$NEW_INTERVAL	= $opt_t;
}

$TIME_ROUNDING	= defined $opt_R ? $opt_R : 0;

if (defined $opt_B) {
	$UseClient = ($opt_B =~ /C/);
	$UseUID = ($opt_B =~ /U/);
	$UseGID	 = ($opt_B =~ /G/);
	$UseFH = ($opt_B =~ /F/);
}

# We need to let the measurement of time be a little bit inexact, so
# that a small rounding error or truncation doesn't throw everything
# off.

$TimerInacc = 0.05;

# Print out the commandline as a comment in the output.

print "#cmdline $cmdline\n";

# It would be nice to make this do the right thing with Latency lines,
# and perhaps file op counts.

while ($l = <>) {
	if ($l =~ /^#C/) {
		print $l;
		next;
	}

	next if ($l =~ /^#/);

	my ($type, $time, $client, $fh, $euid, $egid, @vals) = split (' ', $l);

	next unless ($type eq 'C');

	# Something wrong with the input?

	next if (@vals == 0);

	if (!defined $StartTime) {
		$StartTime = findStartTime ($time, $TIME_ROUNDING);
		if ($NEW_INTERVAL > 0) {
			$EndTime = $StartTime + $NEW_INTERVAL - $TimerInacc;
		}
	}

	if ($EndTime > 0 && $time >= $EndTime) {
		my $diff = int ($time - $StartTime);
		if ($diff > 0) {
			if ($NEW_INTERVAL % $diff != 0) {
				print STDERR "$0: time interval mismatch ";
				print STDERR "(from $diff to $NEW_INTERVAL)\n";
			}
		}

		dumpKeys ($StartTime);

		$StartTime += $NEW_INTERVAL;
		$EndTime = $StartTime + $NEW_INTERVAL - $TimerInacc;
	}


	# Aggregate across clients, files, users, and groups unless
	# specifically asked NOT to.

	$client = $UseClient ? $client : 'u';
	$fh = $UseFH ? $fh : 'u';
	$euid = $UseUID ? $euid : 'u';
	$egid = $UseGID	 ? $egid : 'u';

	$key = "$client,$fh,$euid,$egid";

	if (exists $Totals{$key}) {
		my (@tots) = split (' ', $Totals{$key});
		for (my $i = 0; $i < @vals; $i++) {
			$tots[$i] += $vals[$i];
		}
		$Totals{$key} = join (' ', @tots);
	}
	else {
		$Totals{$key} = join (' ', @vals);
	}
}

if ($EndTime <= 0) {
	dumpKeys ($StartTime);
}

sub dumpKeys {
	my ($start) = @_;
	my ($i, $k, $ks);

	foreach $k ( keys %Totals ) {
		my (@v) = split (' ', $Totals{$k});

		if ($OMIT_ZEROS && $v[0] == 0) {
			next;
		}

		$ks = $k;
		$ks =~ s/,/\ /g;

		print "C $start $ks";

		for ($i = 0; $i < @v; $i++) {
			print " $v[$i]";
		}
		print "\n";

		for ($i = 0; $i < @v; $i++) {
			$v[$i] = 0;
		}
		$Totals{$k} = join (' ', @v);
	}
}

exit 0;
