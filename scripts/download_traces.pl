#!/usr/bin/perl

use lib '/mnt/panzer/rahbera/ChampSim/scripts';
use warnings;
use Getopt::Long;

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
	    $cmd = "megadl $trace_file_url --path=$dir";
    }
    else
    {
	    $cmd = "wget $trace_file_url -O $dir/$trace_file_name";
    }
    system($cmd);
    #print("$cmd\n");
}

sub  trim { my $s = shift; $s =~ s/^\s+|\s+$//g; return $s };
