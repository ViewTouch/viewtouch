#!/usr/bin/perl -w

#-----------------------------------------------------------------------
# Module:  ccprocess.pl
# Description:  Handles processing various cards that have not been and
#  now cannot be settled.
# Author:  Bruce Alon King
# Created:  Fri 05 Aug 2005 11:17:51 AM PDT
#-----------------------------------------------------------------------

########################################################################
#  INITIALIZATION AND GLOBAL VARIABLES
########################################################################
use strict;
use Getopt::Std;
use Socket;

my $default_server = "localhost";
my $default_port   = "10001";

my $options = "f:hp:s:t:u";
my %opts;
getopts( $options, \%opts );
ShowHelp() if ( $opts{'h'} );

my $details_file        = $ARGV[0] || "";
my $totals_file         = $ARGV[1] || "";
my $find_card           = $opts{'f'} ? $opts{'f'} : "";
my $dest_port           = $opts{'p'} ? $opts{'p'} : $default_port;
my $dest_server         = $opts{'s'} ? $opts{'s'} : $default_server;
my $settled_totals      = $opts{'t'} ? $opts{'t'} : "";
my $list_unsettled      = $opts{'u'} ? 1 : 0;


########################################################################
# MAIN LOOP
########################################################################
my %batches;
my @settles;
if ( $find_card ) {
    if ( $totals_file ) {
        FindCreditCardTotals( $find_card, $totals_file );
    } else {
        FindCreditCardRemote( $find_card, $dest_server, $dest_port );
    }
} elsif ( $details_file && $totals_file ) {
    ShowHelp() unless ( -f $details_file );
    ParseDetails( $details_file, \%batches );
    ParseTotals( $totals_file, \%batches, \@settles ); 
    my $unsettled = CollectUnsettled( \%batches );
    if ( $list_unsettled) {
        PrintUnsettled( $unsettled );
    } elsif ( $settled_totals ) {
        ShowSettledTotals( \@settles, $settled_totals );
    }
} else {
    ShowHelp();
}


########################################################################
# SUBROUTINES
########################################################################

#-----------------------------------------------------------------------
# ShowHelp:
#-----------------------------------------------------------------------
sub ShowHelp {
    my ( $progname ) = $0 =~ m|([^/]+)$|;
    print STDERR "Usage:  $progname [OPTIONS] [<details file> <totals file>]\n";
    print STDERR "OPTIONS:\n";
    print STDERR " -f <find>         Specify a card to search for.  The format is\n";
    print STDERR "                   cardnumber:amount, with numbers only, so for example\n";
    print STDERR "                   4012888888881:1599.\n";
    print STDERR " -h                Show this help screen.\n";
    print STDERR " -p <port>         Set connection port (default is $default_port).\n";
    print STDERR " -s <hostname>     Specify the destination host (default is $default_server).\n";
    print STDERR " -t <start batch>  List the total values for settled transactions starting\n";
    print STDERR "                    at <start batch>.\n";
    print STDERR " -u                List the batches that haven't been settled.\n";
    print STDERR "\n";
    exit( 1 );
}

#-----------------------------------------------------------------------
# ParseDetails:
#-----------------------------------------------------------------------
sub ParseDetails {
    my ( $dfile, $batches ) = @_;
    my $retval = 0;

    print "Processing Details $dfile\n";
    if ( open( DETAILS, $dfile ) ) {
        while ( <DETAILS> ) {
            my $detail = ParseDetailLine( $_ );
            if ( $detail ) {
                my $batchnum = $$detail{'batch'};
                if ( exists( $$batches{$batchnum} ) ) {
                    my $batch = $$batches{$batchnum};
                    if ( exists ( $$batch{'UNSETTLED'} ) ) {
                        my $settled = $$batch{'UNSETTLED'};
                        push( @$settled, $detail );
                    } else {
                        my @txns;
                        push( @txns, $detail );
                        $$batch{'UNSETTLED'} = \@txns;
                    }
                } else {
                    my @txns;
                    push( @txns, $detail );
                    my %batch;
                    $batch{'UNSETTLED'} = \@txns;
                    $$batches{$batchnum} = \%batch;
                }
            }
        }
        $retval = 1;
        close( DETAILS );
    } else {
        print STDERR "Unable to open $dfile:  $!\n";
    }

    return( $retval );
}

