/*
 * Copyright 1995-2002 by Frederic Lepied, France. <Lepied@XFree86.org> 
 * Copyright 2002-2008 by Ping Cheng, Wacom Technology. <pingc@wacom.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

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
 * John Joganic <jej@j-arkadia.com>
 */

/*
 * REVISION HISTORY
 *
 * 2005-10-17 47-pc0.7.1 - Added DTU710, DTF720, G4
 * 2005-11-17 47-pc0.7.1-1 - Report tool serial number and ID to Xinput
 * 2005-12-02 47-pc0.7.1-2 - Grap the USB port so /dev/input/mice won't get it
 * 2005-12-21 47-pc0.7.2 - new release
 * 2006-03-21 47-pc0.7.3 - new release
 * 2006-03-31 47-pc0.7.3-1 - new release
 * 2006-05-03 47-pc0.7.4 - new release
 * 2006-07-17 47-pc0.7.5 - Support button/key combined events
 * 2006-11-13 47-pc0.7.7 - Updated Xinerama setup support
 * 2007-01-31 47-pc0.7.7-3 - multiarea support
 * 2007-02-09 47-pc0.7.7-5 - Support keystrokes
 * 2007-03-28 47-pc0.7.7-7 - multiarea support
 * 2007-03-29 47-pc0.7.7-8 - clean up code
 * 2007-05-01 47-pc0.7.7-9 - fixed 2 bugs
 * 2007-05-18 47-pc0.7.7-10 - support new xsetwacom commands
 * 2007-06-05 47-pc0.7.7-11 - Test Ron's patches
 * 2007-06-15 47-pc0.7.7-12 - enable changing number of raw data 
 * 2007-06-25 47-pc0.7.8 - new release
 * 2007-10-25 47-pc0.7.9-1 - Support multimonitors in both horizonal and vertical settings
 * 2007-11-21 47-pc0.7.9-3 - Updated TwinView screen switch offset
 * 2007-12-07 47-pc0.7.9-4 - Support Cintiq 12WX and Bamboo
 * 2007-12-20 47-pc0.7.9-5 - multimonitor support update
 * 2008-01-08 47-pc0.7.9-6 - Configure script change for Xorg 7.3 support
 */

static const char identification[] = "$Identification: 47-0.7.9-6 $";

/****************************************************************************/

#include "xf86Wacom.h"
#include "wcmFilter.h"

static int xf86WcmDevOpen(DeviceIntPtr pWcm);
static void xf86WcmDevReadInput(LocalDevicePtr local);
static void xf86WcmDevControlProc(DeviceIntPtr device, PtrCtrl* ctrl);
static void xf86WcmDevClose(LocalDevicePtr local);
static int xf86WcmDevProc(DeviceIntPtr pWcm, int what);
static Bool xf86WcmDevConvert(LocalDevicePtr local, int first, int num,
		int v0, int v1, int v2, int v3, int v4, int v5, int* x, int* y);
static Bool xf86WcmDevReverseConvert(LocalDevicePtr local, int x, int y,
		int* valuators);
static void xf86WcmInitialTVScreens(LocalDevicePtr local);
extern Bool usbWcmInit(LocalDevicePtr pDev);
extern int usbWcmGetRanges(LocalDevicePtr local);
extern int xf86WcmDevChangeControl(LocalDevicePtr local, xDeviceCtl* control);
extern int xf86WcmDevSwitchMode(ClientPtr client, DeviceIntPtr dev, int mode);

