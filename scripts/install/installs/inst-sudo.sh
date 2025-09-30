#!/bin/sh

#########################################################################
# Module:  inst-sudo.sh
# Description:  Install and configure sudo so that the viewtouch user
#   can shutdown the system.
# Author:  Bruce Alon King
# Modified:  Fri Sep 13 12:06:11 2002
#
# $Log: inst-sudo.sh,v $
# Revision 1.1  2002/09/13 19:13:17  brking
# Scripts to automate the installation and execution of ViewTouch.
#
#########################################################################

cd /usr/ports/security/sudo
make install
cd /usr/local/etc
ftp -a -i 'ftp://mainbox.viewtouch.com:/pub/scripts/sudoers'
chmod 440 sudoers
