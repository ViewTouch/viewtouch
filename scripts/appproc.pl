#!/usr/bin/perl -w

# --------------------------------------------------------------------
# Module:  appproc.pl
# Description:  Processes a list of credit cards to finalize voice
#  authorization, etc.  A file must be supplied.
#
#  File Format:
#  trans date,approval code,amount,card number,expiry,action
#
#  Lines beginning with # are ignored.
#
#  Example:
#  6/7/2004,vital7,$25.25,4510 2036 1541 8014,12/05,AUTH
#
#  The approval code and action fields are optional.  All others
#  are mandatory.  Valid actions are:
#    AUTH     = Sale Authorization
#    PREAUTH  = PreAuthorization
#    COMPLETE = PreAuth Complete
#    FORCE    = Force Authorization (for voice auths)
#    REFUND   = Refund
#
# Author:  Bruce Alon King
# Created:  Fri Jun  4 12:25:54 2004
# --------------------------------------------------------------------


# ####################################################################
# INITIALIZATION AND GLOBAL VARIABLES
# ####################################################################
use strict;
use Getopt::Std;
use MCVE;

my $default_logfile  = "approc.log";
my $default_password = "testing";
my $default_port     = 8333;
my $default_server   = "viewtouch.com";
my $default_user     = "vital";

my $options = "hl:p:P:qQs:u:";
my %opts;
getopts( $options, \%opts );
ShowHelp() if ( $opts{'h'} );

my $logfile  = $opts{'l'} || $default_logfile;
my $password = $opts{'p'} ? $opts{'p'} : $default_password;
my $port     = $opts{'P'} ? $opts{'P'} : $default_port;
my $quiet    = $opts{'q'} ? 1 : 0;
$quiet += 2 if ( $opts{'Q'} );
my $server   = $opts{'s'} ? $opts{'s'} : $default_server;
my $user     = $opts{'u'} ? $opts{'u'} : $default_user;

my $blocking = 1;
my $timeout  = 30;
my $logopen  = 0;


# ####################################################################
# SIGNAL HANDLERS
# ####################################################################
sub siginthandler {
    my ( $signame ) = @_;
    CloseLog();
    exit( 0 );
}
$SIG{INT} = \&siginthandler;


# ####################################################################
# MAIN LOOP
# ####################################################################
my $file = $ARGV[0] || ShowHelp();
my $cards = GatherCards( $file );
LogError( "\nProcessing ".@$cards." card\n" );

my $conn = Connect( $server, $port );
if ( $conn ) {
    ProcessCards( $conn, $cards );
}


# ####################################################################
# SUBROUTINES
# ####################################################################

# --------------------------------------------------------------------
# ShowHelp:  Prints usage information and exits.
# --------------------------------------------------------------------
sub ShowHelp {
    print "Usage:  $0 [OPTIONS] <file>\n";
    print "OPTIONS:\n";
    print "  -h               Show this help screen\n";
    print "  -l <logfile>     Where to put messages (default is $default_logfile)\n";
    print "  -p <password>    Login password (default $default_password)\n";
    print "  -P <port>        TCP Port to connect to (default $default_port)\n";
    print "  -q               Quiet mode (reduces number of messages\n";
    print "  -Q               Extra quiet\n";
    print "  -s <server>      Credit card server (default $default_server)\n";
    print "  -u <user>        Login username (default $default_user)\n";
    print "\n";
    exit( 1 );
}

