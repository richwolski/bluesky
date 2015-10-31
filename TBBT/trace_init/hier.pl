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
# $Id: hier.pl,v 1.14 2003/07/26 20:52:03 ellard Exp $
#
# hier.pl - Tools to map out the file system hierarchy.  This is
# accomplished by snooping out the lookup calls.
#
# This is expensive because the hierarchy can require a LOT of space
# to store for a large system with lots of files (especially if files
# come and go).  Don't construct the hierarchy unless you want it --
# and be prepared to prune it from time to time.

package	hier;

# Tables used by the outside world:

%fh2Parent		= ();
%fh2Name		= ();
%fh2Attr		= ();
%fh2AttrOrig		= ();
%parent2fh		= ();

#RFS: init FS
%rootsName		= ();
%discardFHs = ();
%rootsFHs = ();
#RFS: dependency table
%fhCreate = ();

%rfsAllFHs = ();
%fhType = (); # we use %fhIsDir instead
$rfsLineNum = 0;






# Library-private tables and variables.

%pendingCallsXIDnow	= ();
%pendingCallsXIDfh	= ();
%pendingCallsXIDname	= ();

$nextPruneTime		= -1;
$PRUNE_INTERVAL		= 5 * 60;	# Five minutes.

sub processLine {
	my ($line, $proto, $op, $xid, $client, $now, $response, $fh_type) = @_;

	&addRfsAllFHs($line, $proto, $op, $uxid,
				$now, $response, $fh_type);

	if ($now > $nextPruneTime) {
		&prunePending ($now - $PRUNE_INTERVAL);
		$nextPruneTime = $now + $PRUNE_INTERVAL;
	}

	my $uxid = "$client-$xid";

	# 'lookup', 'create', 'rename', 'delete',
	# 'getattr', 'setattr'

	#RFS: add mkdir/rmdir/symlink
	if ( $op eq 'lookup' || $op eq 'create' || $op eq 'mkdir' || 
	     ($op eq 'symlink' && ($proto eq 'C3' || $proto eq 'R3' ) ) ) {
		return (&doLookup ($line, $proto, $op, $uxid,
				$now, $response, $fh_type));
	}
	elsif ($op eq 'rename') {
	}
	elsif ($op eq 'remove' || $op eq 'rmdir') {
		# RFS: why remove these entries? Just let them exist since 
		# there is generation number available to distinguish btw removed dir/file 
		# and new dir/file with the same inode number.
		#return (&doRemove ($line, $proto, $op, $uxid,
		#		$now, $response, $fh_type));
	}
	elsif ($op eq 'getattr' || $op eq 'read' || $op eq 'write'  ||
	         ($op eq 'readlink' && ($proto eq 'C3' || $proto eq 'R3' ) ) ) {
		return (&doGetAttr ($line, $proto, $op, $uxid,
				$now, $response, $fh_type));
	}
	elsif ($op eq 'setattr') {
	}
}


# get time stamp

sub getTimeStamp{
	my ($line) = @_;

	if (! ($line =~ /^[0-9]/)) {
		print "getTimeStamp return undef\n";
		return undef;
	}
	else {
		my @l = split (' ', $line, 2);
		return $l[0];
	}
}

sub addRfsAllFHs {
	my ($line, $proto, $op, $uxid, $now, $response, $fh_type) = @_;

	my $fh = undef;
	
	my $checkfh = undef;
	$checkfh = nfsd::nfsDumpParseLineField ($line, 'fh');
	if (defined $checkfh) {
		$fh = nfsd::nfsDumpCompressFH ($fh_type, $checkfh);
	}
	
	my $fh2 = undef;

	if ($op eq 'rename' || $op eq 'link') {
		$checkfh = nfsd::nfsDumpParseLineField ($line, 'fh2');
		if (defined $checkfh) {
			$fh2 = nfsd::nfsDumpCompressFH ($fh_type, $checkfh);
		}	
	}

	if (defined $fh) {
		# record the first appearance of the fh
		if ( !exists  $rfsAllFHs{$fh} )  {
			$rfsAllFHs{$fh} = $rfsLineNum ;
		}
	}

	if (defined $fh2) {
		if ( !exists  $rfsAllFHs{$fh2} ) {
			$rfsAllFHs{$fh2} = $rfsLineNum;
		}
	}

	return ;
}