#-----------------------------------------------------------------------
# ParseDetailLine:
#-----------------------------------------------------------------------
sub ParseDetailLine {
    my ( $line ) = @_;
    my $retval = 0;

    $line =~ s|<[^>]+>||g;
    my @columns = split( /\s+/, $line );
    if ( $#columns == 8 && $columns[0] =~ /^\d+$/ ) {
        my %detail;
        $detail{'ttid'}    = $columns[0];
        $detail{'type'}    = $columns[1];
        $detail{'card'}    = $columns[2];
        $detail{'account'} = $columns[3];
        $detail{'expiry'}  = $columns[4];
        $detail{'amount'}  = $columns[5];
        $detail{'authnum'} = $columns[6];
        $detail{'batch'}   = $columns[7];
        $detail{'item'}    = $columns[8];
        $retval = \%detail;
    }

    return( $retval );
}

#-----------------------------------------------------------------------
# ParseTotals:
#-----------------------------------------------------------------------
sub ParseTotals {
    my ( $tfile, $batches, $settles ) = @_;
    my $retval = 0;

    print "Processing Totals $tfile\n";
    if ( open( TOTALS, $tfile ) ) {
        while ( <TOTALS> ) {
            my $total = ParseTotalLine( $_ );
            if ( $total ) {
                if ( $total =~ /SETTLE/ ) {
                    push( @$settles, $total );
                } else {
                    my $batchnum = $$total{'batch'};
                    if ( exists( $$batches{$batchnum} ) ) {
                        my $batch = $$batches{$batchnum};
                        if ( exists ( $$batch{'SETTLED'} ) ) {
                            my $settled = $$batch{'SETTLED'};
                            push( @$settled, $total );
                        } else {
                            my @txns;
                            push( @txns, $total );
                            $$batch{'SETTLED'} = \@txns;
                        }
                    } else {
                        my @txns;
                        push( @txns, $total );
                        my %batch;
                        $batch{'SETTLED'} = \@txns;
                        $$batches{$batchnum} = \%batch;
                    }
                }
            }
        }
        close( TOTALS );
        $retval = 1;
    } else {
        print STDERR "Unable to open $tfile:  $!\n";
    }

    return( $retval );
}

#-----------------------------------------------------------------------
# ParseTotalLine:
#-----------------------------------------------------------------------
{
my $lastbatch = 0;

sub ParseTotalLine {
    my ( $line ) = @_;
    my $retval = 0;

    $line =~ s|<[^>]+>||g;
    my @columns = split( /\s+/, $line );
    if ( @columns ) {
        if ( $#columns == 7 && $columns[0] =~ /^\d+$/ ) {
            my %total;
            $total{'ttid'} = $columns[0];
            $total{'type'} = $columns[1];
            $total{'card'} = $columns[2];
            $total{'account'} = $columns[3];
            $total{'expiry'} = $columns[4];
            $total{'amount'} = $columns[5];
            $total{'authnum'} = $columns[6];
            $total{'batch'} = $columns[7];
            $lastbatch = $total{'batch'};
            $retval = \%total;
        } elsif ( $columns[1] && $columns[1] eq "SETTLE" ) {
            $retval = $lastbatch . "  " . $line;
        } elsif ( $columns[0] =~ /^\d+$/ && $columns[1] ne "SETTLE" ) {
            print "Unknown Totals Line:  $line\n";
        }
    }
    return( $retval );
}
}

