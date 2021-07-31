BEGIN {
  use lib 'lib';
  use strict;
  use Test::More;

  plan tests => 17;

  use_ok('Data::Dumper');
  use_ok('Math::Combinatorics');
}


my @data = qw( a b c d );

my $f = 0;
my @r;
my $c;

#####################
# next_combination()
#####################

$c = Math::Combinatorics->new(
                              data => \@data,
                              count => 2,
                             );

$f = 0;
while(my(@combo) = $c->next_combination){
  $f++;
}
ok($f == 6);

$c = Math::Combinatorics->new(
                              data => \@data,
                              count => 3,
                             );
$f = 0;
while(my(@combo) = $c->next_combination){
  $f++;
}
ok($f == 4);

@data = ( 'a', [], 'b', [] );
$c = Math::Combinatorics->new(
                              data => \@data,
                              count => 2,
                             );
$f = 0;
while(my(@combo) = $c->next_combination){
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

#####################
# combine()
#####################
@data = qw( a b c d );

@r = combine(2,@data);
ok(scalar(@r) == 6, ">>> ".scalar(@r)." == 6 <<<");

@r = combine(3,@data);
ok(scalar(@r) == 4, ">>> ".scalar(@r)." == 6 <<<");

#####################
# next_multiset
#####################

$c = Math::Combinatorics->new(
                              data => \@data,
                              frequency => [1,1,1,1],
                              count => 2,
                             );
$f = 0;
while(my(@combo) = $c->next_multiset){
  $f++;
}
ok($f == 6, ">>> $f == 6 <<<");

$c = Math::Combinatorics->new(
                              data => \@data,
                              frequency => [1,1,1,2],
                              count => 2,
                             );
$f = 0;
while(my(@combo) = $c->next_multiset){
  $f++;
}
ok($f == 7, ">>> $f == 7 <<<");

#####################
# next_permutation()
#####################


$c = Math::Combinatorics->new(
                              data => \@data,
                              count => 2,
                             );

$f = 0;
while(my(@combo) = $c->next_permutation){
  $f++;
}
ok($f == 24);

#####################
# permute()
#####################

@r = permute(@data);
ok(scalar(@r) == 24);

#####################
# next_string
#####################

$c = Math::Combinatorics->new(
                              data => \@data,
                              frequency => [1,1,1,1],
                             );
$f = 0;
while(my(@combo) = $c->next_string){
  $f++;
}
ok($f == 24);

$c = Math::Combinatorics->new(
                              data => \@data,
                              frequency => [1,1,1,2],
                             );
$f = 0;
while(my(@combo) = $c->next_string){
  $f++;
}
ok($f == 60);

#####################
# next_derangement()
#####################

$c = Math::Combinatorics->new(
                              data => \@data,
                              count => 2,
                             );

$f = 0;
while(my(@combo) = $c->next_derangement){
#warn join ' ', @combo;
  $f++;
}
ok($f == 9);

#####################
# derange()
#####################

@r = derange(@data);
ok(scalar(@r) == 9);

#####################