WacomModule gWacomModule =
{
	identification, /* version */
	NULL,           /* input driver pointer */

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

static void xf86WcmKbdLedCallback(DeviceIntPtr di, LedCtrl * lcp)
{
}

static int xf86WcmInitArea(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomToolAreaPtr area = priv->toolarea, inlist;
	WacomCommonPtr common = priv->common;
	double screenRatio, tabletRatio;

	DBG(10, priv->debugLevel, ErrorF("xf86WcmInitArea\n"));

	/* Verify Box */
	if (priv->topX > common->wcmMaxX)
	{
		area->topX = priv->topX = 0;
	}

	if (priv->topY > common->wcmMaxY)
	{
		area->topY = priv->topY = 0;
	}

	/* set unconfigured bottom to max */
	priv->bottomX = xf86SetIntOption(local->options, "BottomX", 0);
	if (priv->bottomX < priv->topX || !priv->bottomX)
	{
		area->bottomX = priv->bottomX = common->wcmMaxX;
	}

	/* set unconfigured bottom to max */
	priv->bottomY = xf86SetIntOption(local->options, "BottomY", 0);
	if (priv->bottomY < priv->topY || !priv->bottomY)
	{
		area->bottomY = priv->bottomY = common->wcmMaxY;
	}

	if (priv->twinview != TV_NONE)
		priv->numScreen = 2;

	if (priv->screen_no != -1 &&
		(priv->screen_no >= priv->numScreen || priv->screen_no < 0))
	{
		if (priv->twinview == TV_NONE || priv->screen_no != 1)
		{
			ErrorF("%s: invalid screen number %d, resetting to default (-1) \n",
					local->name, priv->screen_no);
			priv->screen_no = -1;
		}
	}

	/* need maxWidth and maxHeight for keepshape */
	xf86WcmMappingFactor(local);

	/* Maintain aspect ratio */
	if (priv->flags & KEEP_SHAPE_FLAG)
	{

		screenRatio = ((double)priv->maxWidth / (double)priv->maxHeight);
		tabletRatio = ((double)(common->wcmMaxX - priv->topX) /
				(double)(common->wcmMaxY - priv->topY));

		DBG(2, priv->debugLevel, ErrorF("screenRatio = %.3g, "
			"tabletRatio = %.3g\n", screenRatio, tabletRatio));

		if (screenRatio > tabletRatio)
		{
			area->bottomX = priv->bottomX = common->wcmMaxX;
			area->bottomY = priv->bottomY = (common->wcmMaxY - priv->topY) *
				tabletRatio / screenRatio + priv->topY;
		}
		else
		{
			area->bottomX = priv->bottomX = (common->wcmMaxX - priv->topX) *
				screenRatio / tabletRatio + priv->topX;
			area->bottomY = priv->bottomY = common->wcmMaxY;
		}
		/* active tablet size has been changed */
		xf86WcmMappingFactor(local);
	}
	/* end keep shape */ 

	inlist = priv->tool->arealist;

	/* The first one in the list is always valid */
	if (area != inlist && xf86WcmAreaListOverlap(area, inlist))
	{
		inlist = priv->tool->arealist;

		/* remove this area from the list */
		for (; inlist; inlist=inlist->next)
		{
			if (inlist->next == area)
			{
				inlist->next = area->next;
				xfree(area);
				priv->toolarea = NULL;
 			break;
			}
		}

		/* Remove this device from the common struct */
		if (common->wcmDevices == priv)
			common->wcmDevices = priv->next;
		else
		{
			WacomDevicePtr tmp = common->wcmDevices;
			while(tmp->next && tmp->next != priv)
				tmp = tmp->next;
			if(tmp)
				tmp->next = priv->next;
		}
		xf86Msg(X_ERROR, "%s: Top/Bottom area overlaps with another devices.\n",
			local->conf_idev->identifier);
		return FALSE;
	}
	if (xf86Verbose)
	{
		ErrorF("%s Wacom device \"%s\" top X=%d top Y=%d "
				"bottom X=%d bottom Y=%d\n",
				XCONFIG_PROBED, local->name, priv->topX,
				priv->topY, priv->bottomX, priv->bottomY);
	}
	return TRUE;
}

/*****************************************************************************
 * xf86WcmInitialCoordinates
 ****************************************************************************/

void xf86WcmInitialCoordinates(LocalDevicePtr local, int axes)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	int tabletSize = 0, topx = 0, topy = 0;
	int resolution;

	/* x ax */
	if ( !axes )
	{
		if (priv->twinview == TV_LEFT_RIGHT)
			tabletSize = 2*(priv->bottomX - priv->topX - 2*priv->tvoffsetX);
		else
		{
			if (priv->flags & ABSOLUTE_FLAG)
				tabletSize = priv->bottomX;
			else
				tabletSize = priv->bottomX - priv->topX;
		}
		if (priv->flags & ABSOLUTE_FLAG)
			topx = priv->topX - priv->tvoffsetX;

		resolution = common->wcmResolX;
#if defined WCM_XORG && GET_ABI_MAJOR(ABI_XINPUT_VERSION) > 0
		/* Ugly hack for Xorg 7.3, which doesn't call xf86WcmDevConvert
		 * for coordinate conversion at the moment */
		if (priv->flags & ABSOLUTE_FLAG) tabletSize -= topx;
		topx = 0;
		tabletSize = (int)((double)tabletSize * priv->factorX + 0.5);
		resolution = (int)((double)resolution * priv->factorX + 0.5);
#endif

		InitValuatorAxisStruct(local->dev, 0, topx, tabletSize, 
			resolution, 0, resolution); 
	}
	else /* y ax */
	{
		if (priv->twinview == TV_ABOVE_BELOW)
			tabletSize = 2*(priv->bottomY - priv->topY - 2*priv->tvoffsetY);
		else
		{
			if (priv->flags & ABSOLUTE_FLAG)
				tabletSize = priv->bottomY;
			else
				tabletSize = priv->bottomY - priv->topY;
		}
		if (priv->flags & ABSOLUTE_FLAG)
			topy = priv->topY - priv->tvoffsetY;

		resolution = common->wcmResolY;
#if defined WCM_XORG && GET_ABI_MAJOR(ABI_XINPUT_VERSION) > 0
		/* Ugly hack for Xorg 7.3, which doesn't call xf86WcmDevConvert
		 * for coordinate conversion at the moment */
		if (priv->flags & ABSOLUTE_FLAG) tabletSize -= topy;
		topy = 0;
		tabletSize = (int)((double)tabletSize * priv->factorY + 0.5);
		resolution = (int)((double)resolution * priv->factorY + 0.5);
#endif

		InitValuatorAxisStruct(local->dev, 1, topy, tabletSize, 
			resolution, 0, resolution); 
	}
}

