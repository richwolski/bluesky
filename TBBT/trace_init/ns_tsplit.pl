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
# $Id: ns_tsplit,v 1.4 2003/07/26 20:52:04 ellard Exp $

use Getopt::Std;
use Time::Local;

$ProgDir = $0;
$ProgDir =~ /(^.*)\//;
$ProgDir = $1;
if (!$ProgDir) {
	$ProgDir = ".";
}

# Set the default start and end times.

$DEF_BEGIN_TIME	= "20000101:00:00:00";
$DEF_END_TIME	= "20360101:00:00:00";

# The range includes everything from the start time up to and less
# than the end time (so if you want one day, it is correct to go
# midnight to midnight, instead of going until 23:59:59).

$Usage =<< ".";

Usage: $0 [options] rangeSpec... - [table.ns [table.ns...]]

Command line options:

-h		Print usage message and exit.

Any number of time range specifier can be provided.  The special
symbol "-" is used to separated the time specifiers from the input
file names.  If input is taken from stdin, then the "-" may be
omitted.

The basic time range specification format is:  StartTime-EndTime where
StartTime and EndTime have the form YYMMDD[:HH[:MM[:SS]]].  All
records from the input that have a timestamp greater than or equal to
StartTime and less than EndTime are printed to stdout.

If StartTime is omitted, $DEF_BEGIN_TIME is used.
If EndTime is omitted, $DEF_END_TIME is used.

Note that omitting both the StartTime and EndTime results in a rangeSpec
of "-", which is the special symbol that marks the end of the rangeSpecs.
.

$cmdline = "$0 " . join (' ', @ARGV);
$Options = "h";
if (! getopts ($Options)) {
	print STDERR "$0: Incorrect usage.\n";
	print STDERR $Usage;
	exit (1);
}
if (defined $opt_h) {
	print $Usage;
	exit (0);
}

@StartTimes	= ();
@EndTimes	= ();

while ($ts = shift @ARGV) {
	last if ($ts eq '-');

	my ($ts_s, $ts_e) = split (/-/, $ts);
	my $s;

	if ($ts_s eq '') {
		$ts_s = $DEF_START_TIME;
	}
	$s = &ts2secs ($ts_s);
	if (! defined $s) {
		print STDERR "Failed to translate ($ts_s)\n";
		exit (1);
	}
	push @StartTimes, $s;

	if ($ts_e eq '') {
		$ts_e = $DEF_END_TIME;
	}
	$s = &ts2secs ($ts_e);
	if (! defined $s) {
		print STDERR "Failed to translate ($ts_e)\n";
		exit (1);
	}

	push @EndTimes, $s;
}

print "#cmdline $cmdline\n";

while ($l = <>) {
	if ($l =~ /^#/) {
		print $l;
		next;
	}

	my ($type, $time, $client, $fh, $euid, $egid, @vals) = split (' ', $l);

	next unless ($type eq 'C');

	# Something wrong with the input?

	next if (@vals == 0);

	if (saveRecord ($time)) {
		print $l;
	}
}

exit 0;

sub saveRecord {
	my ($t) = @_;

	for (my $i = 0; $i < @StartTimes ; $i++) {
		if (($StartTimes[$i] <= $t) && ($t < $EndTimes[$i])) {
			return (1);
		}
	}

	return (0);
}

# This is ugly!
#
# The basic time specification format is:  YYMMDD[:HH[:MM[:SS]]].
#
# To make a time range, join two time specs with a -.
#
# The data spec is Y2.01K compliant...  I'm assuming that all the
# traces processed by this tool will be gathered before 2010.  If
# anyone is still gathering NFSv3 traces in 2010, I'm sorry.

sub ts2secs {
	my ($ts) = @_;
	my ($hour, $min, $sec) = (0, 0, 0);

	my (@d) = split (/:/, $ts);

	if ($d[0] =~ /([0-9][0-9])([0-2][0-9])([0-3][0-9])/) {
		$year = $1 + 100;
		$mon = $2 - 1;
		$day = $3;
	}
	else {
		return undef;
	}

	if (@d > 1) {
		if ($d[1] =~ /([0-2][0-9])/) {
			$hour = $1;
		}
		else {
			return undef;
		}
	}

	if (@d > 2) {
		if ($d[2] =~ /([0-5][0-9])/) {
			$min = $1;
		}
		else {
			return undef;
		}
	}

	if (@d > 3) {
		if ($d[3] =~ /([0-5][0-9])/) {
			$sec = $1;
		}
		else {
			return undef;
		}
	}

	my $s = timelocal ($sec, $min, $hour, $day, $mon, $year,
			undef, undef, undef);

	my ($s2, $m2, $h2, $md2, $mon2, $y2, $isdst) = localtime ($s);
	if (($s2 != $sec) || $min != $m2 || $hour != $h2 || $day != $md2) {
		print "failed 1!\n";
	}
	if (($mon != $mon2) || $year != $y2) {
		print "failed 2!\n";
	}

	return ($s);
}