# --------------------------------------------------------------------
# ReadFile:  Returns the contents of a file as a reference to an
#  array of lines.
# --------------------------------------------------------------------
sub ReadFile {
    my ( $filename ) = @_;
    my @contents;
    if ( open( FILE, $filename ) ) {
        while ( <FILE> ) {
            my $line = $_;
            next if ( $line =~ /^\s*$/ );
            next if ( $line =~ /^\#/ );
            $line =~ s/^\s+|\s+$//gs;
            push( @contents, $line );
        }
        close( FILE );
    }
    return( \@contents );
}

# --------------------------------------------------------------------
# GatherCards:  Parses a list (reference) of lines to separate
#  out the card number, expiry, etc.
# --------------------------------------------------------------------
sub GatherCards {
    my ( $list ) = @_;
    my $contents = ReadFile( $file );
    my @cards;

    foreach ( @$contents ) {
        my $line = $_;
        if ( $line =~ /^\s*$/ ) {
            LogWarning( "SKIPPING:  $line\n" );
        } elsif ( $line =~ /^\#/ ) {
            LogWarning( "SKIPPING:  $line\n" );
        } elsif ( $line =~ /^,,/ ) {
            LogWarning( "SKIPPING:  $line\n" );
        } else {
            my $cardref = ParseCard( $line );
            if ( $cardref ) {
                push( @cards, $cardref );
            }
        }
    }

    return( \@cards );
}

# --------------------------------------------------------------------
# ParseCard:
# --------------------------------------------------------------------
sub ParseCard {
    my ( $line ) = @_;
    my %card;

    if ( $line =~ m|^(\d{1,2}/\d{1,2}/\d{4}),([^,]+),([^,]+),([\d ]+),(\d{2}/\d{2})(,([^,]+))?| ) {
        my ( $date, $approval, $amount, $cardnum, $expiry, $action ) = ( $1, $2, $3, $4, $5, $6 );
        if ( defined( $date ) && defined( $approval) && defined( $amount ) &&
             defined( $cardnum ) && defined( $expiry ) ) {
            $cardnum =~ s/ //g;
            $card{'date'} = $date;
            $card{'approval'} = $approval;
            $card{'amount'} = $amount;
            $card{'namount'} = NumericAmount( $amount );
            $card{'famount'} = FloatAmount( $amount );
            $card{'cardnum'} = $cardnum;
            $card{'mcardnum'} = MaskCardNumber( $cardnum );
            $card{'expiry'} = $expiry;
            $card{'action'} = $action if ( defined( $action ) );
        } else {
            LogError( "SKIPPING:  $line\n" );
        }
    } else {
        LogError( "SKIPPING:  $line\n" );
    }

    if ( keys %card ) {
        return( \%card );
    } else {
        return( 0 );
    }
}

# --------------------------------------------------------------------
# MaskCardNumber:
# --------------------------------------------------------------------
sub MaskCardNumber {
    my ( $cardnum ) = @_;
    my $retval = $cardnum;
    my $len = length( $retval ) - 4;

    foreach ( 1..$len ) {
        $retval =~ s/\d/x/;
    }

    return( $retval );
}

# --------------------------------------------------------------------
# NumbericAmount:
# --------------------------------------------------------------------
sub NumericAmount {
    my ( $amount ) = @_;
    my $namount = $amount;

    $namount =~ s/\D//g;

    return( $namount );
}

# --------------------------------------------------------------------
# FloatAmount:
# --------------------------------------------------------------------
sub FloatAmount {
    my ( $amount ) = @_;
    my $famount = $amount;

    $famount =~ s/[^\d\.]//g;

    return( $famount );
}

# --------------------------------------------------------------------
# Connect:  Open a connection to the credit card server and return
#   a handle to it.
# --------------------------------------------------------------------
sub Connect {
    my ( $server, $port ) = @_;

    my $conn = MCVE::MCVE_InitConn();
    MCVE::MCVE_SetIP( $conn, $server, $port );
    MCVE::MCVE_SetBlocking( $conn, $blocking );
    MCVE::MCVE_SetTimeout( $conn, $timeout );
    LogError( "Connecting...\n" );
    if ( !MCVE::MCVE_Connect( $conn ) ) {
        my $error = MCVE::MCVE_ConnectionError( $conn );
        MCVE::MCVE_DestroyConn( $conn );
        MCVE::MCVE_DestroyEngine();
        $conn = 0;
        LogFatal( "    Connection failed:  $error\n" );
    } else {
        LogError( "    Connected\n" );
    }

    return $conn;
}

# --------------------------------------------------------------------
# ProcessCards:  Walk through the list of cards determining what
#  to do with each.
# --------------------------------------------------------------------
sub ProcessCards {
    my ( $conn, $cardlist ) = @_;
    my $action = 1;
    my $success = 0;
    my $nextiter = 1;
    my $holdaction = 1;

    my $idx = 0;
    while ( $$cardlist[$idx] ) {
        my $cardref = $$cardlist[$idx];
        $nextiter = 1;
        if ( $action % 2 ) {
            $action = ParseAction( $cardref );
        }
        if ( $action < 0 || $action % 2 ) {
            $action = AskForAction( $cardref );
            if ( ( $action % 2 ) == 0 ) {
                $holdaction = $action;  # set this as the permanent
            }
        }
        $success = HandleCard( $conn, $cardref, $action );
        if ( $success < 1 ) {
            $nextiter = NextIteration( \$action );
            $action = 1 if ( $nextiter == 0);  # let's ask for an action
        } elsif ( $quiet < 2 && $action % 2 ) {
            print "Press a key...\n";
            my $go = <STDIN>;
        }
        if ( $nextiter )
        {
            $idx += 1;
            $action = $holdaction;
        }
    }
}

# --------------------------------------------------------------------
# HandleCard:
# --------------------------------------------------------------------
sub HandleCard {
    my ( $conn, $cardref, $action ) = @_;
    my $success = 0;

    if ( $action == 1 || $action == 2 ) {
        $success = AuthCard( $conn, $cardref );
    } elsif ( $action == 3 || $action == 4 ) {
        $success = PreauthCard( $conn, $cardref );
    } elsif ( $action == 5 || $action == 6 ) {
        $success = CompleteCard( $conn, $cardref );
    } elsif ( $action == 7 || $action == 8 ) {
        $success = ForceCard( $conn, $cardref );
    } elsif ( $action == 9 || $action == 0 ) {
        $success = RefundCard( $conn, $cardref );
    } else {
        LogError( "Unknown Action for $$cardref{'mcardnum'}, $$cardref{'expiry'}:  $action\n" );
    }

    return( $success );
}

# --------------------------------------------------------------------
# NextIteration:  This should only be called for failed transactions.
# --------------------------------------------------------------------
{
my $try_always = 0;
my $try_never  = 0;
sub NextIteration {
    my $again = 0;

    if ( $try_always ) {
        $again = 0;
    } elsif ( $try_never ) {
        $again = 1;
    } else {
        print "Try again? (Yes/No/Always/neVer)";
        $again = <STDIN>;
        $again =~ s/^\s+|\s+$//gs;
        if ( $again =~ /^\s*$/ ) {
            $again = 0;  # default is try again (skip next iteration)
        } elsif ( $again =~ /^y(es)?$/i ) {
            $again = 0;
        } elsif ( $again =~ /^n(o)?$/i ) {
            $again = 1;
        } elsif ( $again =~ /^(ne)?v(er)?$/i ) {
            $again = 1;
            $try_never = 1;
            $try_always = 0;
        } elsif ( $again =~ /^a(lways)?$/i ) {
            $again = 0;
            $try_always = 1;
            $try_never = 0;
        }
    }
    
    return( $again );
}
}

# --------------------------------------------------------------------
# ParseAction:
# --------------------------------------------------------------------
sub ParseAction {
    my ( $card ) = @_;
    my $retval = -1;

    if ( exists( $$card{'action'} ) ) {
        my $action = $$card{'action'};
        if ( $action eq "AUTH" ) {
            $retval = 1;
        } elsif ( $action eq "AUTHALL" ) {
            $retval = 2;
        } elsif ( $action eq "PREAUTH" ) {
            $retval = 3;
        } elsif ( $action eq "PREAUTHALL" ) {
            $retval = 4;
        } elsif ( $action eq "COMPLETE" ) {
            $retval = 5;
        } elsif ( $action eq "COMPLETEALL" ) {
            $retval = 6;
        } elsif ( $action eq "FORCE" ) {
            $retval = 7;
        } elsif ( $action eq "FORCEALL" ) {
            $retval = 8;
        } elsif ( $action eq "REFUND" ) {
            $retval = 9;
        } elsif ( $action eq "REFUNDALL" ) {
            $retval = 0;
        }
    }

    return( $retval );
}

# --------------------------------------------------------------------
# AskForAction:
# --------------------------------------------------------------------
sub AskForAction {
    my ( $cardref ) = @_;
    my $action = 1;

    system( "clear" );
    print "\n";
    print "$$cardref{'mcardnum'}, $$cardref{'expiry'}, $$cardref{'amount'}\n";
    print "\n";
    print "What would you like to do with this card?\n";
    print "\n";
    print "1.  Authorize this card\n";
    print "2.  Authorize all cards\n";
    print "3.  Pre-Authorize this card\n";
    print "4.  Pre-Authorize all cards\n";
    print "5.  PreAuth Complete this card\n";
    print "6.  PreAuth Complete all cards\n";
    print "7.  Force this card\n";
    print "8.  Force all cards\n";
    print "9.  Refund this card\n";
    print "0.  Refund all cards\n";
    print "\n";
    print "Action:  ";

    $action = <STDIN>;
    chomp( $action );

    return( $action );
}

# --------------------------------------------------------------------
# AuthCard:
# --------------------------------------------------------------------
sub AuthCard {
    my ( $conn, $card ) = @_;
    my $retval = 0;

    ActionMessage( $card, "Authing" );
    my $identifier = MCVE::MCVE_TransNew( $conn );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_USERNAME(), $user );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_PASSWORD(), $password );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_TRANTYPE(), MCVE::MC_TRAN_SALE() );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_ACCOUNT(), $$card{'cardnum'} );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_EXPDATE(), $$card{'expiry'} );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_AMOUNT(), $$card{'famount'} );
    $retval = SendTrans( $conn, $identifier );

    return( $retval );
}

