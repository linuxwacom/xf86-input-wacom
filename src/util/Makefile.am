bin_PROGRAMS = @WCM_PROGS@ @WCM_XF86PROGS@
lib_LTLIBRARIES = libwacomcfg.la

wacomcfgdir = $(includedir)/wacomcfg
wacomcfg_HEADERS = wacomcfg.h

AM_CFLAGS = -Wall -pedantic
DEPFLAGS = @WCM_DEPFLAGS@

# These identify which programs, libraries, and headers could
# potentially be built or installed depending on the results of
# the configuration.
EXTRA_PROGRAMS = wacdump xidump xsetwacom 

# Source dependencies
wacdump_SOURCES = wacdump.c wacscrn.c wacscrn.h \
		wactablet.c wactablet.h \
		wacserial.c wacserial.h \
		wacusb.c wacusb.h 
wacdump_LDADD = -lncurses

xidump_SOURCES = xidump.c wacscrn.c wacscrn.h
xidump_LDFLAGS = @WCM_XIDUMP_LIBS@
if WCM_ENV_NCURSES
xidump_LDADD = -lncurses
endif

libwacomcfg_la_SOURCES = wacomcfg.c wacomcfg.h ../include/Xwacom.h
libwacomcfg_la_LDFLAGS = -no-undefined -version-info @WCM_LIBWACOMCFG_VER@
libwacomcfg_la_LIBADD = @WCM_LIBWACOMCFG_LIBS@

xsetwacom_SOURCES = xsetwacom.c wacomcfg.h wcmAction.c wcmAction.h ../include/Xwacom.h
xsetwacom_LDADD = libwacomcfg.la
