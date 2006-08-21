/* $XConsortium: xf86Wacom.c /main/20 1996/10/27 11:05:20 kaleb $ */
/*
 * Copyright 1995-2002 by Frederic Lepied, France. <Lepied@XFree86.org> 
 * Copyright 2002-2006 by Ping Cheng, Wacom Technology. <pingc@wacom.com>
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

/* $XFree86: xc/programs/Xserver/hw/xfree86/input/wacom/xf86Wacom.c,v 1.26 2001/04/01 14:00:13 tsi Exp $ */

/*
 * This driver is currently able to handle Wacom IV, V, and ISDV4 protocols.
 *
 * Wacom V protocol work done by Raph Levien <raph@gtk.org> and
 * Frédéric Lepied <lepied@xfree86.org>.
 *
 * Many thanks to Dave Fleck from Wacom for the help provided to
 * build this driver.
 *
 * Modified for Linux USB by MATSUMURA Namihiko,
 * Daniel Egger, Germany. <egger@suse.de>,
 * Frederic Lepied <lepied@xfree86.org>,
 * Brion Vibber <brion@pobox.com>,
 * Aaron Optimizer Digulla <digulla@hepe.com>,
 * Jonathan Layes <jonathan@layes.com>.
 *
 */

/*
 * REVISION HISTORY
 *
 * 2002-12-17 26-j0.3.3 - added Intuos2
 * 2002-12-31 26-j0.3.5 - added module loading for usb wacom and evdev
 * 2003-01-01 26-j0.3.6 - fix for 2D Intuos2 mouse buttons
 * 2003-01-25 26-j0.3.7 - cleaned up usb conditions for FreeBSD
 * 2003-01-31 26-j0.5.0 - new release
 * 2003-01-31 26-j0.5.1 - added Ping Cheng's PL code
 * 2003-01-31 26-j0.5.2 - fixed serial number code for Intuos and Intuos2
 * 2003-02-12 26-j0.5.3 - added Ping Cheng's USB patch
 * 2003-02-12 26-j0.5.4 - added Ping Cheng's "device_on" patch
 * 2003-02-22 26-j0.5.5 - added Ping Cheng's "multi" patch
 * 2003-02-22 26-j0.5.6 - applied J. Yen's origin patch
 * 2003-03-06 26-j0.5.7 - added Ping Cheng's "suppress" patch
 * 2003-03-22 26-j0.5.8 - added Dean Townsley's ISDV4 patch
 * 2003-04-02 26-j0.5.9 - added J. Yen's "misc fixes" patch
 * 2003-04-06 26-j0.5.10 - refactoring
 * 2003-04-29 26-j0.5.11 - all devices using same data path
 * 2003-05-01 26-j0.5.12 - changed graphire wheel to report relative
 * 2003-05-02 26-j0.5.13 - added parameter configuration code
 * 2003-05-15 26-j0.5.14 - added relative wheel button 4 and 5
 * 2003-05-15 26-j0.5.15 - intuos filter code on by default, fixed APM init
 * 2003-06-19 26-j0.5.16 - added Intuos2 6x8 id 0x47, suppress of 0 disables
 * 2003-06-25 26-j0.5.17 - support TwinView and kernel 2.5 for USB tablet 
 * 2003-07-10 26-j0.5.18 - fix to Intuos filter, ignores first samples
 * 2003-07-16 26-j0.5.19 - added noise reducing filter, improved USB relative mode
 * 2003-07-24 26-j0.5.20 - added new xsetwacom commands (Mode, SpeedLevel, and ClickForce)
 * 2003-08-13 26-j0.5.21 - added speed acceleration xsetwacom commands (Accel)
 * 2003-09-30 26-j0.5.22 - added TwinView with different resolution support and
			 - enabled ScreenNo option for TwinView
 * 2003-11-10 26-j0.5.23 - support kernel 2.4.22 and user specified tcl/tk src dir
 * 2003-11-18 26-j0.5.24 - support general Tablet PC (ISDV4) and xsetwacom mmonitor
 * 2003-12-10 26-j0.5.25 - support kernel 2.6
 * 2003-01-10 26-j0.5.26 - added double click speed and radius
 * 2004-02-02 26-j0.6.0  - new release
 * 2004-03-02 26-j0.6.1  - new release
 * 2004-04-04 26-j0.6.2  - new release
 * 2004-05-25 26-j0.6.3  - new release
 * 2004-10-05 26-j0.6.5  - new release
 * 2004-11-22 42-j0.6.6  - new release
 * 2005-02-17 42-j0.6.7  - added 64-bit support
 * 2005-03-10 42-j0.6.8  - added Cintiq 21UX support
 * 2005-05-16 47-pc0.6.9 - added tablet orentation rotation for all tablets
 * 2005-10-17 47-pc0.7.1 - Added DTU710, DTF720, G4
 * 2005-11-17 47-pc0.7.1-1 - Report tool serial number and ID to Xinput
 * 2005-12-02 47-pc0.7.1-2 - Grap the USB port so /dev/input/mice won't get it
 * 2005-12-21 47-pc0.7.2 - new release
 * 2006-03-21 47-pc0.7.3 - new release
 * 2006-03-31 47-pc0.7.3-1 - new release
 * 2006-05-03 47-pc0.7.4 - new release
 * 2006-07-17 47-pc0.7.5 - Support button/key combined events
 */

static const char identification[] = "$Identification: 47-0.7.5 $";

/****************************************************************************/

#include "xf86Wacom.h"
#include "wcmFilter.h"

static int xf86WcmDevOpen(DeviceIntPtr pWcm);
static void xf86WcmDevReadInput(LocalDevicePtr local);
static void xf86WcmDevControlProc(DeviceIntPtr device, PtrCtrl* ctrl);
static void xf86WcmDevClose(LocalDevicePtr local);
static int xf86WcmDevProc(DeviceIntPtr pWcm, int what);
static int xf86WcmSetParam(LocalDevicePtr local, int param, int value);
static int xf86WcmDevChangeControl(LocalDevicePtr local, xDeviceCtl* control);
static int xf86WcmDevSwitchMode(ClientPtr client, DeviceIntPtr dev, int mode);
static Bool xf86WcmDevConvert(LocalDevicePtr local, int first, int num,
		int v0, int v1, int v2, int v3, int v4, int v5, int* x, int* y);
static int xf86WcmModelToFile(LocalDevicePtr local);
static Bool xf86WcmDevReverseConvert(LocalDevicePtr local, int x, int y,
		int* valuators);
extern Bool usbWcmInit(LocalDevicePtr pDev);
extern int usbWcmGetRanges(LocalDevicePtr local);

