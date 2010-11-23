/*
 * Copyright 1995-2002 by Frederic Lepied, France. <Lepied@XFree86.org> 
 * Copyright 2002-2010 by Ping Cheng, Wacom. <pingc@wacom.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * This driver is currently able to handle USB Wacom IV and V, serial ISDV4,
 * and bluetooth protocols.
 *
 * Wacom V protocol work done by Raph Levien <raph@gtk.org> and
 * Frédéric Lepied <lepied@xfree86.org>.
 *
 * Modified for Linux USB by MATSUMURA Namihiko,
 * Daniel Egger, Germany. <egger@suse.de>,
 * Frederic Lepied <lepied@xfree86.org>,
 * Brion Vibber <brion@pobox.com>,
 * Aaron Optimizer Digulla <digulla@hepe.com>,
 * Jonathan Layes <jonathan@layes.com>,
 * John Joganic <jej@j-arkadia.com>,
 * Magnus Vigerlöf <Magnus.Vigerlof@ipbo.se>,
 * Peter Hutterer <peter.hutterer@redhat.com>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "xf86Wacom.h"
#include <xf86_OSproc.h>
#include <exevents.h>           /* Needed for InitValuator/Proximity stuff */

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
#include <xserver-properties.h>
#include <X11/extensions/XKB.h>
#include <xkbsrv.h>
#endif

static int wcmDevOpen(DeviceIntPtr pWcm);
static int wcmReady(InputInfoPtr pInfo);
static void wcmDevReadInput(InputInfoPtr pInfo);
static void wcmDevControlProc(DeviceIntPtr device, PtrCtrl* ctrl);
int wcmDevChangeControl(InputInfoPtr pInfo, xDeviceCtl * control);
static void wcmDevClose(InputInfoPtr pInfo);
static int wcmDevProc(DeviceIntPtr pWcm, int what);

WacomModule gWacomModule =
{
	NULL,           /* input driver pointer */

	/* device procedures */
	wcmDevOpen,
	wcmDevReadInput,
	wcmDevControlProc,
	wcmDevChangeControl,
	wcmDevClose,
	wcmDevProc,
	wcmDevSwitchMode,
};

static void wcmKbdLedCallback(DeviceIntPtr di, LedCtrl * lcp)
{
}

static void wcmKbdCtrlCallback(DeviceIntPtr di, KeybdCtrl* ctrl)
{
}

/*****************************************************************************
 * wcmDesktopSize --
 *   calculate the whole desktop size 
 ****************************************************************************/
static void wcmDesktopSize(InputInfoPtr pInfo)
{
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	int i = 0, minX = 0, minY = 0, maxX = 0, maxY = 0;

	wcmInitialScreens(pInfo);
	minX = priv->screenTopX[0];
	minY = priv->screenTopY[0];
	maxX = priv->screenBottomX[0];
	maxY = priv->screenBottomY[0];
	if (priv->numScreen != 1)
	{
		for (i = 1; i < priv->numScreen; i++)
		{
			if (priv->screenTopX[i] < minX)
				minX = priv->screenTopX[i];
			if (priv->screenTopY[i] < minY)
				minY = priv->screenTopY[i];
			if (priv->screenBottomX[i] > maxX)
				maxX = priv->screenBottomX[i];
			if (priv->screenBottomY[i] > maxY)
				maxY = priv->screenBottomY[i];
		}
	}
	priv->maxWidth = maxX - minX;
	priv->maxHeight = maxY - minY;
} 

