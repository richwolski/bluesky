#
# Copyright (c) 2003
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
# $Id: counts.pl,v 1.9 2003/07/28 14:27:16 ellard Exp $

package	counts;

@OpList		= ();
%KeysSeen	= ();

%OpCounts	= ();

sub init {
	my (@oplist) = @_;

	@OpList = @oplist;
}

sub printTitle {
	my ($out) = @_;

	my $str = "#C time client fh euid egid";

	foreach my $op ( @OpList ) {
		$str .= " $op";
	}
	$str .= "\n";

	print $out $str;
} 

sub printOps {
	my ($start_time, $out) = @_;
	my ($k, $str, $op, $nk);

	my @allkeys = sort keys %KeysSeen;

	foreach $k ( @allkeys ) {
		my $tot = "$k,TOTAL";

		if ($main::OMIT_ZEROS &&
			(! exists $OpCounts{$tot} || $OpCounts{$tot} == 0)) {
			next;
		}

		$str = sprintf ("C %s %s", $start_time, &key::key2str ($k));

		foreach $op ( @OpList ) {
			$nk = "$k,$op";
			if (exists $OpCounts{$nk}) {
				if ($op eq 'readM' || $op eq 'writeM') {
					$str .= sprintf (" %.3f",
						$OpCounts{$nk} / (1024 * 1024));
				}
				else {
					$str .= " $OpCounts{$nk}"
				}
			}
			else {
				$str .= " 0";
			}
		}
		$str .= "\n";
		print $out $str;
	}
}

sub resetOpCounts {

	# Clear the counts on everything we've seen.

	foreach my $op ( keys %OpCounts ) {
		$OpCounts{$op} = 0;
	}
}

1;

