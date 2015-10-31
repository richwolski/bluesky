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
# $Id: userUtils.pl,v 1.5 2003/07/26 20:52:04 ellard Exp $
#
# utils.pl - deal with details like turning login names into uids.
#

sub logins2uids {
	my ($str) = @_;

	my @uids = ();
	my @logins = split (/,/, $str);

	my $j = 0;
	for (my $i = 0; $i < @logins; $i++) {
		if ($logins[$i] =~ /^[0-9]/) {
			$uids[$j++] = $logins[$i];
		}
		else {
			my ($login, $pw, $uid, $gid) = getpwnam ($logins[$i]);
			if (defined $uid) {
				$uids[$j++] = $uid;
			}
			else {
				# &&& Warning or error?
				print STDERR "WARNING: Unrecognized login ($logins[$i])\n";
				$uids[$j++] = $logins[$i];
			}
		}
	}

	return (@uids);
}

sub groups2gids {
	my ($str) = @_;

	my @gids = ();
	my @groups = split (/,/, $str);

	for (my $i = 0; $i < @groups; $i++) {
		if ($groups[$i] =~ /^[0-9]/) {
			$gids[$j++] = $groups[$i];
		}
		else {
			my $gid = getgrnam ($groups[$i]);
			if (defined $gid) {
				$gids[$j++] = $gid;
			}
			else {
				# &&& Warning or error?
				print STDERR "WARNING: Unrecognized group ($logins[$i])\n";
				$gids[$j++] = $groups[$i];
			}
		}
	}

	return (@gids);
}

1;
