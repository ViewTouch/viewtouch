#!/usr/bin/perl -w

#-----------------------------------------------------------------------
# Module:  callcenter.pl
# Description:  listens on a socket for or reads from a file order
#  information from a call center.  Parses the information and forwards
#  it on to vt_main in a ViewTouch happy format.
# Author:  Bruce Alon King
# Created:  Thu 18 Aug 2005 05:05:24 PM PDT
#-----------------------------------------------------------------------

########################################################################
# INITIALIZATION AND GLOBAL VARIABLES
########################################################################
use strict;
use XML::Mini::Document;
use Getopt::Std;
use Socket;

# DEFAULT VALUES
my $default_port          = 10999;
my $default_logfile       = "/var/log/vt_callcenter.log";
my $default_vtmain_port   = 10001;
my $default_vtmain_server = "localhost";
my $default_blocking      = 0;
my $MAXLENGTH             = 1048576;

# COMMAND LINE OPTIONS
my $options = "b1L:f:Fhp:P:S:";
my %opts;
getopts( $options, \%opts );
ShowHelp() if ( $opts{'h'} );

my $blocking      = $opts{'b'} ? $opts{'b'} : $default_blocking;
my $runonce       = $opts{'1'} ? 1 : 0;
my $infile        = $opts{'f'} ? $opts{'f'} : "";
my $foreground    = $opts{'F'} ? 1 : 0;
my $logging       = $opts{'l'} ? 1 : 0;
my $logfile       = $opts{'L'} ? $opts{'l'} : $default_logfile;
my $port          = $opts{'p'} ? $opts{'p'} : $default_port;
my $vtmain_port   = $opts{'P'} ? $opts{'P'} : $default_vtmain_port;
my $vtmain_server = $opts{'S'} ? $opts{'S'} : $default_vtmain_server;

$SIG{PIPE} = 'GotSigPipe';


########################################################################
# MAIN LOOP
########################################################################
if ( $infile ) {
    my $xmlraw = ReadDataFile( $infile );
    my $xmlref = ParseXML( $xmlraw );
    my $socket = ConnectSocket( $vtmain_server, $vtmain_port );
    if ( $socket ) {
        SendOutgoingData( $socket, $xmlref );
        my $result = ReadIncomingData( $socket, "</confirm>" );
        SendResult( $socket, $result );
    } else {
        ReportError( "Unable to connect to $vtmain_server:$vtmain_port:  $!\n" );
    }
} else {
    EnterDaemonMode() unless ( $foreground );
    my $serversock = ListenSocket( $port );
    while ( accept( CLIENT, $serversock ) ) {
        ReportError( "Incoming connection...\n" );
        my $xmlraw = ReadDataSocket( \*CLIENT );
        my $xmlref = ParseXML( $xmlraw );
        my $socket = ConnectSocket( $vtmain_server, $vtmain_port );
        if ( $socket ) {
            SendOutgoingData( $socket, $xmlref );
            my $result = ReadIncomingData( $socket, "</confirm>" );
            SendResult( \*CLIENT, $result );
            close( $socket );
        } else {
            ReportError( "Unable to connect to $vtmain_server:$vtmain_port:  $!\n" );
        }
        close( CLIENT );
        last if ( $runonce );
    }
}
CloseLog();


########################################################################
# SUBROUTINES
########################################################################

#-----------------------------------------------------------------------
# GotSigPipe:
#-----------------------------------------------------------------------
sub GotSigPipe {
    print "Got a sig pipe.\n";
    exit( 1 );
}

#-----------------------------------------------------------------------
# ShowHelp:
#-----------------------------------------------------------------------
sub ShowHelp {
    my ( $progname ) = $0 =~ m|([^/]+)$|;
    print STDERR "Usage:  $progname [OPTIONS]\n";
    print STDERR "OPTIONS:\n";
    print STDERR "  -f <file>      Process the given file and exit.\n";
    print STDERR "  -F             Run in the foreground.\n";
    print STDERR "  -h             Show this help screen.\n";
    print STDERR "  -p <port>      Listen on given port instead of $default_port.\n";
    print STDERR "  -P <port>      The port vt_main is listening on (normally $default_vtmain_port).\n";
    print STDERR "  -S <server>    The server on which vt_main is running (normally $default_vtmain_server).\n";
    print STDERR "\n";
    exit( 1 );
}

