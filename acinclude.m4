dnl Macros for configuring the Linux Wacom package
dnl
AC_DEFUN([AC_WCM_CHECK_ENVIRON],[
dnl Variables for various checks
WCM_ENV_TCL=no
WCM_ENV_TK=no
WCM_XIDUMP_DEFAULT=yes
WCM_TCLTKDIR_DEFAULT=/usr
WCM_ENV_NCURSES=no
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
