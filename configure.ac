#  Copyright 2005 Adam Jackson.
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  on the rights to use, copy, modify, merge, publish, distribute, sub
#  license, and/or sell copies of the Software, and to permit persons to whom
#  the Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice (including the next
#  paragraph) shall be included in all copies or substantial portions of the
#  Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
#  ADAM JACKSON BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
#  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
#  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
# Process this file with autoconf to produce a configure script

# Initialize Autoconf
AC_PREREQ([2.60])
AC_INIT([xf86-input-wacom],
        [0.10.99],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],
        [xf86-input-wacom])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])

# Initialize Automake
AM_INIT_AUTOMAKE([foreign dist-bzip2])
AM_MAINTAINER_MODE
AC_USE_SYSTEM_EXTENSIONS

# Initialize libtool
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

# Initialize X.Org macros 1.8 or later for MAN_SUBSTS set by XORG_MANPAGE_SECTIONS
m4_ifndef([XORG_MACROS_VERSION],
          [m4_fatal([must install xorg-macros 1.8 or later before running autoconf/autogen])])
XORG_MACROS_VERSION([1.8])
XORG_DEFAULT_OPTIONS
XORG_WITH_DOXYGEN(1.6.1)

# Checks for libraries.
AC_CHECK_LIB([m], [rint])

# Obtain compiler/linker options from server and required extensions
PKG_CHECK_MODULES(XORG, [xorg-server >= 1.7.0] xproto xext kbproto inputproto randrproto)

# Obtain compiler/linker options for the xsetwacom tool
PKG_CHECK_MODULES(X11, x11 xi xrandr)

# X Server SDK location is required to install wacom header files
# This location is also relayed in the xorg-wacom.pc file
sdkdir=`$PKG_CONFIG --variable=sdkdir xorg-server`

# Workaround overriding sdkdir to be able to create a tarball when user has no
# write permission in sdkdir. See DISTCHECK_CONFIGURE_FLAGS in Makefile.am
AC_ARG_WITH([sdkdir], [], [sdkdir="$withval"])
AC_SUBST([sdkdir])

DRIVER_NAME=wacom
AC_SUBST([DRIVER_NAME])

# -----------------------------------------------------------------------------
#			Configuration options
# -----------------------------------------------------------------------------
# Define a configure option for code debugging
AC_ARG_ENABLE(debug,
              AS_HELP_STRING([--enable-debug],
                             [Enable debugging (default: disabled)]),
                             [DEBUGGING=$enableval], [DEBUGGING=yes])

# Define the C preprocessor macro DEBUG in config.h
if test "x$DEBUGGING" = xyes; then
       AC_DEFINE(DEBUG, 1, [Enable debugging code])
fi

# Define a configure option for an alternate input module directory
AC_ARG_WITH(xorg-module-dir,
            AS_HELP_STRING([--with-xorg-module-dir=DIR],
                           [Default xorg module directory [[default=$libdir/xorg/modules]]]),
            [moduledir="$withval"],
            [moduledir="$libdir/xorg/modules"])
inputdir=${moduledir}/input
AC_SUBST(inputdir)

# Define a configure option for an alternate X Server configuration directory
sysconfigdir=`$PKG_CONFIG --variable=sysconfigdir xorg-server`
AC_ARG_WITH([xorg-conf-dir],
	    AS_HELP_STRING([--with-xorg-conf-dir=DIR],
			   [Default xorg.conf.d directory [[default=from $PKG_CONFIG xorg-server]]]),
            [configdir="$withval"],
            [configdir="$sysconfigdir"])
AC_SUBST(configdir)
AM_CONDITIONAL(HAS_XORG_CONF_DIR, [test "x$sysconfigdir" != "x"])

# enalbe unit-tests if glib-devel is available
AC_ARG_ENABLE(unit-tests, AS_HELP_STRING([--enable-unit-tests],
                          [Enable unit-tests (default: auto)]),
                          [UNITTESTS=$enableval],
                          [UNITTESTS=auto])

# If unittests aren't explicitly disabled, check for required support
if test "x$UNITTESTS" != xno ; then
       PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.16],
                         [HAVE_GLIB=yes], [HAVE_GLIB=no])

       # Check if linker supports -wrap, passed via compiler flags
       # When cross-compiling, reports no, since unit tests run from
       # "make check", so would be running on build machine,  not target
       AC_MSG_CHECKING([whether the linker supports -wrap])
       save_LDFLAGS="$LDFLAGS"
       LDFLAGS="$LDFLAGS -Wl,-wrap,exit"
       AC_RUN_IFELSE([AC_LANG_PROGRAM([[
        void __wrap_exit (int s)
        {
            __real_exit (0);
        }]],
        [[exit (1);]])],
                     [linker_can_wrap="yes"],
                     [linker_can_wrap="no"],
                     [linker_can_wrap="no"])
       AC_MSG_RESULT([$linker_can_wrap])
       LDFLAGS="$save_LDFLAGS"
fi

if test "x$UNITTESTS" = xauto; then
       if test "x$HAVE_GLIB" = xyes && test "x$linker_can_wrap" = xyes; then
           UNITTESTS=yes
       else
           UNITTESTS=no
       fi
fi
if test "x$UNITTESTS" = xyes; then
       if test "x$HAVE_GLIB" = xno; then
           AC_MSG_ERROR([glib required to build unit tests])
       fi
       if test "x$linker_can_wrap" = xno; then
           AC_MSG_ERROR([ld -wrap support required to build unit tests])
       fi
       AC_DEFINE(UNITTESTS, 1, [Enable unit tests])
       AC_SUBST([GLIB_LIBS])
       AC_SUBST([GLIB_CFLAGS])
fi
AM_CONDITIONAL(UNITTESTS, [test "x$UNITTESTS" = xyes])



# -----------------------------------------------------------------------------

AC_CONFIG_FILES([Makefile
                 conf/Makefile
                 doc/Makefile
                 doc/doxygen.conf
                 src/Makefile
                 man/Makefile
                 include/Makefile
                 tools/Makefile
                 test/Makefile
                 xorg-wacom.pc])
AC_OUTPUT
