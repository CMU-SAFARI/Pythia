#!/usr/bin/perl -w

BEGIN { unshift @INC, './lib', '../lib';
    require Config; import Config;
}

$test = 0;
$| = 1;
print "1..",&last,"\n";

sub test {
  $test++; if (shift) {print "ok $test\n";1} else {print "not ok $test\n";0}
}

use Math::Pari;

my $x=PARI(0);
test( eval{$x/$x} || eval {$x**-1} || 1);
sub last {1}
