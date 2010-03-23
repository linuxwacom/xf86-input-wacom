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
        [0.10.5],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],
        [xf86-input-wacom])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])

# Initialize Automake
AM_INIT_AUTOMAKE([foreign dist-bzip2])
AM_MAINTAINER_MODE

# Initialize libtool
AC_PROG_LIBTOOL
AC_DISABLE_STATIC

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.
AC_CHECK_LIB([m], [rint])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Initialize X.Org macros
m4_ifndef([XORG_MACROS_VERSION],
          [m4_fatal([must install xorg-macros 1.4 or later before running autoconf/autogen])])
XORG_MACROS_VERSION([1.4])
XORG_DEFAULT_OPTIONS

# Checks for pkg-config packages.
PKG_CHECK_MODULES(XORG, xorg-server xproto)
PKG_CHECK_MODULES(X11, x11 xi)

# Define a configure option for code debugging
AC_ARG_ENABLE(debug,
              AS_HELP_STRING([--enable-debug],
                             [Enable debugging (default: disabled)]),
                             [DEBUGGING=$enableval], [DEBUGGING=no])

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

# Checks for extensions
XORG_DRIVER_CHECK_EXT(XINPUT, inputproto)

# X Server SDK location is required to install wacom header files
# This location is also relayed in the xorg-wacom.pc file
sdkdir=`$PKG_CONFIG --variable=sdkdir xorg-server`

# Workaround overriding sdkdir to be able to create a tarball when user has no
# write permission in sdkdir. See DISTCHECK_CONFIGURE_FLAGS in Makefile.am
AC_ARG_WITH([sdkdir], [], [sdkdir="$withval"])
AC_SUBST([sdkdir])

DRIVER_NAME=wacom
AC_SUBST([DRIVER_NAME])

AC_CONFIG_FILES([Makefile
                 fdi/Makefile
                 src/Makefile
                 man/Makefile
                 include/Makefile
                 tools/Makefile
                 xorg-wacom.pc])
AC_OUTPUT
