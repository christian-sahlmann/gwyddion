#!/usr/bin/perl
# @(#) $Id$
# A very simple Gwyddion plug-in example in Perl.  Demonstrates reading,
# modifying and outputting the data.
# Written by Yeti <yeti@gwyddion.net>.
# Public domain.
use warnings;
use strict;

# Plug-in information.
# Format is similar to GwyProcessFuncInfo:
# - Name (just an unique identifier)
# - Menu path
# - Run modes (possible values: noninteractive with_defaults interactive modal)
my $plugin_info =
"cutoff
/_Test/_Cutoff (Perl)
noninteractive with_defaults
";

my %valid_arguments = ( 'register' => 1, 'run' => 1 );

my $line_re = "^([^=]+)=(.*)\n";
my $field_re = "^([^=]+)=\\[\n";

my $sizeof_double = length pack 'd', 42;
die if $sizeof_double != 8;

sub dpop {
    my ( $hash, $key ) = @_;
    my $v = $hash->{ $key };
    delete $hash->{ $key };
    return $v;
}

sub read_data {
    open FH, '<:bytes', $_[0];
    my %data;
    while ( my $line = <FH> ) {
        if ( $line =~ m/$field_re/ ) {
            my $key = $1;
            my ( $c, $a );
            read FH, $c, 1;
            die if $c ne '[';
            my $xres = dpop( \%data, "$key/xres" ) + 0;
            my $yres = dpop( \%data, "$key/yres" ) + 0;
            my $xreal = dpop( \%data, "$key/xreal" ) + 0.0;
            my $yreal = dpop( \%data, "$key/yreal" ) + 0.0;
            my $n = $xres*$yres;
            read FH, $a, $n*$sizeof_double;
            my @a = unpack( "d[$n]", $a );
            $c = readline( *FH );
            die if $c ne "]]\n";
            $data{ $key } = { 'xres' => $xres, 'yres' => $yres,
                              'xreal' => $xreal, 'yreal' => $yreal,
                              'data' => \@a };
        }
        elsif ( $line =~ m/$line_re/ ) {
            my $key = $1;
            my $val = $2;
            $data{ $key } = $val;
        }
        else {
            die "Can't understand input\n";
        }
    }
    close FH;
    return \%data;
}

sub print_data {
    open FH, '>:bytes', $_[0];
    my $data = $_[1];
    local $, = undef;
    local $\ = undef;
    for my $k ( keys %$data ) {
        my $v = $data->{ $k };
        next if ref $v;
        print FH "$k=$v\n";
    }
    for my $k ( keys %$data ) {
        my $v = $data->{ $k };
        next if not ref $v;
        printf FH "$k/xres=\%d\n", $v->{ 'xres' };
        printf FH "$k/yres=\%d\n", $v->{ 'yres' };
        my $n = $v->{ 'xres' }*$v->{ 'yres' };
        printf FH "$k/xreal=\%g\n", $v->{ 'xreal' };
        printf FH "$k/yreal=\%g\n", $v->{ 'yreal' };
        print FH "$k=[\n[";
        my $a = $v->{ 'data' };
        $a = pack( "d[$n]", @$a );
        print FH "$a]]\n";
    }
    close FH;
}

sub process_data {
    my $data = $_[0];
    my $df = $data->{ '/0/data' };
    my $xres = $df->{ 'xres' } + 0;
    my $yres = $df->{ 'yres' } + 0;
    my $n = $xres*$yres;
    my $a = $df->{ 'data' };
    my $sum = 0.0;
    my $sum2 = 0.0;
    for my $v ( @$a ) {
        $sum += $v;
        $sum2 += $v**2;
    }
    my $rms = sqrt( ( $sum2 - $sum**2/$n )/$n );
    my $avg = $sum/$n;
    my $min = $avg - $rms;
    my $max = $avg + $rms;
    for my $i ( 0 .. $n-1 ) {
        $a->[$i] = $max if ( $a->[$i] > $max );
        $a->[$i] = $min if ( $a->[$i] < $min );
    }
}

if ( not @ARGV or not defined $valid_arguments{ $ARGV[0] } ) {
    die "Plug-in has to be called from Gwyddion plugin-proxy.\n";
}

my $what = shift @ARGV;
if ( $what eq 'register' ) {
    print $plugin_info;
}
elsif ( $what eq 'run' ) {
    my $run = shift @ARGV;
    my $filename = shift @ARGV;
    my $data = read_data $filename;
    process_data $data;
    print_data $filename, $data;
}
exit 0;
