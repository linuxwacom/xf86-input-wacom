/* $XConsortium: xf86Wacom.c /main/20 1996/10/27 11:05:20 kaleb $ */
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

/* $XFree86: xc/programs/Xserver/hw/xfree86/input/wacom/xf86Wacom.c,v 1.26 2001/04/01 14:00:13 tsi Exp $ */

/*
 * This driver is only able to handle the Wacom IV and Wacom V protocols.
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
 * TO XFREE86 THE MAINTAINERS: don't remove the 3.3 code as I continue
 * to maintain it. Discuss with me before changing things in this driver!
 *
 *  Fred
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
 * 2003-04-02 26-j0.5.10 - refactoring
 * 2003-04-02 26-j0.5.11 - all devices using same data path
 */

static const char identification[] = "$Identification: 26-j0.5.11 $";

/****************************************************************************/

#include "xf86Wacom.h"

static int xf86WcmDevOpen(DeviceIntPtr pWcm);
static void xf86WcmDevReadInput(LocalDevicePtr local);
static void xf86WcmDevControlProc(DeviceIntPtr device, PtrCtrl* ctrl);
static void xf86WcmDevClose(LocalDevicePtr local);
static int xf86WcmDevProc(DeviceIntPtr pWcm, int what);
static int xf86WcmDevChangeControl(LocalDevicePtr local, xDeviceCtl* control);
static int xf86WcmDevSwitchMode(ClientPtr client, DeviceIntPtr dev, int mode);
static Bool xf86WcmDevConvert(LocalDevicePtr local, int first, int num,
		int v0, int v1, int v2, int v3, int v4, int v5, int* x, int* y);
static Bool xf86WcmDevReverseConvert(LocalDevicePtr local, int x, int y,
		int* valuators);
/*****************************************************************************
 * Keyboard symbol data
 ****************************************************************************/

static KeySym wacom_map[] = 
{
	NoSymbol,  /* 0x00 */
	NoSymbol,  /* 0x01 */
	NoSymbol,  /* 0x02 */
	NoSymbol,  /* 0x03 */
	NoSymbol,  /* 0x04 */
	NoSymbol,  /* 0x05 */
	NoSymbol,  /* 0x06 */
	NoSymbol,  /* 0x07 */
	XK_F1,     /* 0x08 */
	XK_F2,     /* 0x09 */
	XK_F3,     /* 0x0a */
	XK_F4,     /* 0x0b */
	XK_F5,     /* 0x0c */
	XK_F6,     /* 0x0d */
	XK_F7,     /* 0x0e */
	XK_F8,     /* 0x0f */
	XK_F9,     /* 0x10 */
	XK_F10,    /* 0x11 */
	XK_F11,    /* 0x12 */
	XK_F12,    /* 0x13 */
	XK_F13,    /* 0x14 */
	XK_F14,    /* 0x15 */
	XK_F15,    /* 0x16 */
	XK_F16,    /* 0x17 */
	XK_F17,    /* 0x18 */
	XK_F18,    /* 0x19 */
	XK_F19,    /* 0x1a */
	XK_F20,    /* 0x1b */
	XK_F21,    /* 0x1c */
	XK_F22,    /* 0x1d */
	XK_F23,    /* 0x1e */
	XK_F24,    /* 0x1f */
	XK_F25,    /* 0x20 */
	XK_F26,    /* 0x21 */
	XK_F27,    /* 0x22 */
	XK_F28,    /* 0x23 */
	XK_F29,    /* 0x24 */
	XK_F30,    /* 0x25 */
	XK_F31,    /* 0x26 */
	XK_F32     /* 0x27 */
};

/* minKeyCode = 8 because this is the min legal key code */

static KeySymsRec wacom_keysyms =
{
	wacom_map, /* map */
	8,         /* min key code */
	0x27,      /* max key code */
	1          /* width */
};

