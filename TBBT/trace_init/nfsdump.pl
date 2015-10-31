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
# $Id: nfsdump.pl,v 1.5 2003/07/26 20:52:04 ellard Exp $
#
# Utility for dealing with raw nfsdump records.

package nfsd;

# If $AllowRisky is set, then allow some optimizations that might be
# "risky" in bizarre situations (but have never been observed to
# actually break anything).  By default, no riskiness is permitted.

$AllowRisky	= 0;

# nfsDumpParseLine -- initializes the global associative array
# %nfsd'nfsDumpLine with information about a record from nfsdump. 
# Returns an empty list if anything goes wrong.  Otherwise, returns a
# list of the protocol (eg R2, C2, R3, C3), the name of the operation,
# and the xid, the client host ID, the time, and for responses, the
# status (via nfsDumpParseLineHeader).  The reason for this particular
# return list is that these are very frequently-accessed values, so it
# can save time to avoid going through the associative array to access
# them.
#
# All records begin with several fixed fields, and then are followed
# by some number of name/value pairs, and finally some diagnostic
# fields (which are mostly ignored by this routine-- the only
# diagnostic this routine cares about is whether the packet as part of
# a jumbo packet or not.  If so, then 'truncated' is set.)

sub nfsDumpParseLine {
	my ($line, $total) = @_;

	my (@rl) = &nfsDumpParseLineHeader ($line);

	if (@rl && $total) {
		&nfsDumpParseLineBody ($line);
	}

	return @rl;
}

sub nfsDumpParseLineBody {
	my ($line) = @_;
	my $i;
	my $client_id;
	my $reseen;

	undef %nfsDumpLine;

	# If the line doesn't start with a digit, then it's certainly
	# not properly formed, so bail out immediately.

	if (! ($line =~ /^[0-9]/)) {
		return undef;
	}

	my @l = split (' ', $line);
	my $lineLen = @l;
	if ($l[$lineLen - 1] eq 'LONGPKT') {
		splice (@l, $lineLen - 1);
		$nfsDumpLine{'truncated'} = 1;
		$lineLen--;
	}

	$nfsDumpLine{'time'}	= $l[0];
	$nfsDumpLine{'srchost'}	= $l[1];
	$nfsDumpLine{'deshost'}	= $l[2];
	$nfsDumpLine{'proto'}	= $l[4];
	$nfsDumpLine{'xid'}	= $l[5];
	$nfsDumpLine{'opcode'}	= $l[6];
	$nfsDumpLine{'opname'}	= $l[7];

	if (($l[4] eq 'R3') || ($l[4] eq 'R2')) {
		$nfsDumpLine{'status'}	= $l[8];

		$client_id = $l[2];
		$reseen = 0;
		for ($i = 9; $i < $lineLen - 10; $i += 2) {
			if (defined $nfsDumpLine{$l[$i]}) {
				$reseen = 1;
				$nfsDumpLine{"$l[$i]-2"} = $l[$i + 1];
			}
			else {
				$nfsDumpLine{$l[$i]} = $l[$i + 1];
			}
		}
	}
	else {
		$client_id = $l[1];
		$reseen = 0;
		for ($i = 8; $i < $lineLen - 6; $i += 2) {
			if (defined $nfsDumpLine{$l[$i]}) {
				$nfsDumpLine{"$l[$i]-2"} = $l[$i + 1];
				$reseen = 1;
			}
			else {
				$nfsDumpLine{$l[$i]} = $l[$i + 1];
			}
		}
	}
}

# Returns an empty list if anything goes wrong.  Otherwise, returns a
# list of the protocol (eg R2, C2, R3, C3), the name of the operation,
# and the xid, the client host ID, and time, and the response status. 
# (For call messages, the status is 'na'.)

sub nfsDumpParseLineHeader {
	my ($line) = @_;

	# If the line doesn't start with a digit, then it's certainly
	# not properly formed, so bail out immediately.

	if (! ($line =~ /^[0-9]/)) {
		return ();
	}
	else {
		my $client_id;
		my $status;

		my @l = split (' ', $line, 10);


		if (($l[4] eq 'R3') || ($l[4] eq 'R2')) {
			$client_id = $l[2];
			$status = $l[8];
		}
		else {
			$client_id = $l[1];
			$status = 'na';
		}

		return ($l[4], $l[7], $l[5], $client_id, $l[0], $status);
	}
}

# nfsDumpParseLineFields -- Just return a subset of the fields,
# without parsing the entire line.

