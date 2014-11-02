#!/bin/sh

BASEDIR=/usr/viewtouch
TARDIR=vt_data_`date +'%b_%d_%Y'`
TARFIL=$TARDIR.tar.gz

FILES="bin/vt_data dat/settings.dat dat/system.dat dat/tables.dat \
      dat/zone_db.dat"

if [ ! -d $TARDIR ]; then
    mkdir $TARDIR
fi
for file in $FILES
do
    cp $BASEDIR/$file $TARDIR
done

tar czf $TARFIL $TARDIR

echo
echo
echo Your data files have been gathered into a file named
echo $TARFIL in the current directory.
echo Please send that file to brking@viewtouch.com
echo
echo Feel free to delete the $TARDIR directory.
echo


