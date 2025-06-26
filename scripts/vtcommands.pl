#!/usr/bin/perl -w

# --------------------------------------------------------------------
# Module:  vtcommands.pl
# Description:  Utility to interface with the vt_main external command
#   facility primarily to run reports at scheduled times.
# Author:  Bruce Alon King
# Created:  Mon Dec  9 16:08:38 2002
# --------------------------------------------------------------------

# ####################################################################
# INITIALIZATION AND GLOBAL VARIABLES
# ####################################################################
use strict;
use Getopt::Std;
use Socket;

my %lmonths = ( january => 0, february => 1, march => 2,
                april => 3, may => 4, june => 5, july => 6,
                august => 7, september => 8, october => 9,
                november => 10, december => 11, );
my %smonths = ( jan => 0, feb => 1, mar => 2, apr => 3, may => 4,
                jun => 5, jul => 6, aug => 7, sep => 8, oct => 9,
                nov => 10, dec => 11, );
my @months = ( keys( %lmonths ), keys( %smonths ) );


my $global_config     = $ENV{'HOME'} . "/.vtcommands.conf";
my $root_dir          = "/usr/viewtouch/";
my $printing_dir      = "/usr/viewtouch/html/";
my $viewtouch_bin     = "bin/";
my $viewtouch_dat     = "dat/";
my $working_dir       = "/tmp";
my $default_from      = MakeDate( "from" );
my $default_to        = MakeDate( "to" );

my $bindir   = "";
my $config   = "";
my $datdir   = "";
my $debug    = 0;
my $endday   = 0;
my $fromdate = "";
my $fromadd  = "";
my $mailadd  = "";
my $mailall  = "";
my $printdir = $printing_dir;
my $period   = "";
my $root     = $root_dir;
my $startvt  = 0;
my $server   = "localhost";
my $todate   = "";
my $tmpdir   = $working_dir;
my $extended = 0;
ReadGlobalConfig();

my $options = "b:c:d:Def:hm:M:p:P:r:st:w:x";
my %opts;
getopts($options, \%opts);
ShowHelp() if ( $opts{'h'} );

$bindir   = $opts{'b'} ? $opts{'b'}   : $bindir;
$config   = $opts{'c'} ? $opts{'c'}   : $config;
$datdir   = $opts{'d'} ? $opts{'d'}   : $datdir;
$debug    = $opts{'D'} ? !$debug      : $debug;
$endday   = $opts{'e'} ? $opts{'e'}   : $endday;
$fromdate = $opts{'f'} ? ParseDate( $opts{'f'}, "from" ) : $default_from;
$fromadd  = $opts{'F'} ? $opts{'F'}   : $fromadd;
$mailadd  = $opts{'m'} ? $opts{'m'}   : $mailadd;
$mailall  = $opts{'M'} ? $opts{'M'}   : $mailall;
$printdir = $opts{'p'} ? $opts{'p'}   : $printdir;
$period   = $opts{'P'} ? $opts{'P'}   : $period;
$root     = $opts{'r'} ? $opts{'r'}   : $root;
$startvt  = $opts{'s'} ? NotRunning() : $startvt;
$server   = $opts{'S'} ? $opts{'S'}   : $server;
$todate   = $opts{'t'} ? ParseDate( $opts{'t'}, "to" ) : $default_to;
$tmpdir   = $opts{'w'} ? $opts{'w'}   : $tmpdir;
$tmpdir  .= "/" unless ( $tmpdir =~ m|/^| );
$tmpdir  .= "viewtouch/" if ( -d $tmpdir );

$extended = $opts{'x'} ? 1 : 0;
if ( $period ) {
    ( $fromdate, $todate ) = ParseDate( $period );
}

$printdir .= "/" unless ( $printdir =~ m|/$| );
$root     .= "/" unless ( $root =~ m|/$| );
$bindir   = $bindir || $root . $viewtouch_bin;
$datdir   = $datdir || $root . $viewtouch_dat;
$bindir   .= "/" unless ( $bindir =~ m|/$| );
$datdir   .= "/" unless ( $datdir =~ m|/$| );

my $printer   = "printer:  file:$tmpdir/ HTML";
my $vtpos     = $bindir . "vtpos";
my $vtcommand;

