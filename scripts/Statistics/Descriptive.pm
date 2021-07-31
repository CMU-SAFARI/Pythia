package Statistics::Descriptive;

use strict;
use warnings;

##This module draws heavily from perltoot v0.4 from Tom Christiansen.

use 5.006;

use vars (qw($Tolerance $Min_samples_number));

our $VERSION = '3.0702';

$Tolerance = 0.0;
$Min_samples_number = 4;

use Statistics::Descriptive::Sparse;
use Statistics::Descriptive::Full;

package Statistics::Descriptive;

##All modules return true.
1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Statistics::Descriptive - Module of basic descriptive statistical functions.

=head1 VERSION

version 3.0702

=head1 SYNOPSIS

    use Statistics::Descriptive;
    my $stat = Statistics::Descriptive::Full->new();
    $stat->add_data(1,2,3,4);
    my $mean = $stat->mean();
    my $var = $stat->variance();
    my $tm = $stat->trimmed_mean(.25);
    $Statistics::Descriptive::Tolerance = 1e-10;

=head1 DESCRIPTION

This module provides basic functions used in descriptive statistics.
It has an object oriented design and supports two different types of
data storage and calculation objects: sparse and full. With the sparse
method, none of the data is stored and only a few statistical measures
are available. Using the full method, the entire data set is retained
and additional functions are available.

Whenever a division by zero may occur, the denominator is checked to be
greater than the value C<$Statistics::Descriptive::Tolerance>, which
defaults to 0.0. You may want to change this value to some small
positive value such as 1e-24 in order to obtain error messages in case
of very small denominators.

Many of the methods (both Sparse and Full) cache values so that subsequent
calls with the same arguments are faster.

=head1 VERSION

version 3.0702

=head1 METHODS

=head2 Sparse Methods

=over 5

=item $stat = Statistics::Descriptive::Sparse->new();

Create a new sparse statistics object.

=item $stat->clear();

Effectively the same as

  my $class = ref($stat);
  undef $stat;
  $stat = new $class;

except more efficient.

=item $stat->add_data(1,2,3);

Adds data to the statistics variable. The cached statistical values are
updated automatically.

=item $stat->count();

Returns the number of data items.

=item $stat->mean();

Returns the mean of the data.

=item $stat->sum();

Returns the sum of the data.

=item $stat->variance();

Returns the variance of the data.  Division by n-1 is used.

=item $stat->standard_deviation();

Returns the standard deviation of the data. Division by n-1 is used.

=item $stat->min();

Returns the minimum value of the data set.

=item $stat->mindex();

Returns the index of the minimum value of the data set.

=item $stat->max();

Returns the maximum value of the data set.

=item $stat->maxdex();

Returns the index of the maximum value of the data set.

=item $stat->sample_range();

Returns the sample range (max - min) of the data set.

=back

=head2 Full Methods

Similar to the Sparse Methods above, any Full Method that is called caches
the current result so that it doesn't have to be recalculated.  In some
cases, several values can be cached at the same time.

=over 5

=item $stat = Statistics::Descriptive::Full->new();

Create a new statistics object that inherits from
Statistics::Descriptive::Sparse so that it contains all the methods
described above.

=item $stat->add_data(1,2,4,5);

Adds data to the statistics variable.  All of the sparse statistical
values are updated and cached.  Cached values from Full methods are
deleted since they are no longer valid.

I<Note:  Calling add_data with an empty array will delete all of your
Full method cached values!  Cached values for the sparse methods are
not changed>

=item $stat->add_data_with_samples([{1 => 10}, {2 => 20}, {3 => 30},]);

Add data to the statistics variable and set the number of samples each value
has been built with. The data is the key of each element of the input array
ref, while the value is the number of samples: [{data1 => smaples1}, {data2 =>
samples2}, ...].

B<NOTE:> The number of samples is only used by the smoothing function and is
ignored otherwise. It is not equivalent to repeat count. In order to repeat
a certain datum more than one time call add_data() like this:

    my $value = 5;
    my $repeat_count = 10;
    $stat->add_data(
        [ ($value) x $repeat_count ]
    );

=item $stat->get_data();

Returns a copy of the data array.

=item $stat->get_data_without_outliers();

Returns a copy of the data array without outliers. The number minimum of
samples to apply the outlier filtering is C<$Statistics::Descriptive::Min_samples_number>,
4 by default.

A function to detect outliers need to be defined (see C<set_outlier_filter>),
otherwise the function will return an undef value.

The filtering will act only on the most extreme value of the data set
(i.e.: value with the highest absolute standard deviation from the mean).

If there is the need to remove more than one outlier, the filtering
need to be re-run for the next most extreme value with the initial outlier removed.

