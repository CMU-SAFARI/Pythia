#!/usr/bin/perl

package Trace;
use warnings;

sub parse
{
	my ($filename) = @_;
	open($fh, '<', $filename) or die "cannot open file $trace_file\n";
	chomp(my @lines = <$fh>);
	close($fh);

	my @trace_info;
	my $rec;
	undef $rec;
	foreach	$elem (@lines)
	{
		if($elem ne "")
		{
			my $idx = index($elem, "=");
			my $key = substr($elem, 0, $idx);
			my $value = substr($elem, $idx+1);
			# my ($key, $value) = split(/=/, $elem);
			if($key eq "NAME")
			{
				if(defined $rec)
				{
					push @trace_info, $rec;
					undef $rec;
				}
			}
			$rec->{$key} = $value;
		}
	}
	push @trace_info, $rec;

	return @trace_info;
}

1;