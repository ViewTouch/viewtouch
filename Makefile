
#########################################################
# ViewTouch Makefile Template
#########################################################

# System Settings Start
PLATFORM        = -DLINUX
TOUCHSCREEN     =
STRIP           = /usr/bin/strip
MAKE            = /usr/bin/make
MAKEDEP         = /usr/X11R6/bin/makedepend
KILLALL_CMD     = /usr/bin/killall
INCDIRS         = -I/usr/include -I/usr/include/freetype2 -I/usr/local/include -Iexternal/ -Imain/ -Iterm/ -Izone/ -I. 
CREDIT_DEF      =
CREDIT_LIB      =
CREDIT_INC      =
TERM_CREDIT     = term/term_credit.o
VT_CCQ_PIPE     =
VIEWTOUCH_PATH  = /usr/viewtouch
DEBUG           = -DDEBUG=1
MALLOC          =
MALLOC_LIB      =
DBGFLAG         = -ggdb3
OPTIMIZE        =
TARGET          = debug
LICENSE_SERVER  = viewtouch.com
# System Settings End

# Compiler stuff
CC        = g++ $(DBGFLAG) -c -o $@ $(INCDIRS) $(CREDIT_INC)
LINK      = g++ $(DBGFLAG) -o $@
DEFINE    = -DVIEWTOUCH_PATH=\"$(VIEWTOUCH_PATH)\" $(PLATFORM) $(TOUCHSCREEN) $(DEBUG) $(MALLOC) \
	        -DBUILD_NUMBER=\"$(BUILD_NUMBER)\" -DKILLALL_CMD=\"$(KILLALL_CMD)\" \
			-DLICENSE_SERVER=\"$(LICENSE_SERVER)\"
CFLAGS    = -Wall -Wshadow $(OPTIMIZE) $(DEFINE)
RM        = rm -f
CP        = cp -f
MV        = mv -f
CHANGELOG = $(HOME)/bin/cvs2cl.pl -I last-release \
	-I docs/ -I scripts/ -I web/ -I Makefile

LIB_PATH = -L/usr/local/lib -L/usr/lib -L/usr/X11R6/lib

LOADER_OBJS    = loader/loader_main.o debug.o main/labels.o generic_char.o \
                 logger.o

MAIN_OBJS      = utility.o data_file.o remote_link.o socket.o \
                 main/manager.o main/license.o main/printer.o \
                 main/terminal.o main/settings.o main/labels.o \
                 main/locale.o main/credit.o main/sales.o \
                 main/check.o main/account.o main/system.o \
                 main/archive.o main/drawer.o main/inventory.o \
                 main/employee.o main/labor.o main/tips.o \
                 main/exception.o main/customer.o main/report.o \
                 main/system_report.o main/system_salesmix.o \
                 main/chart.o main/expense.o debug.o \
                 main/cdu.o main/cdu_att.o external/sha1.o \
                 external/blowfish.o main/license_hash.o conf_file.o

ZONE_OBJS      = zone/zone.o zone/zone_object.o zone/pos_zone.o \
                 zone/layout_zone.o zone/form_zone.o \
                 zone/dialog_zone.o zone/button_zone.o \
                 zone/order_zone.o zone/payment_zone.o \
                 zone/login_zone.o zone/user_edit_zone.o \
                 zone/check_list_zone.o zone/settings_zone.o \
                 zone/report_zone.o zone/table_zone.o \
                 zone/split_check_zone.o zone/drawer_zone.o \
                 zone/printer_zone.o zone/payout_zone.o \
                 zone/inventory_zone.o zone/labor_zone.o \
                 zone/phrase_zone.o zone/merchant_zone.o \
                 zone/account_zone.o zone/hardware_zone.o \
                 zone/search_zone.o zone/chart_zone.o \
                 zone/video_zone.o zone/expense_zone.o \
                 zone/cdu_zone.o zone/creditcard_list_zone.o conf_file.o

TERM_OBJS      = term/term_main.o term/term_view.o term/touch_screen.o \
	             term/layer.o term/term_dialog.o $(TERM_CREDIT) \
			     main/labels.o utility.o remote_link.o image_data.o \
			     debug.o generic_char.o conf_file.o

PRINT_OBJS     = print/print_main.o utility.o socket.o conf_file.o

CDU_OBJS       = cdu/cdu_main.o utility.o socket.o main/cdu_att.o

AUTHORIZE_OBJS = authorize_main.o utility.o

TEMPHASH_OBJS  = main/license_hash.o main/temphash.o external/sha1.o

VT_CCQ_OBJS    = vt_ccq_pipe.o socket.o utility.o conf_file.o

OBJS           = $(MAIN_OBJS) $(ZONE_OBJS)

INC        = basic.hh utility.hh
ZONE_INC   = basic.hh utility.hh zone/zone.hh zone/pos_zone.hh main/terminal.hh

IMAGES     = xpm/darkwood-10.xpm xpm/grayparchment-8.xpm xpm/graymarble-12.xpm \
	         xpm/greenmarble-12.xpm xpm/litewood-8.xpm xpm/litsand-6.xpm \
             xpm/parchment-6.xpm xpm/sand-8.xpm xpm/wood-10.xpm xpm/canvas-8.xpm \
             xpm/pearl-8.xpm xpm/tanparchment-8.xpm xpm/smoke-8.xpm \
             xpm/leather-8.xpm xpm/blueparchment.xpm

