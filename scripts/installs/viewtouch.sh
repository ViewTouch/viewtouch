#!/bin/sh

#################################################
# Module:  viewtouch.sh
# Description:  rc start/stop script
# Author:  Bruce Alon King
# Modified:  Fri Sep 13 12:06:11 2002
#
# $Log: viewtouch.sh,v $
# Revision 1.1  2002/09/13 19:13:17  brking
# Scripts to automate the installation and execution of ViewTouch.
#
#################################################

case "$1" in
start)
	su viewtouch /home/viewtouch/bin/vtstart.sh
	;;
stop)
	killall vt_term vt_main vtpos
	;;
esac
