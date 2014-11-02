#!/bin/sh

#################################################
# Module:  vtstart.sh
# Description:  Script to auto-launch X Windows
#   and ViewTouch for the viewtouch user.
# Author:  Bruce Alon King
# Modified:  Fri Sep 13 12:06:11 2002
#
# $Log: vtstart.sh,v $
# Revision 1.1  2002/09/13 19:13:17  brking
# Scripts to automate the installation and execution of ViewTouch.
#
#################################################

# we won't actually launch ViewTouch.  Let wmaker do that.
/usr/X11R6/bin/startx &
