#!/usr/bin/perl -w

# --------------------------------------------------------------------
# Module:  update-server
# Description:  Provides a central repository for ViewTouch files
#   such as vt_data.
# Author:  Bruce Alon King
# Created:  Tue Dec 28 23:40:29 2004
# --------------------------------------------------------------------

# ####################################################################
# INITIALIZATION AND GLOBAL VARIABLES
# ####################################################################
use strict;
use vars qw/$hostname $debug_mode $cksum/;
BEGIN {
    $debug_mode = 0;
    $cksum = "/usr/bin/cksum";
	$hostname = `hostname`; chomp( $hostname );
    if ( $hostname =~ /asgard|debian|mainbox/ ) {
        $debug_mode = 1;
    }
    push( @INC, "/usr/local/www/cgi-bin/" );
}
use ViewTouch::CGI;

my $filesdir = '/usr/local/www/data/vt_updates';
my $cgi     = new ViewTouch::CGI( 'debug' => $debug_mode );
my $action = $cgi->Get( 'action' ) || 'list';


# ####################################################################
# MAIN LOOP
# ####################################################################
print "Content-Type:  text/plain\n\n";
if ( $action eq "list" ) {
    ListFiles( $cgi );
} elsif ( $action eq "download" ) {
    DownloadFiles( $cgi );
} elsif ( $action eq "upload" ) {
    UploadFiles( $cgi );
}

# ####################################################################
# SUBROUTINES
# ####################################################################

# --------------------------------------------------------------------
# ListFiles: Provides cksum output for all non-dot files.
# --------------------------------------------------------------------
sub ListFiles {
    my ( $cgi ) = @_;
    my $file;

    if ( opendir ( DIR, $filesdir ) ) {
        while ( $file = readdir( DIR ) ) {
            if ( $file !~ /^\./ ) {
                my @out = split( m/ /, `$cksum $filesdir/$file` );
                print "$out[0] $out[1] $file\n";
            }
        }
        closedir( DIR );
    }
}

# --------------------------------------------------------------------
# DownloadFiles:
# --------------------------------------------------------------------
sub DownloadFiles {
    my ( $cgi ) = @_;
}

# --------------------------------------------------------------------
# UploadFiles:
# --------------------------------------------------------------------
sub UploadFiles {
    my ( $cgi ) = @_;
}
