#!/usr/bin/perl -w

# ####################################################################
# MODULE:  vt_data.pl
# DESCRIPTION:  A debugging utility for reading, writing, and
#   otherwise manipulating vt_data.
# AUTHOR:  Bruce Alon King
# CREATED:  Thu Apr 11 14:45:07 2002
# ####################################################################
use vars qw/$hostname/;
BEGIN {
    chomp( $hostname = `hostname` );
    if ( $hostname =~ /mainbox/ ) {
        push( @INC, "/usr/local/apache/cgi-bin" );
    } else {
        push( @INC, "/var/www/cgi-bin" );
    }
}

use strict;
use Getopt::Std;
use ViewTouch::VTData;

my $tempdir = $ENV{'HOME'} . "/tmp/viewtouch/";

my $options = "dDh";
my %opts;
getopts( $options, \%opts );
showHelp() if ( exists( $opts{'h'} ) );

my $debug_mode  = $opts{'d'} ? 1 : 0;
my $date_ranges = $opts{'D'} ? 1 : 0;

my $vtdata = new ViewTouch::VTData( debug => $debug_mode );

showHelp() unless ( @ARGV );
my @files = @ARGV;


# ####################################################################
# MAINLOOP
# ####################################################################
foreach ( @files ) {
    print "Processing $_\n";  #DEBUG
    my $file = $vtdata->ReadFile( $_ );
    my $keys = $vtdata->Keys( $file );
    foreach ( @$keys ) {
        my $key = $_;
        my $value = $vtdata->Get( $file, $key );
        print STDERR "$key:  $value\n";  #DEBUG
    }
}


# ####################################################################
# SUBROUTINES
# ####################################################################

# --------------------------------------------------------------------
# showHelp:
# --------------------------------------------------------------------
sub showHelp {
    my ( $progname ) = $0 =~ m|/([^/]+)$|;
    print "Usage:  $progname [OPTIONS] <file1> [.. <fileN>]\n";
    print "OPTIONS:\n";
    print "  -d          Debug mode.\n";
    print "  -h          Show this help screen.\n";
    print "\n";
    exit( 1 );
}