BINS       = vtpos vt_main vt_term vt_print vt_cdu vt_temphash $(VT_CCQ_PIPE)
DESTDIR =

# Main Build
.PHONY: all binaries clean distclean dep debug install release \
	strip package log

# Object build template

.cc.o:
	$(CC) $(CFLAGS) $(CREDIT_DEF) $<

.c.o:
	$(CC) $(CFLAGS) $(CREDIT_DEF) $<

all: $(TARGET)

binaries: $(BINS)

clean:
	$(RM) $(OBJS) $(LOADER_OBJS) $(TERM_OBJS)
	$(RM) core *~ loader/*~ main/*~ zone/*~ term/*~ print/*~ authorize/*~
	$(RM) -f $(BINS) vt_ccq_pipe tester
	find . -name '*.core' | xargs rm -f
	find . -name '*.o' | xargs rm -f
	rm -f ChangeLog*

distclean:
	$(MAKE) clean
	rm -f Makefile.bak
	rm -f Makefile
	rm -f G*
	rm -f TAGS

install:
	install -d $(DESTDIR)$(VIEWTOUCH_PATH)/bin/
	$(CP) $(BINS) $(DESTDIR)$(VIEWTOUCH_PATH)/bin/
	$(CP) scripts/vtrestart $(DESTDIR)$(VIEWTOUCH_PATH)/bin/
	$(CP) scripts/vtrun $(DESTDIR)$(VIEWTOUCH_PATH)/bin/
	$(CP) scripts/vtcommands.pl $(DESTDIR)$(VIEWTOUCH_PATH)/bin/vtcommands
	$(CP) scripts/keeprunning $(DESTDIR)$(VIEWTOUCH_PATH)/bin/
	$(CP) scripts/keeprunningcron $(DESTDIR)$(VIEWTOUCH_PATH)/bin/
	$(CP) scripts/vt_ping $(DESTDIR)$(VIEWTOUCH_PATH)/bin/
	$(CP) scripts/lpd-restart $(DESTDIR)$(VIEWTOUCH_PATH)/bin/
	$(CP) scripts/vt_openterm $(DESTDIR)$(VIEWTOUCH_PATH)/bin/
	scripts/installer scripts/vt_shutdown $(DESTDIR)$(VIEWTOUCH_PATH)/bin/ \
		VIEWTOUCH_PATH=$(VIEWTOUCH_PATH)
	scripts/installer scripts/vt_pingcheck $(DESTDIR)$(VIEWTOUCH_PATH)/bin/ \
		VIEWTOUCH_PATH=$(VIEWTOUCH_PATH)
	scripts/installer scripts/usercount $(DESTDIR)$(VIEWTOUCH_PATH)/bin/ \
		VIEWTOUCH_PATH=$(VIEWTOUCH_PATH)
	scripts/installer scripts/update-client $(DESTDIR)$(VIEWTOUCH_PATH)/bin/ \
		VIEWTOUCH_PATH=$(VIEWTOUCH_PATH)

debug:
	$(MAKE) binaries "BUILD_NUMBER=`scripts/getbuildnum`"
	$(MAKE) install

dep:
	find . -name '*.cc' -o -name '*.hh' | xargs $(MAKEDEP) $(CREDIT_DEF) $(PLATFORM) $(INCDIRS) $(CREDIT_INC)

log:
	$(CHANGELOG)

package:
	$(CHANGELOG) && $(CP) ChangeLog $(VIEWTOUCH_PATH)/bin/
	@scripts/packagerelease $(CREDIT_PKG)
	$(RM) $(VIEWTOUCH_PATH)/bin/ChangeLog
	@echo Build done, tarball ready

release:
	@scripts/reportdebugs
	$(MAKE) binaries "BUILD_NUMBER=`scripts/getbuildnum`"
	$(MAKE) strip
	$(MAKE) install

strip:
	$(STRIP) $(BINS)

vtpos: $(LOADER_OBJS)
	$(LINK) $^ $(LIB_PATH) -lX11 -lXm -lXt -lXft -lfreetype -lz -lfontconfig -lXrender $(CREDIT_LIB) $(MALLOC_LIB)

vt_main: $(OBJS)
	$(LINK) $^ $(LIB_PATH) -lXt -lX11 -lz -lXft -lfreetype -lfontconfig -lXrender $(CREDIT_LIB) $(MALLOC_LIB)

vt_term: $(TERM_OBJS)
	$(LINK) $^ $(LIB_PATH) -lX11 -lXm -lXmu -lXt -lXpm -lXft -lfreetype -lz -lfontconfig -lXrender $(CREDIT_LIB) $(MALLOC_LIB)

vt_print: $(PRINT_OBJS)
	$(LINK) $^ $(LIB_PATH) $(MALLOC_LIB)

vt_cdu: $(CDU_OBJS)
	$(LINK) $^ $(LIB_PATH) $(MALLOC_LIB)

vt_temphash: $(TEMPHASH_OBJS)
	$(LINK) $^ $(LIB_PATH) $(MALLOC_LIB)

vt_ccq_pipe:  $(VT_CCQ_OBJS)
	$(LINK) $^ $(LIB_PATH) $(MALLOC_LIB)


# Archiving
archive:
	tar -cf viewtouch-src.tar Makefile *.cc *.hh loader/*.cc main/*.cc main/*.hh \
	        zone/*.cc zone/*.hh term/*.cc term/*.hh print/*.cc authorize/*.cc \
			xpm/*.xpm
	bzip2 viewtouch-src.tar

# DO NOT DELETE
