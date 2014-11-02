#!/bin/sh

#########################################################################
# Module:  inst-user.sh
# Description:  Create the viewtouch user and install the user's home
#   directory.
# Author:  Bruce Alon King
# Modified:  Fri Sep 13 12:06:11 2002
#
# $Log: inst-user.sh,v $
# Revision 1.2  2002/10/10 19:54:42  brking
# Added root cronjob to verify the permissions for /dev/lpt0 to ensure
# printing works.
#
# Revision 1.1  2002/09/13 19:13:17  brking
# Scripts to automate the installation and execution of ViewTouch.
#
#########################################################################

cd /etc
ftp -a -i 'ftp://mainbox.viewtouch.com:/pub/scripts/master.passwd'
chmod 600 master.passwd
ftp -a -i 'ftp://mainbox.viewtouch.com:/pub/scripts/passwd'
chmod 644 passwd
ftp -a -i 'ftp://mainbox.viewtouch.com:/pub/scripts/group'
chmod 644 group

cd /home
ftp -a -i 'ftp://mainbox.viewtouch.com:/pub/scripts/viewtouch-home.tar.gz'
tar xzf viewtouch-home.tar.gz
rm viewtouch-home.tar.gz
chown -R viewtouch.viewtouch viewtouch

CRONTABFILE=`/usr/bin/crontab -u root -l`
echo "$CRONTABFILE" >newcrontab
echo ' * * * * * chmod 666 /dev/lpt0' >>newcrontab
crontab -u root newcrontab
rm newcrontab

echo '***** DO NOT FORGET TO TEST PRINTING!!! *****'

cd