/*****************************************************************************
 * xf86WcmInitialTVScreens
 ****************************************************************************/

static void xf86WcmInitialTVScreens(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;

	if (priv->twinview == TV_NONE)
		return;

	priv->numScreen = 2;

	if (priv->twinview == TV_LEFT_RIGHT)
	{
		/* it does not need the offset if always map to a specific screen */
		if (priv->screen_no == -1)
			priv->tvoffsetX = 60;

		/* default resolution */
		if(!priv->tvResolution[0])
		{
			priv->tvResolution[0] = screenInfo.screens[0]->width/2;
			priv->tvResolution[1] = screenInfo.screens[0]->height;
			priv->tvResolution[2] = priv->tvResolution[0];
			priv->tvResolution[3] = priv->tvResolution[1];
		}
	}
	else if (priv->twinview == TV_ABOVE_BELOW)
	{
		/* it does not need the offset if always map to a specific screen */
		if (priv->screen_no == -1)
			priv->tvoffsetY = 60;

		/* default resolution */
		if(!priv->tvResolution[0])
		{
			priv->tvResolution[0] = screenInfo.screens[0]->width;
			priv->tvResolution[1] = screenInfo.screens[0]->height/2;
			priv->tvResolution[2] = priv->tvResolution[0];
			priv->tvResolution[3] = priv->tvResolution[1];
		}
	}

	/* initial screen info */
	priv->screenTopX[0] = 0;
	priv->screenTopY[0] = 0;
	priv->screenBottomX[0] = priv->tvResolution[0];
	priv->screenBottomY[0] = priv->tvResolution[1];
	if (priv->twinview == TV_ABOVE_BELOW)
	{
		priv->screenTopX[1] = 0;
		priv->screenTopY[1] = priv->tvResolution[1];
		priv->screenBottomX[1] = priv->tvResolution[2];
		priv->screenBottomY[1] = priv->tvResolution[1] + priv->tvResolution[3];
	}
	if (priv->twinview == TV_LEFT_RIGHT)
	{
		priv->screenTopX[1] = priv->tvResolution[0];
		priv->screenTopY[1] = 0;
		priv->screenBottomX[1] = priv->tvResolution[0] + priv->tvResolution[2];
		priv->screenBottomY[1] = priv->tvResolution[3];
	}

	DBG(10, priv->debugLevel, ErrorF("xf86WcmInitialTVScreens for \"%s\" "
		"topX0=%d topY0=%d bottomX0=%d bottomY0=%d "
		"topX1=%d topY1=%d bottomX1=%d bottomY1=%d \n",
		local->name, priv->screenTopX[0], priv->screenTopY[0],
		priv->screenBottomX[0], priv->screenBottomY[0],
		priv->screenTopX[1], priv->screenTopY[1],
		priv->screenBottomX[1], priv->screenBottomY[1]));
}

