#!/usr/bin/perl -w
# logjam - a GTK client for LiveJournal.
# Copyright (C) 2000-2002 Evan Martin <evan@livejournal.com>
#
# vim: tabstop=4 shiftwidth=4 noexpandtab syntax=pod:
# $Id: logjam.tmpl-doc.pl,v 1.8 2003/12/08 19:37:29 martine Exp $

# POD documentation for logjam; can generate man and HTML versions of iteself
#
# usage:  logjam.pod.pl [man] [html] [pod]
#
# if only one type given, output to stdout. otherwise, guess output filename.

require 5.006;  # v5.6 that is, but that causes a Silly Warning.
use strict;

use Pod::Man;
use Pod::Html;  # older versions are named Pod::HTML
use IO::File;

# You should manually set the appver and date whenever you update the manual
# (or at least verify the manual is up-to-date).
# This means the manpage can potentially claim it only applies to an older
# version of LogJam, but I think that's better than upping the version
# whenever the LogJam version changes, because in a sense an unchanged manual
# only documents that older version of LogJam.
#  - Evan
our $APPVERSION = "4.1.1";
our $DATE       = "2003-04-25";
our $CENTER     = "LiveJournal";  # man
our $TITLE      = "logjam";       # man, HTML

(our $BASE = $0) =~ s/-doc\.pl//;

our %args = map { lc $_ => 1 } @ARGV;
our $stdout = keys %args <= 1;

if ($args{pod} || keys %args == 0) {
	if ($stdout) {
		print STDOUT while <DATA>;
	} else {
		open POD, ">$BASE.pod" or die "can't write pod: $!";
		print POD while <DATA>;
		close POD or die "can't close pod: $!";
	}
	seek DATA, 0, 0; # next block may need to reuse this handle
}

if ($args{man}) {
	my $p2mparser = Pod::Man->new(release => $APPVERSION, date => $DATE,
			center => $CENTER, name => $TITLE);
	if ($stdout) {
		$p2mparser->parse_from_filehandle(\*DATA);
	} else {
		open POD, ">$BASE.1" or die "can't write pod: $!";
		$p2mparser->parse_from_filehandle(\*DATA, \*POD);
		close POD or die "can't close pod: $!";
	}
}

if ($args{html}) {
	pod2html($stdout ?
			($0, "--title=$TITLE", "--infile=$0") :
			($0, "--title=$TITLE", "--infile=$0", "--outfile=$BASE.1.html"));
}

__DATA__

=head1 NAME

logjam - GTK+ client for LiveJournal

=head1 SYNOPSIS

B<logjam> [I<OPTIONS>] [I<FILE>]

=head1 DESCRIPTION

B<logjam> is a GTK+ client for LiveJournal-based sites
such as livejournal.com.

Aside from writing entries, logjam lets you modify your
friends list, edit your previous entries, and more.

When run with no arguments (or just username option),
logjam will run in the GUI mode.
The user interface is mostly self-explanatory, and won't be
discussed here in detail, except for a few notes below.

=head1 OPTIONS AND COMMANDS

Options can be given in either short or long forms. For help on a
particular commands, type "B<logjam> COMMAND help". For example,
"B<logjam> grep help" will supply help about the grep command.

=over 4

%%%
$out = gen_help($ROOT, $APPLICATION);
$out =~ s/.*(?=Options:)//sm;   # snip help heading
$out =~ s/<nl>/\n/g;
$out =~ s/<tl>/\n    /g;
%%%

=back

Also, GTK+ command line options (such as --display) can be used.

=head1 GUI

This section describes some of the GUI features that aren't immediately
apparent.

=head2 Check Friends

logjam can monitor your friends list and notify you when new entries
are posted there. Enable this by right-clicking on the indicator at
the bottom-left corner of the application window and selecting the
appropriate menu item. You may also configure logjam to start doing this
automatically for you when you login. When new entries are detected, the
indicator will turn red to let you know; click it to resume monitoring or
double-click it to open your browser on your friends page. Optionally,
you can have logjam open a small "floating" indicator which has some
useful GUI settings of its own.

Owners of large friends lists may prefer to be notified only after they
accumulate several new posts. You may set the threshold for this in the
Check Friends settings tab. The default is 1, that is, logjam will tell
you immediately when it detects new traffic on your friends page. There
is a small limit on the maximum threshold allowed, because this feature
is only useful with small threshold values.

=head1 STARTUP

When given a I<FILE> argument, logjam will start up with an existing
file as the base for the composed entry. If the filename given is "-",
the data will be read from standard input. Several aspects of the entry,
such as its subject field and the journal in which to post it to, can
be controlled by other options. This is useful in conjunction with the
B<--commandline> option, which causes logjam to post an entry without
going to GUI mode, allowing completely non-interactive posts. If you do
wish to interactively edit the entry, but don't want to load the GUI,
use the B<--edit> option.

=head2 Autosave

logjam will periodically save a draft of your currently edited entry
in ~/.logjam/draft if you turn on the draft option in the Preferences
dialog. This feature is intended for crash recovery, not archiving. If
you want to keep a copy of your posts, you should use the
S<< Entry > Save As >> menu option before submitting them. A future
version of logjam will support archiving of your journal.

Please note that when you exit the client normally, your draft is
cleared. It does not "stick" for the next invocation, as in the behavior
of some other clients.

=head2 Checking friends from the command line

You can use logjam as a backend for a script or another application
that wishes to check the friends view. This may be useful if you
don't want to use the GUI, or if you have several journals (in
conjunction with B<--username>). To do this, invoke logjam once with
B<--checkfriends=purge> (B<-rpurge> if you're using short options),
and then something like:

	logjam --checkfriends && new-entries-handler

Make sure that your script or application purges the checkfriends
status as described above once the user has acknowledged the new items,
otherwise logjam will always report there's nothing new.  You should also
pay attention to limiting your query rate, despite the fact that logjam
will refuse to flood the server with queries.  For more information,
see the messages on the command line. (To suppress these messages,
use B<--quiet>.)

=head1 SEE ALSO

L<http://logjam.danga.com>

L<http://www.livejournal.com/users/logjam/>

L<http://www.livejournal.com>

=head1 AUTHOR

This manual page was mostly written by Gaal Yahas <gaal@forum2.org>.

