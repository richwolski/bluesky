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
# $Id: ns_split,v 1.4 2003/07/28 14:27:17 ellard Exp $

use Getopt::Std;

$ProgDir = $0;
$ProgDir =~ /(^.*)\//;
$ProgDir = $1;
if (!$ProgDir) {
	$ProgDir = ".";
}

require "$ProgDir/userUtils.pl";

@ADD_USERS	= ();
@DEL_USERS	= ();
@ADD_GROUPS	= ();
@DEL_GROUPS	= ();
@ADD_CLIENTS	= ();
@DEL_CLIENTS	= ();

$Usage =<< ".";

Usage: $0 [options] [table1.ns [table2.ns ... ]]

If no table files are specified, then the input is read from stdin.

Command line options:

-h		Print usage message and exit.

-c c1[,c2]*	Include only activity performed by the specified clients.

-C c1[,c2]*	Exclude activity performed by the specified clients.

-g g1[,g2]*	Include only activity performed by the specified groups.

-G g1[,g2]*	Exclude activity performed by the specified groups.

-u u1[,u2]*	Include only activity performed by the specified users.

-U u1[,u2]*	Exclude activity performed by the specified users.

.

$cmdline = "$0 " . join (' ', @ARGV);
$Options = "c:C:g:G:u:U::";
if (! getopts ($Options)) {
	print STDERR "$0: Incorrect usage.\n";
	print STDERR $Usage;
exit (1);
}
if (defined $opt_h) {
	print $Usage;
	exit (0);
}

if (defined $opt_u) {
	@ADD_USERS = &logins2uids ($opt_u);
}
if (defined $opt_U) {
	@DEL_USERS = &logins2uids ($opt_U);
}
if (defined $opt_g) {
	@ADD_GROUPS = &groups2gids ($opt_g);
}
if (defined $opt_G) {
	@DEL_GROUPS = &groups2gids ($opt_G);
}
if (defined $opt_c) {
	@ADD_CLIENTS = split (/,/, $opt_c);
}
if (defined $opt_C) {
	@DEL_CLIENTS = split (/,/, $opt_C);
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

	if (saveRecord ($client, $fh, $euid, $egid)) {
		print $l;
	}
}

sub saveRecord {
	my ($client, $fh, $euid, $egid) = @_;

	if (@ADD_CLIENTS && !grep (/^$client$/, @ADD_CLIENTS)) {
		return 0;
	}
	if (@DEL_CLIENTS && grep (/^$client$/, @DEL_CLIENTS)) {
		return 0;
	}

	if (@ADD_USERS && !grep (/^$euid$/, @ADD_USERS)) {
		return 0;
	}
	if (@DEL_USERS && grep (/^$euid$/, @DEL_USERS)) {
		return 0;
	}

	if (@ADD_GROUPS && !grep (/^$egid$/, @ADD_GROUPS)) {
		return 0;
	}
	if (@DEL_GROUPS && grep (/^$egid$/, @DEL_GROUPS)) {
		return 0;
	}

	return (1);
}

exit 0;