static int wcmInitArea(InputInfoPtr pInfo)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomToolAreaPtr area = priv->toolarea, inlist;
	WacomCommonPtr common = priv->common;
	double screenRatio, tabletRatio;
	int bottomx = priv->maxX, bottomy = priv->maxY;

	DBG(10, priv, "\n");

	/* verify the box and initialize the area */
	if (priv->topX > bottomx)
		priv->topX = 0;

	if (priv->topY > bottomy)
		priv->topY = 0;

	if (priv->bottomX < priv->topX || !priv->bottomX)
		priv->bottomX = bottomx;

	if (priv->bottomY < priv->topY || !priv->bottomY)
		priv->bottomY = bottomy;

	area->topX = priv->topX;
	area->topY = priv->topY;
	area->bottomX = priv->bottomX;
	area->bottomY = priv->bottomY;

	if (priv->screen_no != -1 &&
		(priv->screen_no >= priv->numScreen || priv->screen_no < 0))
	{
		if (priv->screen_no != 1)
		{
			xf86Msg(X_ERROR, "%s: invalid screen number %d, resetting to default (-1) \n",
					pInfo->name, priv->screen_no);
			priv->screen_no = -1;
		}
	}

	/* need maxWidth and maxHeight for keepshape */
	wcmDesktopSize(pInfo);

	/* Maintain aspect ratio to the whole desktop
	 * May need to consider a specific screen in multimonitor settings
	 */
	if (priv->flags & KEEP_SHAPE_FLAG)
	{

		screenRatio = ((double)priv->maxWidth / (double)priv->maxHeight);
		tabletRatio = ((double)(bottomx - priv->topX) /
				(double)(bottomy - priv->topY));

		DBG(2, priv, "screenRatio = %.3g, "
			"tabletRatio = %.3g\n", screenRatio, tabletRatio);

		if (screenRatio > tabletRatio)
		{
			area->bottomX = priv->bottomX = bottomx;
			area->bottomY = priv->bottomY = (bottomy - priv->topY) *
				tabletRatio / screenRatio + priv->topY;
		}
		else
		{
			area->bottomX = priv->bottomX = (bottomx - priv->topX) *
				screenRatio / tabletRatio + priv->topX;
			area->bottomY = priv->bottomY = bottomy;
		}
	}
	/* end keep shape */ 

	inlist = priv->tool->arealist;

	/* The first one in the list is always valid */
	if (area != inlist && wcmAreaListOverlap(area, inlist))
	{
		inlist = priv->tool->arealist;

		/* remove this overlapped area from the list */
		for (; inlist; inlist=inlist->next)
		{
			if (inlist->next == area)
			{
				inlist->next = area->next;
				free(area);
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
			pInfo->name);
		return FALSE;
	}
	xf86Msg(X_PROBED, "%s: top X=%d top Y=%d "
			"bottom X=%d bottom Y=%d "
			"resol X=%d resol Y=%d\n",
			pInfo->name, priv->topX,
			priv->topY, priv->bottomX, priv->bottomY,
			priv->resolX, priv->resolY);
	return TRUE;
}

/*****************************************************************************
 * wcmVirtualTabletPadding(InputInfoPtr pInfo)
 ****************************************************************************/

void wcmVirtualTabletPadding(InputInfoPtr pInfo)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	int i;

	priv->leftPadding = 0;
	priv->topPadding = 0;

	if (!is_absolute(pInfo)) return;

	if ((priv->screen_no != -1) || (!priv->wcmMMonitor))
	{
		i = priv->currentScreen;

		priv->leftPadding = priv->bottomX - priv->topX;
		priv->topPadding = priv->bottomY - priv->topY;

		priv->leftPadding = (int)(((double)priv->screenTopX[i] * priv->leftPadding )
			/ ((double)(priv->screenBottomX[i] - priv->screenTopX[i])) + 0.5);

		priv->topPadding = (int)((double)(priv->screenTopY[i] * priv->topPadding)
			/ ((double)(priv->screenBottomY[i] - priv->screenTopY[i])) + 0.5);
	}
	DBG(10, priv, "x=%d y=%d \n", priv->leftPadding, priv->topPadding);
	return;
}

/*****************************************************************************
 * wcmVirtualTabletSize(InputInfoPtr pInfo)
 ****************************************************************************/

void wcmVirtualTabletSize(InputInfoPtr pInfo)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	int i, tabletSize;

	if (!is_absolute(pInfo))
	{
		priv->sizeX = priv->bottomX - priv->topX;
		priv->sizeY = priv->bottomY - priv->topY;
		return;
	}

	priv->sizeX = priv->bottomX - priv->topX;
	priv->sizeY = priv->bottomY - priv->topY;

	if ((priv->screen_no != -1) || (!priv->wcmMMonitor))
	{
		i = priv->currentScreen;

		tabletSize = priv->sizeX;
		priv->sizeX += (int)(((double)priv->screenTopX[i] * tabletSize)
			/ ((double)(priv->screenBottomX[i] - priv->screenTopX[i])) + 0.5);
		priv->sizeX += (int)((double)((priv->maxWidth - priv->screenBottomX[i])
			* tabletSize) / ((double)(priv->screenBottomX[i] - priv->screenTopX[i])) + 0.5);

		tabletSize = priv->sizeY;
		priv->sizeY += (int)((double)(priv->screenTopY[i] * tabletSize)
			/ ((double)(priv->screenBottomY[i] - priv->screenTopY[i])) + 0.5);
		priv->sizeY += (int)((double)((priv->maxHeight - priv->screenBottomY[i])
			* tabletSize) / ((double)(priv->screenBottomY[i] - priv->screenTopY[i])) + 0.5);
	}
	DBG(10, priv, "x=%d y=%d \n", priv->sizeX, priv->sizeY);
	return;
}