/*****************************************************************************
 * xf86WcmInitialScreens
 ****************************************************************************/

void xf86WcmInitialScreens(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	int i;

	priv->tvoffsetX = 0;
	priv->tvoffsetY = 0;
	if (priv->twinview != TV_NONE)
	{
		xf86WcmInitialTVScreens(local);
		return;
	}

	/* initial screen info */
	priv->numScreen = screenInfo.numScreens;
	priv->screenTopX[0] = 0;
	priv->screenTopY[0] = 0;
	priv->screenBottomX[0] = 0;
	priv->screenBottomY[0] = 0;
	for (i=0; i<screenInfo.numScreens; i++)
	{
#ifdef WCM_XORG
		priv->screenTopX[i] = dixScreenOrigins[i].x;
		priv->screenTopY[i] = dixScreenOrigins[i].y;
		priv->screenBottomX[i] = dixScreenOrigins[i].x;
		priv->screenBottomY[i] = dixScreenOrigins[i].y;

		DBG(10, priv->debugLevel, ErrorF("xf86WcmInitialScreens from dix for \"%s\" "
			"ScreenOrigins[%d].x=%d ScreenOrigins[%d].y=%d \n",
			local->name, i, priv->screenTopX[i], i, priv->screenTopY[i]));
#else
		if (i > 0)
		{
			/* only support left to right in this case */
			priv->screenTopX[i] = priv->screenBottomX[i-1];
			priv->screenTopY[i] = 0;
			priv->screenBottomX[i] = priv->screenTopX[i];
			priv->screenBottomY[i] = 0;
		}
#endif
		priv->screenBottomX[i] += screenInfo.screens[i]->width;
		priv->screenBottomY[i] += screenInfo.screens[i]->height;

		DBG(10, priv->debugLevel, ErrorF("xf86WcmInitialScreens for \"%s\" "
			"topX[%d]=%d topY[%d]=%d bottomX[%d]=%d bottomY[%d]=%d \n",
			local->name, i, priv->screenTopX[i], i, priv->screenTopY[i],
			i, priv->screenBottomX[i], i, priv->screenBottomY[i]));
	}
}

/*****************************************************************************
 * xf86WcmRegisterX11Devices --
 *    Register the X11 input devices with X11 core.
 ****************************************************************************/

