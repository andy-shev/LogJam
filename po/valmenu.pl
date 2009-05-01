#!/usr/bin/perl -w
#
# tool to help spot bad GTK menu translations in PO files
#
# usage: ./valmenu.pl POFILE

binmode(STDIN, ":utf8");
binmode(STDOUT, ":utf8");

my($last, %glossary, $problems);

$SIG{__WARN__} = sub {
	print STDERR $_[0];
	$problems++;
};

while (<>) {
	$last = $_, next unless s[^msgid "(/.*)"$][$1];
	my $fuzzy = $last =~ /, fuzzy/;
	chomp;
	my $orig = $_;
	(my $tran = <>) =~ s[^msgstr "(.*)"][$1];

	warn "error: fuzzy translation for $orig on line $.\n" if $fuzzy;

	warn "warning: '...' mismatch for $orig on line $.\n" if
		($orig =~ /\.\.\./ && $tran !~ /\.\.\./) ||
		($orig !~ /\.\.\./ && $tran =~ /\.\.\./);

	if (($orig =~ y[/][]) != ($tran =~ y[/][])) {
		warn "error: mismatching '/' count for $orig on line $.\n";
	}
	
	(my $path = $orig) =~ s{/[^/]*$}{/};
	(my $tpath = $tran) =~ s{/[^/]*$}{/};
	$glossary{$path} ||= $tpath if $tpath;

	if (exists $glossary{$path} && $tran !~ /^$glossary{$path}/) {
		warn "error: mismatching prefix for $orig on line $. (wanted $glossary{$path})\n";
	}

	if (($path =~ y[_][]) != ($tpath =~ y[_][])) {
		warn "ERROR: mismatching '_' count for $orig path on line $.\n";
	} elsif (($orig =~ y[_][]) != ($tran =~ y[_][])) {
		warn "warning: mismatching '_' count for $orig element on line $.\n";
	}
	
}

print (($problems ? $problems : 'No'), " problems found with menu translations.\n");