WacomModule gWacomModule =
{
	0,          /* debug level */
	identification, /* version */
	{ NULL, },   /* input driver pointer */

	/* device procedures */
	xf86WcmDevOpen,
	xf86WcmDevReadInput,
	xf86WcmDevControlProc,
	xf86WcmDevClose,
	xf86WcmDevProc,
	xf86WcmDevChangeControl,
	xf86WcmDevSwitchMode,
	xf86WcmDevConvert,
	xf86WcmDevReverseConvert,
};

/*****************************************************************************
 * xf86WcmRegisterX11Devices --
 *    Register the X11 input devices with X11 core.
 ****************************************************************************/

static int xf86WcmRegisterX11Devices (LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	int totalWidth = 0, maxHeight = 0, tabletSize = 0;
	double screenRatio, tabletRatio;
	CARD8 butmap[MAX_BUTTONS];
	int nbaxes, nbbuttons, nbkeys;
	int loop;

	/* Detect tablet configuration, if possible */
	if (priv->common->wcmModel->DetectConfig)
		priv->common->wcmModel->DetectConfig (local);

	nbaxes = priv->naxes;       /* X, Y, Pressure, Tilt-X, Tilt-Y, Wheel */
	nbbuttons = priv->nbuttons; /* Use actual number of buttons, if possible */
	nbkeys = nbbuttons;         /* Same number of keys since any button may be */
	                            /* configured as an either mouse button or key */

	DBG(10,ErrorF("xf86WcmRegisterX11Devices (%s) %d buttons, %d keys, %d axes\n",
			IsStylus(priv) ? "stylus" :
			IsCursor(priv) ? "cursor" :
			IsPad(priv) ? "pad" : "eraser",
			nbbuttons, nbkeys, nbaxes));

	for(loop=1; loop<=nbbuttons; loop++)
		butmap[loop] = loop;

	if (InitButtonClassDeviceStruct(local->dev, nbbuttons, butmap) == FALSE)
	{
		ErrorF("unable to allocate Button class device\n");
		return FALSE;
	}

	if (InitProximityClassDeviceStruct(local->dev) == FALSE)
	{
			ErrorF("unable to init proximity class device\n");
			return FALSE;
	}

	if (nbaxes || nbaxes > 6)
		nbaxes = priv->naxes = 6;

	if (InitValuatorClassDeviceStruct(local->dev, nbaxes,
					  xf86GetMotionEvents,
					  local->history_size,
					  (priv->flags & ABSOLUTE_FLAG) ?
					  Absolute : Relative) == FALSE)
	{
		ErrorF("unable to allocate Valuator class device\n");
		return FALSE;
	}

	if (nbkeys)
	{
		KeySymsRec wacom_keysyms;
		KeySym keymap[MAX_BUTTONS];

		for (loop = 0; loop < nbkeys; loop++)
			if ((priv->button [loop] & AC_TYPE) == AC_KEY)
				keymap [loop] = priv->button [loop] & AC_CODE;
			else
				keymap [loop] = NoSymbol;

		/* There seems to be a long-standing misunderstanding about
		 * how a keymap should be defined. All tablet drivers from
		 * stock X11 source tree are doing it wrong: they leave first
		 * 8 keysyms as VoidSymbol's, and are passing 8 as minimum
		 * key code. But if you look at SetKeySymsMap() from
		 * programs/Xserver/dix/devices.c you will see that
		 * Xserver does not require first 8 keysyms; it supposes
		 * that the map begins at minKeyCode.
		 *
		 * It could be that this assumption is a leftover from
		 * earlier XFree86 versions, but that's out of our scope.
		 * This also means that no keys on extended input devices
		 * with their own keycodes (e.g. tablets) were EVER used.
		 */
		wacom_keysyms.map = keymap;
		/* minKeyCode = 8 because this is the min legal key code */
		wacom_keysyms.minKeyCode = 8;
		wacom_keysyms.maxKeyCode = 8 + nbkeys - 1;
		wacom_keysyms.mapWidth = 1;
		if (InitKeyClassDeviceStruct(local->dev, &wacom_keysyms, NULL) == FALSE)
		{
			ErrorF("unable to init key class device\n");
			return FALSE;
		}
	}

	/* allocate motion history buffer if needed */
	xf86MotionHistoryAllocate(local);

	local->flags |= X11DEVREG_FLAG;

	/* initialize bounding rect */
	if (priv->twinview != TV_NONE && priv->screen_no == -1) 
	{
		priv->tvoffsetX = 60;
		priv->tvoffsetY = 60;
	}

	priv->bottomX = xf86SetIntOption(local->options, "BottomX", 0);
	priv->bottomY = xf86SetIntOption(local->options, "BottomY", 0);
	if (priv->bottomX == 0) priv->bottomX = common->wcmMaxX;
	if (priv->bottomY == 0) priv->bottomY = common->wcmMaxY;

	/* Verify Box validity */
	if (priv->topX > common->wcmMaxX)
	{
		ErrorF("Wacom invalid TopX (%d) reseting to 0\n",
				priv->topX);
		priv->topX = 0;
	}

	if (priv->topY > common->wcmMaxY)
	{
		ErrorF("Wacom invalid TopY (%d) reseting to 0\n",
				priv->topY);
		priv->topY = 0;
	}

	if (priv->bottomX < priv->topX)
	{
		ErrorF("Wacom invalid BottomX (%d) reseting to %d\n",
				priv->bottomX, common->wcmMaxX);
		priv->bottomX = common->wcmMaxX;
	}

	if (priv->bottomY < priv->topY)
	{
		ErrorF("Wacom invalid BottomY (%d) reseting to %d\n",
				priv->bottomY, common->wcmMaxY);
		priv->bottomY = common->wcmMaxY;
	}

	if (priv->screen_no != -1 &&
		(priv->screen_no >= priv->numScreen || priv->screen_no < 0))
	{
		if (priv->twinview == TV_NONE || priv->screen_no != 1)
		{
			ErrorF("%s: invalid screen number %d, resetting to 0\n",
					local->name, priv->screen_no);
			priv->screen_no = 0;
		}
	}

	/* Calculate the ratio according to KeepShape, TopX and TopY */
	if (priv->screen_no != -1)
	{
		priv->currentScreen = priv->screen_no;
		if (priv->twinview == TV_NONE)
		{
			totalWidth = screenInfo.screens[priv->currentScreen]->width;
			maxHeight = screenInfo.screens[priv->currentScreen]->height;
		}
		else
		{
			totalWidth = priv->tvResolution[2*priv->currentScreen];
			maxHeight = priv->tvResolution[2*priv->currentScreen+1];
		}
	}
	else
	{
		int i;
		for (i = 0; i < priv->numScreen; i++)
		{
			totalWidth += screenInfo.screens[i]->width;
			if (maxHeight < screenInfo.screens[i]->height)
				maxHeight=screenInfo.screens[i]->height;
		}
	}

	/* Maintain aspect ratio */
	if (priv->flags & KEEP_SHAPE_FLAG)
	{
		screenRatio = totalWidth / (double)maxHeight;
			tabletRatio = ((double)(common->wcmMaxX - priv->topX)) /
				(common->wcmMaxY - priv->topY);

		DBG(2, ErrorF("screenRatio = %.3g, tabletRatio = %.3g\n"
					, screenRatio, tabletRatio));

		if (screenRatio > tabletRatio)
		{
			priv->bottomX = common->wcmMaxX;
			priv->bottomY = (common->wcmMaxY - priv->topY) *
				tabletRatio / screenRatio + priv->topY;
		}
		else
		{
			priv->bottomX = (common->wcmMaxX - priv->topX) *
				screenRatio / tabletRatio + priv->topX;
			priv->bottomY = common->wcmMaxY;
		}
	} /* end keep shape */

	if (xf86Verbose)
		ErrorF("%s Wacom device \"%s\" top X=%d top Y=%d "
				"bottom X=%d bottom Y=%d\n",
				XCONFIG_PROBED, local->name, priv->topX,
				priv->topY, priv->bottomX, priv->bottomY);

	if (priv->numScreen == 1)
	{
		priv->factorX = totalWidth
			/ (double)(priv->bottomX - priv->topX - 2*priv->tvoffsetX);
		priv->factorY = maxHeight
			/ (double)(priv->bottomY - priv->topY - 2*priv->tvoffsetY);
		DBG(2, ErrorF("X factor = %.3g, Y factor = %.3g\n",
			priv->factorX, priv->factorY));
	}

	if (IsPad(priv))
	{
		/* strip-x and strip-y */
		if (priv->naxes)
		{
			InitValuatorAxisStruct(local->dev, 0, 0, 4097, 1, 1, 1);
			InitValuatorAxisStruct(local->dev, 1, 0, 4097, 1, 1, 1);
		}
		return TRUE;
	}

	/* x and y axes */
	if (priv->twinview == TV_LEFT_RIGHT)
		tabletSize = 2*(priv->bottomX - priv->topX - 2*priv->tvoffsetX);
	else
		tabletSize = priv->bottomX - priv->topX;

	InitValuatorAxisStruct(local->dev, 0, priv->topX, tabletSize, /* max val */
		common->wcmResolX, /* tablet resolution */
		0, common->wcmResolX); /* max_res */

	if (priv->twinview == TV_ABOVE_BELOW)
		tabletSize = 2*(priv->bottomY - priv->topY - 2*priv->tvoffsetY);
	else
		tabletSize = priv->bottomY - priv->topY;

	InitValuatorAxisStruct(local->dev, 1, priv->topY, tabletSize, /* max val */
		common->wcmResolY, /* tablet resolution */
		0, common->wcmResolY); /* max_res */

	/* pressure */
	InitValuatorAxisStruct(local->dev, 2, 0,
		common->wcmMaxZ, /* max val */
		1, 1, 1);

	if (IsCursor(priv))
	{
		/* z-rot and throttle */
		InitValuatorAxisStruct(local->dev, 3, -900, 899, 1, 1, 1);
		InitValuatorAxisStruct(local->dev, 4, -1023, 1023, 1, 1, 1);
	}
	else
	{
		/* tilt-x and tilt-y */
		InitValuatorAxisStruct(local->dev, 3, -64, 63, 1, 1, 1);
		InitValuatorAxisStruct(local->dev, 4, -64, 63, 1, 1, 1);
	}

	if (strstr(common->wcmModel->name, "Intuos3") && IsStylus(priv))
		/* Intuos3 Marker Pen rotation */
		InitValuatorAxisStruct(local->dev, 5, -900, 899, 1, 1, 1);
	else
	{
		/* absolute wheel */
		InitValuatorAxisStruct(local->dev, 5, 0, 1023, 1, 1, 1);
	}

	return TRUE;
}

