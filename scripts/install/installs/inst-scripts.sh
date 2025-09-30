#!/bin/sh

#########################################################################
# Module:  inst-scripts.sh
# Description:  Install any scripts necessary for proper ViewTouch
#   operation.  Example: installs the rc start/stop script.
# Author:  Bruce Alon King
# Modified:  Fri Sep 13 12:06:11 2002
#
# $Log: inst-scripts.sh,v $
# Revision 1.1  2002/09/13 19:13:17  brking
# Scripts to automate the installation and execution of ViewTouch.
#
#########################################################################

cd /usr/local/etc/rc.d
ftp -a -i 'ftp://mainbox.viewtouch.com:/pub/scripts/viewtouch.sh'
chmod 755 viewtouch.sh
cd /home/viewtouch
mkdir bin
cd bin
ftp -a -i 'ftp://mainbox.viewtouch.com:/pub/scripts/vtstart.sh'
chmod 755 vtstart.sh
cd ..
chown -R viewtouch.viewtouch bin