# try setting a local display if we don't have a display.
# ViewTouch currently requires X.  Eventually, I'd like to
# modify it so that it can run background stuff without worrying
# about X, but that could take vast rewrites of the code and may
# not be feasible.
$ENV{'DISPLAY'} = ":0.0" unless ( $ENV{'DISPLAY'} );


# ####################################################################
# MAIN LOOP
# ####################################################################

srand( time ^ $$ ^ unpack "%L*", `ps axww | gzip` );

# If we have a config file, use that.  Otherwise we need to have some
# reports specified on the command line, so @reports should be a
# non-empty list.
MakeDirs();
my $actions = "";
if ( -f $config ) {
    $actions = ReadConfig( $config );
} else {
    $actions = ReadCommandLine( \@ARGV );
}

if ( $debug ) {
    print "Root Dir:       $root\n";
    print "Binary Dir:     $bindir\n";
    print "Print Dir:      $printdir\n";
    print "Temporary Dir:  $tmpdir\n";
    print "From Date:      $fromdate\n";
    print "To Date:        $todate\n";
}

# now process the actions if there are any.
if ( $actions ) {
    my @datadirs;
    if ( $extended ) {
        push( @datadirs, GetDataSubdirs( $datdir ) );
    } else {
        push( @datadirs, $datdir );
    }
    foreach ( @datadirs ) {
        $datdir = $_;
        print "Data Dir:       $datdir\n" if ( $debug );
        $vtcommand = $bindir . ".viewtouch_command_file";
        WriteViewTouchCommands( $actions );
        if ( $startvt ) {
            RunViewTouch();
        } else {
            SignalViewTouch();
        }
        while ( -f $vtcommand ) {
            sleep( 1 );
        }
        if ( @datadirs > 1 ) {
            RenamePrintouts( $tmpdir );
        }
        # email the current batch on -m and move unless -M
        print "Mailing batch...\n" if ( $mailadd && $debug );
        EmailPrintouts( $mailadd, $tmpdir ) if ( $mailadd );
        MovePrintouts( $printdir, $tmpdir ) unless ( $mailall );
    }
    # email all on -M and move any that haven't been moved
    print "Mailing all...\n" if ( $mailall );
    EmailPrintouts( $mailall, $tmpdir ) if ( $mailall );
    MovePrintouts( $printdir, $tmpdir );
} else {
    print "Nothing to do....\n\n";
    ShowHelp();
}

rmdir( $tmpdir );


# ####################################################################
# SUBROUTINES
# ####################################################################

# --------------------------------------------------------------------
# ShowHelp:
# --------------------------------------------------------------------
sub ShowHelp {
    my ( $progname ) = $0 =~ m|/([^/]+)$|;
    print STDERR "Usage:  $progname [OPTIONS] <report1> [ .. <reportN>]\n";
    print STDERR "OPTIONS\n";
    print STDERR "  -b <bin dir>        ViewTouch binary directory.\n";
    print STDERR "  -c <config file>    Configuration file to use.\n";
    print STDERR "  -d <dat dir>        ViewTouch data directory.\n";
    print STDERR "  -D                  Possibly print extra diagnostics.\n";
    print STDERR "  -f <from>           From date for reports.\n";
    print STDERR "  -F <mail address>   From address for outgoing emails.\n";
    print STDERR "  -h                  Show this help screen.\n";
    print STDERR "  -m <mail address>   Mail groups of files to specified address.\n";
    print STDERR "  -M <mail address>   Mail all files to specified address.";
    print STDERR "  -p <print dir>      Where to store printouts.\n";
    print STDERR "  -r <root dir>       Root ViewTouch directory.\n";
    print STDERR "  -s                  Start ViewTouch.\n";
    print STDERR "  -S <mail server>    Mail server for outgoing emails.\n";
    print STDERR "  -t <to>             To date for reports.\n";
    print STDERR "  -w <working dir>    Where temp files go.\n";
    print STDERR "  -x                  Extended processing.\n";
    print STDERR "\n";
    print STDERR "Dates should be in the format:  DD/MM/YYYY,HH:MM\n";
    print STDERR "\n";
    print STDERR "Extended processing means the data directory will be considered\n";
    print STDERR "a remote_data directory.  All immediate subdirectories will be\n";
    print STDERR "processed in alphabetical order as if they were data directories.\n";
    print STDERR "This is one way to run reports for multiple systems.\n";
    print STDERR "\n";
    print STDERR "The from address must be specified with -F if either -m or -M\n";
    print STDERR "is used.\n";
    print STDERR "\n";
    exit( 1 );
}

