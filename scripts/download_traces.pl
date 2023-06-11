#!/usr/bin/perl

use warnings;
use Getopt::Long;

die "\$PYTHIA_HOME env variable is not defined.\nHave you sourced setvars.sh?\n" unless defined $ENV{'PYTHIA_HOME'};

my $megatool_exe = "$ENV{'PYTHIA_HOME'}/scripts/megatools-1.11.1.20230212-linux-x86_64/megatools";
my $input_file;
my $dir=".";
GetOptions('csv=s' => \$input_file,
            'dir=s' => \$dir,
) or die "Usage $0 --csv <csv file containing name and URL of traces> --dir <directory to store>\n";

die "Supply csv file with --csv" unless defined $input_file;

open(my $fh, '<', $input_file) or die "Could not open $input_file\n";
chomp(my @lines = <$fh>);
close($fh);

foreach my $line (@lines)
{
    my @tokens = split(',', trim($line));
    $trace_file_name = trim($tokens[0]);
    $trace_file_url = trim($tokens[1]);
    print "Downloading $trace_file_name...\n";
    my $cmd;
    if($trace_file_url =~ /mega\.nz/)
    {
	    $cmd = "$megatool_exe dl --path=$dir $trace_file_url";
    }
    else
    {
	    $cmd = "wget --no-check-certificate $trace_file_url -O $dir/$trace_file_name";
    }
    system($cmd);
    #print("$cmd\n");
}

my $total_traces = `wc -l $input_file | awk '{print \$1}'`;
my $downloaded = `ls -1 $dir | wc -l`;
chomp($total_traces);
chomp($downloaded);

print "\n";
print "================================\n";
print "Trace downloading completed\n";
print "Downloaded $downloaded/$total_traces traces\n";
print "================================\n";

sub  trim { my $s = shift; $s =~ s/^\s+|\s+$//g; return $s };
