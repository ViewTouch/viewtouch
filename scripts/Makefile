
.PHONY:  vtrun arcfix dat2txt vtbuild vtclean vtsave vtinst vt_data keeprunningcron

BINDIR=${HOME}/bin

vtrun:
	install -m 755 vtrun ${BINDIR}

arcfix:
	install -m 755 arcfix ${BINDIR}

dat2txt:
	install -m 755 dat2txt ${BINDIR}

vtbuild:
	install -m 755 vtbuild ${BINDIR}

vtclean:
	install -m 755 vtclean ${BINDIR}

vtsave:
	install -m 755 vtsave ${BINDIR}

vtinst:
	install -m 755 vtinst ${BINDIR}

vt_data:
	install -m 755 vt_data.pl ${BINDIR}/vt_data

keeprunningcron:
	install -m 755 keeprunningcron ${BINDIR}

rollback: rollback.c
	gcc -I/usr/X11R6/include -L/usr/X11R6/lib -lXm -lcrypt rollback.c -o rollback

getmid: getmid.cc ../main/license_hash.o
	gcc -I/usr/X11R6/include -I.. -I ../main -I../term -o getmid ../main/license_hash.o ../external/sha1.o getmid.cc