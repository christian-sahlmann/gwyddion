package Gwyddion::dump;
# @(#) $Id$
# Written by Yeti <yeti@gwyddion.net>, Public domain.

=head1 NAME

Gwyddion::dump -- Gwyddion plug-in proxy dump dumb file format handling.

=head1 FUNCTIONS

=over

=cut

use warnings;
use strict;

require Exporter;

use IO::File;

our @ISA = qw( Exporter );
our @EXPORT_OK = qw( read write );
our %EXPORT_TAGS = ( all => [ @EXPORT_OK ] );

# Sanity check
our $sizeof_double = length pack 'd', 42;
die if $sizeof_double != 8;

sub _dmove {
    my ( $hash1, $key1, $hash2, $key2, $type ) = @_;

    return if ! exists $hash1->{ $key1 };
    my $value = $hash1->{ $key1 };
    if ( defined $type ) {
        if ( $type eq 'int' ) { $value = int $value }
        elsif ( $type eq 'float' ) { $value = $value + 0.0 }
        else { die "Internal error: Cannot convert to " . $type }
    }
    $hash2->{ $key2 } = $value;
    delete $hash1->{ $key1 };
}

sub _read_dfield {
    my ( $fh, $data, $base ) = @_;
    my ( $c, $a, %dfield, $n, @a );

    $fh->read( $c, 1 );
    die if $c ne '[';
    _dmove( $data, $base . '/xres', \%dfield, 'xres', 'int' );
    _dmove( $data, $base . '/yres', \%dfield, 'yres', 'int' );
    _dmove( $data, $base . '/xreal', \%dfield, 'xreal', 'float' );
    _dmove( $data, $base . '/yreal', \%dfield, 'yreal', 'float' );
    _dmove( $data, $base . '/unit-xy', \%dfield, 'unit-xy' );
    _dmove( $data, $base . '/unit-z', \%dfield, 'unit-z' );
    $n = $dfield{ 'xres' } * $dfield{ 'yres' };
    $fh->read( $a, $n*$sizeof_double );
    @a = unpack( "d[$n]", $a );
    $dfield{ 'data' } = \@a;
    $c = $fh->getline();
    die if $c ne "]]\n";
    $data->{ $base } = \%dfield;
}

=item read( filename )

Read a Gwyddion plug-in proxy dump file.

The file is returned as a hash table of dump key, value pairs.

Data fields are packed as references to hashes with following keys
(not all has to be present):
`xres' (x-resolution, in number of samples),
`yres' (y-resolution, in number of samples),
`xreal' (real x size, in base SI units),
`yreal' (real y size, in base SI units),
`unit-xy' (lateral units, base SI, like `m'),
`unit-z' (value units, base SI, like `m' or `A'),
`data' (the data field data itself, an array of floats).

Fatal errors are not handled, the function simply dies.  If you have
anything meaningful to do after a fatal error, you have to catch
the error.

=cut
sub read {
    my $fh = new IO::File;
    my $line_re = "^([^=]+)=(.*)\n";
    my $field_re = "^([^=]+)=\\[\n";
    my %data;

    die if ! $fh->open( $_[0], '<:bytes' );
    while ( my $line = $fh->getline() ) {
        if ( $line =~ m/$field_re/ ) {
            my $key = $1;
            _read_dfield( $fh, \%data, $key );
        }
        elsif ( $line =~ m/$line_re/ ) {
            my $key = $1;
            my $val = $2;
            $data{ $key } = $val;
        }
        else {
            die "Can't understand input\n"
        }
    }
    $fh->close();
    return \%data;
}

sub _dwrite {
    my ( $fh, $dfield, $base, $key, $fmt ) = @_;

    local $, = undef;
    local $\ = undef;
    if ( exists $dfield->{ $key } ) {
        $fh->printf( '%s/%s=' . $fmt . "\n", $base, $key, $dfield->{ $key } )
    }
}

sub _write_dfield {
    my ( $fh, $dfield, $base ) = @_;
    my ( $n, $a );

    local $, = undef;
    local $\ = undef;
    _dwrite( $fh, $dfield, $base, 'xres', '%d' );
    _dwrite( $fh, $dfield, $base, 'yres', '%d' );
    _dwrite( $fh, $dfield, $base, 'xreal', '%g' );
    _dwrite( $fh, $dfield, $base, 'yreal', '%g' );
    _dwrite( $fh, $dfield, $base, 'unit-xy', '%s' );
    _dwrite( $fh, $dfield, $base, 'unit-z', '%s' );
    $n = $dfield->{ 'xres' }*$dfield->{ 'yres' };
    $fh->print( "$base=[\n[" );
    $a = $dfield->{ 'data' };
    $a = pack( "d[$n]", @$a );
    $fh->print( "$a]]\n" );
}

=item write( data, filename )

Write a Gwyddion plug-in proxy dump file.

The hash table to write is expected to follow the same conventions as
those returned by read(), please see its description for more.

Fatal errors are not handled, the function simply dies.  If you have
anything meaningful to do after a fatal error, you have to catch
the error.

=cut
sub write {
    my $fh = new IO::File;
    my $data = $_[0];

    $fh->open( $_[1], '>:bytes' );
    local $, = undef;
    local $\ = undef;
    for my $k ( keys %$data ) {
        my $v = $data->{ $k };
        next if ref $v;
        $fh->print( "$k=$v\n" );
    }
    for my $k ( keys %$data ) {
        my $v = $data->{ $k };
        next if not ref $v;
        _write_dfield( $fh, $v, $k );
    }
    $fh->close();
}

1;
