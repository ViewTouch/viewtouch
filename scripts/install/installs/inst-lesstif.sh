#!/bin/sh

#########################################################################
# Module:  inst-lesstif.sh
# Description:  Downloads, compiles, and installs Lesstif 0.93.18.
# Author:  Bruce Alon King
# Modified:  Fri Sep 13 12:06:11 2002
#
# $Log: inst-lesstif.sh,v $
# Revision 1.1  2002/09/13 19:13:17  brking
# Scripts to automate the installation and execution of ViewTouch.
#
#########################################################################

ftp -a -i 'ftp://ftp.rge.com/pub/X/lesstif/srcdist/lesstif-0.93.18.tar.gz'
tar xzf lesstif-0.93.18.tar.gz
cd lesstif-0.93.18
./configure && make && make install
cd ..
yes | rm -r lesstif-0.93.18

