#!/usr/bin/perl

package Exp;
use warnings;

sub parse
{
	my ($filename) = @_;
	open($fh, '<', $filename) or die "cannot open file $trace_file\n";
	chomp(my @lines = <$fh>);
	close($fh);

	my %exp_configs;
	my @exps;
	my $exp;
	foreach $elem (@lines)
	{
		$elem =~ s/^\s+|\s+$//g;
		if($elem eq "") {next;}
		if($elem =~ /^#/) {next;}
		
		my @tokens = split(/\s+/, $elem);	
		if($tokens[1] eq "=") # exp config variable
		{
			$exp_configs{$tokens[0]} = join(" ", @tokens[2..$#tokens]);
			#print("config: $tokens[0]\n");
		}
		else # exp declaration
		{
			$exp->{"NAME"} = $tokens[0];
			my @args;
			foreach $token (@tokens[1..$#tokens])
			{
				if($token =~ /^\$/) # exp config variable
				{
					$token =~ s/\$|\(|\)//g;
					if(exists($exp_configs{$token}))
					{
						push(@args, $exp_configs{$token});
					}
					else 
					{
						die "$token is not defined before exp $tokens[0]\n";
					}
				}
				else
				{
					push(@args, $token);
				}
			}
			$exp->{"KNOBS"} = join(" ", @args);
			push(@exps, $exp);
			undef $args;
			undef $exp;
		}
	}

	return @exps;
}

1;