AM_CFLAGS = -Wall

# WacomXI Install
if WACOMXI_INSTALL
tcllibdir = $(libdir)/TkXInput
tcllib_HEADERS = pkgIndex.tcl
tcllib_LTLIBRARIES = libwacomxi.la
cpldir = $(bindir)
cpl_SCRIPTS = wacomcpl wacomcpl-exec
endif

EXTRA_DIST = wacomcpl-exec

# Source dependencies
libwacomxi_la_SOURCES = wacomxi.c wacomxi.h
libwacomxi_la_LDFLAGS = -no-undefined
libwacomxi_la_LIBADD = @WCM_WACOMXI_LIBS@
