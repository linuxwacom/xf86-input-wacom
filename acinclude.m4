dnl Macros for configuring the Linux Wacom package
dnl
AC_DEFUN(AC_WAC_CHECK_ENVIRON,[
dnl Variables for various checks
WAC_ARCH=unknown
WAC_KERNEL=unknown
WAC_ISLINUX=no
WAC_OPTION_MODVER=no
WAC_ENV_KERNEL=no
WAC_KERNEL_WACOM_DEFAULT=no
WAC_ENV_XF86=no
WAC_LINUX_INPUT=
WAC_PATCH_WACDUMP=
WAC_PATCH_WACOMDRV=
dnl Check architecture
AC_MSG_CHECKING(for processor type)
WAC_ARCH=`uname -m`
AC_MSG_RESULT($WAC_ARCH)
dnl
dnl Check kernel type
AC_MSG_CHECKING(for kernel type)
WAC_KERNEL=`uname -s`
AC_MSG_RESULT($WAC_KERNEL)
dnl
dnl Check for linux
AC_MSG_CHECKING(for linux-based kernel)
islinux=`echo $WAC_KERNEL | grep -i linux | wc -l`
if test $islinux != 0; then
	WAC_ISLINUX=yes
fi
AC_MSG_RESULT($WAC_ISLINUX)
if test x$WAC_ISLINUX != xyes; then
	echo "***"
	echo "*** WARNING:"
	echo "*** Linux kernel not detected; linux-specific features will not"
	echo "*** be built including USB support in XFree86 and kernel drivers."
	echo "***"
fi
dnl
dnl Check for linux kernel override
AC_ARG_WITH(linux,
[  --with-linux     Override linux kernel check],
[ WAC_ISLINUX=$withval ])
dnl
dnl Handle linux specific features
if test x$WAC_ISLINUX = xyes; then
	WAC_KERNEL_WACOM_DEFAULT=yes
	WAC_LINUX_INPUT="-DLINUX_INPUT"
	AC_DEFINE(WAC_ENABLE_LINUXINPUT,1,[Enable the Linux Input subsystem])
else
	WAC_PATCH_WACDUMP="(no USB) $WAC_PATCH_WACDUMP"
	WAC_PATCH_WACOMDRV="(no USB) $WAC_PATCH_WACOMDRV"
fi
])
dnl
dnl
dnl
AC_DEFUN(AC_WAC_CHECK_MODVER,[
dnl Guess modversioning
AC_MSG_CHECKING(for kernel module versioning)
kernelrel=`uname -r`
moddir="/lib/modules/$kernelrel/kernel/drivers/usb"

if test -f "$moddir/hid.o.gz"; then
	zcat $moddir/hid.o.gz >config.hid.o
	printk=`nm config.hid.o | grep printk`
	rm config.hid.o
elif test -f "$moddir/hid.o"; then
	printk=`nm $moddir/hid.o | grep printk`
else
	echo "***"; echo "*** WARNING:"
	echo "*** unable to find hid.o or hid.o.gz in kernel module"
	echo "*** directory.  Unable to determine kernel module versioning."
	echo "***"
	printk=""
fi

if test -n "$printk"; then
	printk=`echo "$printk" | grep printk_R`
	if test -n "$printk"; then
		WAC_OPTION_MODVER=yes
		AC_MSG_RESULT(yes)
	else
		AC_MSG_RESULT(no)
	fi
else
	AC_MSG_RESULT([unknown; assuming no])
	WAC_OPTION_MODVER="unknown (assuming no)"
fi
])
AC_DEFUN(AC_WAC_CHECK_KERNELSOURCE,[
dnl Check for kernel build environment
AC_ARG_WITH(kernel,
[  --with-kernel=dir   Specify kernel source directory],
[
	WAC_KERNELDIR="$withval"
	AC_MSG_CHECKING(for valid kernel source tree)
	if test -f "$WAC_KERNELDIR/include/linux/input.h"; then
		AC_MSG_RESULT(ok)
		WAC_ENV_KERNEL=yes
	else
		AC_MSG_RESULT(missing input.h)
		AC_MSG_ERROR("Unable to find $WAC_KERNELDIR/include/linux/input.h")
	fi
],
[
	dnl guess directory
	AC_MSG_CHECKING(for kernel sources)
	WAC_KERNELDIR="/usr/src/linux-2.4"
	if test -f "$WAC_KERNELDIR/include/linux/input.h"; then
		WAC_ENV_KERNEL=yes
		AC_MSG_RESULT($WAC_KERNELDIR)
	else
		WAC_KERNELDIR="/usr/src/linux"
		if test -f "$WAC_KERNELDIR/include/linux/input.h"; then
			WAC_ENV_KERNEL=yes
			AC_MSG_RESULT($WAC_KERNELDIR)
		else
			AC_MSG_RESULT(not found)
			echo "***"
			echo "*** WARNING:"
			echo "*** Unable to guess kernel source directory"
			echo "*** Looked at /usr/src/linux-2.4"
			echo "*** Looked at /usr/src/linux"
			echo "*** Kernel modules will not be built"
			echo "***"
		fi
	fi
])])
AC_DEFUN(AC_WAC_CHECK_XFREE86SOURCE,[
dnl Check for XFree86 build environment
AC_ARG_WITH(xf86,
[  --with-xf86=dir   Specify XF86 build directory],
[
	WAC_XF86DIR="$withval";
	AC_MSG_CHECKING(for valid XFree86 build environment)
	if test -f $WAC_XF86DIR/$XF86SUBDIR/include/xf86Version.h; then
		WAC_ENV_XF86=yes
		AC_MSG_RESULT(ok)
	else
		AC_MSG_RESULT("xf86Version.h missing")
		AC_MSG_ERROR("Unable to find $WAC_XF86DIR/$XF86SUBDIR/include/xf86Version.h")
	fi
	WAC_XF86DIR=`(cd $WAC_XF86DIR; pwd)`
])])