# --------------------------------------------------------------------
# MakeDirs:
# --------------------------------------------------------------------
sub MakeDirs {
    # REFACTOR FIX: Changed bin and dat directory permissions from 0755 to 0775
    # to ensure both directories have identical permissions. This maintains
    # consistency with existing /usr/viewtouch/bin directory which has 775 permissions.
    # Combined with umask(0022) fix in main/manager.cc, this ensures proper directory access.
    unless ( -d $bindir ) {
        if ( !mkdir( $bindir, 0775 ) ) {
            print STDERR "Failed to create $bindir:  $!\n";
        }
    }
    unless ( -d $datdir ) {
        if ( !mkdir( $datdir, 0775 ) ) {
            print STDERR "Failed to create $datdir:  $!\n";
        }
    }
    unless ( -d $printdir ) {
        if ( !mkdir( $printdir, 0755 ) ) {
            print STDERR "Failed to create $printdir:  $!\n";
        }
    }
    unless ( -d $root ) {
        if ( !mkdir( $root, 0755 ) ) {
            print STDERR "Failed to create $root:  $!\n";
        }
    }
    unless ( -d $tmpdir ) {
        if ( !mkdir( $tmpdir, 0755 ) ) {
            print STDERR "Failed to create $tmpdir:  $!\n";
        }
    }
}

# --------------------------------------------------------------------
# DateToString:  Converts a date array returned by gmtime() or
#   localtime() to a string in the format "dd/mm/yyyy".
# --------------------------------------------------------------------
sub DateToString {
    my ( $d, $type ) = @_;
    my $date = "";
    $date = $$d[3] . "/" . ($$d[4] + 1) . "/" . ($$d[5] + 1900);
    $date .= ",00:00";
    return( $date );
}

# --------------------------------------------------------------------
# MakeDate:  Creates two dates for yesterday morning and yesterday
#   evening and returns them both as a list.
# --------------------------------------------------------------------
sub MakeDate {
    my ( $type, $when ) = @_;
    $when = -1 unless ( defined( $when ) );
    if ( $type eq "to" ) {
        $when += 1;
    }
    my $date = time();
    $date += ( 86400 * $when );
    my @d = localtime($date);
    $date = DateToString( \@d, $type );
    return( $date );
}

# --------------------------------------------------------------------
# RunViewTouch:
# --------------------------------------------------------------------
sub RunViewTouch {
    my @command = ( $vtpos, "-n" );
    if ( $datdir ne $viewtouch_dat ) {
        push( @command, ( "-p", $datdir ) );
    }
    system( @command );
}

# --------------------------------------------------------------------
# SignalViewTouch:
# --------------------------------------------------------------------
sub SignalViewTouch {
    system( "killall", "-USR2", "vt_main" );
}

# --------------------------------------------------------------------
# ReadConfig:
# --------------------------------------------------------------------
sub ReadConfig {
    my ( $file ) = @_;
}