#-----------------------------------------------------------------------
# ReadDataFile: No size checking.  This is supposed to be only for
#  debugging at the moment, and we trust the developers.
#-----------------------------------------------------------------------
sub ReadDataFile {
    my ( $filename ) = @_;
    my $retval = "";

    if ( open( INFILE, $filename ) ) {
        $retval = join( "", <INFILE> );
        close( INFILE );
    } else {
        ReportError( "Unable to read $filename:  $!\n" );
    }

    return( \$retval );
}

#-----------------------------------------------------------------------
# ReadDataSocket:
#-----------------------------------------------------------------------
sub ReadDataSocket {
    my ( $clientsock ) = @_;
    my $retval = "";
    my @xml;

    my $startline = <$clientsock>;
    while ( $startline =~ /^\s*$/ ) {
        $startline = <$clientsock>;
    }
    my $nextline  = <$clientsock>;
    if ( $startline =~ /^<\?xml/s && $nextline =~ /^<ord>/s ) {
        push( @xml, $startline );
        push( @xml, $nextline );
        my $line = <$clientsock>;
        while ( $line && $line !~ m|^</ord>| ) {
            push( @xml, $line );
            $line = <$clientsock>;
        }
        push( @xml, $line );
        if ( $line && $line =~ m|^</ord>| ) {
            $retval = join( "", @xml );
        }
    } else {
        ReportError( "Got a weird starting combo:  '$startline', '$nextline'\n" );
    }
    
    return( $retval ? \$retval : 0 );
}

#-----------------------------------------------------------------------
# ParseXML:
#-----------------------------------------------------------------------
sub ParseXML {
    my ( $xml ) = @_;

    my $xmlref = "";
    if ( $xml ) {
        my $xmldoc = XML::Mini::Document->new();
        $xmldoc->parse( $$xml );
        $xmlref = $xmldoc->toHash();
    } else {
        ReportError( "ParseXML doesn't have anything to parse.\n" );
    }

    return ( $xmlref );
}

#-----------------------------------------------------------------------
# EnterDaemonMode:
#-----------------------------------------------------------------------
sub EnterDaemonMode {
    my $pid = fork();
    if ( $pid ) {
        exit( 0 );
    } elsif ( ! defined( $pid ) ) {
        die "Couldn't fork:  $!\n";
    }
}

#-----------------------------------------------------------------------
# ListenSocket:
#-----------------------------------------------------------------------
sub ListenSocket {
    my ( $serv_port ) = @_;

    socket( SERVER, PF_INET, SOCK_STREAM, getprotobyname( 'tcp' ) );
    setsockopt( SERVER, SOL_SOCKET, SO_REUSEADDR, 1 );
    my $serv_addr = sockaddr_in( $port, INADDR_ANY );
    unless ( bind( SERVER, $serv_addr ) ) {
        die "Couldn't bind to port $serv_port:  $!\n";
    }
    unless ( listen( SERVER, SOMAXCONN ) ) {
        die "Couldn't listen on port $serv_port:  $!\n";
    }

    return( \*SERVER );
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
        ReportError( "Could not convert $serv_name to an Internet address:  $!\n" );
    }
    my $paddr = sockaddr_in( $serv_port, $inet_addr );
    if ( connect( REMOTE_SERV, $paddr ) ) {
        $retval = \*REMOTE_SERV;
        if ( $blocking == 0 ) {
            my $old_fh = select( REMOTE_SERV );
            $| = 1;
            select( $old_fh );
        }
    } else {
        ReportError( "Could not connect to $serv_name:$serv_port:  $!\n" );
    }


    return( $retval );
}