# --------------------------------------------------------------------
# PreauthCard:
# --------------------------------------------------------------------
sub PreauthCard {
    my ( $conn, $card ) = @_;
    my $retval = 0;

    ActionMessage( $card, "Preauthing" );
    my $identifier = MCVE::MCVE_TransNew( $conn );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_USERNAME(), $user );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_PASSWORD(), $password );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_TRANTYPE(), MCVE::MC_TRAN_PREAUTH() );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_ACCOUNT(), $$card{'cardnum'} );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_EXPDATE(), $$card{'expiry'} );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_AMOUNT(), $$card{'namount'} );
    $retval = SendTrans( $conn, $identifier );

    return( $retval );
}

# --------------------------------------------------------------------
# CompleteCard:
# --------------------------------------------------------------------
sub CompleteCard {
    my ( $conn, $card ) = @_;
    my $retval = 0;

    ActionMessage( $card, "Completing" );
    my $identifier = MCVE::MCVE_TransNew( $conn );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_USERNAME(), $user );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_PASSWORD(), $password );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_TRANTYPE(), MCVE::MC_TRAN_PREAUTHCOMPLETE() );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_ACCOUNT(), $$card{'cardnum'} );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_EXPDATE(), $$card{'expiry'} );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_AMOUNT(), $$card{'namount'} );
    $retval = SendTrans( $conn, $identifier );

    return( $retval );
}

