dnl Macros for configuring the Linux Wacom package
dnl
AC_DEFUN([AC_WCM_CHECK_ENVIRON],[
dnl Variables for various checks
WCM_ENV_TCL=no
WCM_ENV_TK=no
WCM_XIDUMP_DEFAULT=yes
WCM_ENV_XLIB=no
dnl Check architecture

WCM_XLIBDIR_DEFAULT=/usr/X11R6/lib
WCM_XLIBDIR_DEFAULT2=/usr/lib
if test "$WCM_OPTION_XSERVER64" = "yes"; then
	CFLAGS="$CFLAGS -D__amd64__"
	WCM_XSERVER64="-D_XSERVER64"
	test `echo $WCM_ARCH | grep -c "x86_64"` == 0 || WCM_KSTACK="-mpreferred-stack-boundary=4 -mcmodel=kernel"
	WCM_XLIBDIR_DEFAULT=/usr/X11R6/lib64
	if test -d /usr/lib64; then
		WCM_XLIBDIR_DEFAULT2=/usr/lib64
	fi
fi
if test -f "$WCM_XLIBDIR_DEFAULT/Server/include/xf86Version.h" || 
test -f "$WCM_XLIBDIR_DEFAULT/Server/xf86Version.h"; then
	WCM_XORGSDK_DEFAULT=$WCM_XLIBDIR_DEFAULT/Server
else
	WCM_XORGSDK_DEFAULT=/usr
fi

WCM_TCLTKDIR_DEFAULT=/usr
XF86SUBDIR=programs/Xserver/hw/xfree86

WCM_ENV_NCURSES=no
])