/*****************************************************************************
 * xf86WcmDevOpen --
 *    Open the physical device and init information structs.
 ****************************************************************************/

static int xf86WcmDevOpen(DeviceIntPtr pWcm)
{
	LocalDevicePtr local = (LocalDevicePtr)pWcm->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr)PRIVATE(pWcm);
	WacomCommonPtr common = priv->common;
 
	DBG(10,ErrorF("xf86WcmDevOpen\n"));

	/* Drop read error counter */
	common->wcmReadErrorCount = 0;

	/* open file, if not already open */
	if (common->fd_refs == 0)
	{
		if ((xf86WcmOpen (local) != Success) || (local->fd < 0))
		{
			DBG(1,ErrorF("Failed to open device (fd=%d)\n", local->fd));
			if (local->fd >= 0)
			{
				DBG(1,ErrorF("Closing device\n"));
				xf86WcmClose(local->fd);
			}
			local->fd = -1;
			return FALSE;
		}
		common->fd = local->fd;
		common->fd_refs = 1;
	}

	/* Grab the common descriptor, if it's available */
	if (local->fd < 0)
	{
		local->fd = common->fd;
		common->fd_refs++;
	}

	/* The rest of initialization is done only once */
	if (local->flags & X11DEVREG_FLAG)
		return TRUE;

	if (!xf86WcmRegisterX11Devices (local))
		return FALSE;

	return TRUE;
}

/*****************************************************************************
 * xf86WcmDevReadInput --
 *   Read the device on IO signal
 ****************************************************************************/

static void xf86WcmDevReadInput(LocalDevicePtr local)
{
	int loop=0, i;
	#define MAX_READ_LOOPS 10

	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	/* If there were read errors for USB tablet, it means the device was 
	 * disconnected or changed. The only way to make the tablet alive again 
	 * is to reconnect or reregister it. 
	 */
	if (common->wcmReadErrorCount > 0)
	{
		if (common->wcmDevCls == &gWacomUSBDevice)
		{
			usbWcmInit(local);
			usbWcmGetRanges(local);
			xf86WcmRegisterX11Devices(local);
			/* reinitialize all the other defined tools on the same port */
			for (i=0; i<common->wcmNumDevices; i++)
			{
				if (((WacomDevicePtr)(common->wcmDevices[i]->private))->common == common  
						&& common->wcmDevices[i] != local)
					xf86WcmRegisterX11Devices(common->wcmDevices[i]);
			}
			common->wcmReadErrorCount = 0;
			return;
		}
		common->wcmReadErrorCount = 0;
	}

	/* move data until we exhaust the device */
	for (loop=0; loop < MAX_READ_LOOPS; ++loop)
	{
		/* dispatch */
		common->wcmDevCls->Read(local);

		/* verify that there is still data in pipe */
		if (!xf86WcmReady(local->fd)) break;
	}

	/* report how well we're doing */
	if (loop >= MAX_READ_LOOPS)
		DBG(1,ErrorF("xf86WcmDevReadInput: Can't keep up!!!\n"));
	else if (loop > 0)
		DBG(10,ErrorF("xf86WcmDevReadInput: Read (%d)\n",loop));
}					

