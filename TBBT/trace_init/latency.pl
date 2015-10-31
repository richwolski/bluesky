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
# $Id: latency.pl,v 1.8 2003/07/28 14:27:16 ellard Exp $
#
# latency.pl -

package	latency;

%PendingOps	= ();
%OpCount	= ();
%OpTime		= ();
%KeysSeen	= ();

@OpList		= ();

sub init {
	my (@oplist) = @_;

	@OpList = @oplist;
}

# Bugs:  might not recognize the actual response packets.  It's an
# approximation.

sub update {
	my ($key, $proto, $op, $xid, $client, $now) = @_;

	my $uxid = "$client-$xid";

	if ($proto eq 'C3' || $proto eq 'C2') {
		$PendingOps{$uxid} = $now;
	}
	elsif (exists $PendingOps{$uxid}) {
		my $elapsed = $now - $PendingOps{$uxid};

		$KeysSeen{$key} = 1;

		$OpTime{"$key,$op"} += $elapsed;
		$OpCount{"$key,$op"}++;

		$OpTime{"$key,TOTAL"} += $elapsed;
		$OpCount{"$key,TOTAL"}++;

		$OpTime{"$key,INTERESTING"} += $elapsed;
		$OpCount{"$key,INTERESTING"}++;

		delete $PendingOps{$uxid};
	}
}

sub resetOpCounts {

	my $k;

	foreach $k ( keys %OpTime ) {
		$OpTime{$k} = 0.0;
	}
	foreach $k ( keys %OpCount ) {
		$OpCount{$k} = 0;
	}

	return ;
}

sub printTitle {
	my $str = "#L time client euid egid fh";

	foreach my $op ( @OpList ) {
		$str .= " $op-cnt $op-lat";
	}
	$str .= "\n";

	print $str;
} 

sub printOps {
	my ($start_time, $out) = @_;
	my ($k, $str, $op, $nk, $latms, $cnt);

	my @allkeys = sort keys %KeysSeen;

	foreach $k ( @allkeys ) {
		my $tot = "$k,TOTAL";

		if ($main::OMIT_ZEROS &&
			(! exists $OpCounts{$tot} || $OpCounts{$tot} == 0)) {
			next;
		}

		$str = sprintf ("L %s %s", $start_time, &key::key2str ($k));

		foreach $op ( @OpList ) {
			$nk = "$k,$op";

			if (exists $OpCount{$nk}) {
				$cnt = $OpCount{"$k,$op"};
			}
			else {
				$cnt = 0;
			}

			if ($cnt > 0) {
				$latms = 1000 * $OpTime{$nk} / $cnt;
			}
			else {
				$latms = -1;
			}

			$str .= sprintf (" %d %.4f", $cnt, $latms);
		}

		print $out "$str\n";
	}
}

# Purge all the pending XID records dated earlier than $when (which is
# typically at least $PRUNE_INTERVAL seconds ago).  This is important
# because otherwise missing XID records can pile up, eating a lot of
# memory. 
  
sub prunePending {
	my ($when) = @_;

	foreach my $uxid ( keys %PendingOps ) {
		if ($PendingOps{$uxid} < $when) {
			delete $PendingOps{$uxid};
		}
	}

	return ;
}

1;
