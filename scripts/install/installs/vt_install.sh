#!/bin/sh

#########################################################################
# Module:  vt_install.sh
# Description:  Downloads and executes the individual
#   install scripts.
# Author:  Bruce Alon King
# Modified:  Fri Sep 13 12:06:11 2002
#
# $Log: vt_install.sh,v $
# Revision 1.1  2002/09/13 19:13:17  brking
# Scripts to automate the installation and execution of ViewTouch.
#
#########################################################################

for command in inst-user.sh inst-xf86.sh inst-lesstif.sh \
    inst-viewtouch.sh inst-sudo.sh inst-scripts.sh
do
    ftp -a -i "ftp://mainbox.viewtouch.com/pub/scripts/$command"
    $command >$command.log 2>$command.err
    if [ $? != 0 ]; then
        echo got bad exit for $command
        exit
    else
        echo $command ran successfully
    fi
done

echo Installation complete.  Time to reboot and test.