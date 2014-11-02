#!/bin/sh

#########################################################################
# Module:  inst-xf86.sh
# Description:  Download and install XFree86 4.1.0
# Author:  Bruce Alon King
# Modified:  Fri Sep 13 12:06:11 2002
#
# $Log: inst-xf86.sh,v $
# Revision 1.1  2002/09/13 19:13:17  brking
# Scripts to automate the installation and execution of ViewTouch.
#
#########################################################################

mkdir xfree86-4.1.0
cd xfree86-4.1.0
ftp -a -i 'ftp://ftp.xfree86.org/pub/XFree86/4.1.0/binaries/FreeBSD-4.x/*'
yes | sh ./Xinstall.sh
cd ..
yes | rm -r xfree86-4.1.0

cd /etc/X11
cp XF86Config XF86Config.original
ftp -a -i 'ftp://mainbox.viewtouch.com:/pub/scripts/XF86Config'