WacomModule gWacomModule =
{
	0,          /* debug level */
	wacom_map,  /* key map */
	identification, /* version */

	/* XFree86 V4 specific variables */
	#if XFREE86_V4
	{
		NULL,   /* input driver pointer */
	},
	#elif XFREE86_V3
	{
		0,      /* unused */
	},
	#endif

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
 * xf86WcmDevOpen --
 *    Open the physical device and init information structs.
 ****************************************************************************/

static int xf86WcmDevOpen(DeviceIntPtr pWcm)
{
	LocalDevicePtr local = (LocalDevicePtr)pWcm->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr)PRIVATE(pWcm);
	WacomCommonPtr common = priv->common;
	int totalWidth = 0, maxHeight = 0;
	double screenRatio, tabletRatio;
	int loop;

	/* open file, if not already open */
	if (local->fd < 0)
	{
		if ((common->wcmInitNumber > 2) ||
				(priv->initNumber == common->wcmInitNumber))
		{
			if (xf86WcmOpen(local) != Success)
			{
				if (local->fd >= 0)
					SYSCALL(xf86WcmClose(local->fd));
				local->fd = -1;
			}
			else
			{
				/* report the file descriptor to all devices */
				for(loop=0; loop<common->wcmNumDevices; loop++)
				common->wcmDevices[loop]->fd = local->fd;
			}
			common->wcmInitNumber++;
			priv->initNumber = common->wcmInitNumber;
		}
		else
		{
			priv->initNumber = common->wcmInitNumber;
		}
	}

	/* if factorX is set, initialize bounding rect */
	if (local->fd != -1 && priv->factorX == 0.0)
	{

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
			(priv->screen_no >= priv->numScreen ||
			priv->screen_no < 0))
		{
			ErrorF("%s: invalid screen number %d, resetting to 0\n",
					local->name, priv->screen_no);
			priv->screen_no = 0;
		}

		/* Calculate the ratio according to KeepShape, TopX and TopY */
		if (priv->screen_no != -1)
		{
			priv->currentScreen = priv->screen_no;
			totalWidth = screenInfo.screens[priv->screen_no]->width;
			maxHeight = screenInfo.screens[priv->screen_no]->height;
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
				/ (double)(priv->bottomX - priv->topX);
			priv->factorY = maxHeight
				/ (double)(priv->bottomY - priv->topY);
			DBG(2, ErrorF("X factor = %.3g, Y factor = %.3g\n",
				priv->factorX, priv->factorY));
		}
	} /* end bounding rect */

	/* x and y axes */
	InitValuatorAxisStruct(pWcm, 0, 0,
		priv->bottomX - priv->topX, /* max val */
		mils(common->wcmResolX), /* tablet resolution */
		0, mils(common->wcmResolX)); /* max_res */

	InitValuatorAxisStruct(pWcm, 1, 0,
		priv->bottomY - priv->topY, /* max val */
		mils(common->wcmResolY), /* tablet resolution */
		0, mils(common->wcmResolY)); /* max_res */

	/* pressure */
	InitValuatorAxisStruct(pWcm, 2, 0,
		common->wcmMaxZ, /* max val */
		1, 1, 1);

	if (IsCursor(priv))
	{
		/* z-rot and throttle */
		InitValuatorAxisStruct(pWcm, 3, -900, 899, 1, 1, 1);
		InitValuatorAxisStruct(pWcm, 4, -1023, 1023, 1, 1, 1);
	}
	else
	{
		/* tilt-x and tilt-y */
		InitValuatorAxisStruct(pWcm, 3, -64, 63, 1, 1, 1);
		InitValuatorAxisStruct(pWcm, 4, -64, 63, 1, 1, 1);
	}

	/* wheel */
	InitValuatorAxisStruct(pWcm, 5, 0, 1023, 1, 1, 1);

	return (local->fd != -1);
}

/*****************************************************************************
 * xf86WcmDevReadInput --
 *   Read the device on IO signal
 ****************************************************************************/

static void xf86WcmDevReadInput(LocalDevicePtr local)
{
	/* Dispatch device-specific read */
	((WacomDevicePtr)local->private)->common->wcmDevCls->Read(local);
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
	int loop, num = 0;

	for (loop=0; loop<common->wcmNumDevices; loop++)
	{
		if (common->wcmDevices[loop]->fd >= 0)
			num++;
	}
	DBG(4, ErrorF("Wacom number of open devices = %d\n", num));

	if (num == 1)
		SYSCALL(xf86WcmClose(local->fd));

	local->fd = -1;
}
 
