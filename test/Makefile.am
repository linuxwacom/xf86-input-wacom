if UNITTESTS
include ../src/common.mk

check_PROGRAMS = wacom-tests
check_LTLIBRARIES = libwacom-test.la
libwacom_test_la_SOURCES =$(DRIVER_SOURCES)
libwacom_test_la_CFLAGS = -DDISABLE_STATIC -I$(top_srcdir)/src $(XORG_CFLAGS) $(CWARNFLAGS) -fvisibility=default

TESTS=$(check_PROGRAMS)

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/include
AM_CFLAGS = $(XORG_CFLAGS) $(CWARNFLAGS) $(UDEV_CFLAGS)
TEST_LDADD= libwacom-test.la
COMMON_SOURCES=fake-symbols.c fake-symbols.h

wacom_tests_LDADD=$(TEST_LDADD) $(UDEV_LIBS)
wacom_tests_SOURCES=wacom-tests.c $(COMMON_SOURCES)

endif