void xf86WcmReadPacket(LocalDevicePtr local)
{
	WacomCommonPtr common = ((WacomDevicePtr)(local->private))->common;
	int len, pos, cnt, remaining;
	short sID[4];

	if (!common->wcmModel) return;

	remaining = sizeof(common->buffer) - common->bufpos;

	if (common->wcmDevCls == &gWacomUSBDevice && !common->wcmReadErrorCount)
	{
		ioctl(local->fd, EVIOCGID, sID);

		if (common->tablet_id != sID[2])
		{
			common->wcmReadErrorCount++;
			DBG(10, ErrorF("Wacom device ID changed from %d to %d\n", common->tablet_id, sID[2]));
		}
	}

	DBG(10, ErrorF("xf86WcmDevReadPacket: device=%s fd=%d "
		"pos=%d remaining=%d\n",
		common->wcmDevice, local->fd,
		common->bufpos, remaining));

	/* fill buffer with as much data as we can handle */
	len = xf86WcmRead(local->fd,
		common->buffer + common->bufpos, remaining);

	if (len <= 0)
	{
		if (common->wcmDevCls == &gWacomUSBDevice)
		{
			common->wcmReadErrorCount++;
			xf86WcmWait(500);
		}
		ErrorF("Error reading wacom device : %s\n", strerror(errno));
		return;
	}

	/* account for new data */
	common->bufpos += len;
	DBG(10, ErrorF("xf86WcmReadPacket buffer has %d bytes\n",
		common->bufpos));

	pos = 0;

	/* while there are whole packets present, parse data */
	while ((common->bufpos - pos) >=  common->wcmPktLength)
	{
		/* parse packet */
		cnt = common->wcmModel->Parse(common, common->buffer + pos);
		if (cnt <= 0)
		{
			DBG(1,ErrorF("Misbehaving parser returned %d\n",cnt));
			break;
		}
		pos += cnt;
	}

	if (pos)
	{
		/* if half a packet remains, move it down */
		if (pos < common->bufpos)
		{
			DBG(7, ErrorF("MOVE %d bytes\n", common->bufpos - pos));
			memmove(common->buffer,common->buffer+pos,
				common->bufpos-pos);
			common->bufpos -= pos;
		}

		/* otherwise, reset the buffer for next time */
		else
		{
			common->bufpos = 0;
		}
	}
}

/*****************************************************************************
 * xf86WcmDevControlProc --
 ****************************************************************************/

static void xf86WcmDevControlProc(DeviceIntPtr device, PtrCtrl* ctrl)
{
	DBG(2, ErrorF("xf86WcmControlProc\n"));
}

/*****************************************************************************
 * xf86WcmDevClose --
 ****************************************************************************/

static void xf86WcmDevClose(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	DBG(4, ErrorF("Wacom number of open devices = %d\n", common->fd_refs));

	if (local->fd >= 0)
	{
		local->fd = -1;
		if (!--common->fd_refs)
		{
			DBG(1,ErrorF("Closing device; uninitializing.\n"));
	    		xf86WcmClose (common->fd);
		}
	}
}
 
/*****************************************************************************
 * xf86WcmDevProc --
 *   Handle the initialization, etc. of a wacom
 ****************************************************************************/

static int xf86WcmDevProc(DeviceIntPtr pWcm, int what)
{
	LocalDevicePtr local = (LocalDevicePtr)pWcm->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr)PRIVATE(pWcm);

	DBG(2, ErrorF("BEGIN xf86WcmProc dev=%p priv=%p "
			"type=%s(%s) flags=%d fd=%d what=%s\n",
			(void *)pWcm, (void *)priv,
			IsStylus(priv) ? "stylus" :
			IsCursor(priv) ? "cursor" :
			IsPad(priv) ? "pad" : "eraser", 
			local->name, priv->flags, local ? local->fd : -1,
			(what == DEVICE_INIT) ? "INIT" :
			(what == DEVICE_OFF) ? "OFF" :
			(what == DEVICE_ON) ? "ON" :
			(what == DEVICE_CLOSE) ? "CLOSE" : "???"));

	switch (what)
	{
		case DEVICE_INIT: 
			/* Try to open the device later (on DEVICE_OFF/ON) */
			break; 

		case DEVICE_ON:
			if (!xf86WcmDevOpen(pWcm))
			{
				DBG(1, ErrorF("xf86WcmProc ON FAILED\n"));
				return !Success;
			}
			xf86AddEnabledDevice(local);
			pWcm->public.on = TRUE;
			break;

		case DEVICE_OFF:
		case DEVICE_CLOSE:
			if (local->fd >= 0)
			{
				xf86RemoveEnabledDevice(local);
				xf86WcmDevClose(local);
			}
			pWcm->public.on = FALSE;
			break;

		default:
			ErrorF("wacom unsupported mode=%d\n", what);
			return !Success;
			break;
	} /* end switch */

	DBG(2, ErrorF("END xf86WcmProc Success \n"));
	return Success;
}

/*****************************************************************************
 * xf86WcmSetParam
 ****************************************************************************/