#-----------------------------------------------------------------------
# Select:  Wraps the select system call to make it easier to use.
#-----------------------------------------------------------------------
sub Select {
    my ( $fileh ) = @_;
    my $rin = "";
    my $timeout = $blocking ? undef : 1;
    my $retval = 0;

    vec( $rin, fileno( $fileh ), 1 ) = 1;
    $retval = select( $rin, undef, undef, $timeout );

    return $retval;
}

#-----------------------------------------------------------------------
# ReadSocket:  Implements read() with select().
#-----------------------------------------------------------------------
sub ReadSocket {
    my ( $socket, $block ) = @_;
    my $retval = "";
    my $readline = "";

    if ( Select( $socket ) ) {
        $retval = <$socket>;
    }

    return( $retval );
}

#-----------------------------------------------------------------------
# SendOutgoingData:
#-----------------------------------------------------------------------
sub SendOutgoingData {
    my ( $socket, $hashref ) = @_;
    my $retval = 0;

    if ( $socket ) {
        print $socket "remoteorder\n";
        my $response = ReadSocket( $socket );
        my $order = GetOrder( $hashref );
        if ( $order ) {
            #  Add the top information
            foreach ( qw/orderID orderType orderDate orderTime orderForDate orderForTime
        	             restaurantID orderNotes deliveryCharge otherCharge subTotal
        	             taxAmount total firstName lastName phoneNo phoneExt email
                         street suite crossStreet city state zip/ ) {
                SendKeyValue( $socket, $order, $_ );
            }
            #  Add the customer information
            my $customer = GetCustomer( $order );
            if ( $customer ) {
                foreach ( qw/customerName firstName lastName phoneNo phoneExt email
                             address street suite crossStreet city state zip/ ) {
                    SendKeyValue( $socket, $customer, $_ );
                }
            }
            #  Add the payment information
            my $payment = GetPayment( $order );
            if ( $payment ) {
                foreach ( qw/payType cardHolderName cardNo cardExpiryMonth cardExpiryYear
            	             cardCID/ ) {
                    SendKeyValue( $socket, $payment, $_ );
                }
            }
            #  Send the items
            my $order_details = GetOrderDetails( $order );
            if ( $order_details ) {
                foreach ( @$order_details ) {
                    my $itemhash = $_;
                    if ( exists( $$itemhash{'itemDetail'} ) ) {
                        # print all keys except itemDetail, which should go last
                        foreach ( sort keys %$itemhash ) {
                            if ( $_ ne "itemDetail" ) {
                                SendKeyValue( $socket, $itemhash, $_ );
                            }
                        }
                        # print itemDetail
                        if ( exists( $$itemhash{'itemDetail'} ) ) {
                            my $idlist = ForceArray( $$itemhash{'itemDetail'} );
                            foreach ( @$idlist ) {
                                my $idhash = $_;
                                foreach ( sort keys %$idhash ) {
                                    SendKeyValue( $socket, $idhash, $_ );
                                }
                                SendKeyValue( $socket, "done", "EndDetail" );
                            }
                        }
                        SendKeyValue( $socket, "done", "EndItem" );
                    } elsif ( exists( $$itemhash{'product'} ) ) {
                        my $product = ForceArray( $$itemhash{'product'} );
                        foreach ( @$product ) {
                            SendProduct( $socket, $_ );
                        }
                    }
                }
            }
            else
            {
            }
            SendKeyValue( $socket, "done", "EndOrder");
        } else {
            ReportError( "Don't have an order to send.\n" );
        }
    } else {
        ReportError( "Do not have a socket to write.\n" );
    }

    return( $retval );
}

#-----------------------------------------------------------------------
# ForceArray:  Some of the objects are returned as hash references if
#  there is only one, and array references if there is more than one.
#  But we always want an array reference, so we'll force it.
#-----------------------------------------------------------------------
sub ForceArray {
    my ( $ref ) = @_;
    my $retval = "";

    if ( $ref =~ /^HASH/ ) {
        my @array;
        push( @array, $ref );
        $retval = \@array;
    } elsif ( $ref =~ /^ARRAY/ ) {
        $retval = $ref;
    } else {
        print STDERR "Unknown reference:  $ref\n";
    }

    return( $retval );
}