# --------------------------------------------------------------------
# ReadGlobalConfig:
# --------------------------------------------------------------------
sub ReadGlobalConfig {
    if ( open( CONF, $global_config ) ) {
        while ( <CONF> ) {
            next if ( /^\s*$/ );
            next if ( /^\s*#/ );
            my ( $key, $value ) = $_ =~ /^\s*([^:]+):\s+(.*)$/;
            $key =~ s/^\s+|\s+$//g;
            $value =~ s/^\s+|\s+$//g;
            if ( $key eq 'binary_directory' ) {
                $bindir = $value;
            } elsif ( $key eq 'command_file' ) {
                $config = $value;
            } elsif ( $key eq 'data_directory' ) {
                $datdir = $value;
            } elsif ( $key eq 'print_directory' ) {
                $printdir = $value;
            } elsif ( $key eq 'root_directory' ) {
                $root = $value;
            } elsif ( $key eq 'temporary_directory' ) {
                $tmpdir = $value;
            } elsif ( $key eq 'mail_server' ) {
                $server = $value;
            } elsif ( $key eq 'reply_to' ) {
                $fromadd = $value;
            }
        }
        close( CONF );
    }
}

# --------------------------------------------------------------------
# ReadCommandLine:
# --------------------------------------------------------------------
sub ReadCommandLine {
    my ( $arglist ) = @_;
    my $actions = "";
    if ( $endday ) {
        $actions .= "endday\n";
    }
    if ( @$arglist ) {
        if ( $startvt ) {
            $actions .= "nologin\n";
        }
        $actions .= $printer . "\n";
        foreach ( @$arglist ) {
            $actions .= "report: " . $_ . " " . $fromdate . " " . $todate . "\n";
        }
    }
    if ( $actions && $startvt ) {
        $actions .= "exitsystem\n";
    }
    return( $actions );
}

# --------------------------------------------------------------------
# WriteViewTouchCommands:
# --------------------------------------------------------------------
sub WriteViewTouchCommands {
    my ( $commands ) = @_;
    my $retval = 0;

    if ( open( FILE, ">$vtcommand" ) ) {
        print FILE $commands;
        close( FILE );
        $retval = 1;
    } else {
        print STDERR "Cannot write $vtcommand: $!\n";
    }
    return( $retval );
}

# --------------------------------------------------------------------
# NotRunning:  We want to allow this script to start ViewTouch, but
#   only if ViewTouch is not already running.  We don't want to
#   accidently interfere with an active process, especially if there
#   are important transactions in progress.
# Returns 1 if ViewTouch is not active, 0 otherwise
# --------------------------------------------------------------------
sub NotRunning {
    my $retval = 1;
    my $vt_main = `ps ax | grep vt_main | grep -v grep`;
    my $vt_term = `ps ax | grep vt_term | grep -v grep`;

    $retval = 0 if ( $vt_main && $vt_term );
    return( $retval );
}

# --------------------------------------------------------------------
# GetDataSubdirs:
# --------------------------------------------------------------------
sub GetDataSubdirs {
    my @subdirs;
    if ( opendir( DIR, $datdir ) ) {
        my @dirs = readdir( DIR );
        close( DIR );
        @dirs = grep( $_ !~ /^\./, @dirs );
        foreach ( @dirs ) {
            my $item = $datdir . $_;
            next unless ( -d $item );
            next unless ( -f $item . "/settings.dat" );
            $item .= "/" unless ( $item =~ m|/$| );
            push( @subdirs, $item );
        }
    }
    return( @subdirs );
}

# --------------------------------------------------------------------
# RenamePrintouts:
# --------------------------------------------------------------------
sub RenamePrintouts {
    my ( $dir ) = @_;
    my @files;
    if ( opendir( DIR, $dir ) ) {
        @files = readdir( DIR );
        closedir( DIR );
        @files = grep( $_ !~ /^\./, @files );
        map( $_ = $printdir . $_, @files );
    }
    foreach ( @files ) {
        next if ( /^\./ );
        my $file = $_;
        my ( $dirname ) = $datdir =~ m|/([^/]+)/$|;
        my $newname = $file;
        $newname =~ s/^([^-]+)(.*)$/$1-$dirname$2/;
        system( "mv", $file, $newname );
    }
}

# --------------------------------------------------------------------
# MovePrintouts:
# --------------------------------------------------------------------
sub MovePrintouts {
    my ( $dest, $src ) = @_;
    $dest .= "/" unless ( $dest =~ m|/$| );
    my @files;
    if ( opendir( DIR, $src ) ) {
        @files = readdir( DIR );
        @files = grep( $_ !~ /^\./, @files );
        closedir( DIR );
    }
    foreach ( @files ) {
        my $file = $src . $_;
        system( "mv", "-f", $file, $dest );
    }
}

# --------------------------------------------------------------------
# ParseDate:
# --------------------------------------------------------------------
sub ParseDate {
    my ( $date, $type ) = @_;
    $type = "both" unless ( $type );
    $date = lc( $date );
    my $retfrom = "";
    my $retto   = "";
    if ( $date =~ /^(day|month)([\-\+]?\d+)?$/ ) {
        my ( $period, $count ) = ( $1, $2 );
        $count = 0 unless ( $count );
        if ( $period eq "day" ) {
            $retfrom = MakeDate( "from", $count );
            $retto   = MakeDate( "to", $count );
        } elsif ( $period eq "month" ) {
            $retfrom = MakeDateMonthMod( "from", $count );
            $retto   = MakeDateMonthMod( "to", $count );
        }
    } elsif ( grep( $date =~ /^\+?$_$/, @months ) ) {
        $retfrom = MakeDateMonth( "from", $date );
        $retto   = MakeDateMonth( "to", $date );
    } elsif ( $date =~ m|^\d{2}/\d{2}/\d{4},\d{2}:\d{2}$| ) {
        $retfrom = $date;
        $retto = $date;
    }
    if ( $type eq "from" ) {
        return( $retfrom );
    } elsif ( $type eq "to" ) {
        return ( $retto );
    } else {
        return( $retfrom, $retto );
    }
}

# --------------------------------------------------------------------
# MakeDateMonthMod:  Calculate a date range (from and to) based on a
#  specification like "today minus one month".
# --------------------------------------------------------------------
sub MakeDateMonthMod {
    my ( $type, $mod ) = @_;
    my $date = "";
    if ( $type eq "to" ) {
        $mod += 1;
    }
    my @d = localtime();
    $d[3] = 1;
    $d[4] += $mod;
    while ( $d[4] > 11 ) {
        $d[4] -= 12;
        $d[5] += 1;
    }
    while ( $d[4] < 0 ) {
        $d[4] += 12;
        $d[5] -= 1;
    }
    $date = DateToString( \@d, $type );
    return( $date );
}

# --------------------------------------------------------------------
# MakeDateMonth:  Calculates a date range based on the specified
#  month.
# --------------------------------------------------------------------
sub MakeDateMonth {
    my ( $type, $month ) = @_;
    my $date = "";
    my $forcefor = ( $month =~ s/^\+// ) ? 1 : 0;
    my $mon = $lmonths{$month};
    $mon = $smonths{$month} unless ( defined( $mon ) );
    my @d = localtime();
    $d[3] = 1;
    if ( ( $mon > $d[4] ) && !$forcefor ) {
        $d[5] -= 1;
    }
    $d[4] = $mon;
    if ( $type eq "to" ) {
        $d[4] += 1;
        if ( $d[4] > 11 ) {
            $d[4] -= 12;
            $d[5] += 1;
        }
    }
    $date = DateToString( \@d, $type );
    return( $date );
}

# --------------------------------------------------------------------
# EmailPrintouts:
# --------------------------------------------------------------------
sub EmailPrintouts {
    my ( $address, $directory ) = @_;
    return( 0 ) unless ( $fromadd );

    # first get the list of files
    my @files;
    if ( opendir( DIR, $directory ) ) {
        @files = readdir( DIR );
        @files = grep( $_ !~ /^\./, @files );
        closedir( DIR );
    }

    # now generate the email headers
    my $subject = GetSubject( $files[0] );
    my $boundary = GetBoundary();
    my $email = "";
    $email .= "From:  $fromadd\n";
    $email .= "To:  $address\n";
    $email .= "Subject:  $subject\n";
    $email .= "MIME-Version:  1.0\n";
    $email .= "Content-Type: multipart/mixed;\n";
    $email .= "  boundary=\"$boundary\"\n";
    $email .= "\n";

    foreach ( @files ) {
        my $name = $_;
        my $file = $directory . $name;
        $email .= "\n--$boundary\n";
        $email .= "Content-Type: text/html;\n";
        $email .= "  name=\"$name\"\n";
        $email .= "Content-Disposition: attachment;\n";
        $email .= "  filename=\"$name\"\n";
        $email .= "\n";
        my $contents = ReadFile( $file );
        foreach ( @$contents ) {
            my $line = $_;
            if ( $line =~ /^\./ ) {
                $line = "." . $line;
            }
            $email .= $line;
        }
    }

    # finish it out and send it
    $email .= "\n--$boundary--\n";
    SendMail( $server, $fromadd, $address, $email );
}

# --------------------------------------------------------------------
# GetSubject:  Analyzes a filename.  If the filename includes a
#   subdirectory name, as it would if -x is used, then that is
#   added to the subject.  Otherwise, "all" is added.
# --------------------------------------------------------------------
sub GetSubject {
    my ( $file ) = @_;
    my $name = "";
    if ( $file =~ /^[^\-]+\-([^\-\d]+)\-/ ) {
        $name = "for $1";
    }
    my $subject = "ViewTouch reports $name";
    return( $subject );
}

# --------------------------------------------------------------------
# GetBoundary:  Generates a MIME boundary of 50 characters using
#   the number 0-9 and the letters a-z in upper and lower case.
# --------------------------------------------------------------------
sub GetBoundary {
    my $boundary = "";

    foreach ( 0..50 ) {
        my $elem = int( rand( 62 ) );
        if ( $elem < 10 ) {
            $boundary .= chr( $elem + 48 );
        } elsif ( $elem < 36 ) {
            $elem -= 10;
            $boundary .= chr( $elem + 65 );
        } else {
            $elem -= 36;
            $boundary .= chr( $elem + 97 );
        }
    }
    return( $boundary );
}

# --------------------------------------------------------------------
# ReadFile:
# --------------------------------------------------------------------
sub ReadFile {
    my ( $file ) = @_;
    my @contents = "";
    if ( open( FILE, $file ) ) {
        @contents = <FILE>;
        close( FILE );
    }
    return( \@contents );
}

# --------------------------------------------------------------------
# SendMail:  Communicates with the outgoing server to send the email.
# --------------------------------------------------------------------
sub SendMail {
    my ( $server, $from, $to, $message ) = @_;
    my $retval = 1;
    if ( $debug ) {
        print "    Connecting to $server\n";
        print "    Sending message from $from\n";
        print "    Sending message to $to\n";
    }

    # set up the connection
    my @result;
    my $proto = getprotobyname( 'tcp' );
    my $port = getservbyname( 'smtp', 'tcp' );
    my $iaddr = inet_aton( $server );
    my $paddr = sockaddr_in( $port, $iaddr );
    unless ( socket( SOCKET, PF_INET, SOCK_STREAM, $proto ) ) {
        print STDERR "No socket:  $!\n";
        return( $retval );
    }
    unless ( connect( SOCKET, $paddr ) ) {
        print STDERR "No Connection to $server:  $!\n";
        return( $retval );
    }
    my $sock = *SOCKET;
    my $buffer;

    if ( CheckResult( $sock, 299, "Connecting" ) ) {
        close( SOCKET );
        return( $retval );
    }

    $buffer = "MAIL FROM:$from\r\n";
    syswrite( SOCKET, $buffer, length( $buffer ) );
    if ( CheckResult( $sock, 299, "From address" ) ) {
        close( SOCKET );
        return( $retval );
    }

    $buffer = "RCPT TO:$to\r\n";
    syswrite( SOCKET, $buffer, length( $buffer ) );
    if ( CheckResult( $sock, 299, "Recipient address" ) ) {
        close( SOCKET );
        return( $retval );
    }

    syswrite( SOCKET, "DATA\r\n", 6 );
    if ( CheckResult( $sock, 399, "Sending data" ) ) {
        close( SOCKET );
        return( $retval );
    }

    syswrite( SOCKET, $message, length( $message ) );
    syswrite( SOCKET, ".\r\n", 3 );

    $retval = CheckResult( $sock, 299, "Data sent" );
    close( SOCKET );
    return( $retval );
}

# --------------------------------------------------------------------
# CheckResult:  Returns 0 if there are no errors, 1 otherwise.
# --------------------------------------------------------------------
sub CheckResult {
    my ( $socket, $max, $message ) = @_;
    $max = 299 unless ( $max );
    my $retval = 0;
    my $result = "";

    sysread( $socket, $result, 256 );
    $result =~ s/^\s+|\s+$//sg;
    my $shortres = $result;
    $shortres =~ s/^(\d+).*$/$1/g;

    if ( $debug ) {
        print STDERR "$message:  $result\n";
    } elsif ( $shortres > $max ) {
        print STDERR "Error $message:  $result\n";
        $retval = 1;
    }

    return( $retval );
}