static int xf86WcmSetParam(LocalDevicePtr local, int param, int value)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	int bn;
	char st[32];

	switch (param) 
	{
	    case XWACOM_PARAM_TOPX:
		xf86ReplaceIntOption(local->options, "TopX", value);
		priv->topX = xf86SetIntOption(local->options, "TopX", 0);
		break;
	    case XWACOM_PARAM_TOPY:
		xf86ReplaceIntOption(local->options, "TopY", value);
		priv->topY = xf86SetIntOption(local->options, "TopY", 0);
		break;
	    case XWACOM_PARAM_BOTTOMX:
		xf86ReplaceIntOption(local->options, "BottomX", value);
		priv->bottomX = xf86SetIntOption(local->options, "BottomX", 0);
		break;
	    case XWACOM_PARAM_BOTTOMY:
		xf86ReplaceIntOption(local->options, "BottomY", value);
		priv->bottomY = xf86SetIntOption(local->options, "BottomY", 0);
		break;
	    case XWACOM_PARAM_BUTTON1:
	    case XWACOM_PARAM_BUTTON2:
	    case XWACOM_PARAM_BUTTON3:
	    case XWACOM_PARAM_BUTTON4:
	    case XWACOM_PARAM_BUTTON5:
	    case XWACOM_PARAM_BUTTON6:
	    case XWACOM_PARAM_BUTTON7:
	    case XWACOM_PARAM_BUTTON8:
	    case XWACOM_PARAM_BUTTON9:
	    case XWACOM_PARAM_BUTTON10:
	    case XWACOM_PARAM_BUTTON11:
	    case XWACOM_PARAM_BUTTON12:
	    case XWACOM_PARAM_BUTTON13:
	    case XWACOM_PARAM_BUTTON14:
	    case XWACOM_PARAM_BUTTON15:
	    case XWACOM_PARAM_BUTTON16:
	    case XWACOM_PARAM_BUTTON17:
	    case XWACOM_PARAM_BUTTON18:
	    case XWACOM_PARAM_BUTTON19:
	    case XWACOM_PARAM_BUTTON20:
	    case XWACOM_PARAM_BUTTON21:
	    case XWACOM_PARAM_BUTTON22:
	    case XWACOM_PARAM_BUTTON23:
	    case XWACOM_PARAM_BUTTON24:
	    case XWACOM_PARAM_BUTTON25:
	    case XWACOM_PARAM_BUTTON26:
	    case XWACOM_PARAM_BUTTON27:
	    case XWACOM_PARAM_BUTTON28:
	    case XWACOM_PARAM_BUTTON29:
	    case XWACOM_PARAM_BUTTON30:
	    case XWACOM_PARAM_BUTTON31:
	    case XWACOM_PARAM_BUTTON32:
		bn = param - XWACOM_PARAM_BUTTON1 + 1;
		if (bn > priv->nbuttons)
			return BadValue;
		snprintf (st, sizeof (st), "Button%d", bn);
		xf86ReplaceIntOption (local->options, st, value);
		priv->button[bn - 1] = xf86SetIntOption (local->options, st, bn);
		break;
	    case XWACOM_PARAM_DEBUGLEVEL:
		if ((value < 1) || (value > 100)) return BadValue;
		xf86ReplaceIntOption(local->options, "DebugLevel", value);
		gWacomModule.debugLevel = value;
		break;
	    case XWACOM_PARAM_RAWFILTER:
		if ((value < 0) || (value > 1)) return BadValue;
		if (value) 
		{
			common->wcmFlags |= RAW_FILTERING_FLAG;
			xf86ReplaceStrOption(local->options, "RawFilter", "On");
		}
		else 
		{
			common->wcmFlags &= ~(RAW_FILTERING_FLAG);
			xf86ReplaceStrOption(local->options, "RawFilter", "Off");
		}
		break;
	    case XWACOM_PARAM_PRESSCURVE:
	    {
		if ( !IsCursor(priv) && !IsPad (priv) ) 
		{
			char chBuf[64];
			int x0 = (value >> 24) & 0xFF;
			int y0 = (value >> 16) & 0xFF;
			int x1 = (value >> 8) & 0xFF;
			int y1 = value & 0xFF;
			if ((x0 > 100) || (y0 > 100) || (x1 > 100) || (y1 > 100))
			    return BadValue;
			snprintf(chBuf,sizeof(chBuf),"%d,%d,%d,%d",x0,y0,x1,y1);
			xf86ReplaceStrOption(local->options, "PressCurve",chBuf);
			xf86WcmSetPressureCurve(priv,x0,y0,x1,y1);
		}
		break;
	    }
	    case XWACOM_PARAM_MODE:
		if ((value < 0) || (value > 1)) return BadValue;
		if (value) 
		{
			priv->flags |= ABSOLUTE_FLAG;
			xf86ReplaceStrOption(local->options, "Mode", "Absolute");
		}
		else 
		{
			priv->flags &= ~ABSOLUTE_FLAG;
			xf86ReplaceStrOption(local->options, "Mode", "Relative");
		}
		break;
	    case XWACOM_PARAM_SPEEDLEVEL:
		if ((value < 1) || (value > 11)) return BadValue;
		if (value > 6) priv->speed = 2.00*((double)value - 6.00);
		else priv->speed = ((double)value) / 6.00;
		sprintf(st, "%.3f", priv->speed);
		xf86AddNewOption(local->options, "Speed", st);
		break;
	    case XWACOM_PARAM_ACCEL:
		if ((value < 1) || (value > MAX_ACCEL)) return BadValue;
		priv->accel = value-1;
		xf86ReplaceIntOption(local->options, "Accel", priv->accel);
		break;
	    case XWACOM_PARAM_CLICKFORCE:
		if ((value < 1) || (value > 21)) return BadValue;
		common->wcmThreshold = (int)((double)
				(value*common->wcmMaxZ)/100.00+0.5);
		xf86ReplaceIntOption(local->options, "Threshold", 
				common->wcmThreshold);
		break;
	    case XWACOM_PARAM_XYDEFAULT:
		xf86WcmSetParam (local, XWACOM_PARAM_TOPX, 0);
		xf86WcmSetParam (local, XWACOM_PARAM_TOPY, 0);
		xf86WcmSetParam (local, XWACOM_PARAM_BOTTOMX, common->wcmMaxX);
		xf86WcmSetParam (local, XWACOM_PARAM_BOTTOMY, common->wcmMaxY);
		break;
	    case XWACOM_PARAM_GIMP:
		if ((value != 0) && (value != 1)) return BadValue;
		common->wcmGimp = value;
		if (value)
		{
			xf86ReplaceStrOption(local->options, "Gimp", "on");
		}
		else
		{
			xf86ReplaceStrOption(local->options, "Gimp", "off");
		}
		break;
	    case XWACOM_PARAM_MMT:
		if ((value != 0) && (value != 1)) return BadValue;
		common->wcmMMonitor = value;
		if (value)
		{
			xf86ReplaceStrOption(local->options, "MMonitor", "on");
		}
		else
		{
			xf86ReplaceStrOption(local->options, "MMonitor", "off");
		}
		break;
	    case XWACOM_PARAM_TPCBUTTON:
		if ((value != 0) && (value != 1)) return BadValue;
		priv->common->wcmTPCButton = value;
		if (value)
		{
			xf86ReplaceStrOption(local->options, "TPCButton", "on");
		}
		else
		{
			xf86ReplaceStrOption(local->options, "TPCButton", "off");
		}
		break;
	    case XWACOM_PARAM_CURSORPROX:
		if (IsCursor (priv))
		{
			char chBuf[64];
			int d = value & 0xFFFF;
			int h = (value >> 16) & 0xFFFF;
			if ((d > 63) || (h > 63))
				return BadValue;
			snprintf(chBuf,sizeof(chBuf),"%d,%d",d,h);
			xf86ReplaceStrOption(local->options, "CursorProx",chBuf);
			common->wcmCursorProxoutDist = d;
			common->wcmCursorProxoutHyst = h;
		}
		break;
	    default:
    		DBG(10, ErrorF("xf86WcmSetParam invalid param %d\n",param));
		return BadMatch;
	}
	return Success;
}

/*****************************************************************************
 * xf86WcmGetParam
 ****************************************************************************/

