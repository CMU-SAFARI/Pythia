package Statistics::Descriptive::Smoother::Weightedexponential;
use strict;
use warnings;

use Carp;
use base 'Statistics::Descriptive::Smoother';

our $VERSION = '3.0702';

sub _new {
    my ($class, $args) = @_;

    if (scalar @{$args->{data}} != scalar @{$args->{samples}}) {
        carp("Number of data values and samples need to be the same\n");
        return;
    }

    return bless $args || {}, $class;
}

# The name of the variables used in the code refers to the explanation in the pod
sub get_smoothed_data {
    my ($self) = @_;

    my (@smoothed_values, @Wts);
    # W(0) = N(0)
    push @Wts, @{$self->{samples}}[0];
    # S(0) = X(0)
    push @smoothed_values, @{$self->{data}}[0];
    my $C = $self->get_smoothing_coeff();

    foreach my $idx (1 .. ($self->{count} -1)) {
        my $Xt   = $self->{data}->[$idx];
        my $Nt   = $self->{samples}->[$idx];
        my $St_1 = $smoothed_values[-1];
        my $Wt_1 = $Wts[-1];

        push @Wts, $self->_get_Wt($Wt_1, $Nt);

        my $coeff_a = $self->_get_coeff_A($Wt_1, $Nt);
        my $coeff_b = $self->_get_coeff_B($Wt_1, $Nt);

        my $smoothed_value = ( $St_1 * $coeff_a + $Xt * $coeff_b ) / ( $coeff_a + $coeff_b );
        push @smoothed_values, $smoothed_value;
    }
    return @smoothed_values;
}

sub _get_Wt {
    my ($self, $Wt_1, $Nt) = @_;

    my $C = $self->get_smoothing_coeff();
    my $coeff_a = $self->_get_coeff_A($Wt_1, $Nt);
    my $coeff_b = $self->_get_coeff_B($Wt_1, $Nt);;

    return (($Wt_1 * $coeff_a + $Nt * $coeff_b)/($coeff_a + $coeff_b));
}

sub _get_coeff_A {
    my ($self, $Wt_1, $Nt) = @_;

    my $C = $self->get_smoothing_coeff();
    return $C * ( $Wt_1 / ($Wt_1 + $Nt) );
}

sub _get_coeff_B {
    my ($self, $Wt_1, $Nt) = @_;

    my $C = $self->get_smoothing_coeff();
    return (1 - $C) * ( $Nt / ($Nt + $Wt_1) );
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Statistics::Descriptive::Smoother::Weigthedexponential - Implement weighted
exponential smoothing

=head1 VERSION

version 3.0702

=head1 SYNOPSIS

  use Statistics::Descriptive::Smoother;
  my $smoother = Statistics::Descriptive::Smoother->instantiate({
           method   => 'weightedexponential',
           coeff    => 0.5,
           data     => [1, 2, 3, 4, 5],
           samples  => [110, 120, 130, 140, 150],
    });
  my @smoothed_data = $smoother->get_smoothed_data();

=head1 DESCRIPTION

This module implement the weighted exponential smoothing algorithm to smooth
the trend of a series of statistical data.

This algorithm can help to control large swings in the unsmoothed data that
arise from small samples for those data points.

The algorithm implements the following formula:

W(0) = N(0)

W(t) = ( W(t-1) * CoeffA + N(t) * CoeffB ) / (CoeffA + CoeffB)

CoeffA = C * ( W(t-1) / (W(t-1) + N(t) ) )

CoeffB = (1 - C) * ( N(t) * (W(t-1) + N(t)) )

S(t) = (S(t-1)*CoeffA + X(t)*CoeffB) / (CoeffA + CoeffB)

where:

=over 3

=item * t = index in the series

=item * S(t) = smoothed series value at position t

=item * C = smoothing coefficient. Value in the [0;1] range. C<0> means that the series is not smoothed at all,
while C<1> the series is universally equal to the initial unsmoothed value.

=item * X(t) = unsmoothed series value at position t

=back

=head1 VERSION

version 3.0702

=head1 METHODS

=over 5

=item $stats->get_smoothed_data();

Returns a copy of the smoothed data array.

=back

=head1 AUTHOR

Fabio Ponciroli

=head1 COPYRIGHT

Copyright(c) 2012 by Fabio Ponciroli.

=head1 LICENSE

This file is licensed under the MIT/X11 License:
http://www.opensource.org/licenses/mit-license.php.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

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

  perldoc Statistics::Descriptive::Smoother::Weightedexponential

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