#-----------------------------------------------------------------------
# SendProduct:
#-----------------------------------------------------------------------
sub SendProduct {
    my ( $socket, $phash ) = @_;
    SendKeyValue( $socket, $phash, "productCode" );
    foreach ( sort keys %$phash ) {
        if ( $_ ne "addons" && $_ ne "productCode" ) {
            SendKeyValue( $socket, $phash, $_ );
        }
    }
    if ( exists( $$phash{'addons'} ) ) {
        my $addons = ForceArray( $$phash{'addons'} );
        foreach ( @$addons ) {
            my $addon = $_;
            foreach ( sort keys %$addon ) {
                SendKeyValue( $socket, $addon, $_ );
            }
            SendKeyValue( $socket, "done", "EndAddon" );
        }
    }
    SendKeyValue( $socket, "done", "EndProduct" );
}

#-----------------------------------------------------------------------
# ReadIncomingData:
#-----------------------------------------------------------------------
sub ReadIncomingData {
    my ( $socket, $terminator) = @_;
    my $result = "";
    my $block = "";

    if ( $socket ) {
        $block = ReadSocket( $socket );
        while ( $block && $block !~ m|\Q$terminator\E| ) {
            $result .= $block;
            $block = ReadSocket( $socket );
        }
        $result .= $block if ( $block );
    } else {
        ReportError( "Do not have a socket to read.\n" );
    }

    return( $result );
}

#-----------------------------------------------------------------------
# SendResult:
#    <?xml version="1.0" encoding="iso-8859-1" ?>
#    <confirm>
#        <ccordnum>1537</ccordnum>
#        <vtordnum>1182</vtordnum>
#        <status>active</status>
#        <printstatus>printed</printstatus>
#    </confirm>
#-----------------------------------------------------------------------
sub SendResult {
    my ( $socket, $result ) = @_;
    if ( $result ) {
        my ( $ordnum, $vtordnum, $status, $prtstatus ) = split( /:/, $result );
        if ( defined( $socket ) &&
             defined( $ordnum ) &&
             defined( $vtordnum ) &&
             defined( $status ) &&
             defined( $prtstatus ) ) {
            print $socket "<?xml version=\"1.0\" encoding=\"iso-8859-1\" ?>\n";
            print $socket "<confirm>\n";
            print $socket "    <ccordnum>$ordnum</ccordnum>\n";
            print $socket "    <vtordnum>$vtordnum</vtordnum>\n";
            print $socket "    <status>$status</status>\n";
            print $socket "    <printstatus>$prtstatus</printstatus>\n";
            print $socket "</confirm>\n";
        }
    } else {
        ReportError( "Did not get a response from the server.\n" );
    }
}

#-----------------------------------------------------------------------
# AddKeyValue:
#-----------------------------------------------------------------------
sub AddKeyValue {
    my ( $dataref, $hashref, $key ) = @_;
    my $retval = 0;

    if ( $key && $hashref && $$hashref{$key} ) {
        $$dataref .= ucfirst( $key ) . ":  $$hashref{$key}\n";
        $retval = 1;
    }

    return( $retval );
}

#-----------------------------------------------------------------------
# SendKeyValue:
#-----------------------------------------------------------------------
sub SendKeyValue {
    my ( $fhandle, $value, $key ) = @_;
    my $retval = 0;

    if ( $key ) {
        if ( $value =~ /^HASH/ ) {
            if ( $$value{$key} ) {
                print $fhandle ucfirst( $key ) . ":  $$value{$key}\n";
                $retval = 1;
            }
        } elsif ( $value ) {
            print $fhandle ucfirst( $key) . ":  $value\n";
            $retval = 1;
        }
    }

    return( $retval );
}

