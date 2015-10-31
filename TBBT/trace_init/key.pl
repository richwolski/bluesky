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
# $Id: key.pl,v 1.11 2003/07/26 20:52:04 ellard Exp $

package key;

sub makeKey {
	my ($line, $proto, $op, $xid, $client, $now) = @_;
	my ($client_id, $fh, $euid, $egid) = ('u', 'u', 'u', 'u');
	my ($uxid) = "$client-$xid";

	if ($proto eq 'R3' || $proto eq 'R2') {
		if (exists $PendingKeyStr{$uxid}) {
			return ($PendingKeyStr{$uxid});
		}
		else {
			return 'u,u,u,u';
		}
	}

	if ($main::UseClient) {
		$client_id = $client;
		$client_id =~ s/\..*//g
	}
	if ($main::UseFH && $op ne 'null') {
		my $tag = ($op eq 'commit') ? 'file' : 'fh';

		$fh = nfsd::nfsDumpParseLineField ($line, $tag);
		if (! defined $fh) {
			print STDERR "undefined fh ($line)\n";
		}

		$fh = nfsd::nfsDumpCompressFH ($main::FH_TYPE, $fh);

	}
	if ($main::UseUID && $op ne 'null') {
		$euid = nfsd::nfsDumpParseLineField ($line, 'euid');
	}
	if ($main::UseGID && $op ne 'null') {
		$egid = nfsd::nfsDumpParseLineField ($line, 'egid');
	}

	my $key = "$client_id,$fh,$euid,$egid";
	$KeysSeen{$key} = 1;

	$PendingKeyStr{$uxid} = $key;
	$PendingKeyTime{$uxid} = $now;

	return ($key);
}

sub key2str {
	my ($key) = @_;

	my ($client_id, $fh, $euid, $egid) = split (/,/, $key);

	if ($client_id ne 'u') {

		# just for aesthetics:
		$client_id = sprintf ("%.8x", hex ($client_id));
		$client_id =~ /^(..)(..)(..)(..)/;
		$client_id = sprintf ("%d.%d.%d.%d",
				hex ($1), hex ($2), 
				hex ($3), hex ($4)); 
		$client_id = sprintf ("%-15s", $client_id);
	}

	if ($euid ne 'u') {
		$euid = hex ($euid);
	}
	if ($egid ne 'u') {
		$egid = hex ($egid);
	}

	return ("$client_id $fh $euid $egid");
}

# Purge all the pending XID records dated earlier than $when (which is
# typically at least $PRUNE_INTERVAL seconds ago).  This is important
# because otherwise missing XID records can pile up, eating a lot of
# memory. 
  
sub prunePending {
	my ($when) = @_;

	foreach my $uxid ( keys %PendingKeyTime ) {
		if ($PendingKeyTime{$uxid} < $when) {
			delete $PendingKeyTime{$uxid};
			delete $PendingKeyStr{$uxid};
		}
	}

	return ;
}

1;