/*****************************************************************************
 * wcmInitialCoordinates
 ****************************************************************************/

void wcmInitialCoordinates(InputInfoPtr pInfo, int axis)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	int topx = 0, topy = 0, resolution_x, resolution_y;
	int bottomx = priv->maxX, bottomy = priv->maxY;

	wcmMappingFactor(pInfo);

	if (is_absolute(pInfo))
	{
		topx = priv->topX;
		topy = priv->topY;
		bottomx = priv->sizeX + priv->topX;
		bottomy = priv->sizeY + priv->topY;
	}
	resolution_x = priv->resolX;
	resolution_y = priv->resolY;

	switch(axis)
	{
		case 0:
			InitValuatorAxisStruct(pInfo->dev, 0,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
					XIGetKnownProperty(AXIS_LABEL_PROP_ABS_X),
#endif
					topx, bottomx,
					resolution_x, 0, resolution_x
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 12
					, Absolute
#endif
					);
			break;
		case 1:
			InitValuatorAxisStruct(pInfo->dev, 1,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
					XIGetKnownProperty(AXIS_LABEL_PROP_ABS_Y),
#endif
					topy, bottomy,
					resolution_y, 0, resolution_y
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 12
					, Absolute
#endif
					);
			break;
		default:
			xf86Msg(X_ERROR, "%s: Cannot initialize axis %d.\n", pInfo->name, axis);
			break;
	}

	return;
}

/*****************************************************************************
 * wcmInitialToolSize --
 *    Initialize logical size and resolution for individual tool.
 ****************************************************************************/

static void wcmInitialToolSize(InputInfoPtr pInfo)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;
	WacomToolPtr toollist = common->wcmTool;
	WacomToolAreaPtr arealist;

	/* assign max and resolution here since we don't get them during
	 * the configuration stage */
	if (IsTouch(priv))
	{
		priv->maxX = common->wcmMaxTouchX;
		priv->maxY = common->wcmMaxTouchY;
		priv->resolX = common->wcmTouchResolX;
		priv->resolY = common->wcmTouchResolY;
	}
	else
	{
		priv->maxX = common->wcmMaxX;
		priv->maxY = common->wcmMaxY;
		priv->resolX = common->wcmResolX;
		priv->resolY = common->wcmResolY;
	}

	for (; toollist; toollist=toollist->next)
	{
		arealist = toollist->arealist;
		for (; arealist; arealist=arealist->next)
		{
			if (!arealist->bottomX) 
				arealist->bottomX = priv->maxX;
			if (!arealist->bottomY)
				arealist->bottomY = priv->maxY;
		}
	}

	return;
}

/*****************************************************************************
 * wcmDevInit --
 *    Set up the device's buttons, axes and keys
 ****************************************************************************/