#-----------------------------------------------------------------------
# GetOrder:  Just extracts an order from the bigger block.  Probably an
#  unnecessary function, but if things change it could be very useful.
#  NOTE:  Keep in mind that this is the "ord" block, not the
#  orderDetails block, so it has everything, not just the menu items.
#-----------------------------------------------------------------------
sub GetOrder {
    my ( $hashref ) = @_;
    my $retval = 0;

    if ( $hashref && exists( $$hashref{'ord'} ) ) {
        $retval = $$hashref{'ord'};
    }

    return( $retval );
}

#-----------------------------------------------------------------------
# GetCustomer:
#-----------------------------------------------------------------------
sub GetCustomer {
    my ( $hashref ) = @_;
    my $retval = 0;

    if ( $hashref && exists( $$hashref{'customerInformation'} ) ) {
        $retval = $$hashref{'customerInformation'};
    }

    return( $retval );
}

#-----------------------------------------------------------------------
# GetPayment:
#-----------------------------------------------------------------------
sub GetPayment {
    my ( $hashref ) = @_;
    my $retval = 0;

    if ( $hashref && exists( $$hashref{'paymentInformation'} ) ) {
        $retval = $$hashref{'paymentInformation'};
    }

    return( $retval );
}

#-----------------------------------------------------------------------
# GetOrderDetails:
#-----------------------------------------------------------------------#
sub GetOrderDetails {
    my ( $hashref ) = @_;
    my $retval = 0;

    if ( $hashref && exists( $$hashref{'orderDetails'} ) ) {
        my $od = $$hashref{'orderDetails'};
        $retval = $$od{'item'};
        if ( $retval =~ /^HASH/ ) {
            my @array;
            push( @array, $retval );
            $retval = \@array;
        }
    }

    return( $retval );
}

#-----------------------------------------------------------------------
# DebugPrint:  Print some information about the parsed XML just to
#  see what we got.
#-----------------------------------------------------------------------
sub DebugPrint {
    my ( $xmlref ) = @_;

    foreach ( sort keys %$xmlref ) {
        my $key = $_;
        print "$key := $$xmlref{$key}\n";
        if ( $key eq "ord" ) {
            my $order = $$xmlref{$key};
            foreach ( sort keys %$order ) {
                my $okey = $_;
                if ( $okey =~ /customerInformation|paymentInformation/) {
                    print "    $okey\n";
                    my $shash = $$order{$okey};
                    foreach ( sort keys %$shash ) {
                        my $skey = $_;
                        print "        $skey:  $$shash{$skey}\n";
                    }
                } elsif ( $okey =~ /orderDetails/ ) {
                    print "    $okey\n";
                    my $shash = $$order{$okey};
                    my $iarr = $$shash{'item'};
                    foreach ( @$iarr ) {
                        print "        item\n";
                        my $ihash = $_;
                        foreach ( sort keys %$ihash ) {
                            my $ikey = $_;
                            if ( $ikey ne "itemDetail" ) {
                                print "            $ikey:  $$ihash{$ikey}\n";
                            }
                        }
                        if ( exists $$ihash{'itemDetail'} ) {
                            my $idetail = ForceArray( $$ihash{'itemDetail'} );
                            foreach ( @$idetail ) {
                                my $idhash = $_;
                                print "            idetail\n";
                                foreach ( sort keys %$idhash ) {
                                    my $idkey = $_;
                                    print "                $idkey:  $$idhash{$idkey}\n";
                                }
                            }
                        }
                    }
                } else {
                    print "    $okey:  $$order{$okey}\n";
                }
            }
        }
    }
}

{
my $logopen = 0;

#-----------------------------------------------------------------------
# ReportError:
#-----------------------------------------------------------------------
sub ReportError {
    my ( $message ) = @_;

    if ( $foreground == 0 || $logging ) {
        if ( !defined( $logopen) || $logopen == 0 ) {
            if ( open( LOG, $logfile ) ) {
                $logopen = 1;
            } else {
                return;
            }
        }
        print LOG $message;
    } else {
        print $message;
    }
}

#-----------------------------------------------------------------------
# CloseLog:
#-----------------------------------------------------------------------
sub CloseLog {
    if ( $logopen ) {
        close( LOG );
    }
}
}
