#!/usr/bin/perl -w

#-----------------------------------------------------------------------
# Module:  callcenter-test.pl
# Description:  A little script to help with callcenter.pl debugging.
# Author:  Bruce Alon King
# Updated:  Mon 22 Aug 2005 12:00:33 PM PDT
#-----------------------------------------------------------------------

########################################################################
# INITIALIZATION AND GLOBAL VARIABLES
########################################################################
use strict;
use Socket;
use Getopt::Std;

my $default_blocking = 1;
my $default_file     = "$ENV{'HOME'}/tmp/callcenter/sample4.txt";
my $default_port     = 10999;
my $default_server   = "localhost";

my $options = "bf:p:s:";
my %opts;
getopts( $options, \%opts );

my $blocking = !$default_blocking if ( exists( $opts{'b'} ) );
my $file     = $opts{'f'} ? $opts{'f'} : $default_file;
my $port     = $opts{'p'} ? $opts{'p'} : $default_port;
my $server   = $opts{'s'} ? $opts{'s'} : $default_server;


########################################################################
# MAIN LOOP
########################################################################
print "Sending File:  $file\n";
my $sock = ConnectSocket( $server, $port );
if ( $sock ) {
    print "Connected....";
    my $contents = ReadFile( $file );
    if ( $contents ) {
        print "Sending....";
        SendData( $contents, $sock );
        ReadData( $sock );
    }
}
print "\n";


########################################################################
# SUBROUTINES
########################################################################

#-----------------------------------------------------------------------
# ConnectSocket:  Make an outgoing TCP connection and return a
#  reference to the socket handle.
#-----------------------------------------------------------------------
sub ConnectSocket {
    my ( $serv_name, $serv_port ) = @_;
    my $retval = 0;

    socket( REMOTE_SERV, PF_INET, SOCK_STREAM, getprotobyname( 'tcp' ) );
    my $inet_addr = inet_aton( $serv_name );
    if ( ! $inet_addr ) {
        print STDERR "Could not convert $serv_name to an Internet address:  $!\n";
    } else {
        my $paddr = sockaddr_in( $serv_port, $inet_addr );
        if ( ! connect( REMOTE_SERV, $paddr ) ) {
            print STDERR "Could not connect to $serv_name:$serv_port:  $!\n";
        } else {
            $retval = \*REMOTE_SERV;
            if ( ! $blocking ) {
                my $old_fd = select( REMOTE_SERV );
                $| = 1;
                select( $old_fd );
            }
        }
    }
    
    return( $retval );
}

#-----------------------------------------------------------------------
# ReadFile:  Reads a file into a scalar and returns a reference to that
#  scalar.  So it's just one big lump.
#-----------------------------------------------------------------------
sub ReadFile {
    my ( $filename ) = @_;
    my $contents = "";

    if ( open( FILE, $filename ) ) {
        $contents = join( "", <FILE> );
        close( FILE );
    } else {
        print STDERR "Could not read $filename:  $!\n";
    }

    return( $contents ? \$contents : $contents );
}

#-----------------------------------------------------------------------
# SendData:
#-----------------------------------------------------------------------
sub SendData {
    my ( $source, $dest ) = @_;
    my $retval = 0;

    if ( $source && $$source ) {
        print "Sending $$source\n";
        print $dest $$source;
    } else {
        print STDERR "Don't have anything to send:  $source\n";
    }

    return( $retval );
}

#-----------------------------------------------------------------------
# ReadData:
#-----------------------------------------------------------------------
sub ReadData {
    my ( $source ) = @_;
    my $retval = 0;

    if ( $source ) {
        while ( <$source> ) {
            print "Read:  $_";
        }
    }

    return( $retval );
}