static int wcmDevInit(DeviceIntPtr pWcm)
{
	InputInfoPtr pInfo = (InputInfoPtr)pWcm->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;
	unsigned char butmap[WCM_MAX_BUTTONS+1];
	int nbaxes, nbbuttons, nbkeys;
	int loop;
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
        Atom btn_labels[WCM_MAX_BUTTONS] = {0};
        Atom axis_labels[MAX_VALUATORS] = {0};
#endif

	/* Detect tablet configuration, if possible */
	if (priv->common->wcmModel->DetectConfig)
		priv->common->wcmModel->DetectConfig (pInfo);

	nbaxes = priv->naxes;       /* X, Y, Pressure, Tilt-X, Tilt-Y, Wheel */
	nbbuttons = priv->nbuttons; /* Use actual number of buttons, if possible */

	/* if more than 3 buttons, offset by the four scroll buttons,
	 * otherwise, alloc 7 buttons for scroll wheel. */
	nbbuttons = (nbbuttons > 3) ? nbbuttons + 4 : 7;

	/* make sure nbbuttons stays in the range */
	if (nbbuttons > WCM_MAX_BUTTONS)
		nbbuttons = WCM_MAX_BUTTONS;

	nbkeys = nbbuttons;         /* Same number of keys since any button may be 
	                             * configured as an either mouse button or key */

	if (!nbbuttons)
		nbbuttons = nbkeys = 1;	    /* Xserver 1.5 or later crashes when 
			            	     * nbbuttons = 0 while sending a beep 
			             	     * This is only a workaround. 
				     	     */

	DBG(10, priv,
		"(%s) %d buttons, %d keys, %d axes\n",
		pInfo->type_name,
		nbbuttons, nbkeys, nbaxes);

	for(loop=1; loop<=nbbuttons; loop++)
		butmap[loop] = loop;

	/* FIXME: button labels would be nice */
	if (InitButtonClassDeviceStruct(pInfo->dev, nbbuttons,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
					btn_labels,
#endif
					butmap) == FALSE)
	{
		xf86Msg(X_ERROR, "%s: unable to allocate Button class device\n", pInfo->name);
		return FALSE;
	}

	if (InitFocusClassDeviceStruct(pInfo->dev) == FALSE)
	{
		xf86Msg(X_ERROR, "%s: unable to init Focus class device\n", pInfo->name);
		return FALSE;
	}

	if (InitPtrFeedbackClassDeviceStruct(pInfo->dev,
		wcmDevControlProc) == FALSE)
	{
		xf86Msg(X_ERROR, "%s: unable to init ptr feedback\n", pInfo->name);
		return FALSE;
	}

	if (InitProximityClassDeviceStruct(pInfo->dev) == FALSE)
	{
			xf86Msg(X_ERROR, "%s: unable to init proximity class device\n", pInfo->name);
			return FALSE;
	}

	if (!nbaxes || nbaxes > 6)
		nbaxes = priv->naxes = 6;

	/* axis_labels is just zeros, we set up each valuator with the
	 * correct property later */
	if (InitValuatorClassDeviceStruct(pInfo->dev, nbaxes,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
					  axis_labels,
#endif
					  GetMotionHistorySize(),
					  (is_absolute(pInfo) ?  Absolute : Relative) | OutOfProximity) == FALSE)
	{
		xf86Msg(X_ERROR, "%s: unable to allocate Valuator class device\n", pInfo->name);
		return FALSE;
	}


#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
	if (!InitKeyboardDeviceStruct(pInfo->dev, NULL, NULL, wcmKbdCtrlCallback)) {
		xf86Msg(X_ERROR, "%s: unable to init kbd device struct\n", pInfo->name);
		return FALSE;
	}
#endif
	if(InitLedFeedbackClassDeviceStruct (pInfo->dev, wcmKbdLedCallback) == FALSE) {
		xf86Msg(X_ERROR, "%s: unable to init led feedback device struct\n", pInfo->name);
		return FALSE;
	}

	if (!IsPad(priv))
	{
		wcmInitialToolSize(pInfo);

		if (wcmInitArea(pInfo) == FALSE)
		{
			return FALSE;
		}

		wcmInitialCoordinates(priv->pInfo, 0);
		wcmInitialCoordinates(priv->pInfo, 1);

		/* Rotation rotates the Max X and Y */
		wcmRotateTablet(pInfo, common->wcmRotate);

		/* pressure normalized to FILTER_PRESSURE_RES */
		InitValuatorAxisStruct(pInfo->dev, 2,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
				XIGetKnownProperty(AXIS_LABEL_PROP_ABS_PRESSURE),
#endif
				0, FILTER_PRESSURE_RES, 1, 1, 1
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 12
				, Absolute
#endif
				);

	} else {
		/* The pad doesn't have a pressure axis, so initialise third
		 * axis as unknown relative axis on the pad. This way, we
		 * can leave the strip/abswheel axes on later axes and don't
		 * run the danger of clients misinterpreting the axis info
		 */
		InitValuatorAxisStruct(pInfo->dev, 2,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
				None,
#endif
				-1, -1, 0, -1, -1
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 12
				, Relative
#endif
				);
	}

	if (IsCursor(priv))
	{
		/* z-rot and throttle */
		InitValuatorAxisStruct(pInfo->dev, 3,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
		XIGetKnownProperty(AXIS_LABEL_PROP_ABS_RZ),
#endif
		-900, 899, 1, 1, 1
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 12
		, Absolute
#endif
		);
		InitValuatorAxisStruct(pInfo->dev, 4,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
		XIGetKnownProperty(AXIS_LABEL_PROP_ABS_THROTTLE),
#endif
		-1023, 1023, 1, 1, 1
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 12
		, Absolute
#endif
		);
	}
	else if (IsPad(priv))
	{
		/* strip-x and strip-y */
		if (TabletHasFeature(common, WCM_STRIP))
		{
			InitValuatorAxisStruct(pInfo->dev, 3,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
				0, /* XXX what is this axis?*/
#endif
				0, common->wcmMaxStripX, 1, 1, 1
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 12
				, Absolute
#endif
				);
			InitValuatorAxisStruct(pInfo->dev, 4,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
				0, /* XXX what is this axis?*/
#endif
				0, common->wcmMaxStripY, 1, 1, 1
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 12
				, Absolute
#endif
				);
		}
	}
	else
	{
		/* tilt-x and tilt-y */
		InitValuatorAxisStruct(pInfo->dev, 3,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
				XIGetKnownProperty(AXIS_LABEL_PROP_ABS_TILT_X),
#endif
				-64, 63, 1, 1, 1
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 12
				, Absolute
#endif
				);
		InitValuatorAxisStruct(pInfo->dev, 4,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
				XIGetKnownProperty(AXIS_LABEL_PROP_ABS_TILT_Y),
#endif
				-64, 63, 1, 1, 1
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 12
				, Absolute
#endif
				);
	}

	if (IsStylus(priv))
	{
		int maxRotation = MAX_ROTATION_RANGE + MIN_ROTATION - 1;
		/* Art Marker Pen rotation or Airbrush absolute Wheel */
		InitValuatorAxisStruct(pInfo->dev, 5,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
				XIGetKnownProperty(AXIS_LABEL_PROP_ABS_WHEEL),
#endif
				MIN_ROTATION, maxRotation, 1, 1, 1
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 12
				, Absolute
#endif
				);
	}
	else if ((TabletHasFeature(common, WCM_RING)) && IsPad(priv))
		/* Touch ring */
		InitValuatorAxisStruct(pInfo->dev, 5,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
				XIGetKnownProperty(AXIS_LABEL_PROP_ABS_WHEEL),
#endif
				0, 71, 1, 1, 1
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 12
				, Absolute
#endif
				);

	if (IsTouch(priv))
	{
		/* hard prox out */
		priv->hardProx = 0;
	}

	InitWcmDeviceProperties(pInfo);
	XIRegisterPropertyHandler(pInfo->dev, wcmSetProperty, NULL, wcmDeleteProperty);

	return TRUE;
}

