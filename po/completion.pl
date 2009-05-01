#!/usr/bin/perl -w
# vim: set ts=4 sw=4 :

use strict;
use Getopt::Long qw(:config pass_through);

our %langs = (
	'en_GB.po'       => 'English (British)',
	'en_US.UTF-8.po' => 0,
	'es.po'          => 'Spanish',
	'de.po'          => 'German',
	'he.po'          => 'Hebrew',
	'ja.po'          => 'Japanese',
	'ru_RU.po'       => 'Russian',
	'uk_UA.po'       => 'Ukranian',
);

our($opt_h, $opt_p);

GetOptions qw(-h|html -p|htmlpage);

$opt_h = 1 if $opt_p;

html_header() if $opt_p;
for my $file (@ARGV) {
	next unless $langs{$file};
	my ($msgcount, $msgtrans, $msgfuzzy) = parsefile($file);
	report($file, $msgcount, $msgtrans, $msgfuzzy);
}
html_footer() if $opt_p;

sub parsefile {
	my ($file) = @_;
	my $msgcount = 0;
	my $msgtrans = 0;
	my $msgfuzzy = 0;
	my $inmsg = 0;
	my $fuzzy = 0;

	open(INFILE, "<$file");
	my @filedata = <INFILE>;
	close(INFILE);

	while (scalar @filedata) {
		$_ = shift @filedata;
		if (/^#:/) {                           # new message
			$msgcount++ if not $inmsg;
			$inmsg = 1;
			$fuzzy = 0;
		} elsif (/^#,\s*fuzzy/) {
			$fuzzy = 1;
		} elsif ($msgcount > 0 && /^msgstr/) {
			# msgstr lines sometimes only have data on the next
			# line.  grab another line to be sure we don't miss
			# anything.
			if (/""/) {
				my $nextline = shift @filedata;
				$_ .= $nextline if defined $nextline;
			}
			if (/"[^"]+"/) {
				$msgfuzzy++ if $fuzzy;
				$msgtrans++ if not $fuzzy;
			}
			$inmsg = 0;
		}
	}
	return ($msgcount, $msgtrans, $msgfuzzy);
}

sub report {
	my ($file, $msgcount, $msgtrans, $msgfuzzy) = @_;
	warn "$_[0]: no messages?\n" if not $msgcount;
	$opt_h ? report_html(@_) : report_text(@_);
}

sub report_text {
	my ($file, $msgcount, $msgtrans, $msgfuzzy) = @_;
	print "$file: $msgtrans trans / $msgfuzzy fuzzy / $msgcount total: ";
	printf("%0.2f%% complete.\n", $msgtrans * 100.0 / $msgcount);
}

sub report_html {
	my ($file, $msgcount, $msgtrans, $msgfuzzy) = @_;
	my ($ttrans, $tfuzzy, $tuntrans, $twidth);
	my $untrans = $msgcount - $msgfuzzy - $msgtrans;
	my $widthpx = 200;
	$ttrans   = $msgtrans / $msgcount*$widthpx;
	$tfuzzy   = $msgfuzzy / $msgcount*$widthpx;
	$tuntrans = $untrans  / $msgcount*$widthpx;
	$twidth   = $widthpx + 2; # 2px of border
	my $complete = sprintf "%0.2f%%", $msgtrans * 100.0 / $msgcount;
	my $lang = $langs{$file} || $file;
	print<<EOF;
<tr>
<td>$lang</td>
<td><table style='border: solid 1px black; width: $twidth' border='0' cellspacing='0' cellpadding='0'><tr>
<td style='width:${ttrans}px; height:10px;' bgcolor="gray"></td>
<td style='width:${tfuzzy}px' bgcolor="lightgray"></td>
<td style='width:${tuntrans}px' bgcolor="white"></td>
</tr></table></td>
<td style='text-align: right'>$complete</td>
</tr>
EOF
}
sub html_header {
	print<<EOF;
<html>
<head><title>LogJam translation statistics</title><head>
<body>
<table border=0 cellpadding=2 cellspacing=2>
EOF
}
sub html_footer {
	print<<EOF;
</table>
</body></html>
EOF
}
