#!/bin/sh

#########################################################################
# Module:  inst-viewtouch.sh
# Description:  Download and install ViewTouch bin and dat directories.
# Author:  Bruce Alon King
# Modified:  Fri Sep 13 12:06:11 2002
#
# $Log: inst-viewtouch.sh,v $
# Revision 1.2  2002/10/14 22:14:47  brking
# Use the newer isntaller files.
#
# Revision 1.1  2002/09/13 19:13:17  brking
# Scripts to automate the installation and execution of ViewTouch.
#
#########################################################################

mkdir viewtouch
cd viewtouch
mkdir /usr/viewtouch
ftp -a -i 'ftp://mainbox.viewtouch.com/pub/viewtouch/FreeBSD/viewtouch-FreeBSD-latest.pl'
perl ./viewtouch-FreeBSD-Latest.pl alsodat
chown -R viewtouch.viewtouch /usr/viewtouch
