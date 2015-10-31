#!/usr/bin/perl
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
# $Id: ns_loc2gmt,v 1.6 2003/07/28 14:27:16 ellard Exp $
#
# A helper application used by ns_quickview to convert from local time
# to GMT.  nfsdump records time as localtime, but gnuplot only know
# how to deal with dates expressed as Greenwich Mean Time (which it
# then displays using the local time, for some reason).
#
# This is a fragile tool -- it must be run in the same time zone in
# which the data was collected via nfsdump, or else it will not do the
# proper conversion.  Improvements welcomed!
#
# The 'C' (count) and 'L' (latency) records use the second column for
# dates, expressed as seconds.microseconds in localtime.  The seconds
# portion is the only part of the data modified by this program. 
# Comment lines are passed through unaltered.
#
# There is no error checking.  Garbage in, garbage out.
#
# Note - we're throwing the microseconds away.


use Getopt::Std;
require 'timelocal.pl';

$Usage =<< ".";

Usage: $0 [options] [table1.ns [table2.ns ... ]]

If no table files are specified, then the input is read from stdin.

Command line options:

-h		Print usage message and exit.

-d secs		Time offset, in seconds, to subtract from each time
		in the input tables.  Note that all times are rounded
		down to the nearest second.

IMPORTANT NOTE:  this must be run in the same time zone in which the
data was collected via nfsdump, or else it will not do the proper
conversion unless the -d option is used.

.

$Options = "d:";
if (! getopts ($Options)) {
	print STDERR "$0: Incorrect usage.\n";
	print STDERR $Usage;
	exit (1);
}
if (defined $opt_h) {
	print $Usage;
	exit (0);
}

$TimeOffset = 0;
if (defined $opt_d) {
	$TimeOffset = int ($opt_d);
}

while ($line = <>) {
	if ($line =~ /^#/) {
		print $line;
		next;
	}

	@arr = split (' ', $line);

	($secs, $usec) = split (/\./, $arr[1]);
	$secs -= $TimeOffset;

	$arr[1] = &timegm (localtime ($secs));

	print join (' ', @arr);
	print "\n";
}

exit (0);

