AM_CFLAGS = -Wall

# WacomXI Install
if WACOMXI_INSTALL
tcllibdir = @WCM_WACOMXI_LIBDIR@/TkXInput
tcllib_HEADERS = pkgIndex.tcl
tcllib_LTLIBRARIES = libwacomxi.la
cpldir = @WCM_WACOMXI_BINDIR@
cpl_SCRIPTS = wacomcpl wacomcpl-exec
endif

# Source dependencies
libwacomxi_la_SOURCES = wacomxi.c wacomxi.h
libwacomxi_la_LDFLAGS = -no-undefined
libwacomxi_la_LIBADD = @WCM_WACOMXI_LIBS@
