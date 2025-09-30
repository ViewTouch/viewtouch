#!/usr/bin/perl -w

use strict;

my $logfile = "/usr/viewtouch/dat/openterm_server.log";

my %parms = GetQuery();

if ( keys %parms ) {
    if ( open( FILE, ">>".$logfile ) ) {
        print FILE "New Instance....\n";
        foreach ( sort keys %parms ) {
            print FILE "  $_:  $parms{$_}\n";
        }
    }
}
if ( exists( $parms{'ipadd'} ) && exists( $parms{'name'} ) ) {
    system( "/usr/viewtouch/bin/vt_openterm -n $parms{'name'} $parms{'ipadd'}" );
}

print "Content-type:  text/plain\n\n";

sub GetQuery {
    my $query_string = $ENV{"QUERY_STRING"};
    my %query;

    if ( $query_string ) {
        my @pairs = split( /&/, $query_string );
        foreach ( @pairs ) {
            my ( $key, $value ) = split( /=/, $_ );
            if ( $key eq "ipadd" && $value =~ /[0-9\.]{7,15}/ ) {
                $query{'ipadd'} = $value;
            } elsif ( $key eq "name" && $value ) {
                $query{'name'} = $value;
            }
        }
    }

    return %query;
}