# --------------------------------------------------------------------
# ForceCard:
# --------------------------------------------------------------------
sub ForceCard {
    my ( $conn, $card ) = @_;
    my $retval = 0;

    ActionMessage( $card, "Forcing" );
    my $identifier = MCVE::MCVE_TransNew( $conn );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_USERNAME(), $user );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_PASSWORD(), $password );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_TRANTYPE(), MCVE::MC_TRAN_FORCE() );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_ACCOUNT(), $$card{'cardnum'} );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_EXPDATE(), $$card{'expiry'} );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_AMOUNT(), $$card{'namount'} );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_APPRCODE(), $$card{'approval'} );
    $retval = SendTrans( $conn, $identifier );

    return( $retval );
}

# --------------------------------------------------------------------
# RefundCard:
# --------------------------------------------------------------------
sub RefundCard {
    my ( $conn, $card ) = @_;
    my $retval = 0;

    ActionMessage( $card, "Refunding" );
    my $identifier = MCVE::MCVE_TransNew( $conn );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_USERNAME(), $user );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_PASSWORD(), $password );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_TRANTYPE(), MCVE::MC_TRAN_REFUND() );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_ACCOUNT(), $$card{'cardnum'} );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_EXPDATE(), $$card{'expiry'} );
    MCVE::MCVE_TransParam( $conn, $identifier, MCVE::MC_AMOUNT(), $$card{'namount'} );
    $retval = SendTrans( $conn, $identifier );

    return( $retval );
}

