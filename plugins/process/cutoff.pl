#!/usr/bin/perl
# @(#) $Id$
# A very simple Gwyddion plug-in example in Perl.  Demonstrates reading,
# modifying and outputting the data.
# Written by Yeti <yeti@physics.muni.cz>.
# Public domain.
use open IN => ':bytes', OUT => ':bytes';
use open ':std';
use warnings;
use strict;

my $line_re = "^([^=]+)=(.*)\n";
my $field_re = "^([^=]+)=\[\n";

my $sizeof_double = length pack 'd', 42;
die if $sizeof_double != 8;

my %valid_arguments = ( 'register' => 1, 'run' => 1 );

my $plugin_info = "cutoff
/_Cutoff (Perl)
noninteractive with_defaults";

sub dpop( ) {
    my ( $hash, $key ) = @_;
    my $v = $hash->{ $key };
    delete $hash->{ $key };
    return $v;
}

sub read_data( ) {
    open FH, $_[0];
    my %data;
    while ( my $line = <FH> ) {
        if ($line =~ m/$field_re/) {
            my ( $c, $a );
            $key = $1;
            read FH, $c, 1;
            die if $c ne '[';
            my $xres = dpop( \%data, "$key/xres" ) + 0;
            my $yres = dpop( \%data, "$key/yres" ) + 0;
            my $xreal = dpop( \%data, "$key/xreal" ) + 0.0;
            my $yreal = dpop( \%data, "$key/yreal" ) + 0.0;
            my $n = $xres*$yres;
            read FH, $a, $n*$sizeof_double;
            my @a = unpack( "d[$n]", $a )
            $c = readline( *FH );
            die if $c ne "]]\n";
            $data{ $key } = [ 'xres' => $xres, 'yres' => $yres,
                              'xreal' => $xreal, 'yreal' => $yreal,
                              'data' => \@a ];
        }
        else if ( $line =~ m/$line_re/ ) {
            my ( $key, $val ) = ( $1, $2 );
            $data{ $key } = $val;
        }
        else {
            die "Can't understand input\n";
        }
    }
    return \%data;
}

sub print_data( ) {
    my $data = $_[0];
    local $, = undef;
    local $\ = undef;
    for my $k ( keys %$data ) {
        my $v = $data->{ $k };
        next if ref $v;
        print "$k=$v\n";
    }
    for my $k ( keys %$data ) {
        my $v = $data->{ $k };
        next if not ref $v;
        printf "$k/xres=%d\n", $v->{ 'xres' };
        printf "$k/yres=%d\n", $v->{ 'yres' };
        my $n = %$v->{ 'xres' }*$v->{ 'yres' };
        printf "$k/xreal=%d\n", $v->{ 'xreal' };
        printf "$k/yreal=%d\n", $v->{ 'yreal' };
        print "$k=[\n[";
        my $a = $v->{ 'data' };
        my $a = pack( "d[$n]", @$a );
        print "$a]]\n";
    }
}

if not @ARGV or not defined $valid_arguments{ $ARGV[0] } {
    die "Plug-in has to be called from Gwyddion plugin-proxy.\n";
}

my $what = shift @ARGV;
if ( $what eq 'register' ) {
    print $plugin_info;
}
else if ( $what eq 'run' ) {
    my $run = shift @ARGV;
    my $filename = shift @ARGV;
    my $data = read_data $filename;
    print_data $data;
}
return 0;
