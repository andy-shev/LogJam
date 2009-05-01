#!/usr/bin/perl -w0777

use strict;
use YAML ();

our $output;
$output = $_;

our $MAGIC = "\%\%\%";

our $ROOT = YAML::LoadFile("opts.yml");
our $APPLICATION = $ROOT->{application} || "logjam";
#::DD($ROOT);

@ARGV = $0 unless @ARGV;
$output = <>;
$output =~ s/$MAGIC(.*?)$MAGIC/"my \$out;$1;\$out"/egems;
die $@ if $@;
print $output;

######################################################################

sub gen_helpvar {
	my ($y, $path) = @_;
	my $out = "";
	my $desc = $y->{longdesc} || $y->{desc};

	$out .= "$desc\n" if $desc;
	if (@{ $y->{options} }) {
		$out .= "Options:\n";
		for my $opt (@{ $y->{options} }) {
			my($s, $l, $a, $d) = @$opt{ qw/short long arg desc/ };
			if ($s) {
				$out .= sprintf "  %-38s %-s\n",
					"-$s, --$l",
					($d || "(No description available)");
			} else {
				$out .= "\n";
			}
		}
		$out .= "\n";
	}

	$out =~ s/\n*$//s;

	my $realout = $out;
	$realout =~ s!^(.*)$!"$1\\n"!mg;  # wrap each line in quotes and stick in the newline.
	$realout =~ s!$! \\!mg;           # add the #define-continuing-\,
	$realout =~ s! \\$!!s;            # except the last line.
	$realout =~ s!^!\t!mg;            # tab each line in.
	$realout = "#define \U${path}_help_text\E \\\n$realout\n";

	return $realout;
}

sub gen_help {
	my ($y, $path) = @_;
	my $out = "";
	my $desc = $y->{longdesc} || $y->{desc};
	$out .= "$desc\n" if $desc;
	if (@{ $y->{options} }) {
		$out .= "Options:\n";
		for my $opt (@{ $y->{options} }) {
			my($s, $l, $a, $d) = @$opt{ qw/short long arg desc/ };
			if (!($s || $l)) { # spacer
				$out .= "  E<32>\n"; # hack to keep this one POD paragraph
				next;
			}
			$out .= sprintf "  %-38s<tl>%-s\n",
				($s ? "B<-$s>, " : "    ") . ($l ? "B<--$l>" . ($a ? "=I<$a>" : "") : ""),
				($d || "(No description available)");
		}
		$out .= "\n";
	}

	if (@{ $y->{subcommands} }) {
		$out .= "Subcommands:\n";
		for my $cmd (@{ $y->{subcommands} }) {
			my($n, $d) = @$cmd{qw/name desc/};
			$out .= sprintf "  %-16s %-s\n", "B<$n>", $d;
		}
		$out .= "\n";
	}
	$out =~ s/\n\n$/\n/;
	return $out;
}

sub gen_dispatch {
	my ($y, $path) = @_;
	my $out = "#define \U${path}_subcommands\E { \\\n";

	$path = $path eq $APPLICATION ? "" : "${path}_";
	
	for my $cmd (@{ $y->{subcommands} }) {
		my($n, $d, $r, $e, $p) = @$cmd{qw/name desc requireuser existinguser needpw/};
		for ($e, $p) {
			$_ ||= "FALSE";
			$_ = "TRUE" if $_ ne "FALSE";
		}
		$r ||= "TRUE";
		$r = "TRUE" if $r ne "FALSE";
		$out .= "\t{ \"$n\", _(\"$d\"), $r, $e, $p, do_${path}$n }, \\\n";
	}
	return $out . "\t{ NULL }, \\\n}\n";
}

sub gen_shortopts {
	my ($y, $path) = @_;
	# the + means "stop reading args as soon as we hit a non-option char".
	my $out = "#define \U${path}_short_options\E \"+";
	for my $opt (@{ $y->{options} }) {
		next unless $opt->{short}; # XXX: should this even be allowed?
		$out .= $opt->{short};
		$out .= ":" if $opt->{arg};
	}
	return $out . "h\"\n";
}

sub gen_longopts {
	my ($y, $path) = @_;
	my $out = "#define \U${path}_long_options\E { \\\n";
	for my $opt (@{ $y->{options} }) {
		next unless $opt->{long}; # XXX: should this even be allowed?
		$out .= sprintf "\t{ \"%s\", %s, %s, '%s' }, \\\n",
			$opt->{long},
			($opt->{arg} ?
				($opt->{required} ? "required" : "optional") :
				"no") . "_argument",
			($opt->{flag} || "0"),
			($opt->{short} or die "no short option for $path/".$opt->{long});
	}
	$out .= "\t{ \"help\", no_argument, 0, 'h' }, \\\n";
	$out .= "\t{ \"list-subcommands\", no_argument, 0, OPT_LIST_SUBCOMMANDS }, \\\n";
	return $out . "\t{ 0, 0, 0, 0 }, \\\n}\n";
}


sub ::D { require Data::Dumper; Data::Dumper::Dumper(@_) } sub ::DD { print &::D; die }

__END__
%%%
$out  = "this is a hackish demo.\n";
$out .= ("=" x 70) . "\n";
$out .= "YAML is: " . ::D($ROOT);
$out .= ("=" x 70) . "\n";
$out .= "help var/* root */ = \n" . gen_helpvar($ROOT, $APPLICATION);
$out .= ("=" x 70) . "\n";
$out .= "help, doc /* root */ = \n" . gen_help($ROOT, $APPLICATION);
$out .= ("=" x 70) . "\n";
$out .= gen_dispatch($ROOT, $APPLICATION);

# demo: nested command
#die ::D($ROOT->{subcommands});
my ($cf) = grep { $_->{name} eq 'checkfriends' } @{ $ROOT->{subcommands} };
#die ::D($cf);
$out .= gen_dispatch($cf, $cf->{name});

$out .= gen_shortopts($ROOT, $APPLICATION);
$out .= gen_longopts($ROOT, $APPLICATION);

$out .= ("=" x 70) . "\n";
$out .= "The above was just a demo.\n";
$out .= "\n\nUsage: $0 templatefile > outputfile\n";
%%%