Bool wcmIsWacomDevice (char* fname)
{
	int fd = -1;
	struct input_id id;

	SYSCALL(fd = open(fname, O_RDONLY));
	if (fd < 0)
		return FALSE;

	if (ioctl(fd, EVIOCGID, &id) < 0)
	{
		SYSCALL(close(fd));
		return FALSE;
	}

	SYSCALL(close(fd));

	switch(id.vendor)
	{
		case WACOM_VENDOR_ID:
		case WALTOP_VENDOR_ID:
		case HANWANG_VENDOR_ID:
			return TRUE;
		default:
			break;
	}
	return FALSE;
}

/*****************************************************************************
 * wcmEventAutoDevProbe -- Probe for right input device
 ****************************************************************************/
#define DEV_INPUT_EVENT "/dev/input/event%d"
#define EVDEV_MINORS    32
char *wcmEventAutoDevProbe (InputInfoPtr pInfo)
{
	/* We are trying to find the right eventX device */
	int i, wait = 0;
	const int max_wait = 2000;

	/* If device is not available after Resume, wait some ms */
	while (wait <= max_wait) 
	{
		for (i = 0; i < EVDEV_MINORS; i++) 
		{
			char fname[64];
			Bool is_wacom;

			sprintf(fname, DEV_INPUT_EVENT, i);
			is_wacom = wcmIsWacomDevice(fname);
			if (is_wacom) 
			{
				xf86Msg(X_PROBED, "%s: probed device is %s (waited %d msec)\n",
					pInfo->name, fname, wait);
				xf86ReplaceStrOption(pInfo->options, "Device", fname);

				/* this assumes there is only one Wacom device on the system */
				return xf86FindOptionValue(pInfo->options, "Device");
			}
		}
		wait += 100;
		xf86Msg(X_ERROR, "%s: waiting 100 msec (total %dms) for device to become ready\n", pInfo->name, wait);
		usleep(100*1000);
	}
	xf86Msg(X_ERROR, "%s: no Wacom event device found (checked %d nodes, waited %d msec)\n",
		pInfo->name, i + 1, wait);
	xf86Msg(X_ERROR, "%s: unable to probe device\n", pInfo->name);
	return FALSE;
}

