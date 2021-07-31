package Statistics::Descriptive::Smoother;

use strict;
use warnings;

use Carp;

our $VERSION = '3.0702';

sub instantiate {
    my ($class, $args) = @_;

    my $method     = delete $args->{method};
    my $coeff      = delete $args->{coeff} || 0;
    my $ra_samples = delete $args->{samples};
    my $ra_data    = delete $args->{data};

    if ($coeff < 0 || $coeff > 1) {
        carp("Invalid smoothing coefficient C $coeff\n");
        return;
    }
    if (@$ra_data < 2) {
        carp("Need at least 2 samples to smooth the data\n");
        return;
    }
    $method = ucfirst(lc($method));
    my $sub_class = __PACKAGE__."::$method";
    eval "require $sub_class";
    die "No such class $sub_class: $@" if $@;

    return $sub_class->_new({
        data       => $ra_data,
        samples    => $ra_samples,
        count      => scalar @$ra_data,
        coeff      => $coeff,
    });
}

sub get_smoothing_coeff { $_[0]->{coeff} }

sub set_smoothing_coeff {
    my ($self, $coeff) = @_;

    if ($coeff < 0 || $coeff > 1) {
        carp("Invalid smoothing coefficient C $coeff\n");
        return;
    }

    $self->{coeff} = $coeff;
    return 1;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Statistics::Descriptive::Smoother - Base module for smoothing statistical data

=head1 VERSION

version 3.0702

=head1 SYNOPSIS

  use Statistics::Descriptive::Smoother;
  my $smoother = Statistics::Descriptive::Smoother->instantiate({
           method   => 'exponential',
           coeff    => 0.5,
           data     => [1, 2, 3, 4, 5],
           samples  => [110, 120, 130, 140, 150],
    });
  my @smoothed_data = $smoother->get_smoothed_data();

=head1 DESCRIPTION

This module provide methods to smooth the trend of a series of statistical data.

The methods provided are the C<Exponential> and the C<Weighted Exponential> (see respectively
C<Statistics::Descriptive::Smoother::Exponential> and C<Statistics::Descriptive::Smoother::Weightedexponential>
for more details).

This class is just a factory that will instantiate the object to perform the
chosen smoothing algorithm.

=head1 VERSION

version 3.0702

=head1 METHODS

=over 5

=item Statistics::Descriptive::Smoother->instantiate({});

Create a new Smoother object.

This method require several parameters:

=over 5

=item method

Method used for the smoothing. Allowed values are: C<exponential> and C<weightedexponential>

=item coeff

Smoothing coefficient. It needs to be in the [0;1] range, otherwise undef will be reutrned.
C<0> means that the series is not smoothed at all, while C<1> the series is universally equal to the initial unsmoothed value.

=item data

Array ref with the data of the series. At least 2 values are needed to smooth the series, undef is returned otherwise.

=item samples

Array ref with the samples each data value has been built with. This is an optional parameter since it is not used by all the
smoothing algorithm.

=back

=item $smoother->get_smoothing_coeff();

Returns the smoothing coefficient.

=item $smoother->set_smoothing_coeff(0.5);

Set the smoothing coefficient value. It needs to be in the [0;1] range, otherwise undef will be reutrned.

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

  perldoc Statistics::Descriptive::Smoother

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
