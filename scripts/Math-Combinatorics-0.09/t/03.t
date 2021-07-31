BEGIN {
  use lib 'lib';
  use strict;
  use Test::More;

  plan tests => 3;

  use_ok('Data::Dumper');
  use_ok('Math::Combinatorics');
}

my @S = qw(d a c b);
my @R = map {\$_} qw(d a c b);

my $rsort = sub { my ($a,$b) = @_;  return $b cmp $a };
#my $rsort = sub { my ($a,$b) = @_;  return $b cmp $a };

my $set     = Math::Combinatorics->new(
    count   => scalar(@S)  ,
    data    =>      [ @S ] ,
    compare => $rsort      ,
);
$c = join '', $set->next_combination();
is( $c, q{dcba}, q{expecting reverse ordered array}      );

__DATA__
#! /usr/bin/perl

use warnings;
use strict;

use lib q{.};
use Math::Combinatorics qw<combine>;
use Data::Dumper;

use Test::More tests => 5;

    # test with a regular array and with an array of references
    #
my @TEST_ARR =    qw{d a c b};
my @TEST_REF = \( qw{d a c b} );

# Combine without passing a compare function, Combination should be sorted
# by the internal compare function, which sorts alphabetically.
#
{
    my $c = join q{}, @{ (combine(scalar @TEST_ARR, @TEST_ARR))[0] };
    is( $c, q{abcd}, q{expecting ordered array} );
}

# Combine with passing a compare function as defined in the manual. I expect
# it to die.
#
{
    my $alphabetically = sub {
        my $a = shift;   die qq{REFARGS\n}  if ref $a;
        my $b = shift;   die qq{REFARGS\n}  if ref $b;
        return $a cmp $b;
    };
    my $c = join q{}, eval {
        my $set     = Math::Combinatorics->new(
            count   => scalar @TEST_ARR   ,
            data    =>      [ @TEST_ARR ] ,
            compare => $alphabetically    ,
        );
        $set->next_combination();
    };
    is( $@, q{},     q{expecting compare not to die} );
    is( $c, q{abcd}, q{expecting ordered array}      );
}

# do the same with a list of references
#
{
    my $alphabetically = sub {
        my $a = shift;   die qq{NOREFARGS\n}  unless q{SCALAR} eq ref $a;
        my $b = shift;   die qq{NOREFARGS\n}  unless q{SCALAR} eq ref $a;
        return $$a cmp $$b;
    };

    # we now have an array of refs, so we need a map {} in between
    #
    my $c = join q{}, map { $$_ } eval {
        my $set     = Math::Combinatorics->new(
            count   => scalar @TEST_REF   ,
            data    =>      [ @TEST_REF ] ,
            compare => $alphabetically    ,
        );
        $set->next_combination();
    };
    is( $@, q{},     q{expecting compare not to die} );
    is( $c, q{abcd}, q{expecting ordered array}      );
}