/*****************************************************************************
 * wcmOpen --
 ****************************************************************************/

Bool wcmOpen(InputInfoPtr pInfo)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;

	DBG(1, priv, "opening device file\n");

	pInfo->fd = xf86OpenSerial(pInfo->options);
	if (pInfo->fd < 0)
	{
		xf86Msg(X_ERROR, "%s: Error opening %s (%s)\n", pInfo->name,
			common->device_path, strerror(errno));
		return !Success;
	}

	return Success;
}

/*****************************************************************************
 * wcmDevOpen --
 *    Open the physical device and init information structs.
 ****************************************************************************/

static int wcmDevOpen(DeviceIntPtr pWcm)
{
	InputInfoPtr pInfo = (InputInfoPtr)pWcm->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;
	WacomModelPtr model = common->wcmModel;
	struct stat st;

	DBG(10, priv, "\n");

	/* open file, if not already open */
	if (common->fd_refs == 0)
	{
		if ((wcmOpen (pInfo) != Success) || !common->device_path)
		{
			DBG(1, priv, "Failed to open device (fd=%d)\n", pInfo->fd);
			if (pInfo->fd >= 0)
			{
				DBG(1, priv, "Closing device\n");
				xf86CloseSerial(pInfo->fd);
			}
			pInfo->fd = -1;
			return FALSE;
		}

		if (fstat(pInfo->fd, &st) == -1)
		{
			/* can not access major/minor */
			DBG(1, priv, "stat failed (%s).\n", strerror(errno));

			/* older systems don't support the required ioctl.
			 * So, we have to let it pass */
			common->min_maj = 0;
		}
		else
			common->min_maj = st.st_rdev;
		common->fd = pInfo->fd;
		common->fd_refs = 1;
	}

	/* Grab the common descriptor, if it's available */
	if (pInfo->fd < 0)
	{
		pInfo->fd = common->fd;
		common->fd_refs++;
	}

	/* start the tablet data */
	if (model->Start && (model->Start(pInfo) != Success))
		return !Success;

	return TRUE;
}

static int wcmReady(InputInfoPtr pInfo)
{
#ifdef DEBUG
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
#endif
	int n = xf86WaitForInput(pInfo->fd, 0);
	DBG(10, priv, "%d numbers of data\n", n);

	if (n >= 0) return n ? 1 : 0;
	xf86Msg(X_ERROR, "%s: select error: %s\n", pInfo->name, strerror(errno));
	return 0;
}

/*****************************************************************************
 * wcmDevReadInput --
 *   Read the device on IO signal
 ****************************************************************************/

static void wcmDevReadInput(InputInfoPtr pInfo)
{
	int loop=0;
	#define MAX_READ_LOOPS 10

	/* move data until we exhaust the device */
	for (loop=0; loop < MAX_READ_LOOPS; ++loop)
	{
		/* verify that there is still data in pipe */
		if (!wcmReady(pInfo)) break;

		/* dispatch */
		wcmReadPacket(pInfo);
	}

#ifdef DEBUG
	/* report how well we're doing */
	if (loop > 0)
	{
		WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;

		if (loop >= MAX_READ_LOOPS)
			DBG(1, priv, "Can't keep up!!!\n");
		else
			DBG(10, priv, "Read (%d)\n",loop);
	}
#endif
}