sub doLookup {
	my ($line, $proto, $op, $uxid, $now, $response, $fh_type) = @_;

	if ($proto eq 'C3' || $proto eq 'C2') {
		my $tag = ($proto eq 'C3') ? 'name' : 'fn';
		my $name = nfsd::nfsDumpParseLineField ($line, $tag);

		# All directories have (at least) three names:  the
		# given name, and "." and "..".  We're only interested
		# in the given name.

		if ($name eq '"."' || $name eq '".."') {
			return ;
		}

		my $fh = nfsd::nfsDumpCompressFH ($fh_type,
			nfsd::nfsDumpParseLineField ($line, 'fh'));

		$pendingCallsXIDnow{$uxid} = $now;
		$pendingCallsXIDfh{$uxid} = $fh;
		$pendingCallsXIDname{$uxid} = $name;
	}
	elsif ($proto eq 'R3' || $proto eq 'R2') {
		if (! exists $pendingCallsXIDnow{$uxid}) {
			return ;
		}

		my $pfh = $pendingCallsXIDfh{$uxid};
		my $name = $pendingCallsXIDname{$uxid};

		delete $pendingCallsXIDnow{$uxid};
		delete $pendingCallsXIDfh{$uxid};
		delete $pendingCallsXIDname{$uxid};

		if ($response eq 'OK') {
			my $cfh = nfsd::nfsDumpCompressFH ($fh_type,
					nfsd::nfsDumpParseLineField ($line, 'fh'));

			my $type = nfsd::nfsDumpParseLineField ($line, 'ftype');

			#if ($type == 2) 
			{
				$fhIsDir{$cfh} = $type;
			}

			# Original code
			# $fh2Parent{$cfh} = $pfh;
			# $fh2Name{$cfh} = $name;
			# $parent2fh{"$pfh,$name"} = $cfh;
			# RFS code: in case of the rename, we will record the name of the old name
			$fh2Parent{$cfh} = $pfh;
			if (! exists   $fh2Name{$cfh}) {
				$fh2Name{$cfh} = $name;
				$parent2fh{"$pfh,$name"} = $cfh;
			} else {
				# keep the old name in the fh2Name{$cfh}
				# and we also add the newname and pfh mapping
				#$fh2Name{$cfh} = $name;
				$parent2fh{"$pfh,$name"} = $cfh;
			}

			my ($size, $mode, $atime, $mtime, $ctime, $nlink) =
					nfsd::nfsDumpParseLineFields ($line,
					'size', 'mode',
					'atime', 'mtime', 'ctime', 'nlink');
			my $ts = getTimeStamp($line);

			# RFS: modify here to get/maintain more file attributes
			# we can just check the ctime and compare it with trace-start-time
			# to decide whether to create a file/diretory.
			# atime - last access time of the file
			# mtime - last modification time of the file
			# ctime - last file status change time
			
			#$fh2Attr{$cfh} = "$size $mode $atime $mtime $ctime";
			if  (! exists $fh2AttrOrig{$cfh} ) {
				$fh2AttrOrig{$cfh} = "$size $mode $op $atime $mtime $ctime $nlink $ts";
			}
			$fh2Attr{$cfh} = "$size $mode $op $atime $mtime $ctime $nlink $ts";
		}

	}

	return ;
}