dnl
AC_DEFUN([AC_WCM_CHECK_XORG_SDK],[
dnl Check for X11 sdk environment
dnl handle default case
AC_ARG_WITH(xorg-sdk,
AS_HELP_STRING([--with-xorg-sdk=dir], [Specify Xorg SDK directory]),
[ WCM_XORGSDK_DIR="$withval"; ])
if test x$WCM_ENV_XFREE86 != xyes; then
        dnl handle default case
	if test "$WCM_XORGSDK_DIR" = "yes" || test "$WCM_XORGSDK_DIR" == ""; then
		WCM_XORGSDK_DIR=$WCM_XORGSDK_DEFAULT
	fi
	if test -n "$WCM_XORGSDK_DIR"; then
		AC_MSG_CHECKING(for valid Xorg SDK)
		if test -f $WCM_XORGSDK_DIR/include/xf86Version.h; then
			WCM_XORGSDK_DIR=$WCM_XORGSDK_DIR/include
			WCM_ENV_XORGSDK=yes
			AC_MSG_RESULT(ok)
		elif test -f $WCM_XORGSDK_DIR/include/xorg/xf86Version.h; then
			WCM_ENV_XORGSDK=yes
			WCM_XORGSDK_DIR=$WCM_XORGSDK_DIR/include/xorg
			AC_MSG_RESULT(ok)
		elif test -f $WCM_XORGSDK_DIR/xc/include/xf86Version.h; then
			WCM_ENV_XORGSDK=yes
			WCM_XORGSDK_DIR=$WCM_XORGSDK_DIR/xc/include
			AC_MSG_RESULT(ok)
		else
			WCM_ENV_XORGSDK=no
			AC_MSG_RESULT("xf86Version.h missing")
			AC_MSG_RESULT([Tried $WCM_XORGSDK_DIR/include, $WCM_XORGSDK_DIR/include/xorg and $WCM_XORGSDK_DIR/xc/include])
		fi
	fi
fi
AM_CONDITIONAL(WCM_ENV_XORGSDK, [test x$WCM_ENV_XORGSDK = xyes])
])
AC_DEFUN([AC_WCM_CHECK_XSOURCE],[
dnl Check for X build environment
if test -d x-includes; then
	WCM_XFREE86_DIR=x-includes
fi
AC_ARG_WITH(x-src,
AS_HELP_STRING([--with-x-src=dir], [Specify X driver build directory]),
[ WCM_XFREE86_DIR="$withval"; ])
if test -n "$WCM_XFREE86_DIR"; then
	AC_MSG_CHECKING(for valid XFree86/X.org build environment)
	if test -f $WCM_XFREE86_DIR/xc/$XF86SUBDIR/xf86Version.h; then
		WCM_ENV_XFREE86=yes
		WCM_XFREE86_DIR="$WCM_XFREE86_DIR/xc"
		AC_MSG_RESULT(ok)
	elif test -f $WCM_XFREE86_DIR/$XF86SUBDIR/xf86Version.h; then
		WCM_ENV_XFREE86=yes
		AC_MSG_RESULT(ok)
	else
		WCM_ENV_XFREE86=no
		AC_MSG_RESULT(xf86Version.h missing)
		AC_MSG_RESULT(Tried $WCM_XFREE86_DIR/$XF86SUBDIR and $WCM_XFREE86_DIR/xc/$XF86SUBDIR)
	fi
fi
AM_CONDITIONAL(WCM_ENV_XFREE86, [test x$WCM_ENV_XFREE86 = xyes])
])
AC_DEFUN([AC_WCM_CHECK_XLIB],[
dnl Check for XLib development environment
WCM_XLIBDIR=
AC_ARG_WITH(xlib,
AS_HELP_STRING([--with-xlib=dir], [uses a specified X11R6 directory]),
[WCM_XLIBDIR=$withval])

dnl handle default case
AC_MSG_CHECKING(for X lib directory)

if test -d $WCM_XLIBDIR && test -f $WCM_XLIBDIR/libX11.so; then
	WCM_ENV_XLIB=yes
	AC_MSG_RESULT(found)
elif test -f $WCM_XLIBDIR_DEFAULT2/libX11.so; then
	WCM_ENV_XLIB=yes
	WCM_XLIBDIR=$WCM_XLIBDIR_DEFAULT2
	AC_MSG_RESULT(found)
elif test -d $WCM_XLIBDIR_DEFAULT/X11 ||
		test -d $WCM_XLIBDIR_DEFAULT; then
	WCM_ENV_XLIB=yes
	WCM_XLIBDIR=$WCM_XLIBDIR_DEFAULT
	AC_MSG_RESULT(found)
else
	AC_MSG_RESULT([not found, tried $WCM_XLIBDIR_DEFAULT/X11 and $WCM_XLIBDIR_DEFAULT2])
	WCM_ENV_XLIB=no
fi
])
AC_DEFUN([AC_WCM_CHECK_TCL],[
dnl Check for TCL development environment
WCM_TCLDIR=
AC_ARG_WITH(tcl, 
AS_HELP_STRING([--with-tcl=dir], [uses a specified tcl directory  ]),
[ WCM_TCLDIR=$withval ])

dnl get tcl version
AC_PATH_PROG([TCLSH],[tclsh],[no])
if test "x$TCLSH" != "xno"; then
	AC_MSG_CHECKING([for tcl version])
	version=$(echo ["puts [set tcl_version]"] | $TCLSH)
	AC_MSG_RESULT([$version])
fi

dnl handle default case
if test "$WCM_TCLDIR" = "yes" || test "$WCM_TCLDIR" == ""; then
	AC_MSG_CHECKING([for tcl header files])
	dir="$WCM_TCLTKDIR_DEFAULT/include";
	for i in "" tcl/ "tcl$version/"; do
		if test "x$WCM_ENV_TCL" != "xyes"; then
			if test -f "$dir/$i/tcl.h"; then
				AC_MSG_RESULT([$dir/$i])
				WCM_ENV_TCL=yes
				WCM_TCLDIR="$dir/$i"
				CFLAGS="$CFLAGS -I$WCM_TCLDIR"
			fi
		fi
	done
	if test "x$WCM_ENV_TCL" != "xyes"; then
		AC_MSG_RESULT([not found; tried $WCM_TCLTKDIR_DEFAULT/include, tcl, and "tcl$version"; ])		
		echo "***"; echo "*** WARNING:"
		echo "*** The tcl development environment does not appear to"
		echo "*** be installed. The header file tcl.h does not appear"
		echo "*** in the include path. Do you have the tcl rpm or"
		echo "*** equivalent package properly installed?  Some build"
		echo "*** features will be unavailable."
		echo "***"
	fi

dnl handle specified case
elif test "$WCM_TCLDIR" != "no"; then
	AC_MSG_CHECKING([for tcl header files])
	if test -f "$WCM_TCLDIR/include/tcl.h"; then
		AC_MSG_RESULT(found)
		WCM_ENV_TCL=yes
		if test "$WCM_TCLDIR" != "/usr"; then
			CFLAGS="$CFLAGS -I$WCM_TCLDIR/include"
		fi
	elif test -f "$WCM_TCLDIR/tcl.h"; then
		AC_MSG_RESULT(found)
		WCM_ENV_TCL=yes
		if test "$WCM_TCLDIR" != "/usr"; then
			CFLAGS="$CFLAGS -I$WCM_TCLDIR"
		fi
	else
		AC_MSG_RESULT([not found; tried $WCM_TCLDIR/include/tcl.h and $WCM_TCLDIR/tcl.h])
		echo "***"; echo "*** WARNING:"
		echo "*** The tcl development environment does not appear to"
		echo "*** be installed. The header file tcl.h does not appear"
		echo "*** in the include path. Do you have the tcl rpm or"
		echo "*** equivalent package properly installed?  Some build"
		echo "*** features will be unavailable."
		echo "***"
	fi
fi
])
AC_DEFUN([AC_WCM_CHECK_TK],[
dnl Check for TK development environment
WCM_TKDIR=
AC_ARG_WITH(tk,
AS_HELP_STRING([--with-tk=dir], [uses a specified tk directory  ]), 
[ WCM_TKDIR=$withval ])

dnl handle default case
if test "$WCM_TKDIR" = "yes" || test "$WCM_TKDIR" == ""; then
	AC_MSG_CHECKING([for tk header files])
	if test -f "$WCM_TCLTKDIR_DEFAULT/include/tk.h"; then
		AC_MSG_RESULT(found)
		WCM_ENV_TK=yes
		WCM_TKDIR="$WCM_TCLTKDIR_DEFAULT/include"
	elif test -f "$WCM_TCLDIR/include/tk.h" || test -f "$WCM_TCLDIR/tk.h"; then
		AC_MSG_RESULT(found)
		WCM_ENV_TK=yes
		WCM_TKDIR="$WCM_TCLDIR"
	else
		AC_MSG_RESULT([not found; tried $WCM_TCLTKDIR_DEFAULT/include and $WCM_TCLDIR/include])
		echo "***"; echo "*** WARNING:"
		echo "*** The tk development environment does not appear to"
		echo "*** be installed. The header file tk.h does not appear"
		echo "*** in the include path. Do you have the tk rpm or"
		echo "*** equivalent package properly installed?  Some build"
		echo "*** features will be unavailable."
		echo "***"
	fi
dnl handle specified case
elif test "$WCM_TKDIR" != "no"; then
	AC_MSG_CHECKING([for tk header files])
	if test -f "$WCM_TKDIR/include/tk.h"; then
		AC_MSG_RESULT(found)
		WCM_ENV_TK=yes
		if test "$WCM_TCLDIR" != "$WCM_TKDIR" && test "$WCM_TKDIR" != "/usr"; then
			CFLAGS="$CFLAGS -I$WCM_TKDIR/include"
		fi
	elif test -f "$WCM_TKDIR/tk.h"; then
		AC_MSG_RESULT(found)
		WCM_ENV_TK=yes
		if test "$WCM_TCLDIR" != "$WCM_TKDIR" && test "$WCM_TKDIR" != "/usr"; then
			CFLAGS="$CFLAGS -I$WCM_TKDIR"
		fi
	else
		AC_MSG_RESULT([not found; tried $WCM_TKDIR/include/tk.h and $WCM_TKDIR/tk.h])
		echo "***"; echo "*** WARNING:"
		echo "*** The tk library does not appear to be installed."
		echo "*** Do you have the tk rpm or equivalent package properly"
		echo "*** installed?  Some build features will be unavailable."
		echo "***"
	fi
fi
])
AC_DEFUN([AC_WCM_CHECK_NCURSES],[
dnl Check for ncurses development environment
AC_CHECK_HEADER(ncurses.h, [WCM_ENV_NCURSES=yes])
if test x$WCM_ENV_NCURSES != xyes; then
	AC_CHECK_HEADER(ncurses/ncurses.h, [WCM_ENV_NCURSES=yes])
fi
if test x$WCM_ENV_NCURSES != xyes; then
	echo "***"; echo "*** WARNING:"
	echo "*** The ncurses development library does not appear to be installed."
	echo "*** The header file ncurses.h does not appear in the include path."
	echo "*** Do you have the ncurses-devel rpm or equivalent package"
	echo "*** properly installed?  Some build features will be unavailable."
	echo "***"
	AC_DEFINE(WCM_ENABLE_NCURSES,0,[ncurses header files available])
else
	AC_DEFINE(WCM_ENABLE_NCURSES,1,[ncurses header files available])
fi
AM_CONDITIONAL(WCM_ENV_NCURSES, [test x$WCM_ENV_NCURSES = xyes])
])