static int xf86WcmGetParam(LocalDevicePtr local, int param)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	int bn;
	DBG(10, ErrorF("xf86WcmGetParam param = %d\n",param));

	switch (param)
	{
	case XWACOM_PARAM_TOPX:
		return priv->topX;
	case XWACOM_PARAM_TOPY:
		return priv->topY;
	case XWACOM_PARAM_BOTTOMX:
		return priv->bottomX;
	case XWACOM_PARAM_BOTTOMY:
		return priv->bottomY;
	case XWACOM_PARAM_BUTTON1:
	case XWACOM_PARAM_BUTTON2:
	case XWACOM_PARAM_BUTTON3:
	case XWACOM_PARAM_BUTTON4:
	case XWACOM_PARAM_BUTTON5:
	case XWACOM_PARAM_BUTTON6:
	case XWACOM_PARAM_BUTTON7:
	case XWACOM_PARAM_BUTTON8:
	case XWACOM_PARAM_BUTTON9:
	case XWACOM_PARAM_BUTTON10:
	case XWACOM_PARAM_BUTTON11:
	case XWACOM_PARAM_BUTTON12:
	case XWACOM_PARAM_BUTTON13:
	case XWACOM_PARAM_BUTTON14:
	case XWACOM_PARAM_BUTTON15:
	case XWACOM_PARAM_BUTTON16:
	case XWACOM_PARAM_BUTTON17:
	case XWACOM_PARAM_BUTTON18:
	case XWACOM_PARAM_BUTTON19:
	case XWACOM_PARAM_BUTTON20:
	case XWACOM_PARAM_BUTTON21:
	case XWACOM_PARAM_BUTTON22:
	case XWACOM_PARAM_BUTTON23:
	case XWACOM_PARAM_BUTTON24:
	case XWACOM_PARAM_BUTTON25:
	case XWACOM_PARAM_BUTTON26:
	case XWACOM_PARAM_BUTTON27:
	case XWACOM_PARAM_BUTTON28:
	case XWACOM_PARAM_BUTTON29:
	case XWACOM_PARAM_BUTTON30:
	case XWACOM_PARAM_BUTTON31:
	case XWACOM_PARAM_BUTTON32:
		bn = param - XWACOM_PARAM_BUTTON1;
		if (bn >= priv->nbuttons)
			return -1;
		return priv->button [bn];
	case XWACOM_PARAM_DEBUGLEVEL:
		return gWacomModule.debugLevel;
	case XWACOM_PARAM_RAWFILTER:
		return (common->wcmFlags & RAW_FILTERING_FLAG) ? 1 : 0;
	case XWACOM_PARAM_PRESSCURVE:
		if (!IsCursor (priv) && !IsPad (priv))
			return (priv->nPressCtrl [0] << 24) |
			       (priv->nPressCtrl [1] << 16) |
			       (priv->nPressCtrl [2] << 8) |
			       (priv->nPressCtrl [3]);
		return -1;
	case XWACOM_PARAM_MODE:
		return (priv->flags & ABSOLUTE_FLAG) ? 1 : 0;
	case XWACOM_PARAM_SPEEDLEVEL:
		return (priv->speed > 1) ?
			(int) (priv->speed / 2) + 6 :
			(int) (priv->speed * 6);
	case XWACOM_PARAM_ACCEL:
		return priv->accel + 1;
	case XWACOM_PARAM_CLICKFORCE:
		return !common->wcmMaxZ ? 0 :
			(int) (((common->wcmThreshold + 0.5) * 100) / common->wcmMaxZ);
	case XWACOM_PARAM_XYDEFAULT:
		return -1;
	case XWACOM_PARAM_GIMP:
		return common->wcmGimp;
	case XWACOM_PARAM_MMT:
		return common->wcmMMonitor;
	case XWACOM_PARAM_TPCBUTTON:
		return common->wcmTPCButton;
	case XWACOM_PARAM_CURSORPROX:
		if (IsCursor (priv))
			return common->wcmCursorProxoutDist |
			       (common->wcmCursorProxoutHyst << 16);
		return -1;
	}
	DBG(10, ErrorF("xf86WcmGetParam invalid param %d\n",param));
	return -1;
}

/*****************************************************************************
 * xf86WcmGetDefaultParam
 ****************************************************************************/

static int xf86WcmGetDefaultParam(LocalDevicePtr local, int param)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	char *s;
	DBG(10, ErrorF("xf86WcmGetDefaultParam param = %d\n",param));

	switch (param)
	{
	case XWACOM_PARAM_TOPX:
		return 0;
	case XWACOM_PARAM_TOPY:
		return 0;
	case XWACOM_PARAM_BOTTOMX:
		return common->wcmMaxX;
	case XWACOM_PARAM_BOTTOMY:
		return common->wcmMaxY;		
	case XWACOM_PARAM_BUTTON1:
	case XWACOM_PARAM_BUTTON2:
	case XWACOM_PARAM_BUTTON3:
	case XWACOM_PARAM_BUTTON4:
	case XWACOM_PARAM_BUTTON5:
		return (param - XWACOM_PARAM_BUTTON1 + 1);
	case XWACOM_PARAM_MODE:
                if (IsCursor(priv) || IsPad(priv))
			return 0;
                else
			return 1;
	case XWACOM_PARAM_SPEEDLEVEL:
		return 6;
	case XWACOM_PARAM_ACCEL:
		return 0;
	case XWACOM_PARAM_CLICKFORCE:
		return 6;
	case XWACOM_PARAM_GIMP:
		return 1;
	case XWACOM_PARAM_MMT:
		return 1;
	case XWACOM_PARAM_TPCBUTTON:
		s = xf86FindOptionValue(local->options, "ForceDevice");
		if ( s )
			return 1;
		else
			return 0;
	case XWACOM_PARAM_PRESSCURVE:
		if (!IsCursor (priv) && !IsPad (priv))
			return (0 << 24) | (0 << 16) | (100 << 8) | 100;
		return -1;
	case XWACOM_PARAM_GETMODEL:
		xf86WcmModelToFile(local);
		return 0;
	}
	DBG(10, ErrorF("xf86WcmGetDefaultParam invalid param %d\n",param));
	return -1;
}

/*****************************************************************************
 * xf86WcmModelToFile
 ****************************************************************************/