static int xf86WcmRegisterX11Devices (LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	CARD8 butmap[MAX_BUTTONS+1];
	int nbaxes, nbbuttons, nbkeys;
	int loop;

	/* Detect tablet configuration, if possible */
	if (priv->common->wcmModel->DetectConfig)
		priv->common->wcmModel->DetectConfig (local);

	nbaxes = priv->naxes;       /* X, Y, Pressure, Tilt-X, Tilt-Y, Wheel */
	nbbuttons = priv->nbuttons; /* Use actual number of buttons, if possible */
	nbkeys = nbbuttons;         /* Same number of keys since any button may be */
	                            /* configured as an either mouse button or key */

	DBG(10, priv->debugLevel, ErrorF("xf86WcmRegisterX11Devices "
		"(%s) %d buttons, %d keys, %d axes\n",
		IsStylus(priv) ? "stylus" :
		IsCursor(priv) ? "cursor" :
		IsPad(priv) ? "pad" : "eraser",
		nbbuttons, nbkeys, nbaxes));

	/* initialize screen bounding rect */
	xf86WcmInitialScreens(local);

	if (xf86WcmInitArea(local) == FALSE)
	{
		return FALSE;
	}

	for(loop=1; loop<=nbbuttons; loop++)
		butmap[loop] = loop;

	if (InitButtonClassDeviceStruct(local->dev, nbbuttons, butmap) == FALSE)
	{
		ErrorF("unable to allocate Button class device\n");
		return FALSE;
	}

	if (InitFocusClassDeviceStruct(local->dev) == FALSE)
	{
		ErrorF("unable to init Focus class device\n");
		return FALSE;
	}

	if (InitPtrFeedbackClassDeviceStruct(local->dev,
		xf86WcmDevControlProc) == FALSE)
	{
		ErrorF("unable to init ptr feedback\n");
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
#if defined WCM_XFREE86 || GET_ABI_MAJOR(ABI_XINPUT_VERSION) == 0
					  xf86GetMotionEvents,
					  local->history_size,
#else
					  GetMotionHistory,
					  GetMotionHistorySize(),
#endif
					  ((priv->flags & ABSOLUTE_FLAG) ?
					  Absolute : Relative) | 
					  OutOfProximity ) == FALSE)
	{
		ErrorF("unable to allocate Valuator class device\n");
		return FALSE;
	}


	if (nbkeys)
	{
		KeySymsRec wacom_keysyms;
		KeySym keymap[256];

		for (loop = 0; loop < nbkeys; loop++)
			if ((priv->button [loop] & AC_TYPE) == AC_KEY)
				keymap [loop] = priv->button [loop] & AC_CODE;
			else
				keymap [loop] = NoSymbol;
		for(loop = nbkeys; loop<256; loop++)
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
		wacom_keysyms.maxKeyCode = 255;
		wacom_keysyms.mapWidth = 1;
		if (InitKeyClassDeviceStruct(local->dev, &wacom_keysyms, NULL) == FALSE)
		{
			ErrorF("unable to init key class device\n");
			return FALSE;
		}
	}

	if(InitLedFeedbackClassDeviceStruct (local->dev, xf86WcmKbdLedCallback) == FALSE) {
		ErrorF("unable to init led feedback device struct\n");
		return FALSE;
	}

#if defined WCM_XFREE86 || GET_ABI_MAJOR(ABI_XINPUT_VERSION) == 0
	/* allocate motion history buffer if needed */
	xf86MotionHistoryAllocate(local);
