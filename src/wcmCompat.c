/*
 * Copyright 1995-2003 by Frederic Lepied, France. <Lepied@XFree86.org>
 *                                                                            
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is  hereby granted without fee, provided that
 * the  above copyright   notice appear  in   all  copies and  that both  that
 * copyright  notice   and   this  permission   notice  appear  in  supporting
 * documentation, and that   the  name of  Frederic   Lepied not  be  used  in
 * advertising or publicity pertaining to distribution of the software without
 * specific,  written      prior  permission.     Frederic  Lepied   makes  no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.                   
 *                                                                            
 * FREDERIC  LEPIED DISCLAIMS ALL   WARRANTIES WITH REGARD  TO  THIS SOFTWARE,
 * INCLUDING ALL IMPLIED   WARRANTIES OF MERCHANTABILITY  AND   FITNESS, IN NO
 * EVENT  SHALL FREDERIC  LEPIED BE   LIABLE   FOR ANY  SPECIAL, INDIRECT   OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA  OR PROFITS, WHETHER  IN  AN ACTION OF  CONTRACT,  NEGLIGENCE OR OTHER
 * TORTIOUS  ACTION, ARISING    OUT OF OR   IN  CONNECTION  WITH THE USE    OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "xf86Wacom.h"

/*****************************************************************************
 * XFree86 V4 Compatibility Functions
 ****************************************************************************/

#if XFREE86_V4

int xf86WcmWait(int t)
{
	int err = xf86WaitForInput(-1, ((t) * 1000));
	if (err != -1)
		return Success;

	ErrorF("Wacom select error : %s\n", strerror(errno));
	return err;
}

int xf86WcmReady(int fd)
{
	int n = xf86WaitForInput(fd, 0);
	if (n >= 0) return n ? 1 : 0;
	ErrorF("Wacom select error : %s\n", strerror(errno));
	return 0;
}

/*****************************************************************************
 * XFree86 V3 Compatibility Functions
 ****************************************************************************/

#elif XFREE86_V3

int xf86WcmWait(int t)
{
	int err;
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = (t) * 1000;
	SYSCALL(err = select(0, NULL, NULL, NULL, &timeout));

	if (err == -1)
		ErrorF("Wacom select error : %s\n", strerror(errno));

	return err;
}

int xf86WcmReady(int fd)
{
	fd_set readfds;
	struct timeval timeout;
	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	SYSCALL(n = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout));
	if (n >= 0) return n ? 1 : 0;
	ErrorF("Wacom select error : %s\n", strerror(errno));
	return 0;
}

int xf86WcmOpenTablet(LocalDevicePtr local)
{
	int fd;
	WacomDevicePtr	priv = (WacomDevicePtr)local->private;
	WacomCommonPtr	common = priv->common;
	SYSCALL(fd = open(common->wcmDevice, O_RDWR|O_NDELAY, 0));
	return fd;
}

int xf86WcmSetSerialSpeed(int fd, int rate)
{
	int err, baud;
	struct termios	termios_tty;

	/* select baud from rate */
	if (rate == 38400) baud = B38400;
	else if (rate == 19200) baud = B19200;
	else baud = B9600;
    
#ifdef POSIX_TTY
	SYSCALL(err = tcgetattr(fd, &termios_tty));
	if (err == -1)
	{
		ErrorF("Wacom tcgetattr error : %s\n", strerror(errno));
		return -1;
	}

	termios_tty.c_iflag = IXOFF;
	termios_tty.c_oflag = 0;
	termios_tty.c_cflag = baud|CS8|CREAD|CLOCAL;
	termios_tty.c_lflag = 0;

	termios_tty.c_cc[VINTR] = 0;
	termios_tty.c_cc[VQUIT] = 0;
	termios_tty.c_cc[VERASE] = 0;
	termios_tty.c_cc[VEOF] = 0;
#ifdef VWERASE
	termios_tty.c_cc[VWERASE] = 0;
#endif
#ifdef VREPRINT
	termios_tty.c_cc[VREPRINT] = 0;
#endif
	termios_tty.c_cc[VKILL] = 0;
	termios_tty.c_cc[VEOF] = 0;
	termios_tty.c_cc[VEOL] = 0;
#ifdef VEOL2
	termios_tty.c_cc[VEOL2] = 0;
#endif
	termios_tty.c_cc[VSUSP] = 0;
#ifdef VDSUSP
	termios_tty.c_cc[VDSUSP] = 0;
#endif
#ifdef VDISCARD
	termios_tty.c_cc[VDISCARD] = 0;
#endif
#ifdef VLNEXT
	termios_tty.c_cc[VLNEXT] = 0; 
#endif
	
	/* minimum 1 character in one read call and timeout to 100 ms */
	termios_tty.c_cc[VMIN] = 1;
	termios_tty.c_cc[VTIME] = 10;

	SYSCALL(err = tcsetattr(fd, TCSANOW, &termios_tty));
	if (err == -1)
	{
		ErrorF("Wacom tcsetattr TCSANOW error : %s\n", strerror(errno));
		return -1;
	}

#else
#error Code for OSs without POSIX tty functions
#endif

	return 0;
}

int xf86WcmFlushTablet(int fd)
{
	int err;
	fd_set readfds;
	struct timeval timeout;
	char dummy[1];
    
	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);

	do
	{
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		SYSCALL(err = select(FD_SETSIZE, &readfds,
			NULL, NULL, &timeout));

		if (err > 0)
		{
			SYSCALL(err = read(fd, &dummy, 1));
			DBG(10, ErrorF("xf86WcmFlushTablet: read %d bytes\n",
				err));
		}
	} while (err > 0);

	return err;
}

int xf86WcmWaitForTablet(int fd)
{
	int err;
	fd_set readfds;
	struct timeval timeout;
    
	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);
    
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	SYSCALL(err = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout));
	return err;
}

#endif /* XFREE86_V3 */