static int xf86WcmModelToFile(LocalDevicePtr local)
{
	FILE		*fp = 0;
	LocalDevicePtr	localDevices = xf86FirstLocalDevice();
	WacomDevicePtr	priv = NULL, lprv;
	char		m1[32], m2[32], *m3;			
	int 		i = 0, x = 0, y = 0;

    	DBG(10, ErrorF("xf86WcmModelToFile \n"));
	fp = fopen("/etc/wacom.dat", "w+");
	if ( fp )
	{
		while(localDevices) 
		{
			m3 = xf86FindOptionValue(localDevices->options, "Type");
			if (m3 && (strstr(m3, "eraser") || strstr(m3, "stylus") 
					|| strstr(m3, "cursor")))
				lprv = (WacomDevicePtr)localDevices->private;
			else
				lprv = NULL;
			if ( lprv && lprv->common && lprv->common->wcmModel ) 
			{
				sscanf((char*)(lprv->common->wcmModel)->name, "%s %s", m1, m2);
				if ( lprv->common->wcmEraserID )
					fprintf(fp, "%s %s %s %s\n", localDevices->name, m2, m3, lprv->common->wcmEraserID);
				else
					fprintf(fp, "%s %s %s %s\n", localDevices->name, m2, m3, localDevices->name);
				if (lprv->twinview != TV_NONE)
				{
					priv = lprv;
				}
				if( !priv ) priv = lprv;
			}
			localDevices = localDevices->next;
		}
		/* write TwinView ScreenInfo */
		if (priv->twinview == TV_ABOVE_BELOW)
		{
			fprintf(fp, "Screen0 %d %d %d %d\n", priv->tvResolution[0], 
				priv->tvResolution[1], 0, 0);
			fprintf(fp, "Screen1 %d %d %d %d\n", priv->tvResolution[2], 
				priv->tvResolution[3], 0, priv->tvResolution[1]);
		}
		else if (priv->twinview == TV_LEFT_RIGHT)
		{
			fprintf(fp, "Screen0 %d %d %d %d\n", priv->tvResolution[0], 
				priv->tvResolution[1], 0, 0);
			fprintf(fp, "Screen1 %d %d %d %d\n", priv->tvResolution[2], 
				priv->tvResolution[3], priv->tvResolution[0], 0);
		}
		/* write other screen setup info */
		else
		{
			for (i = 0; i<screenInfo.numScreens; i++)
			{
				fprintf(fp, "Screen%d %d %d %d %d\n", 
				i, screenInfo.screens[i]->width,
				screenInfo.screens[i]->height, x, y);
				x += screenInfo.screens[i]->width;
			}
		}
		fclose(fp);
	} 
	return(Success);
}

/*****************************************************************************
* xf86WcmDevSwitchMode --
*****************************************************************************/

static int xf86WcmDevSwitchMode(ClientPtr client, DeviceIntPtr dev, int mode)
{
	LocalDevicePtr local = (LocalDevicePtr)dev->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr)local->private;

	DBG(3, ErrorF("xf86WcmSwitchMode dev=%p mode=%d\n", (void *)dev, mode));

	if (mode == Absolute)
		priv->flags |= ABSOLUTE_FLAG;
	else if (mode == Relative)
		priv->flags &= ~ABSOLUTE_FLAG; 
	else
	{
		DBG(10, ErrorF("xf86WcmSwitchMode dev=%p invalid mode=%d\n",
				(void *)dev, mode));
		return BadMatch;
	}

	return Success;
}

/*****************************************************************************
 * xf86WcmDevChangeControl --
 ****************************************************************************/

static int xf86WcmDevChangeControl(LocalDevicePtr local, xDeviceCtl* control)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	xDeviceResolutionCtl* res = (xDeviceResolutionCtl *)control;
	int i, rc = Success, *r = (int*)(res+1);
	
	if (control->control != DEVICE_RESOLUTION || (res->num_valuators < 1
			&& res->num_valuators > 3) || res->first_valuator != 0)
		return BadMatch;

	switch (res->num_valuators)
	{
		case  1:
		{
			AxisInfoPtr a;

			DBG (10, ErrorF("xf86WcmQueryControl: dev %s query 0x%x at %d\n",
				local->dev->name, r [0], priv->naxes));
			/* Since X11 doesn't provide any sane protocol for querying
			 * device parameters, we have to do a dirty trick here:
			 * we set the resolution of last axis to asked value,
			 * then we query it via XGetDeviceControl().
			 * After the query is done, XChangeDeviceControl is called
			 * again with r [0] == 0, which restores default resolution.
			 */
			a = local->dev->valuator->axes + priv->naxes - 1;
			a->resolution = a->min_resolution = a->max_resolution =
				r [0] ? xf86WcmGetParam (local, r [0]) : 1;
			break;
		}
		case 2:
		{
			DBG (10, ErrorF("xf86WcmChangeControl: dev %s set 0x%x to 0x%x\n",
				local->dev->name, r [0], r [1]));
			rc = xf86WcmSetParam (local, r [0], r[1]);
			break;
		}
		case 3:
		{
			AxisInfoPtr a;

			DBG (10, ErrorF("xf86WcmQueryControl: dev %s query 0x%x at %d\n",
				local->dev->name, r [0], priv->naxes));
			/* Since X11 doesn't provide any sane protocol for querying
			 * device parameters, we have to do a dirty trick here:
			 * we set the resolution of last axis to asked value,
			 * then we query it via XGetDeviceControl().
			 * After the query is done, XChangeDeviceControl is called
			 * again with r [0] == 0, which restores default resolution.
			 */
			a = local->dev->valuator->axes + priv->naxes - 1;
			a->resolution = a->min_resolution = a->max_resolution =
				r [0] ? xf86WcmGetDefaultParam (local, r [0]) : 1;
			break;
		}
	}
	/* Set resolution to current values so that X core doesn't barf */
	for (i = 0; i < res->num_valuators; i++)
		r [i] = local->dev->valuator->axes [i].resolution;
        return rc;
}

/*****************************************************************************
 * xf86WcmDevConvert --
 
 ****************************************************************************/