/*****************************************************************************
 * xf86WcmDevProc --
 *   Handle the initialization, etc. of a wacom
 ****************************************************************************/

static int xf86WcmDevProc(DeviceIntPtr pWcm, int what)
{
	CARD8 map[(32 << 4) + 1];
	int nbaxes;
	int nbbuttons;
	int loop;
	LocalDevicePtr local = (LocalDevicePtr)pWcm->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr)PRIVATE(pWcm);

	DBG(2, ErrorF("BEGIN xf86WcmProc dev=0x%x priv=0x%x "
			"type=%s flags=%d what=%d\n",
			pWcm, priv,
			(DEVICE_ID(priv->flags) == STYLUS_ID) ? "stylus" :
			(DEVICE_ID(priv->flags) == CURSOR_ID) ? "cursor" :
			"eraser", priv->flags, what));

	switch (what)
	{
		case DEVICE_INIT: 
			DBG(1, ErrorF("xf86WcmProc pWcm=0x%x what=INIT\n", pWcm));

			nbaxes = 6;  /* X, Y, Pressure, Tilt-X, Tilt-Y, Wheel */

			switch(DEVICE_ID(priv->flags))
			{
				case ERASER_ID:
					nbbuttons = 1;
					break;
				case STYLUS_ID:
					nbbuttons = 4;
					break;
				default:
					nbbuttons = 16;
					break;
			}

			for(loop=1; loop<=nbbuttons; loop++) map[loop] = loop;

			if (InitButtonClassDeviceStruct(pWcm,
					nbbuttons, map) == FALSE)
			{
				ErrorF("unable to allocate Button class device\n");
				return !Success;
			}

			if (InitFocusClassDeviceStruct(pWcm) == FALSE)
			{
				ErrorF("unable to init Focus class device\n");
				return !Success;
			}

			if (InitPtrFeedbackClassDeviceStruct(pWcm,
					xf86WcmDevControlProc) == FALSE)
			{
				ErrorF("unable to init ptr feedback\n");
				return !Success;
			}

			if (InitProximityClassDeviceStruct(pWcm) == FALSE)
			{
				ErrorF("unable to init proximity class device\n");
				return !Success;
			}

			if (InitKeyClassDeviceStruct(pWcm,
					&wacom_keysyms, NULL) == FALSE)
			{
				ErrorF("unable to init key class device\n"); 
				return !Success;
			}

			if (InitValuatorClassDeviceStruct(pWcm, 
					nbaxes, xf86GetMotionEvents, 
					local->history_size,
					((priv->flags & ABSOLUTE_FLAG) ?
						Absolute : Relative) |
						OutOfProximity) == FALSE)
			{
				ErrorF("unable to allocate Valuator class device\n"); 
				return !Success;
			}
			else
			{
				/* allocate motion history buffer if needed */
				xf86MotionHistoryAllocate(local);
#if XFREE86_V3
				AssignTypeAndName(pWcm, local->atom,
						local->name);
#endif
			}

			/* open the device to gather informations */
			if (!xf86WcmDevOpen(pWcm))
			{
				/* Sometimes PL does not open the first time */
				DBG(1, ErrorF("xf86WcmProc try to open "
						"pWcm=0x%x again\n", pWcm));
				if (!xf86WcmDevOpen(pWcm))
				{
					DBG(1, ErrorF("xf86WcmProc "
						"pWcm=0x%x what=INIT FALSE\n",
						pWcm));
					return !Success;
				}
			}
			break; 

		case DEVICE_ON:
			DBG(1, ErrorF("xf86WcmProc pWcm=0x%x what=ON\n", pWcm));

			if ((local->fd < 0) && (!xf86WcmDevOpen(pWcm)))
			{
				pWcm->inited = FALSE;
				return !Success;
			}
#if XFREE86_V4     
			xf86AddEnabledDevice(local);
#else
			AddEnabledDevice(local->fd);
#endif
			pWcm->public.on = TRUE;
			break;

		case DEVICE_OFF:
		case DEVICE_CLOSE:
			DBG(1, ErrorF("xf86WcmProc pWcm=0x%x what=%s\n", pWcm,
				(what == DEVICE_CLOSE) ?  "CLOSE" : "OFF"));

			if (local->fd >= 0)
			{
#if XFREE86_V4     
				xf86RemoveEnabledDevice(local);
#else
				RemoveEnabledDevice(local->fd);
#endif
				xf86WcmDevClose(local);
			}
			pWcm->public.on = FALSE;
			break;

		default:
			ErrorF("wacom unsupported mode=%d\n", what);
			return !Success;
	} /* end switch */

	DBG(2, ErrorF("END xf86WcmProc Success what=%d dev=0x%x priv=0x%x\n",
			what, pWcm, priv));

	return Success;
}