#-----------------------------------------------------------------------
# CollectUnsettled: Technically, we could have just done this with
#   ParseDetails.  The ParseTotals block was basically irrelevant, but
#   I wanted the data there just in case.
#-----------------------------------------------------------------------
sub CollectUnsettled {
    my ( $batches ) = @_;
    my %unsettled;

    foreach ( sort { $a <=> $b } keys %$batches ) {
        my $batchnum = $_;
        my $batch = $$batches{$batchnum};
        my @blist;
        if ( exists( $$batch{'UNSETTLED'} ) ) {
            my $txns = $$batch{'UNSETTLED'};
            foreach ( @$txns ) {
                my $txn = $_;
                if ( $$txn{'type'} ne "PREAUTH" ) {
                    push( @blist, $txn );
                }
            }
        }
        if ( @blist ) {
            my @blist2 = @blist;
            $unsettled{$batchnum} = \@blist2;
            $#blist = -1;
        }
    }

    return( \%unsettled );
}

#-----------------------------------------------------------------------
# PrintUnsettled:
#-----------------------------------------------------------------------
sub PrintUnsettled {
    my ( $unsettled ) = @_;
    foreach ( sort { $a <=> $b } keys %$unsettled ) {
        my $batchnum = $_;
        print "Unsettled Batch:  $batchnum\n";
        my $batchlist = $$unsettled{$batchnum};
        foreach ( @$batchlist ) {
            my $txn = $_;
            print "\t$$txn{'card'} $$txn{'account'} $$txn{'expiry'} $$txn{'amount'}\n";
        }
    }
}

#-----------------------------------------------------------------------
# ShowSettledTotals:
#-----------------------------------------------------------------------
sub ShowSettledTotals {
    my ( $settles, $startnum ) = @_;

    my $total_count = 0;
    my $total_value = 0;
    foreach ( @$settles ) {
        my $line = $_;
        my @columns = split( /\s+/, $line );
        next unless ( $columns[0] >= $startnum );
        my $auth = $columns[6];
        my ( $count, $value ) = $auth =~ /^A:(\d+)\|(.*)$/;
        $total_count += $count;
        $total_value += $value;
    }

    print "Total number of settled transactions:  $total_count\n";
    print "Total value of settle transactions:  $total_value\n";
}

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
        }
    }

    my $old_fh = select( REMOTE_SERV );
    $| = 1;
    select( $old_fh );
   
    return( $retval );
}

#-----------------------------------------------------------------------
# FindCreditCardRemote:
#-----------------------------------------------------------------------
sub FindCreditCardRemote {
    my ( $find_data, $server, $port ) = @_;
    my ( $cardnum, $value ) = split( /:/, $find_data );

    if ( $cardnum && $value ) {
        my $socket = ConnectSocket( $server, $port );
        if ( $socket ) {
            print $socket "finddata $cardnum $value";
            my ( $result ) = <$socket>;
        }
    } else {
        print STDERR "Error in data format:  $find_data.\n";
    }
}

#-----------------------------------------------------------------------
# FindCreditCardTotals:
#-----------------------------------------------------------------------
sub FindCreditCardTotals {
    my ( $card, $totals ) = @_;

    my ( $cardnum, $amount ) = split( /:/, $card );
    my $clen = length( $cardnum );
    my $shortcard = substr( $cardnum, 0, $clen - 4);
    if ( open( TOTALS, $totals ) ) {
        while ( <TOTALS> ) {
            my $line = $_;
            if ( $line ) {
                my @items = split( /\s+/, $line );
                if ( $#items == 7 && $items[1] eq "SALE" ) {
                    my $ilen = length( $items[3] );
                    my $icard = substr( $items[3], 0, $ilen - 4);
                    if ( $shortcard eq $icard ) {
                        print "MATCH:  $line\n";
                    }
                }
            }
        }
        close( TOTALS );
    } else {
        print STDERR "Unable to read $totals:  $!\n";
    }
}