void wcmReadPacket(InputInfoPtr pInfo)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;
	int len, pos, cnt, remaining;

	DBG(10, common, "fd=%d\n", pInfo->fd);

	remaining = sizeof(common->buffer) - common->bufpos;

	DBG(1, common, "pos=%d remaining=%d\n", common->bufpos, remaining);

	/* fill buffer with as much data as we can handle */
	len = xf86ReadSerial(pInfo->fd,
		common->buffer + common->bufpos, remaining);

	if (len <= 0)
	{
		/* for all other errors, hope that the hotplugging code will
		 * remove the device */
		if (errno != EAGAIN && errno != EINTR)
			xf86Msg(X_ERROR, "%s: Error reading wacom device : %s\n", pInfo->name, strerror(errno));
		return;
	}

	/* account for new data */
	common->bufpos += len;
	DBG(10, common, "buffer has %d bytes\n", common->bufpos);

	len = common->bufpos;
	pos = 0;

	while (len > 0)
	{
		/* parse packet */
		cnt = common->wcmModel->Parse(pInfo, common->buffer + pos, len);
		if (cnt <= 0)
		{
			if (cnt < 0)
				DBG(1, common, "Misbehaving parser returned %d\n",cnt);
			break;
		}
		pos += cnt;
		len -= cnt;
	}

	/* if half a packet remains, move it down */
	if (len)
	{
		DBG(7, common, "MOVE %d bytes\n", common->bufpos - pos);
		memmove(common->buffer,common->buffer+pos, len);
	}

	common->bufpos = len;
}

int wcmDevChangeControl(InputInfoPtr pInfo, xDeviceCtl * control)
{
#ifdef DEBUG
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	DBG(3, priv, "\n");
#endif
	return Success;
}

/*****************************************************************************
 * wcmDevControlProc --
 ****************************************************************************/

static void wcmDevControlProc(DeviceIntPtr device, PtrCtrl* ctrl)
{
#ifdef DEBUG
	InputInfoPtr pInfo = (InputInfoPtr)device->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;

	DBG(4, priv, "called\n");
#endif
	return;
}

/*****************************************************************************
 * wcmDevClose --
 ****************************************************************************/

static void wcmDevClose(InputInfoPtr pInfo)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;

	DBG(4, priv, "Wacom number of open devices = %d\n", common->fd_refs);

	if (pInfo->fd >= 0)
	{
		pInfo->fd = -1;
		if (!--common->fd_refs)
		{
			DBG(1, common, "Closing device; uninitializing.\n");
			xf86CloseSerial (common->fd);
		}
	}
}

/*****************************************************************************
 * wcmDevProc --
 *   Handle the initialization, etc. of a wacom tablet. Called by the server
 *   once with DEVICE_INIT when the device becomes available, then
 *   DEVICE_ON/DEVICE_OFF possibly multiple times as the device is enabled
 *   and disabled. DEVICE_CLOSE is called before removal of the device.
 ****************************************************************************/

static int wcmDevProc(DeviceIntPtr pWcm, int what)
{
	InputInfoPtr pInfo = (InputInfoPtr)pWcm->public.devicePrivate;
#ifdef DEBUG
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
#endif
	Status rc = !Success;

	DBG(2, priv, "BEGIN dev=%p priv=%p "
			"type=%s flags=%d fd=%d what=%s\n",
			(void *)pWcm, (void *)priv,
			pInfo->type_name,
			priv->flags, pInfo ? pInfo->fd : -1,
			(what == DEVICE_INIT) ? "INIT" :
			(what == DEVICE_OFF) ? "OFF" :
			(what == DEVICE_ON) ? "ON" :
			(what == DEVICE_CLOSE) ? "CLOSE" : "???");

	switch (what)
	{
		case DEVICE_INIT:
			if (!wcmDevInit(pWcm))
				goto out;
			break;

		case DEVICE_ON:
			if (!wcmDevOpen(pWcm))
				goto out;
			xf86AddEnabledDevice(pInfo);
			pWcm->public.on = TRUE;
			break;

		case DEVICE_OFF:
		case DEVICE_CLOSE:
			if (pInfo->fd >= 0)
			{
				xf86RemoveEnabledDevice(pInfo);
				wcmDevClose(pInfo);
			}
			pWcm->public.on = FALSE;
			break;

		default:
			xf86Msg(X_ERROR, "%s: invalid mode=%d. This is an X server bug.\n",
				pInfo->name, what);
			goto out;
	} /* end switch */

	rc = Success;

out:
	if (rc != Success)
		DBG(1, priv, "Failed during %d\n", what);
	return rc;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