/*****************************************************************************
 * xf86WcmDevChangeControl --
 ****************************************************************************/

static int xf86WcmDevChangeControl(LocalDevicePtr local, xDeviceCtl* control)
{
	xDeviceResolutionCtl* res;
	int* resolutions;
	char str[10];

	res = (xDeviceResolutionCtl*)control;

	if ((control->control != DEVICE_RESOLUTION) || (res->num_valuators < 1))
		return (BadMatch);

	resolutions = (int *)(res +1);

	DBG(3, ErrorF("xf86WcmChangeControl changing to %d "
			"(suppressing under)\n",resolutions[0]));

	sprintf(str, "SU%d\r", resolutions[0]);
	SYSCALL(write(local->fd, str, strlen(str)));

	return Success;
}

/*****************************************************************************
* xf86WcmDevSwitchMode --
*****************************************************************************/

static int xf86WcmDevSwitchMode(ClientPtr client, DeviceIntPtr dev, int mode)
{
	LocalDevicePtr local = (LocalDevicePtr)dev->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr)local->private;

	DBG(3, ErrorF("xf86WcmSwitchMode dev=0x%x mode=%d\n", dev, mode));

	if (mode == Absolute)
		priv->flags = priv->flags | ABSOLUTE_FLAG;
	else if (mode == Relative)
		priv->flags = priv->flags & ~ABSOLUTE_FLAG; 
	else
	{
		DBG(1, ErrorF("xf86WcmSwitchMode dev=0x%x invalid mode=%d\n",
				dev, mode));
		return BadMatch;
	}

	return Success;
}

/*****************************************************************************
 * xf86WcmDevConvert --
 *   Convert valuators to X and Y. Only used by core events.
 ****************************************************************************/

static Bool xf86WcmDevConvert(LocalDevicePtr local, int first, int num,
		int v0, int v1, int v2, int v3, int v4, int v5, int* x, int* y)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
    
	DBG(6, ErrorF("xf86WcmDevConvert\n"));

	if (first != 0 || num == 1)
		return FALSE;
#ifdef PANORAMIX
	if (!noPanoramiXExtension && (priv->flags & ABSOLUTE_FLAG))
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
	*x = v0 * priv->factorX + 0.5;
	*y = v1 * priv->factorY + 0.5;

	DBG(6, ErrorF("Wacom converted v0=%d v1=%d to x=%d y=%d\n",
		v0, v1, *x, *y));
	return TRUE;
}

/*****************************************************************************
 * xf86WcmDevReverseConvert --
 *  Convert X and Y to valuators. Only used by core events.
 ****************************************************************************/

static Bool xf86WcmDevReverseConvert(LocalDevicePtr local, int x, int y,
		int* valuators)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
    
	valuators[0] = x / priv->factorX + 0.5;
	valuators[1] = y / priv->factorY + 0.5;
#ifdef PANORAMIX
	if (!noPanoramiXExtension && (priv->flags & ABSOLUTE_FLAG))
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
	DBG(6, ErrorF("Wacom converted x=%d y=%d to v0=%d v1=%d\n", x, y,
		valuators[0], valuators[1]));

	return TRUE;
}