sub doRemove {
	my ($line, $proto, $op, $uxid, $now, $response, $fh_type) = @_;

	if ($proto eq 'C3' || $proto eq 'C2') {
		my $tag = ($proto eq 'C3') ? 'name' : 'fn';
		my $name = nfsd::nfsDumpParseLineField ($line, $tag);

		# All directories have (at least) three names:  the
		# given name, and "." and "..".  We're only interested
		# in the given name.

		if ($name eq '"."' || $name eq '".."') {
			return ;
		}

		my $pfh = nfsd::nfsDumpCompressFH ($fh_type,
			nfsd::nfsDumpParseLineField ($line, 'fh'));

		if (! exists $parent2fh{"$pfh,$name"}) {
			return ;
		}

		$pendingCallsXIDnow{$uxid} = $now;
		$pendingCallsXIDfh{$uxid} = $pfh;
		$pendingCallsXIDname{$uxid} = $name;
	}
	elsif ($proto eq 'R3' || $proto eq 'R2') {
		if (! exists $pendingCallsXIDnow{$uxid}) {
			return ;
		}

		my $pfh = $pendingCallsXIDfh{$uxid};
		my $name = $pendingCallsXIDname{$uxid};

		delete $pendingCallsXIDfh{$uxid};
		delete $pendingCallsXIDname{$uxid};
		delete $pendingCallsXIDnow{$uxid};

		if (! exists $parent2fh{"$pfh,$name"}) {
			return ;
		}

		my $cfh = $parent2fh{"$pfh,$name"};

		if ($response eq 'OK') {
			if ($op eq 'remove') {
				printFileInfo ($cfh, 'D');

				delete $fh2Parent{$cfh};
				delete $fh2Name{$cfh};
				delete $fh2Attr{$cfh};
				delete $fhs2AttrOrig{$cfg};
				delete $parent2fh{"$pfh,$name"};
			}
		}
	}

	return ;
}

sub doGetAttr {
	my ($line, $proto, $op, $uxid, $now, $response, $fh_type) = @_;

	if ($proto eq 'C3' || $proto eq 'C2') {
		my $fh = nfsd::nfsDumpCompressFH ($fh_type,
			nfsd::nfsDumpParseLineField ($line, 'fh'));

		#if (nfsd::nfsDumpParseLineField ($line, 'fh')
		#		eq '00018961-57570100-d2440000-61890100') {
		#	printf STDERR "Seen it ($op)\n";
		#}

		if (! defined $fh) {
			return ;
		}

		$pendingCallsXIDnow{$uxid} = $now;
		$pendingCallsXIDfh{$uxid} = $fh;
# RFS debug code
#my $wantfh = "6189010057570100200000000000862077ed3800d24400006189010057570100";
#if ($fh eq $wantfh) {
#	print "JIAWU: doGetAttr call $wantfh\n";
#}
	}
	else {
		if (! exists $pendingCallsXIDnow{$uxid}) {
			return ;
		}

		my $fh = $pendingCallsXIDfh{$uxid};
		delete $pendingCallsXIDfh{$uxid};
		delete $pendingCallsXIDnow{$uxid};
# RFS debug code
#my $wantfh = "6189010057570100200000000000862077ed3800d24400006189010057570100";
#if ($fh eq $wantfh) {
#	print "JIAWU: doGetAttr response $wantfh\n";
#}

		if ($response ne 'OK') {
			return ;
		}

		my ($ftype) = nfsd::nfsDumpParseLineFields ($line, 'ftype');
		if (!defined $ftype) {
			print STDERR "BAD $line";
			return ;
		}

		#if ($ftype == 2) 
		{
			$fhIsDir{$fh} = $ftype;
		}

		#RFS comment: here if fh is a directory, then it will not be add 
		# in the two hash table %fh2Attr(%fh2AttrOrig) and %fh2Name
		# if ($ftype != 1) {
		#	return ;
		#}
		if ($ftype != 1) {
			#return ;
		}


		my ($mode, $size, $atime, $mtime, $ctime, $nlink) =
				nfsd::nfsDumpParseLineFields ($line,
				'mode', 'size', 'atime', 'mtime', 'ctime', 'nlink');
		my $ts = getTimeStamp($line);

			# RFS: modify here to get/maintain more file attributes
			# we can just check the ctime and compare it with trace-start-time
			# to decide whether to create a file/diretory.
			# atime - last access time of the file
			# mtime - last modification time of the file
			# ctime - last file status change time

			# $fh2Attr{$fh} = "$size $mode $atime $mtime $ctime";

			if  (! exists $fh2AttrOrig{$fh} ) {
				$fh2AttrOrig{$fh} = "$size $mode $op $atime $mtime $ctime $nlink $ts";
			}
			$fh2Attr{$fh} = "$size $mode $op $atime $mtime $ctime $nlink $ts";
	}
}