sub nfsDumpParseLineFields {
	my ($line, @fields) = @_;
	my $i;

	# If the line doesn't start with a digit, then
	# it's certainly not properly formed, so bail out
	# immediately.

	if (! ($line =~ /^[0-9]/)) {
		return ();
	}

	my $rest;
	if ($AllowRisky) {
		$rest = $line;
	}
	else {
		my @foo = split (' ', $line, 9);
		$rest = ' ' . $foo[8];
	}

	my $fl = @fields;
	my @l = ();
	for ($i = 0; $i < $fl; $i++) {
		my $field = $fields[$i];

		$rest =~ /\ $field\ +([^\ ]+)/;
		$l[$i] = $1;
	}

	return (@l);
}

# nfsDumpParseLineField -- Just return ONE of the fields,
# without parsing the entire line.

sub nfsDumpParseLineField {
	my ($line, $field) = @_;

	# If the line doesn't start with a digit, then
	# it's certainly not properly formed, so bail out
	# immediately.

	if (! ($line =~ /^[0-9]/)) {
		return undef;
	}

	my $rest;
	if ($AllowRisky) {
		$rest = $line;
	}
	else {
		my @foo = split (' ', $line, 9);
		$rest = ' ' . $foo[8];
	}

	$rest =~ /\ $field\ +([^\ ]+)/;
	return $1;
}

# Returns a new file handle that has all the "useful" information as
# the original, but requires less storage space.  File handles
# typically contain quite a bit of redundancy or unused bytes.
#
# This routine only knows about the advfs and netapp formats.  If
# you're using anything else, just use anything else as the mode, and
# the original file handle will be returned.
#
# If you extend this to handle more file handles, please send the new
# code to me (ellard@eecs.harvard.edu) so I can add it to the
# distribution.

sub nfsDumpCompressFH {
	my ($mode, $fh) = @_;

	if ($mode eq 'advfs') {

		# The fh is a long hex string:
		# 8 chars: file system ID
		# 8 chars: apparently unused.
		# 8 chars: unused.
		# 8 chars: inode
		# 8 chars: generation
		# rest of string: mount point (not interesting).
		# So all we do is pluck out the fsid, inode,
		# and generation number, and throw the rest away.

		$fh =~ /^(........)(........)(........)(........)(........)/;

		return ("$1-$4-$5");
	}
	elsif ($mode eq 'netapp') {

		# Here's the netapp format (from Shane Owara):
		#
		# 4 bytes     mount point file inode number
		# 4 bytes     mount point file generation number
		# 
		# 2 bytes     flags
		# 1 byte      snapshot id
		# 1 byte      unused
		#
		# 4 bytes     file inode number
		# 4 bytes     file generation number
		# 4 bytes     volume identifier
		#
		# 4 bytes     export point fileid
		# 1 byte      export point snapshot id
		# 3 bytes     export point snapshot generation number
		#
		# The only parts of this that are interesting are
		# inode, generation, and volume identifier (and probably
		# a lot of the bits of the volume identifier could be
		# tossed, since we don't have many volumes...).

		$fh =~ /^(........)(........)(........)(........)(........)(........)(........)/;

		return ("$4-$5-$6-$1");
	}
	elsif ($mode eq 'RFSNN') {

		# Here's the netapp format (from Shane Owara):
		#
		# 4 bytes     mount point file inode number
		# 4 bytes     mount point file generation number
		# 
		# 2 bytes     flags
		# 1 byte      snapshot id
		# 1 byte      unused
		#
		# 4 bytes     file inode number
		# 4 bytes     file generation number
		# 4 bytes     volume identifier
		#
		# 4 bytes     export point fileid
		# 1 byte      export point snapshot id
		# 3 bytes     export point snapshot generation number
		#
		# The only parts of this that are interesting are
		# inode, generation, and volume identifier (and probably
		# a lot of the bits of the volume identifier could be
		# tossed, since we don't have many volumes...).
		
		# 61890100575701002000000  0009ac710e9ea381  0d24400006189010057570100
		# 61890100575701002000000  0009ac70ed2ea381  0d24400006189010057570100
		# 61890100575701002000000  000479a1e008d782  0d24400006189010057570100
		# Ningning need only 24-39 (or 12-19 bytes)

		$fh =~ /^(........)(........)(........)(........)(........)(........)/;

		return ("$4$5");
	}else {

		return ($fh);
	}
}

sub testMain {
	$lineNum = 0;

	while (<STDIN>) {
		$line = $_;
		$lineNum++;

		&nfsDumpParseLine ($line);
	}
}

1;

# end of nfsdump.pl
