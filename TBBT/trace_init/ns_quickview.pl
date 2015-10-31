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
# $Id: ns_quickview,v 1.9 2003/07/28 14:27:17 ellard Exp $

use Getopt::Std;

$ProgDir = $0;
$ProgDir =~ /(^.*)\//;
$ProgDir = $1;
if (!$ProgDir) {
	$ProgDir = ".";
}

$GNUPLOT_PATH	= "gnuplot";
$GNUPLOT_OPTS	= "-persist";
$LOC2GMT_PATH	= "$ProgDir/ns_loc2gmt";

$OUTPUT		= "qv.ps";
$GP_OUTPUT	= "qv.gp";
$TERMINAL	= "postscript";
$SIZE		= undef;
$EXTRA_CMD	= undef;
$TITLE_TEXT	= undef;
$PLOT_OPTS	= '';
@OPS		= ( 7 );
@SCALES		= ();

$Usage =<< ".";

Usage: $0 [options] table1.ns [table2.ns ...]

At least one table file must be specified.

Command line options:

-h		Print usage message and exit.

-c cmd		Pass an extra command directly to gnuplot.

-g		Save the gnuplot commands to $GP_OUTPUT instead
		of executing the immediately.

-k		Show the key for each line on the plot.  The default
		is not to show the key.

-l		If more than one input file is specified, superimpose
		the plot for each so that they all appear to begin at
		the same time as the first row in the first file.

-o filename	Save the plot to the specified file.  The default
		output file is \"$OUTPUT\".

-O op1,...,opN	Plot the specified columns from the table.  The
		default is $OPS[0], the total operation count.
		Note that the operations must be specified by column
		number, not by name.

-t terminal	Create a plot for the given gnuplot terminal specification.
		The default is \"$TERMINAL\".  See the gnuplot
		documentation for more information.

-T string	Use the specified string as the title for the plot.
		The default is no title.

-s sizespec	Set the output size specifier.  See the gnuplot
		documentation for more information.

-S s1,..,sN	Set the scaling factor for each file.  This is useful
		for plotting files aggregated across different time
		intervals on the same scale.

.

$Options = "c:ghklo:O:P:t:T:s:S:";
if (! getopts ($Options)) {
	print STDERR "$0: Incorrect usage.\n";
	print STDERR $Usage;
	exit (1);
}
if (defined $opt_h) {
	print $Usage;
	exit (0);
}

$EXTRA_CMD	= $opt_c	if (defined $opt_c);
$OUTPUT		= $opt_o	if (defined $opt_o);
$TERMINAL	= $opt_t	if (defined $opt_t);
$SIZE		= $opt_s	if (defined $opt_s);
$TITLE_TEXT	= $opt_T	if (defined $opt_T);
$PLOT_OPTS	= $opt_P	if (defined $opt_P);
@OPS	= split (/,/, $opt_O)	if (defined $opt_O);
@SCALES	= split (/,/, $opt_S)	if (defined $opt_S);

$SAVE_GNUPLOT	= defined $opt_g;
$SHOW_KEY	= defined $opt_k;
$LAYER		= defined $opt_l;

@FILES = @ARGV;

if (@FILES == 0) {
	print STDERR "No tables to plot?\n";
	exit (1);
}

&makePlot;
exit (0);

sub makePlot {
	my @starts = ();
	my @ends = ();

	my $nfiles = @FILES;
	my $nops = @OPS;

	if ($SAVE_GNUPLOT) {
		die unless open (PLOT, ">$GP_OUTPUT");
	}
	else {
		die unless open (PLOT, "|$GNUPLOT_PATH $GNUPLOT_OPTS");
	}

	$preamble = `cat $ProgDir/template.gp`;
	print PLOT $preamble;

	print PLOT "set terminal $TERMINAL\n";
	print PLOT "set output \'$OUTPUT\'\n";
	if (defined $SIZE) {
		print PLOT "set size $SIZE\n";
	}

	if (defined $TITLE_TEXT) {
		print PLOT "set title \"$TITLE_TEXT\"\n";
	}

	if (defined $EXTRA_CMD) {
		print PLOT "$EXTRA_CMD\n";
	}

	print PLOT "set key below\n";

	if ($LAYER) {
		for ($i = 0; $i < $nfiles; $i++) {
			($starts [$i], $ends [$i]) =
					findFileTimeSpan ($FILES [$i]);
		}
	}

	print PLOT "plot $PLOT_OPTS \\\n";
	for ($i = 0; $i < $nfiles; $i++) {
		my $offset = 0;
		my $scale = 1;
		if (defined $SCALES[$i]) {
			$scale = $SCALES[$i];
		}

		my $file = $FILES [$i];

		if ($LAYER) {
			$offset = $starts [$i] - $starts [0];
		}

		for ($j = 0; $j < @OPS; $j++) {
			my $op = $OPS[$j];

			print PLOT "\t\"< $LOC2GMT_PATH -d $offset $file\" \\\n";
			print PLOT "\t\tusing 2:(\$$op*$scale) \\\n";
			if ($SHOW_KEY) {
				print PLOT "\t\ttitle \'$file:$op\'\\\n";
			}
			print PLOT "\t\twith lines lw 2";
			if ($i != $nfiles - 1 || $j != $nops - 1) {
				print PLOT ",\\\n";
			}
			else {
				print PLOT "\n";
			}
		}
	}
	print PLOT "\n";
	close PLOT;
}

# Find the earliest and latest times in a file created by nfsscan, and
# return them as a list.  Returns the empty list if anything goes
# wrong.

sub findFileTimeSpan {
	my ($fname) = @_;
	my ($smallest, $largest) = (-1, -1);

	if (! open (FF, "<$fname")) {
		return ();
	}

	while (my $l = <FF>) {
		if ($l =~ /^#/) {
			next;
		}

		@a = split (' ', $l);

		if ($smallest < 0 || $smallest > $a[1]) {
			$smallest = $a[1];
		}
		if ($largest < 0 || $largest < $a[1]) {
			$largest = $a[1];
		}
	}

	close FF;

	if ((! defined $smallest) || (! defined $largest)) {
		return ();
	}

	return ($smallest, $largest);
}