# Purge all the pending XID records dated earlier than $when (which is
# typically at least $PRUNE_INTERVAL seconds ago).  This is important
# because otherwise missing XID records can pile up, eating a lot of
# memory. 
  
sub prunePending {
	my ($when) = @_;

	foreach my $uxid ( keys %pendingCallsXIDnow ) {
		if ($pendingCallsXIDnow{$uxid} < $when) {
# RFS debug code
my $fh = $pendingCallsXIDfh{$uxid};
my $wantfh = "6189010057570100200000000000862077ed3800d24400006189010057570100";
if ($fh eq $wantfh) {
	print "JIAWU: prunePending $wantfh\n";
}
#enf RFS
			delete $pendingCallsXIDnow{$uxid};
		}
	}

	return ;
}

# Return as much of the path for the given fh as possible.  It may or
# may not reach the root (or the mount point of the file system), but
# right now we don't check.  Usually on busy systems the data is
# complete enough so that most paths are complete back to the mount
# point.

sub findPath {
	my ($fh) = @_;
	my $isdir = 0;
	my $cnt = 0;
	my $MaxPathLen = 40;

	if (exists $fhIsDir{$fh} && $fhIsDir{$fh}==2) {
		$isdir = 1;
	}

	my @path = ();
	while ($fh && exists $fh2Name{$fh}) {
		unshift (@path, $fh2Name{$fh});

		if ( ($fh2Name{$fh} ne '"RFSNN0"' ) ) {
			if (! exists $fh2Parent{$fh}) {
				print STDERR "$fh2Name{$fh} ";
				if ( ($fh2Name{$fh} eq '"RFSNN0"' ) ) {
					print STDERR "eq RFSNN0\n";
				} else {
					print STDERR "NOT eq RFSNN0\n";
				}
			}
			if ($fh eq $fh2Parent{$fh}) {
				unshift (@path, '(LOOP)');
				last;
			}
		}

		if ($cnt++ > $MaxPathLen) {
			print STDERR "findPath: path too long (> $MaxPathLen)\n";
			unshift (@path, '(TOO-LONG)');
			last;
		}

		$fh = $fh2Parent{$fh};
	}

	# RFS: append the ~user (fh and !exists $fh2Name{$fh} and type is Directory)
	if ($fh && !exists $fh2Name{$fh} && (exists $fhIsDir{$fh} && $fhIsDir{$fh}==2) ) {
		if (exists $rootsName{$fh}) {
			#print "JIAWU: $rootsName{$fh}\n";
			unshift(@path, $rootsName{$fh});
		} else {
			print "JIAWU: WARNING! No rootsName for this fh: $fh\n";
			unshift(@path, $fh);
		}
	} else {
		if ($fh && !exists $fh2Name{$fh} && (!exists $fhIsDir{$fh} || (exists $fhIsDir{$fh} && $fhIsDir{$fh}!=2)) ) {
			if (exists $discardFHs{$fh}) {
				open NOATTRDIR, ">>noattrdirdiscard" || die "open noattrdirdiscard failed\n";
				print NOATTRDIR "$fh DISCARD\n";
				close NOATTRDIR;
			} else {
				# RFS: if a possible fh without attr and name, then regard it as a special root ~/RFSNN0
				unshift(@path, '"RFSNN0"');
				$fhIsDir{$fh}=2;
				$fh2Name{$fh} = '"RFSNN0"';
				$rootsName{$fh} = '"RFSNN0"';
				open NOATTRDIR, ">>noattrdir-root";
				print NOATTRDIR "$fh /RFSNN0/\n";
				close NOATTRDIR;
			}
		}
	}

	
	my $str = '';
	$cnt = 0;
	foreach my $p ( @path ) {
		$p =~ s/^.//;
		$p =~ s/.$//;
		$str .= "/$p";
		$cnt++;
	}

	if ($isdir) {
		$str .= '/';
	}

	if ($cnt == 0) {
		$str = '.';
	}

	return ($str, $cnt);
}


