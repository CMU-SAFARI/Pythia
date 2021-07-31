#!/usr/bin/perl

package Metric;
use warnings;

sub  trim { my $s = shift; $s =~ s/^\s+|\s+$//g; return $s };

sub parse
{
	my ($filename) = @_;
	open($fh, '<', $filename) or die "cannot open file $trace_file\n";
	chomp(my @lines = <$fh>);
	close($fh);

	my @metric_info;
	my $rec;
	foreach	$elem (@lines)
	{
		undef $rec;
		$elem = trim($elem);
		if($elem eq "") {next;}
		if($elem =~ /^#/) {next;}

		my ($name, $type) = split(/:/, $elem);
		$rec->{"NAME"} = trim($name);
		$rec->{"TYPE"} = trim($type);
		push @metric_info, $rec;
	}

	return @metric_info;
}

1;