#endif

	/* x */
	xf86WcmInitialCoordinates(local, 0);

	/* y */
	xf86WcmInitialCoordinates(local, 1);

	/* pressure */
	InitValuatorAxisStruct(local->dev, 2, 0, 
		common->wcmMaxZ, 1, 1, 1);

	if (IsCursor(priv))
	{
		/* z-rot and throttle */
		InitValuatorAxisStruct(local->dev, 3, -900, 899, 1, 1, 1);
		InitValuatorAxisStruct(local->dev, 4, -1023, 1023, 1, 1, 1);
	}
	else if (IsPad(priv))
	{
		/* strip-x and strip-y */
		if (priv->naxes)
		{
			InitValuatorAxisStruct(local->dev, 3, 0, common->wcmMaxStripX, 1, 1, 1);
			InitValuatorAxisStruct(local->dev, 4, 0, common->wcmMaxStripY, 1, 1, 1);
		}
	}
	else
	{
		/* tilt-x and tilt-y */
		InitValuatorAxisStruct(local->dev, 3, -64, 63, 1, 1, 1);
		InitValuatorAxisStruct(local->dev, 4, -64, 63, 1, 1, 1);
	}

	if ((strstr(common->wcmModel->name, "Intuos3") || 
		strstr(common->wcmModel->name, "CintiqV5")) 
			&& IsStylus(priv))
		/* Art Marker Pen rotation */
		InitValuatorAxisStruct(local->dev, 5, -900, 899, 1, 1, 1);
	else if (strstr(common->wcmModel->name, "Bamboo") && IsPad(priv))
		InitValuatorAxisStruct(local->dev, 5, 0, 71, 1, 1, 1);
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
 
	DBG(10, priv->debugLevel, ErrorF("xf86WcmDevOpen\n"));

	/* Device has been open */
	if (priv->wcmDevOpenCount)
		return TRUE;

	/* open file, if not already open */
	if (common->fd_refs == 0)
	{
		if ((xf86WcmOpen (local) != Success) || (local->fd < 0))
		{
			DBG(1, priv->debugLevel, ErrorF("Failed to open "
				"device (fd=%d)\n", local->fd));
			if (local->fd >= 0)
			{
				DBG(1, priv->debugLevel, ErrorF("Closing device\n"));
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
	int loop=0;
	#define MAX_READ_LOOPS 10

	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

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
		DBG(1, priv->debugLevel, ErrorF("xf86WcmDevReadInput: Can't keep up!!!\n"));
	else if (loop > 0)
		DBG(10, priv->debugLevel, ErrorF("xf86WcmDevReadInput: Read (%d)\n",loop));
}					

void xf86WcmReadPacket(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	int len, pos, cnt, remaining;

 	DBG(10, common->debugLevel, ErrorF("xf86WcmReadPacket: device=%s"
		" fd=%d \n", common->wcmDevice, local->fd));

	remaining = sizeof(common->buffer) - common->bufpos;

	DBG(1, common->debugLevel, ErrorF("xf86WcmReadPacket: pos=%d"
		" remaining=%d\n", common->bufpos, remaining));

	/* fill buffer with as much data as we can handle */
	len = xf86WcmRead(local->fd,
		common->buffer + common->bufpos, remaining);

	if (len <= 0)
	{
		/* In case of error, we assume the device has been
		 * disconnected. So we close it and iterate over all
		 * wcmDevices to actually close associated devices. */
		WacomDevicePtr wDev = common->wcmDevices;
		for(; wDev; wDev = wDev->next)
		{
			if (wDev->local->fd >= 0)
				xf86WcmDevProc(wDev->local->dev, DEVICE_OFF);
		}
		ErrorF("Error reading wacom device : %s\n", strerror(errno));
		return;
	}

	/* account for new data */
	common->bufpos += len;
	DBG(10, common->debugLevel, ErrorF("xf86WcmReadPacket buffer has %d bytes\n",
		common->bufpos));

	pos = 0;

	/* while there are whole packets present, parse data */
	while ((common->bufpos - pos) >=  common->wcmPktLength)
	{
		/* parse packet */
		cnt = common->wcmModel->Parse(local, common->buffer + pos);
		if (cnt <= 0)
		{
			DBG(1, common->debugLevel, ErrorF("Misbehaving parser returned %d\n",cnt));
			break;
		}
		pos += cnt;
	}
 
	if (pos)
	{
		/* if half a packet remains, move it down */
		if (pos < common->bufpos)
		{
			DBG(7, common->debugLevel, ErrorF("MOVE %d bytes\n", common->bufpos - pos));
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
}

/*****************************************************************************
 * xf86WcmDevClose --
 ****************************************************************************/

static void xf86WcmDevClose(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	DBG(4, priv->debugLevel, ErrorF("Wacom number of open devices = %d\n", common->fd_refs));

	if (local->fd >= 0)
	{
		local->fd = -1;
		if (!--common->fd_refs)
		{
			DBG(1, common->debugLevel, ErrorF("Closing device; uninitializing.\n"));
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

	DBG(2, priv->debugLevel, ErrorF("BEGIN xf86WcmProc dev=%p priv=%p "
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
		/* All devices must be opened here to initialize and
		 * register even a 'pad' which doesn't "SendCoreEvents"
		 */
		case DEVICE_INIT:
			priv->wcmDevOpenCount = 0;
			if (!xf86WcmDevOpen(pWcm))
			{
				DBG(1, priv->debugLevel, ErrorF("xf86WcmProc INIT FAILED\n"));
				return !Success;
			}
			priv->wcmDevOpenCount++;
			break; 

		case DEVICE_ON:
			if (!xf86WcmDevOpen(pWcm))
			{
				DBG(1, priv->debugLevel, ErrorF("xf86WcmProc ON FAILED\n"));
				return !Success;
			}
			priv->wcmDevOpenCount++;
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
			priv->wcmDevOpenCount = 0;
			break;

		default:
			ErrorF("wacom unsupported mode=%d\n", what);
			return !Success;
			break;
	} /* end switch */

	DBG(2, priv->debugLevel, ErrorF("END xf86WcmProc Success \n"));
	return Success;
}

/*****************************************************************************
 * xf86WcmDevConvert --
 *  Convert X & Y valuators so core events can be generated with 
 *  coordinates that are scaled and suitable for screen resolution.
 ****************************************************************************/

static Bool xf86WcmDevConvert(LocalDevicePtr local, int first, int num,
		int v0, int v1, int v2, int v3, int v4, int v5, int* x, int* y)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	double temp;
    
	DBG(6, priv->debugLevel, ErrorF("xf86WcmDevConvert v0=%d v1=%d on screen %d \n",
		 v0, v1, priv->currentScreen));

	if (first != 0 || num == 1) 
 		return FALSE;

	*x = 0;
	*y = 0;

	if (priv->flags & ABSOLUTE_FLAG)
	{
		int leftPadding = 0;
		int topPadding = 0;				

		v0 = v0 - priv->topX - priv->tvoffsetX;
		v1 = v1 - priv->topY - priv->tvoffsetY;

		if (priv->twinview == TV_NONE)
		{
			if (priv->screen_no == -1)
			{
				leftPadding = priv->screenTopX[priv->currentScreen];
				topPadding = priv->screenTopY[priv->currentScreen];				
			}
			*x = - leftPadding;
			*y = - topPadding;
		}
		else
		{
			*x = priv->screenTopX[priv->currentScreen];
			*y = priv->screenTopY[priv->currentScreen];
		}
	}
	temp = ((double)v0 * priv->factorX + 0.5);
	*x += temp;
	temp = ((double)v1 * priv->factorY + 0.5);
	*y += temp;

	DBG(6, priv->debugLevel, ErrorF("Wacom converted v0=%d v1=%d to x=%d y=%d\n", v0, v1, *x, *y));
	return TRUE;
}

/*****************************************************************************
 * xf86WcmDevReverseConvert --
 *  Convert X and Y to valuators in relative mode where the position of 
 *  the core pointer must be translated into device cootdinates before 
 *  the extension and core events are generated in Xserver.
 ****************************************************************************/

static Bool xf86WcmDevReverseConvert(LocalDevicePtr local, int x, int y,
		int* valuators)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	int i = 0;

	DBG(6, priv->debugLevel, ErrorF("xf86WcmDevReverseConvert x=%d y=%d \n", x, y));
	priv->currentSX = x;
	priv->currentSY = y;

	if (!(priv->flags & ABSOLUTE_FLAG))
	{
		if (!priv->devReverseCount)
		{
			valuators[0] = (((double)x / priv->factorX) + 0.5);
			valuators[1] = (((double)y / priv->factorY) + 0.5);

			/* reset valuators to report raw values */
			for (i=2; i<priv->naxes; i++)
				valuators[i] = 0;

			priv->devReverseCount = 1;
		}
		else
			priv->devReverseCount = 0;
	}
	DBG(6, priv->debugLevel, ErrorF("Wacom converted x=%d y=%d"
		" to v0=%d v1=%d v2=%d v3=%d v4=%d v5=%d\n", x, y,
		valuators[0], valuators[1], valuators[2], 
		valuators[3], valuators[4], valuators[5]));

	return TRUE;
}