$total_unknown_fh = 0;
$total_known_fh = 0;

sub printAll {
	my ($start_time, $out) = @_;

	my %allfh = ();
	my $fh;
	my $u = 0;
	my $k = 0;

	# RFS print more information here
	open (OUT_RFS, ">rfsinfo") ||
		die "Can't create $OutFileBaseName.rfs.";
		
	foreach $fh ( keys %fh2Attr ) {
		$allfh{$fh} = 1; 
	}
	foreach $fh ( keys %fh2Name ) {
		$allfh{$fh} = 1; 
	}

	#RFS: before printFileInfo, name those roots' name

	#RFS there are three kind of fh
	# 1. fh/name paired (fh/attr must)
	# 2. fh/attr but no fh/name: type file (discard related operations)
	# 3. fh/attr but no fh/name: type dir (keep as persuedo root)
	$u = $k = 0;
	my $sn=1;
	foreach $fh ( keys %allfh ) {
		if (exists $fh2Parent{$fh} ) {
			$k++;
		}
		else {
			$u++;
			my $type = (exists $fhIsDir{$fh} && $fhIsDir{$fh}==2) ? 'D' : 'F';
			if ($type eq 'D') {
				$rootsName{$fh} = sprintf("\"RFSNN%d\"", $sn++);
				$rootsFHs{$fh} = 2;
			}
			else {
				$discardFHs{$fh} = 1;
			}
		}
	}
	print OUT_RFS "#stat: fh with parent = $k, fh without parent = $u\n";
	$u = keys %rootsFHs;
	print OUT_RFS "#RFS: root fh list($u)\n";
	foreach $fh (keys %rootsName) {
		print OUT_RFS "#RFS: $rootsName{$fh} $fh\n";
	}
	$u = keys %discardFHs;
	print OUT_RFS "#RFS: discard fh list($u)\n";
	print OUT_RFS join("\n", keys %discardFHs, "");
	

	print $out "#F type state fh path pathcount attrOrig(size,mode,op,atime,mt,ct) attrLast(size,mode,op,at,mt,ct)\n";

	print $out "#T starttime = $start_time\n";
	foreach $fh ( keys %allfh ) {
		printFileInfoOutputFile ($fh, 'A', $out);
	}
	
	my $numfh2Name = keys %fh2Name;
	my $numfh2Attr = keys %fh2Attr;
	print OUT_RFS "fh2name has $numfh2Name, fh2Attr has $numfh2Attr\n";

	
	$u = $k = 0;
	foreach $fh ( keys %allfh ) {
		if ( exists $fh2Name{$fh} ) {$k++;}
		else {$u++;}
	}
	print OUT_RFS "#stat: total fh with name = $k, without name = $u\n";

	print OUT_RFS "#stat: finally, total known fh = $total_known_fh, unknown = $total_unknown_fh\n";

# Note: fh with name (8303), fh without name (103)
#          root fh list: 18
#          discard fh list: 85
#          known fh (8321): ( fh with name(8303) + root fh list (18) = 8321)
#          unknown fh (85)
#
# All fh from the those data structures: 8321 + 85 = 8303+103
# Or, in keys %allfh
#
# 
	print OUT_RFS "#RFS\n";
	close OUT_RFS;	

	open (MISSED, ">missdiscardfh") ||
			die "Can't create missdiscardfh.";
	foreach $fh (keys %rfsAllFHs) {
		if ( !exists $allfh{$fh} && 
		     ( (defined $fh2Name{$fh}) && ($fh2Name{$fh} ne '"RFSNN0"')) ) {
			print MISSED "$fh LN: $rfsAllFHs{$fh}\n"
		}
	}
	close MISSED;

# check for a special fh
#my $wantfh = "6189010057570100200000000000862077ed3800d24400006189010057570100";
#if ($allfh{$wantfh} == 1) {
#	print OUT_RFS "JIAWU: found $wantfh\n";
#} else {
#	print OUT_RFS "JIAWU: NOT found $wantfh\n";
#}
#foreach $fh ( keys %allfh ) {
#	if ( $fh eq $wantfh ) {
#		print OUT_RFS "JIAWU: found $wantfh\n";
#		printFileInfoOutputFile ($fh, 'JIAWU', *OUT_RFS);
#		last;
#	}
#}
#print OUT_RFS "JIAWU: after \n";


}