static Bool xf86WcmDevConvert(LocalDevicePtr local, int first, int num,
		int v0, int v1, int v2, int v3, int v4, int v5, int* x, int* y)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
    
	DBG(6, ErrorF("xf86WcmDevConvert v0=%d v1=%d \n", v0, v1));

	if (first != 0 || num == 1) 
 		return FALSE;

	*x = 0;
	*y = 0;

	if (priv->flags & ABSOLUTE_FLAG)
	{
		if (priv->twinview == TV_NONE)
		{
			v0 = v0 > priv->bottomX ? priv->bottomX - priv->topX : 
				v0 < priv->topX ? 0 : v0 - priv->topX;
			v1 = v1 > priv->bottomY ? priv->bottomY - priv->topY : 
				v1 < priv->topY ? 0 : v1 - priv->topY;
		}

#ifdef PANORAMIX
		if (!noPanoramiXExtension && priv->common->wcmGimp 
			&& priv->common->wcmMMonitor)
		{
			int i, totalWidth, leftPadding = 0;
			for (i = 0; i < priv->currentScreen; i++)
				leftPadding += screenInfo.screens[i]->width;
			for (totalWidth = leftPadding; i < priv->numScreen; i++)
				totalWidth += screenInfo.screens[i]->width;
			v0 -= (priv->bottomX - priv->topX) * leftPadding
				/ (double)totalWidth + 0.5;
		}
#endif
		if (priv->twinview != TV_NONE)
		{
			v0 -= priv->topX - priv->tvoffsetX;
			v1 -= priv->topY - priv->tvoffsetY;
               		if (priv->twinview == TV_LEFT_RIGHT)
			{
				if (v0 > priv->bottomX - priv->tvoffsetX && priv->screen_no == -1)
				{
					if (priv->currentScreen == 0)
						v0 = priv->bottomX - priv->tvoffsetX;
					else
					{
						v0 -= priv->bottomX - priv->topX - 2*priv->tvoffsetX;
						if (v0 > priv->bottomX - priv->tvoffsetX)
							v0 = 2*(priv->bottomX - priv->tvoffsetX) - v0;
					}
				}
				if (priv->currentScreen == 1)
				{
                       			*x = priv->tvResolution[0] + priv->tvResolution[2]
						* v0 / (priv->bottomX - priv->topX - 2*priv->tvoffsetX);
					*y = v1 * priv->tvResolution[3] /
						(priv->bottomY - priv->topY - 2*priv->tvoffsetY) + 0.5;
				}
				else
				{
					*x = priv->tvResolution[0] * v0 
						 / (priv->bottomX - priv->topX - 2*priv->tvoffsetX);
					*y = v1 * priv->tvResolution[1] /
						(priv->bottomY - priv->topY - 2*priv->tvoffsetY) + 0.5;
				}
			}
            		if (priv->twinview == TV_ABOVE_BELOW)
			{
				if (v1 > priv->bottomY - priv->tvoffsetY && priv->screen_no == -1)
				{
					if (priv->currentScreen == 0)
						v1 = priv->bottomY - priv->tvoffsetY;
					else
					{
						v1 -= priv->bottomY - priv->topY - 2*priv->tvoffsetY;
						if (v1 > priv->bottomY - priv->tvoffsetY)
							v1 = 2*(priv->bottomY - priv->tvoffsetY) - v1;
					}
				}
				if (priv->currentScreen == 1)
				{
 					*x = v0 * priv->tvResolution[2] /
						(priv->bottomX - priv->topX - 2*priv->tvoffsetX) + 0.5;
					*y = priv->tvResolution[1] + 
						priv->tvResolution[3] * v1 / 
						(priv->bottomY - priv->topY - 2*priv->tvoffsetY);
				}
				else
				{
					*x = v0 * priv->tvResolution[0] /
						(priv->bottomX - priv->topX - 2*priv->tvoffsetX) + 0.5;
					*y = priv->tvResolution[1] * v1 /
						(priv->bottomY - priv->topY - 2*priv->tvoffsetY);
				}
			}
			return TRUE;
		}
	}
	*x += v0 * priv->factorX + 0.5;
	*y += v1 * priv->factorY + 0.5;

	DBG(6, ErrorF("Wacom converted v0=%d v1=%d to x=%d y=%d\n", v0, v1, *x, *y));
	return TRUE;
}

/*****************************************************************************
 * xf86WcmDevReverseConvert --
 *  Convert X and Y to valuators. Only used by core events.
 *  Handdle relatve screen to tablet convert
 *  We don't support screen mapping yet (Ping April 22, 2005)
 ****************************************************************************/

static Bool xf86WcmDevReverseConvert(LocalDevicePtr local, int x, int y,
		int* valuators)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;

 	DBG(6, ErrorF("xf86WcmDevReverseConvert x=%d y=%d \n", x, y));
	priv->currentSX = x;
	priv->currentSY = y;

	if (!(priv->flags & ABSOLUTE_FLAG))
	{
		valuators[0] = x / priv->factorX + 0.5;
		valuators[1] = y / priv->factorY + 0.5;
	}

#ifdef NEVER 
/* This is for absolute screen mapping */
#ifdef PANORAMIX
	if (!noPanoramiXExtension && (priv->flags & ABSOLUTE_FLAG) && 
		priv->common->wcmGimp && priv->common->wcmMMonitor)
	{
		int i, totalWidth, leftPadding = 0;
		for (i = 0; i < priv->currentScreen; i++)
			leftPadding += screenInfo.screens[i]->width;
		for (totalWidth = leftPadding; i < priv->numScreen; i++)
			totalWidth += screenInfo.screens[i]->width;
		valuators[0] += (priv->bottomX - priv->topX)
			* leftPadding / (double)totalWidth + 0.5;
	}
#endif
	if (priv->twinview != TV_NONE && (priv->flags & ABSOLUTE_FLAG))
	{
                if (priv->twinview == TV_LEFT_RIGHT)
		{
			if (x > priv->tvResolution[0])
				x -= priv->tvResolution[0];
			if (priv->currentScreen == 1)
			{
				valuators[0] = x * (priv->bottomX - priv->topX - 2*priv->tvoffsetX)
				 	/ priv->tvResolution[2] + 0.5;
				valuators[1] = y * (priv->bottomY - priv->topY - 2*priv->tvoffsetY) /
					priv->tvResolution[3] + 0.5;
			}
			else
			{
				valuators[0] = x * (priv->bottomX - priv->topX - 2*priv->tvoffsetX)
					 / priv->tvResolution[0] + 0.5;
				valuators[1] = y * (priv->bottomY - priv->topY - 2*priv->tvoffsetY) /
					priv->tvResolution[1] + 0.5;
			}
		}
                if (priv->twinview == TV_ABOVE_BELOW)
		{
			if (y > priv->tvResolution[1])
				y -= priv->tvResolution[1];
			if (priv->currentScreen == 1)
			{
				valuators[0] = x * (priv->bottomX - priv->topX - 2*priv->tvoffsetX) /
					priv->tvResolution[2] + 0.5;
				valuators[1] = y *(priv->bottomY - priv->topY - 2*priv->tvoffsetY)
					/ priv->tvResolution[3] + 0.5;
			}
			else
			{
				valuators[0] = x * (priv->bottomX - priv->topX - 2*priv->tvoffsetX) /
					priv->tvResolution[0] + 0.5;
				valuators[1] = y *(priv->bottomY - priv->topY - 2*priv->tvoffsetY)
					/ priv->tvResolution[1] + 0.5;
			}
		}
	}
	if (priv->flags & ABSOLUTE_FLAG)
	{
		valuators[0] += priv->topX + priv->tvoffsetX;
		valuators[1] += priv->topY + priv->tvoffsetY;
	}
#endif
	DBG(6, ErrorF("Wacom converted x=%d y=%d to v0=%d v1=%d v2=%d v3=%d v4=%d v5=%d\n", x, y,
		valuators[0], valuators[1], valuators[2], valuators[3], valuators[4], valuators[5]));

	return TRUE;
}
