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
# $Id: ns_timeagg,v 1.4 2003/07/10 20:13:31 ellard Exp $

# Allows rounding of the start time, so that minor clock drift doesn't
# make trace periods appear to begin at funny times (i.e.  one or two
# seconds after midnight, instead of midnight).
#
# This is only meant to help with SMALL amounts of drift.  Trying to
# use this to correct for anything more than a small fraction of the
# sample interval is asking for trouble.

require 'timelocal.pl';

sub findStartTime {
	my ($time, $rounding) = @_;

	my $new_time = int ($time);

	if (defined $rounding && $rounding != 0) {
		$rounding = int ($rounding);

		my $leftover = $new_time % $rounding;

		if ($leftover > $rounding / 2) {
			$new_time += $rounding;
		}

		$new_time -= ($new_time % $rounding);
	}

	return $new_time;
}

1;