# --------------------------------------------------------------------
# ActionMessage:
# --------------------------------------------------------------------
sub ActionMessage {
    my ( $card, $action ) = @_;

    LogError( "$action $$card{'mcardnum'}, $$card{'expiry'} for $$card{'famount'}...\n" );
}

# --------------------------------------------------------------------
# SendTrans:
# --------------------------------------------------------------------
sub SendTrans {
    my ( $conn, $identifier ) = @_;
    my $retval = 0;
    my $status = 0;

    if ( MCVE::MCVE_TransSend( $conn, $identifier ) ) {
        $status = MCVE::MCVE_ReturnStatus( $conn, $identifier );
        if ( $status == MCVE::MCVE_SUCCESS() ) {
            $retval = 1;
        } else {
            LogError( "  Transaction failed.\n" );
        }
        my $code = MCVE::MCVE_TEXT_Code( MCVE::MCVE_ReturnCode( $conn, $identifier ) );
        my $auth = MCVE::MCVE_TransactionAuth( $conn, $identifier );
        my $text = MCVE::MCVE_TransactionText( $conn, $identifier );
        LogError( "  Code:  $code\n" );
        LogError( "  Auth:  $auth\n" ) if ( $auth );
        LogError( "  Text:  $text\n" ) if ( $text );
    } else {
        LogError( "Failure structuring/sending transaction, probably improperly structured" );
    }

    return( $retval );
}

# --------------------------------------------------------------------
# OpenLog:
# --------------------------------------------------------------------
sub OpenLog {
    if ( defined( $logfile ) && $logfile ) {
        if ( open( LOGFILE, ">".$logfile ) ) {
            $logopen = 1;
        } else {
            print STDERR "Unable to open $logfile:  $!\n";
            exit( 1 );
        }
    } else {
        print STDERR "No logfile defined\n";
        exit( 1 );
    }

    return( $logopen );
}

# --------------------------------------------------------------------
# CloseLog:
# --------------------------------------------------------------------
sub CloseLog {
    if ( $logopen ) {
        close( LOGFILE );
    }
}

# --------------------------------------------------------------------
# LogWarning:
# --------------------------------------------------------------------
sub LogWarning {
    my ( $message ) = @_;

    if ( $logopen || OpenLog() ) {
        print LOGFILE $message;
    }
    print $message if ( $quiet < 1 );
}

# --------------------------------------------------------------------
# LogError:
# --------------------------------------------------------------------
sub LogError {
    my ( $message ) = @_;

    if ( $logopen || OpenLog() ) {
        print LOGFILE $message;
    }
    print $message if ( $quiet < 2 );
}

# --------------------------------------------------------------------
# LogFatal:
# --------------------------------------------------------------------
sub LogFatal {
    my ( $message ) = @_;

    if ( $logopen || OpenLog() ) {
        print LOGFILE $message;
    }
    print $message;

    exit( 1 );
}