sub printFileInfoOutputFile {
	my ($fh, $state, $out) = @_;

	my ($p, $c) = findPath ($fh);
	
	if ($c == 0) {$total_unknown_fh++;}
	else {$total_known_fh++;}
	
	#my $type = (exists $fhIsDir{$fh} && $fhIsDir{$fh}==2) ? 'D' : 'F';
	my $type = $fhIsDir{$fh};
	if (!defined $type) 
	{
		print STDERR "unknown ftype(U) for fh: $fh\n"; 
		$type = 'U';
	}
	my $attr = (exists $fh2Attr{$fh}) ?
			$fh2Attr{$fh} : "-1 -1 -1 -1 -1 -1 -1 -1";
	my $attrOrig = (exists $fh2AttrOrig{$fh}) ?
			$fh2AttrOrig{$fh} : "-1 -1 -1 -1 -1 -1 -1 -1";

	print $out "F $type $state $fh $p $c $attrOrig $attr\n";
}

sub printFileInfo {
	my ($fh, $state) = @_;

	my ($p, $c) = findPath ($fh);
	
	if ($c == 0) {$total_unknown_fh++;}
	else {$total_known_fh++;}
	
	my $type = (exists $fhIsDir{$fh} && $fhIsDir{$fh}==2) ? 'D' : 'F';
	my $attr = (exists $fh2Attr{$fh}) ?
			$fh2Attr{$fh} : "-1 -1 -1 -1 -1 -1 -1 -1";
	my $attrOrig = (exists $fh2AttrOrig{$fh}) ?
			$fh2AttrOrig{$fh} : "-1 -1 -1 -1 -1 -1 -1 -1";

	print "F $type $state $fh $p $c $attrOrig $attr\n";
}

#
# The flow to create the dependency table
# 
# create(dirfh, name, attr) -->newfh, new attr
# mkdir(dirfh, name, attr) -> newfh, new attr
#
# remove(dirfh, name) --> status
# rmdir(dirfh, name) --> status
# rename(dirfh, name, todirfh, toname) --> status
#
# link(newdirfh, newname, dirfh, name) --> status (newdir/newname=>dir/name)
# syslink(newdirfh, newname, string) --> status (newdir/newname=>"string")
# readlink(fh) --> string
# lookup(dirfh, name) --> fh, attr
# getattr(fh) --> attr
# setattr(fh, attr) --> attr
# read(fh, offset, count) -> attr, data
# write(fh, offset, count, data) --> attr
# readdir(dirfh, cookie, count) --> entries
# statfs(fh) --> status
#
#
#
#
# for each line the trace file: 
# 	if (op == R2 or R3) continue; #skip the response line
# 	switch (the op)
# 	{
#	# CREATION OPs:
#	case create:
# 	case remove:
# 	# DELETE OPs:
#	case mkdir:
#	case rmdir:
# 	# other OPs
#
#
#
#
#
1;