This is not always needed since the test (for example Grubb's test) usually can only detect
the most exreme value. If there is more than one extreme case in a set,
then the standard deviation will be high enough to make neither case an outlier.

=item $stat->set_outlier_filter($code_ref);

Set the function to filter out the outlier.

C<$code_ref> is the reference to the subroutine implementing the filtering
function.

Returns C<undef> for invalid values of C<$code_ref> (i.e.: not defined or not a
code reference), C<1> otherwise.

=over 4

=item

Example #1: Undefined code reference

    my $stat = Statistics::Descriptive::Full->new();
    $stat->add_data(1, 2, 3, 4, 5);

    print $stat->set_outlier_filter(); # => undef

=item

Example #2: Valid code reference

    sub outlier_filter { return $_[1] > 1; }

    my $stat = Statistics::Descriptive::Full->new();
    $stat->add_data( 1, 1, 1, 100, 1, );

    print $stat->set_outlier_filter( \&outlier_filter ); # => 1
    my @filtered_data = $stat->get_data_without_outliers();
    # @filtered_data is (1, 1, 1, 1)

In this example the series is really simple and the outlier filter function as well.
For more complex series the outlier filter function might be more complex
(see Grubbs' test for outliers).

The outlier filter function will receive as first parameter the Statistics::Descriptive::Full object,
as second the value of the candidate outlier. Having the object in the function
might be useful for complex filters where statistics property are needed (again see Grubbs' test for outlier).

=back

=item $stat->set_smoother({ method => 'exponential', coeff => 0, });

Set the method used to smooth the data and the smoothing coefficient.
See C<Statistics::Smoother> for more details.

=item $stat->get_smoothed_data();

Returns a copy of the smoothed data array.

The smoothing method and coefficient need to be defined (see C<set_smoother>),
otherwise the function will return an undef value.

=item $stat->sort_data();

Sort the stored data and update the mindex and maxdex methods.  This
method uses perl's internal sort.

=item $stat->presorted(1);

=item $stat->presorted();

If called with a non-zero argument, this method sets a flag that says
the data is already sorted and need not be sorted again.  Since some of
the methods in this class require sorted data, this saves some time.
If you supply sorted data to the object, call this method to prevent
the data from being sorted again. The flag is cleared whenever add_data
is called.  Calling the method without an argument returns the value of
the flag.

=item $stat->skewness();

Returns the skewness of the data.
A value of zero is no skew, negative is a left skewed tail,
positive is a right skewed tail.
This is consistent with Excel.

=item $stat->kurtosis();

Returns the kurtosis of the data.
Positive is peaked, negative is flattened.

=item $x = $stat->percentile(25);

=item ($x, $index) = $stat->percentile(25);

Sorts the data and returns the value that corresponds to the
percentile as defined in RFC2330:

=over 4

=item

For example, given the 6 measurements:

-2, 7, 7, 4, 18, -5

Then F(-8) = 0, F(-5) = 1/6, F(-5.0001) = 0, F(-4.999) = 1/6, F(7) =
5/6, F(18) = 1, F(239) = 1.

Note that we can recover the different measured values and how many
times each occurred from F(x) -- no information regarding the range
in values is lost.  Summarizing measurements using histograms, on the
other hand, in general loses information about the different values
observed, so the EDF is preferred.

Using either the EDF or a histogram, however, we do lose information
regarding the order in which the values were observed.  Whether this
loss is potentially significant will depend on the metric being
measured.

We will use the term "percentile" to refer to the smallest value of x
for which F(x) >= a given percentage.  So the 50th percentile of the
example above is 4, since F(4) = 3/6 = 50%; the 25th percentile is
-2, since F(-5) = 1/6 < 25%, and F(-2) = 2/6 >= 25%; the 100th
percentile is 18; and the 0th percentile is -infinity, as is the 15th
percentile, which for ease of handling and backward compatibility is returned
as undef() by the function.

Care must be taken when using percentiles to summarize a sample,
because they can lend an unwarranted appearance of more precision
than is really available.  Any such summary must include the sample
size N, because any percentile difference finer than 1/N is below the
resolution of the sample.

=back

(Taken from:
I<RFC2330 - Framework for IP Performance Metrics>,
Section 11.3.  Defining Statistical Distributions.
RFC2330 is available from:
L<http://www.ietf.org/rfc/rfc2330.txt> .)

If the percentile method is called in a list context then it will
also return the index of the percentile.

=item $x = $stat->quantile($Type);

Sorts the data and returns estimates of underlying distribution quantiles based on one
or two order statistics from the supplied elements.

This method use the same algorithm as Excel and R language (quantile B<type 7>).

The generic function quantile produces sample quantiles corresponding to the given probabilities.

B<$Type> is an integer value between 0 to 4 :

  0 => zero quartile (Q0) : minimal value
  1 => first quartile (Q1) : lower quartile = lowest cut off (25%) of data = 25th percentile
  2 => second quartile (Q2) : median = it cuts data set in half = 50th percentile
  3 => third quartile (Q3) : upper quartile = highest cut off (25%) of data, or lowest 75% = 75th percentile
  4 => fourth quartile (Q4) : maximal value

Example :

  my @data = (1..10);
  my $stat = Statistics::Descriptive::Full->new();
  $stat->add_data(@data);
  print $stat->quantile(0); # => 1
  print $stat->quantile(1); # => 3.25
  print $stat->quantile(2); # => 5.5
  print $stat->quantile(3); # => 7.75
  print $stat->quantile(4); # => 10

=item $stat->median();

Sorts the data and returns the median value of the data.

=item $stat->harmonic_mean();

Returns the harmonic mean of the data.  Since the mean is undefined
if any of the data are zero or if the sum of the reciprocals is zero,
it will return undef for both of those cases.

=item $stat->geometric_mean();

Returns the geometric mean of the data.

=item my $mode = $stat->mode();

Returns the mode of the data. The mode is the most commonly occurring datum.
See L<http://en.wikipedia.org/wiki/Mode_%28statistics%29> . If all values
occur only once, then mode() will return undef.

=item $stat->trimmed_mean(ltrim[,utrim]);

C<trimmed_mean(ltrim)> returns the mean with a fraction C<ltrim>
of entries at each end dropped. C<trimmed_mean(ltrim,utrim)>
returns the mean after a fraction C<ltrim> has been removed from the
lower end of the data and a fraction C<utrim> has been removed from the
upper end of the data.  This method sorts the data before beginning
to analyze it.

All calls to trimmed_mean() are cached so that they don't have to be
calculated a second time.

=item $stat->frequency_distribution_ref($num_partitions);

=item $stat->frequency_distribution_ref(\@bins);

=item $stat->frequency_distribution_ref();

C<frequency_distribution_ref($num_partitions)> slices the data into
C<$num_partitions> sets (where $num_partitions is greater than 1) and counts
the number of items that fall into each partition. It returns a reference to a
hash where the keys are the numerical values of the partitions used. The
minimum value of the data set is not a key and the maximum value of the data
set is always a key. The number of entries for a particular partition key are
the number of items which are greater than the previous partition key and less
then or equal to the current partition key. As an example,

   $stat->add_data(1,1.5,2,2.5,3,3.5,4);
   $f = $stat->frequency_distribution_ref(2);
   for (sort {$a <=> $b} keys %$f) {
      print "key = $_, count = $f->{$_}\n";
   }

prints

   key = 2.5, count = 4
   key = 4, count = 3

since there are four items less than or equal to 2.5, and 3 items
greater than 2.5 and less than 4.

C<frequency_distribution_refs(\@bins)> provides the bins that are to be used
for the distribution.  This allows for non-uniform distributions as
well as trimmed or sample distributions to be found.  C<@bins> must
be monotonic and must contain at least one element.  Note that unless the
set of bins contains the full range of the data, the total counts returned will
be less than the sample size.

Calling C<frequency_distribution_ref()> with no arguments returns the last
distribution calculated, if such exists.

=item my %hash = $stat->frequency_distribution($partitions);

=item my %hash = $stat->frequency_distribution(\@bins);

=item my %hash = $stat->frequency_distribution();

Same as C<frequency_distribution_ref()> except that it returns the hash
clobbered into the return list. Kept for compatibility reasons with previous
versions of Statistics::Descriptive and using it is discouraged.

=item $stat->least_squares_fit();

=item $stat->least_squares_fit(@x);

C<least_squares_fit()> performs a least squares fit on the data,
assuming a domain of C<@x> or a default of 1..$stat->count().  It
returns an array of four elements C<($q, $m, $r, $rms)> where

=over 4

=item C<$q and $m>

satisfy the equation C($y = $m*$x + $q).

=item C<$r>

is the Pearson linear correlation cofficient.

=item C<$rms>

is the root-mean-square error.

=back

If case of error or division by zero, the empty list is returned.

The array that is returned can be "coerced" into a hash structure
by doing the following:

  my %hash = ();
  @hash{'q', 'm', 'r', 'err'} = $stat->least_squares_fit();

Because calling C<least_squares_fit()> with no arguments defaults
to using the current range, there is no caching of the results.

=back

=head1 REPORTING ERRORS

I read my email frequently, but since adopting this module I've added 2
children and 1 dog to my family, so please be patient about my response
times.  When reporting errors, please include the following to help
me out:

=over 4

=item *

Your version of perl.  This can be obtained by typing perl C<-v> at
the command line.

=item *

Which version of Statistics::Descriptive you're using.  As you can
see below, I do make mistakes.  Unfortunately for me, right now
there are thousands of CD's with the version of this module with
the bugs in it.  Fortunately for you, I'm a very patient module
maintainer.

=item *

Details about what the error is.  Try to narrow down the scope
of the problem and send me code that I can run to verify and
track it down.

=back

=head1 AUTHOR

Current maintainer:

Shlomi Fish, L<http://www.shlomifish.org/> , C<shlomif@cpan.org>

Previously:

Colin Kuskie

My email address can be found at http://www.perl.com under Who's Who
or at: https://metacpan.org/author/COLINK .

=head1 CONTRIBUTORS

Fabio Ponciroli & Adzuna Ltd. team (outliers handling)

=head1 REFERENCES

RFC2330, Framework for IP Performance Metrics

The Art of Computer Programming, Volume 2, Donald Knuth.

Handbook of Mathematica Functions, Milton Abramowitz and Irene Stegun.

Probability and Statistics for Engineering and the Sciences, Jay Devore.

=head1 COPYRIGHT

Copyright (c) 1997,1998 Colin Kuskie. All rights reserved.  This
program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

Copyright (c) 1998 Andrea Spinelli. All rights reserved.  This program
is free software; you can redistribute it and/or modify it under the
same terms as Perl itself.

Copyright (c) 1994,1995 Jason Kastner. All rights
reserved.  This program is free software; you can redistribute it
and/or modify it under the same terms as Perl itself.

=head1 LICENSE

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

=head1 AUTHOR

Shlomi Fish <shlomif@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 1997 by Jason Kastner, Andrea Spinelli, Colin Kuskie, and others.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=head1 BUGS

Please report any bugs or feature requests on the bugtracker website
L<https://github.com/shlomif/perl-Statistics-Descriptive/issues>

When submitting a bug or request, please include a test-file or a
patch to an existing test-file that illustrates the bug or desired
feature.

=for :stopwords cpan testmatrix url annocpan anno bugtracker rt cpants kwalitee diff irc mailto metadata placeholders metacpan

=head1 SUPPORT

=head2 Perldoc

You can find documentation for this module with the perldoc command.

  perldoc Statistics::Descriptive

=head2 Websites

The following websites have more information about this module, and may be of help to you. As always,
in addition to those websites please use your favorite search engine to discover more resources.

=over 4

=item *

MetaCPAN

A modern, open-source CPAN search engine, useful to view POD in HTML format.

L<https://metacpan.org/release/Statistics-Descriptive>

=item *

Search CPAN

The default CPAN search engine, useful to view POD in HTML format.

L<http://search.cpan.org/dist/Statistics-Descriptive>

=item *

RT: CPAN's Bug Tracker

The RT ( Request Tracker ) website is the default bug/issue tracking system for CPAN.

L<https://rt.cpan.org/Public/Dist/Display.html?Name=Statistics-Descriptive>

=item *

AnnoCPAN

The AnnoCPAN is a website that allows community annotations of Perl module documentation.

L<http://annocpan.org/dist/Statistics-Descriptive>

=item *

CPAN Ratings

The CPAN Ratings is a website that allows community ratings and reviews of Perl modules.

L<http://cpanratings.perl.org/d/Statistics-Descriptive>

=item *

CPANTS

The CPANTS is a website that analyzes the Kwalitee ( code metrics ) of a distribution.

L<http://cpants.cpanauthors.org/dist/Statistics-Descriptive>

=item *

CPAN Testers

The CPAN Testers is a network of smoke testers who run automated tests on uploaded CPAN distributions.

L<http://www.cpantesters.org/distro/S/Statistics-Descriptive>

=item *

CPAN Testers Matrix

The CPAN Testers Matrix is a website that provides a visual overview of the test results for a distribution on various Perls/platforms.

L<http://matrix.cpantesters.org/?dist=Statistics-Descriptive>

=item *

CPAN Testers Dependencies

The CPAN Testers Dependencies is a website that shows a chart of the test results of all dependencies for a distribution.

L<http://deps.cpantesters.org/?module=Statistics::Descriptive>

=back

=head2 Bugs / Feature Requests

Please report any bugs or feature requests by email to C<bug-statistics-descriptive at rt.cpan.org>, or through
the web interface at L<https://rt.cpan.org/Public/Bug/Report.html?Queue=Statistics-Descriptive>. You will be automatically notified of any
progress on the request by the system.

=head2 Source Code

The code is open to the world, and available for you to hack on. Please feel free to browse it and play
with it, or whatever. If you want to contribute patches, please send me a diff or prod me to pull
from your repository :)

L<https://github.com/shlomif/perl-Statistics-Descriptive>

  git clone git://github.com/shlomif/perl-Statistics-Descriptive.git

=cut
