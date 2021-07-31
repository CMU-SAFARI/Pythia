BEGIN {
  use lib 'lib';
  use strict;
  use Test::More;

  plan tests => 5;

  use_ok('Data::Dumper');
  use_ok('Math::Combinatorics');
}

my $c;
my $f = 0;
my @r;
my @data;

@data = ( 'a', [], 'b', [] );
$c = Math::Combinatorics->new(
                              data => \@data,
                              count => 2,
                             );
$f = 0;
while(my(@combo) = $c->next_combination){
  #warn "combo $f is: ".join " ", @combo;
  $f++;
}
ok($f == 6, ">>> $f == 6 <<<");

@data = ([],[],[],[]);
$c = Math::Combinatorics->new(
                              data => \@data,
                              count => 2,
                             );
$f = 0;
while(my(@combo) = $c->next_combination){
  $f++;
}
ok($f == 6, ">>> $f == 6 <<<");

@data = (1..10);
$c = Math::Combinatorics->new(
                              data => \@data,
                              count => 2,
                             );
$f = 0;
while(my(@combo) = $c->next_combination){
  $f++;
}
ok($f == 45, ">>> $f == 45 <<<");

