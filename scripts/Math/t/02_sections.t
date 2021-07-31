#!/usr/bin/perl -w

BEGIN { unshift @INC, './lib', '../lib';
    require Config; import Config;
}

$test = 0;
$| = 1;
my %sec = qw(1 max 2 lift 3 erfc 4 factorint 5 elleta 6 idealnorm 7 polresultant 8 matrank 9 sumalt 10 ploth 11 forstep); # 2.3.5
my $secs = keys %sec;
my $mx = 10;
$mx < $_ and $mx = $_ for keys %sec;
print "1..", &last ,"\n";

sub test {
  $test++; if (shift) {print "ok $test\n";1} else {print "not ok $test\n";0}
}

use Math::Pari;

my %secOf;
for my $sec (0..$mx) {
  $secOf{$_} = $sec for Math::Pari::listPari($sec);
  test(1);
}
for my $sec (sort keys %sec) {
  my $f = $sec{$sec};
#  warn "$f => actual=$secOf{$f}\n";
  warn "Mismatch: $f => stored=$sec, actual=$secOf{$f}\n"
   unless test( $secOf{$f} == $sec );
}
sub last {1+$mx+keys %sec}
