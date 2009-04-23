/*
 * Copyright 1995-2002 by Frederic Lepied, France. <Lepied@XFree86.org>
 * Copyright 2002-2009 by Ping Cheng, Wacom. <pingc@wacom.com>
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

#include "xf86Wacom.h"
#include "wcmFilter.h"

/*****************************************************************************
 * xf86WcmAllocate --
 ****************************************************************************/

LocalDevicePtr xf86WcmAllocate(char* name, int flag)
{
	LocalDevicePtr local;
	WacomDevicePtr priv;
	WacomCommonPtr common;
	int i, j;
	WacomToolPtr     tool;
	WacomToolAreaPtr area;

	priv = (WacomDevicePtr) xcalloc(1, sizeof(WacomDeviceRec));
	if (!priv)
		return NULL;

	common = (WacomCommonPtr) xcalloc(1, sizeof(WacomCommonRec));
	if (!common)
	{
		xfree(priv);
		return NULL;
	}

	tool = (WacomToolPtr) xcalloc(1, sizeof(WacomTool));
	if(!tool)
	{
		xfree(priv);
		xfree(common);
		return NULL;
	}

	area = (WacomToolAreaPtr) xcalloc(1, sizeof(WacomToolArea));
	if(!area)
	{
		xfree(tool);
		xfree(priv);
		xfree(common);
		return NULL;
	}

	local = xf86AllocateInput(gWacomModule.wcmDrv, 0);
	if (!local)
	{
		xfree(area);
		xfree(tool);
		xfree(priv);
		xfree(common);
		return NULL;
	}

	local->name = name;
	local->flags = 0;
	local->device_control = gWacomModule.DevProc;
	local->read_input = gWacomModule.DevReadInput;
	local->control_proc = gWacomModule.DevChangeControl;
	local->close_proc = gWacomModule.DevClose;
	local->switch_mode = gWacomModule.DevSwitchMode;
	local->conversion_proc = gWacomModule.DevConvert;
	local->reverse_conversion_proc = gWacomModule.DevReverseConvert;
	local->fd = -1;
	local->atom = 0;
	local->dev = NULL;
	local->private = priv;
	local->private_flags = 0;
#if WCM_XINPUTABI_MAJOR == 0
	local->history_size  = 0;
#endif
	local->old_x = -1;
	local->old_y = -1;

	priv->next = NULL;
	priv->local = local;
	priv->flags = flag;          /* various flags (device type, absolute, first touch...) */
	priv->oldX = 0;             /* previous X position */
	priv->oldY = 0;             /* previous Y position */
	priv->oldZ = 0;             /* previous pressure */
	priv->oldTiltX = 0;         /* previous tilt in x direction */
	priv->oldTiltY = 0;         /* previous tilt in y direction */
	priv->oldStripX = 0;	    /* previous left strip value */
	priv->oldStripY = 0;	    /* previous right strip value */
	priv->oldButtons = 0;        /* previous buttons state */
	priv->oldWheel = 0;          /* previous wheel */
	priv->topX = 0;              /* X top */
	priv->topY = 0;              /* Y top */
	priv->bottomX = 0;           /* X bottom */
	priv->bottomY = 0;           /* Y bottom */
	priv->wcmMaxX = 0;           /* max tool logical X value */
	priv->wcmMaxY = 0;           /* max tool logical Y value */
	priv->wcmResolX = 0;         /* tool X resolution in points/inch */
	priv->wcmResolY = 0;         /* tool Y resolution in points/inch */
	priv->sizeX = 0;	     /* active X size */
	priv->sizeY = 0;	     /* active Y size */
	priv->factorX = 0.0;         /* X factor */
	priv->factorY = 0.0;         /* Y factor */
	priv->common = common;       /* common info pointer */
	priv->oldProximity = 0;      /* previous proximity */
	priv->hardProx = 1;	     /* previous hardware proximity */
	priv->old_serial = 0;	     /* last active tool's serial */
	priv->old_device_id = IsStylus(priv) ? STYLUS_DEVICE_ID :
		(IsEraser(priv) ? ERASER_DEVICE_ID : 
		(IsCursor(priv) ? CURSOR_DEVICE_ID : 
		(IsTouch(priv) ? TOUCH_DEVICE_ID :
		PAD_DEVICE_ID)));

	priv->devReverseCount = 0;   /* flag for relative Reverse call */
	priv->serial = 0;            /* serial number */
	priv->screen_no = -1;        /* associated screen */
	priv->speed = DEFAULT_SPEED; /* rel. mode speed */
	priv->accel = 0;	     /* rel. mode acceleration */
	priv->nPressCtrl [0] = 0;    /* pressure curve x0 */
	priv->nPressCtrl [1] = 0;    /* pressure curve y0 */
	priv->nPressCtrl [2] = 100;  /* pressure curve x1 */
	priv->nPressCtrl [3] = 100;  /* pressure curve y1 */

	/* Default button and expresskey values */
	for (i=0; i<MAX_BUTTONS; i++)
		priv->button[i] = (AC_BUTTON | (i + 1));

	for (i=0; i<MAX_BUTTONS; i++)
		for (j=0; j<256; j++)
			priv->keys[i][j] = 0;

	priv->nbuttons = MAX_BUTTONS;		/* Default number of buttons */
	priv->relup = 5;			/* Default relative wheel up event */
	priv->reldn = 4;			/* Default relative wheel down event */
	
	priv->wheelup = IsPad (priv) ? 4 : 0;	/* Default absolute wheel up event */
	priv->wheeldn = IsPad (priv) ? 5 : 0;	/* Default absolute wheel down event */
	priv->striplup = 4;			/* Default left strip up event */
	priv->stripldn = 5;			/* Default left strip down event */
	priv->striprup = 4;			/* Default right strip up event */
	priv->striprdn = 5;			/* Default right strip down event */
	priv->naxes = 6;			/* Default number of axes */
	priv->debugLevel = 0;			/* debug level */
	priv->numScreen = screenInfo.numScreens; /* configured screens count */
	priv->currentScreen = -1;                /* current screen in display */

	priv->maxWidth = 0;			/* max active screen width */
	priv->maxHeight = 0;			/* max active screen height */
	priv->leftPadding = 0;			/* left padding for virtual tablet */
	priv->topPadding = 0;			/* top padding for virtual tablet */
	priv->twinview = TV_NONE;		/* not using twinview gfx */
	priv->tvoffsetX = 0;			/* none X edge offset for TwinView setup */
	priv->tvoffsetY = 0;			/* none Y edge offset for TwinView setup */
	for (i=0; i<4; i++)
		priv->tvResolution[i] = 0;	/* unconfigured twinview resolution */
	priv->wcmMMonitor = 1;			/* enabled (=1) to support multi-monitor desktop. */
						/* disabled (=0) when user doesn't want to move the */
						/* cursor from one screen to another screen */

	/* JEJ - throttle sampling code */
	priv->throttleValue = 0;
	priv->throttleStart = 0;
	priv->throttleLimit = -1;
	
	common->wcmDevice = "";                  /* device file name */
	common->wcmFlags = RAW_FILTERING_FLAG;   /* various flags */
	common->wcmDevices = priv;
	common->npadkeys = 0;		   /* Default number of pad keys */
	common->wcmProtocolLevel = 4;      /* protocol level */
	common->wcmThreshold = 0;       /* unconfigured threshold */
	common->wcmLinkSpeed = 9600;    /* serial link speed */
	common->wcmISDV4Speed = 38400;  /* serial ISDV4 link speed */
	common->debugLevel = 0;         /* shared debug level can only 
					 * be changed though xsetwacom */

	common->wcmDevCls = &gWacomSerialDevice; /* device-specific functions */
	common->wcmModel = NULL;                 /* model-specific functions */
	common->wcmEraserID = 0;	 /* eraser id associated with the stylus */
	common->wcmTPCButtonDefault = 0; /* default Tablet PC button support is off */
	common->wcmTPCButton = 
		common->wcmTPCButtonDefault; /* set Tablet PC button on/off */
	common->wcmTouch = 0;              /* touch is disabled */
	common->wcmTouchDefault = 0; 	   /* default to disable when touch isn't supported */
	common->wcmCapacity = -1;          /* Capacity is disabled */
	common->wcmCapacityDefault = -1;    /* default to -1 when capacity isn't supported */
					   /* 3 when capacity is supported */
	common->wcmRotate = ROTATE_NONE;   /* default tablet rotation to off */
	common->wcmMaxX = 0;               /* max digitizer logical X value */
	common->wcmMaxY = 0;               /* max digitizer logical Y value */
	common->wcmMaxTouchX = 1024;       /* max touch X value */
	common->wcmMaxTouchY = 1024;       /* max touch Y value */
        common->wcmMaxZ = 0;               /* max Z value */
        common->wcmMaxCapacity = 0;        /* max capacity value */
 	common->wcmMaxDist = 0;            /* max distance value */
	common->wcmResolX = 0;             /* digitizer X resolution in points/inch */
	common->wcmResolY = 0;             /* digitizer Y resolution in points/inch */
	common->wcmTouchResolX = 0;        /* touch X resolution in points/inch */
	common->wcmTouchResolY = 0;        /* touch Y resolution in points/inch */
	common->wcmMaxStripX = 4096;       /* Max fingerstrip X */
	common->wcmMaxStripY = 4096;       /* Max fingerstrip Y */
	common->wcmMaxtiltX = 128;	   /* Max tilt in X directory */
	common->wcmMaxtiltY = 128;	   /* Max tilt in Y directory */
	common->wcmMaxCursorDist = 0;	/* Max distance received so far */
	common->wcmCursorProxoutDist = 0;
			/* Max mouse distance for proxy-out max/256 units */
	common->wcmCursorProxoutDistDefault = PROXOUT_INTUOS_DISTANCE; 
			/* default to Intuos */
	common->wcmSuppress = DEFAULT_SUPPRESS;    
			/* transmit position if increment is superior */
	common->wcmRawSample = DEFAULT_SAMPLES;    
			/* number of raw data to be used to for filtering */

	/* tool */
	priv->tool = tool;
	common->wcmTool = tool;
	tool->next = NULL;          /* next tool in list */
	tool->typeid = DEVICE_ID(flag); /* tool type (stylus/touch/eraser/cursor/pad) */
	tool->serial = 0;           /* serial id */
	tool->current = NULL;       /* current area in-prox */
	tool->arealist = area;      /* list of defined areas */
	/* tool area */
	priv->toolarea = area;
	area->next = NULL;    /* next area in list */
	area->topX = 0;       /* X top */
	area->topY = 0;       /* Y top */
	area->bottomX = 0;    /* X bottom */
	area->bottomY = 0;    /* Y bottom */
	area->device = local; /* associated WacomDevice */

	return local;
}

/* xf86WcmAllocateStylus */

LocalDevicePtr xf86WcmAllocateStylus(void)
{
	LocalDevicePtr local = xf86WcmAllocate(XI_STYLUS, ABSOLUTE_FLAG|STYLUS_ID);

	if (local)
		local->type_name = "Wacom Stylus";

	return local;
}

/* xf86WcmAllocateTouch */

LocalDevicePtr xf86WcmAllocateTouch(void)
{
	LocalDevicePtr local = xf86WcmAllocate(XI_TOUCH, ABSOLUTE_FLAG|TOUCH_ID);

	if (local)
		local->type_name = "Wacom Touch";

	return local;
}

/* xf86WcmAllocateCursor */

LocalDevicePtr xf86WcmAllocateCursor(void)
{
	LocalDevicePtr local = xf86WcmAllocate(XI_CURSOR, CURSOR_ID);

	if (local)
		local->type_name = "Wacom Cursor";

	return local;
}

/* xf86WcmAllocateEraser */

LocalDevicePtr xf86WcmAllocateEraser(void)
{
	LocalDevicePtr local = xf86WcmAllocate(XI_ERASER, ABSOLUTE_FLAG|ERASER_ID);

	if (local)
		local->type_name = "Wacom Eraser";

	return local;
}

/* xf86WcmAllocatePad */

LocalDevicePtr xf86WcmAllocatePad(void)
{
	LocalDevicePtr local = xf86WcmAllocate(XI_PAD, PAD_ID);

	if (local)
		local->type_name = "Wacom Pad";

	return local;
}

/* xf86WcmPointInArea - check whether the point is within the area */

Bool xf86WcmPointInArea(WacomToolAreaPtr area, int x, int y)
{
	if (area->topX <= x && x <= area->bottomX &&
	    area->topY <= y && y <= area->bottomY)
		return 1;
	return 0;
}

/* xf86WcmAreasOverlap - check if two areas are overlapping */

static Bool xf86WcmAreasOverlap(WacomToolAreaPtr area1, WacomToolAreaPtr area2)
{
	if (xf86WcmPointInArea(area1, area2->topX, area2->topY) ||
	    xf86WcmPointInArea(area1, area2->topX, area2->bottomY) ||
	    xf86WcmPointInArea(area1, area2->bottomX, area2->topY) ||
	    xf86WcmPointInArea(area1, area2->bottomX, area2->bottomY))
		return 1;
	if (xf86WcmPointInArea(area2, area1->topX, area1->topY) ||
	    xf86WcmPointInArea(area2, area1->topX, area1->bottomY) ||
	    xf86WcmPointInArea(area2, area1->bottomX, area1->topY) ||
	    xf86WcmPointInArea(area2, area1->bottomX, area1->bottomY))
	        return 1;
	return 0;
}

/* xf86WcmAreaListOverlaps - check if the area overlaps any area in the list */
Bool xf86WcmAreaListOverlap(WacomToolAreaPtr area, WacomToolAreaPtr list)
{
	for (; list; list=list->next)	
		if (area != list && xf86WcmAreasOverlap(list, area))
			return 1;
	return 0;
}

/* 
 * Be sure to set vmin appropriately for your device's protocol. You want to
 * read a full packet before returning
 *
 * JEJ - Actually, anything other than 1 is probably a bad idea since packet
 * errors can occur.  When that happens, bytes are read individually until it
 * starts making sense again.
 */

static const char *default_options[] =
{
	"BaudRate",    "9600",
	"StopBits",    "1",
	"DataBits",    "8",
	"Parity",      "None",
	"Vmin",        "1",
	"Vtime",       "10",
	"FlowControl", "Xoff",
	NULL
};

/* xf86WcmUninit - called when the device is no longer needed. */

static void xf86WcmUninit(InputDriverPtr drv, LocalDevicePtr local, int flags)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
    
	DBG(1, priv->debugLevel, ErrorF("xf86WcmUninit\n"));

#ifndef WCM_XORG_XSERVER_1_4
	gWacomModule.DevProc(local->dev, DEVICE_OFF);
#endif

	/* free pressure curve */
	if (priv->pPressCurve)
		xfree(priv->pPressCurve);
    
	/* free priv here otherwise X server 1.6 or later crashes 
	 */
	xfree(priv);
	local->private = NULL;

	xf86DeleteInput(local, 0);    
}

/* xf86WcmMatchDevice - locate matching device and merge common structure */

static Bool xf86WcmMatchDevice(LocalDevicePtr pMatch, LocalDevicePtr pLocal)
{
	WacomDevicePtr privMatch = (WacomDevicePtr)pMatch->private;
	WacomDevicePtr priv = (WacomDevicePtr)pLocal->private;
	WacomCommonPtr common = priv->common;
	char * type;

	if ((pLocal != pMatch) &&
		(pMatch->device_control == gWacomModule.DevProc) &&
		!strcmp(privMatch->common->wcmDevice, common->wcmDevice))
	{
		DBG(2, priv->debugLevel, ErrorF(
			"xf86WcmInit wacom port share between"
			" %s and %s\n", pLocal->name, pMatch->name));
		type = xf86FindOptionValue(pMatch->options, "Type");
		if ( type && (strstr(type, "eraser")) )
			privMatch->common->wcmEraserID=pMatch->name;
		else
		{
			type = xf86FindOptionValue(pLocal->options, "Type");
			if ( type && (strstr(type, "eraser")) )
			{
				privMatch->common->wcmEraserID=pLocal->name;
			}
		}
		xfree(common);
		common = priv->common = privMatch->common;
		priv->next = common->wcmDevices;
		common->wcmDevices = priv;
		return 1;
	}
	return 0;
}

/* xf86WcmInit - called when the module subsection is found in XF86Config */

static LocalDevicePtr xf86WcmInit(InputDriverPtr drv, IDevPtr dev, int flags)
{
	LocalDevicePtr local = NULL;
	LocalDevicePtr fakeLocal = NULL;
	WacomDevicePtr priv = NULL;
	WacomCommonPtr common = NULL;
	char		*s, b[12];
	int		i, oldButton;
	LocalDevicePtr localDevices;

	WacomToolPtr tool = NULL;
	WacomToolAreaPtr area = NULL;

	gWacomModule.wcmDrv = drv;

	fakeLocal = (LocalDevicePtr) xcalloc(1, sizeof(LocalDeviceRec));
	if (!fakeLocal)
		return NULL;

	fakeLocal->conf_idev = dev;

	/* Force default serial port options to exist because the serial init
	 * phasis is based on those values.
	 */
	xf86CollectInputOptions(fakeLocal, default_options, NULL);

	/* Type is mandatory */
	s = xf86FindOptionValue(fakeLocal->options, "Type");

	if (s && (xf86NameCmp(s, "stylus") == 0))
		local = xf86WcmAllocateStylus();
	else if (s && (xf86NameCmp(s, "touch") == 0))
		local = xf86WcmAllocateTouch();
	else if (s && (xf86NameCmp(s, "cursor") == 0))
		local = xf86WcmAllocateCursor();
	else if (s && (xf86NameCmp(s, "eraser") == 0))
		local = xf86WcmAllocateEraser();
	else if (s && (xf86NameCmp(s, "pad") == 0))
		local = xf86WcmAllocatePad();
	else
	{
		xf86Msg(X_ERROR, "%s: No type or invalid type specified.\n"
				"Must be one of stylus, touch, cursor, eraser, or pad\n",
				dev->identifier);
		goto SetupProc_fail;
	}
    
	if (!local)
	{
		xfree(fakeLocal);
		return NULL;
	}

	priv = (WacomDevicePtr) local->private;
	common = priv->common;

	local->options = fakeLocal->options;
	local->conf_idev = fakeLocal->conf_idev;
	local->name = dev->identifier;
	xfree(fakeLocal);
    
	common->wcmDevice = xf86FindOptionValue(local->options, "Device");

#ifdef LINUX_INPUT
	/* Autoprobe if not given */
	if (!common->wcmDevice || !strcmp (common->wcmDevice, "auto-dev")) 
	{
		common->wcmFlags |= AUTODEV_FLAG;
		if (! (common->wcmDevice = xf86WcmEventAutoDevProbe (local))) 
		{
			xf86Msg(X_ERROR, "%s: unable to probe device\n",
				dev->identifier);
			goto SetupProc_fail;
		}
	}
#else
	if (!common->wcmDevice)
	{
		xf86Msg(X_ERROR, "%s: No Device specified.\n", dev->identifier);
		goto SetupProc_fail;
	}
#endif

	/* Lookup to see if there is another wacom device sharing
	 * the same serial line.
	 */
	localDevices = xf86FirstLocalDevice();
    
	if (common->wcmDevice)
	{
		for (; localDevices != NULL; localDevices = localDevices->next)
		{
			if (xf86WcmMatchDevice(localDevices,local))
			{
				common = priv->common;
				break;
			}
		}
	}

	/* Process the common options. */
	xf86ProcessCommonOptions(local, local->options);

	/* Optional configuration */

	xf86Msg(X_CONFIG, "%s device is %s\n", dev->identifier,
			common->wcmDevice);

	priv->debugLevel = xf86SetIntOption(local->options,
		"DebugLevel", priv->debugLevel);
	if (priv->debugLevel > 0)
		xf86Msg(X_CONFIG, "WACOM: %s debug level set to %d\n",
			dev->identifier, priv->debugLevel);

	common->debugLevel = xf86SetIntOption(local->options,
		"CommonDBG", common->debugLevel);
	if (common->debugLevel > 0)
		xf86Msg(X_CONFIG, "WACOM: %s tablet common debug level set to %d\n",
			dev->identifier, common->debugLevel);

	s = xf86FindOptionValue(local->options, "Mode");

	if (s && (xf86NameCmp(s, "absolute") == 0))
		priv->flags |= ABSOLUTE_FLAG;
	else if (s && (xf86NameCmp(s, "relative") == 0))
		priv->flags &= ~ABSOLUTE_FLAG;
	else if (s)
	{
		xf86Msg(X_ERROR, "%s: invalid Mode (should be absolute or "
			"relative). Using default.\n", dev->identifier);

		/* stylus/eraser defaults to absolute mode 
		 * cursor defaults to relative mode 
		 */
		if (IsCursor(priv)) 
			priv->flags &= ~ABSOLUTE_FLAG;
		else 
			priv->flags |= ABSOLUTE_FLAG;
	}

	/* Pad is always in relative mode when it's a core device.
	 * Always in absolute mode when it is not a core device.
	 */
	if (IsPad(priv))
		xf86WcmSetPadCoreMode(local);

	/* Store original local Core flag so it can be changed later */
	if (local->flags & (XI86_ALWAYS_CORE | XI86_CORE_POINTER))
		priv->flags |= COREEVENT_FLAG;

	xf86Msg(X_CONFIG, "%s is in %s mode\n", local->name,
		(priv->flags & ABSOLUTE_FLAG) ? "absolute" : "relative");

	/* ISDV4 support */
	s = xf86FindOptionValue(local->options, "ForceDevice");

	if (s && (xf86NameCmp(s, "ISDV4") == 0))
	{
		common->wcmForceDevice=DEVICE_ISDV4;
		common->wcmDevCls = &gWacomISDV4Device;
		xf86Msg(X_CONFIG, "%s: forcing TabletPC ISD V4 protocol\n",
			dev->identifier);
		common->wcmTPCButtonDefault = 1; /* Tablet PC buttons on by default */
	}

	s = xf86FindOptionValue(local->options, "Rotate");

	if (s)
	{
		if (xf86NameCmp(s, "CW") == 0)
			common->wcmRotate=ROTATE_CW;
		else if (xf86NameCmp(s, "CCW") ==0)
			common->wcmRotate=ROTATE_CCW;
		else if (xf86NameCmp(s, "HALF") ==0)
			common->wcmRotate=ROTATE_HALF;
		xf86Msg(X_CONFIG, "WACOM: Rotation is set to %s\n", s);
	}

	common->wcmSuppress = xf86SetIntOption(local->options, "Suppress",
			common->wcmSuppress);
	if (common->wcmSuppress != 0) /* 0 disables suppression */
	{
		if (common->wcmSuppress > MAX_SUPPRESS)
			common->wcmSuppress = MAX_SUPPRESS;
		if (common->wcmSuppress < DEFAULT_SUPPRESS)
			common->wcmSuppress = DEFAULT_SUPPRESS;
	}
	xf86Msg(X_CONFIG, "WACOM: suppress value is %d\n", common->wcmSuppress);      
    
	if (xf86SetBoolOption(local->options, "Tilt",
			(common->wcmFlags & TILT_REQUEST_FLAG)))
	{
		common->wcmFlags |= TILT_REQUEST_FLAG;
	}

	if (xf86SetBoolOption(local->options, "RawFilter",
			(common->wcmFlags & RAW_FILTERING_FLAG)))
	{
		common->wcmFlags |= RAW_FILTERING_FLAG;
	}

#ifdef WCM_ENABLE_LINUXINPUT
	if (xf86SetBoolOption(local->options, "USB",
			(common->wcmDevCls == &gWacomUSBDevice)))
	{
		static int already_tried_it = 0;
		/* best effort attempt at loading the wacom and evdev
		 * kernel modules. try it just once */
		if (!already_tried_it) {
			already_tried_it = 1;
			(void)xf86LoadKernelModule("wacom");
			(void)xf86LoadKernelModule("evdev");
    		}
		common->wcmDevCls = &gWacomUSBDevice;
		xf86Msg(X_CONFIG, "%s: reading USB link\n", dev->identifier);
	}
#else
	if (xf86SetBoolOption(local->options, "USB", 0))
	{
		ErrorF("The USB version of the driver isn't "
			"available for your platform\n");
	}
#endif

	/* pressure curve takes control points x1,y1,x2,y2
	 * values in range from 0..100.
	 * Linear curve is 0,0,100,100
	 * Slightly depressed curve might be 5,0,100,95
	 * Slightly raised curve might be 0,5,95,100
	 */
	s = xf86FindOptionValue(local->options, "PressCurve");
	if (s && !IsCursor(priv) && !IsTouch(priv)) 
	{
		int a,b,c,d;
		if ((sscanf(s,"%d,%d,%d,%d",&a,&b,&c,&d) != 4) ||
			(a < 0) || (a > 100) || (b < 0) || (b > 100) ||
			(c < 0) || (c > 100) || (d < 0) || (d > 100))
			xf86Msg(X_CONFIG, "WACOM: PressCurve not valid\n");
		else
		{
			xf86WcmSetPressureCurve(priv,a,b,c,d);
			xf86Msg(X_CONFIG, "WACOM: PressCurve %d,%d,%d,%d\n",
				a,b,c,d);
		}
	}

	if (IsCursor(priv))
	{
		common->wcmCursorProxoutDist = xf86SetIntOption(local->options, "CursorProx", 0);
		if (common->wcmCursorProxoutDist < 0 || common->wcmCursorProxoutDist > 255)
			xf86Msg(X_CONFIG, "WACOM: CursorProx invalid %d \n", common->wcmCursorProxoutDist);
	}

	/* Configure Monitors' resoluiton in TwinView setup.
	 * The value is in the form of "1024x768,1280x1024" 
	 * for a desktop of monitor 1 at 1024x768 and 
	 * monitor 2 at 1280x1024
	 */
	s = xf86FindOptionValue(local->options, "TVResolution");
	if (s)
	{
		int a,b,c,d;
		if ((sscanf(s,"%dx%d,%dx%d",&a,&b,&c,&d) != 4) ||
			(a <= 0) || (b <= 0) || (c <= 0) || (d <= 0))
			xf86Msg(X_CONFIG, "WACOM: TVResolution not valid\n");
		else
		{
			priv->tvResolution[0] = a;
			priv->tvResolution[1] = b;
			priv->tvResolution[2] = c;
			priv->tvResolution[3] = d;
			xf86Msg(X_CONFIG, "WACOM: TVResolution %d,%d %d,%d\n",
				a,b,c,d);
		}
	}
    
	priv->screen_no = xf86SetIntOption(local->options, "ScreenNo", -1);
	if (priv->screen_no != -1)
		xf86Msg(X_CONFIG, "%s: attached screen number %d\n",
			dev->identifier, priv->screen_no);
 
	if (xf86SetBoolOption(local->options, "KeepShape", 0))
	{
		priv->flags |= KEEP_SHAPE_FLAG;
		xf86Msg(X_CONFIG, "%s: keeps shape\n", dev->identifier);
	}

	priv->topX = xf86SetIntOption(local->options, "TopX", 0);
	if (priv->topX != 0)
		xf86Msg(X_CONFIG, "%s: top x = %d\n", dev->identifier,
			priv->topX);

	priv->topY = xf86SetIntOption(local->options, "TopY", 0);
	if (priv->topY != 0)
		xf86Msg(X_CONFIG, "%s: top y = %d\n", dev->identifier,
			priv->topY);

	priv->bottomX = xf86SetIntOption(local->options, "BottomX", 0);
	if (priv->bottomX != 0)
		xf86Msg(X_CONFIG, "%s: bottom x = %d\n", dev->identifier,
			priv->bottomX);

	priv->bottomY = xf86SetIntOption(local->options, "BottomY", 0);
	if (priv->bottomY != 0)
		xf86Msg(X_CONFIG, "%s: bottom y = %d\n", dev->identifier,
			priv->bottomY);

	priv->serial = xf86SetIntOption(local->options, "Serial", 0);
	if (priv->serial != 0)
		xf86Msg(X_CONFIG, "%s: serial number = %u\n", dev->identifier,
			priv->serial);

	tool = priv->tool;
	area = priv->toolarea;
	area->topX = priv->topX;
	area->topY = priv->topY;
	area->bottomX = priv->bottomX;
	area->bottomY = priv->bottomY;
	tool->serial = priv->serial;

	/* The first device doesn't need to add any tools/areas as it
	 * will be the first anyway. So if different, add tool
	 * and/or area to the existing lists 
	 */
	if(tool != common->wcmTool)
	{
		WacomToolPtr toollist = NULL;
		for(toollist = common->wcmTool; toollist; toollist = toollist->next)
			if(tool->typeid == toollist->typeid && tool->serial == toollist->serial) 
				break;

		if(toollist) /* Already have a tool with the same type/serial */
		{
			WacomToolAreaPtr arealist;

			xfree(tool);
			priv->tool = tool = toollist;
			arealist = toollist->arealist;

			/* Add the area to the end of the list */
			while(arealist->next)
				arealist = arealist->next;
			arealist->next = area;
		}
		else /* No match on existing tool/serial, add tool to the end of the list */
		{
			toollist = common->wcmTool;
			while(toollist->next)
				toollist = toollist->next;
			toollist->next = tool;
		}
	}

	common->wcmThreshold = xf86SetIntOption(local->options, "Threshold",
			common->wcmThreshold);
	if (common->wcmThreshold > 0)
		xf86Msg(X_CONFIG, "%s: threshold = %d\n", dev->identifier,
			common->wcmThreshold);

	priv->wcmMaxX = xf86SetIntOption(local->options, "MaxX",
		common->wcmMaxX);
	if (priv->wcmMaxX > 0)
		xf86Msg(X_CONFIG, "%s: max x set to %d by xorg.conf\n", dev->identifier,
			priv->wcmMaxX);

	/* Update tablet logical max X */
	if (!IsTouch(priv)) common->wcmMaxX = priv->wcmMaxX;

	priv->wcmMaxY = xf86SetIntOption(local->options, "MaxY",
		common->wcmMaxY);
	if (priv->wcmMaxY > 0)
		xf86Msg(X_CONFIG, "%s: max y set to %d by xorg.conf\n", dev->identifier,
			priv->wcmMaxY);

	/* Update tablet logical max Y */
	if (!IsTouch(priv)) common->wcmMaxY = priv->wcmMaxY;

	common->wcmMaxZ = xf86SetIntOption(local->options, "MaxZ",
		common->wcmMaxZ);
	if (common->wcmMaxZ != 0)
		xf86Msg(X_CONFIG, "%s: max z = %d\n", dev->identifier,
			common->wcmMaxZ);

	common->wcmUserResolX = xf86SetIntOption(local->options, "ResolutionX",
		common->wcmUserResolX);
	if (common->wcmUserResolX != 0)
		xf86Msg(X_CONFIG, "%s: resol x = %d\n", dev->identifier,
			common->wcmUserResolX);

	common->wcmUserResolY = xf86SetIntOption(local->options, "ResolutionY",
		common->wcmUserResolY);
	if (common->wcmUserResolY != 0)
		xf86Msg(X_CONFIG, "%s: resol y = %d\n", dev->identifier,
			common->wcmUserResolY);

	common->wcmUserResolZ = xf86SetIntOption(local->options, "ResolutionZ",
		common->wcmUserResolZ);
	if (common->wcmUserResolZ != 0)
		xf86Msg(X_CONFIG, "%s: resol z = %d\n", dev->identifier,
			common->wcmUserResolZ);

	if (xf86SetBoolOption(local->options, "ButtonsOnly", 0))
	{
		priv->flags |= BUTTONS_ONLY_FLAG;
		xf86Msg(X_CONFIG, "%s: buttons only\n", dev->identifier);
	}

	/* Tablet PC button applied to the whole tablet. Not just one tool */
	if ( priv->flags & STYLUS_ID )
	{
		common->wcmTPCButton = xf86SetBoolOption(local->options, 
			"TPCButton", common->wcmTPCButtonDefault);
		if ( common->wcmTPCButton )
			xf86Msg(X_CONFIG, "%s: Tablet PC buttons are on \n", common->wcmDevice);
	}

	/* Touch applies to the whole tablet */
	common->wcmTouch = xf86SetBoolOption(local->options, "Touch", common->wcmTouchDefault);
	if ( common->wcmTouch )
		xf86Msg(X_CONFIG, "%s: Touch is enabled \n", common->wcmDevice);

	/* Touch capacity applies to the whole tablet */
	common->wcmCapacity = xf86SetBoolOption(local->options, "Capacity", common->wcmCapacityDefault);
	if ( common->wcmCapacity >= 0 )
		xf86Msg(X_CONFIG, "%s: Touch capacity is enabled \n", common->wcmDevice);

	/* Mouse cursor stays in one monitor in a multimonitor setup */
	if ( !priv->wcmMMonitor )
	{
		priv->wcmMMonitor = xf86SetBoolOption(local->options, "MMonitor", 1);
		if ( !priv->wcmMMonitor )
			xf86Msg(X_CONFIG, "%s: Cursor will stay in one monitor \n", common->wcmDevice);
	}


	for (i=0; i<MAX_BUTTONS; i++)
	{
		sprintf(b, "Button%d", i+1);
		s = xf86SetStrOption(local->options, b, NULL);
		if (s)
		{
			oldButton = priv->button[i];
			priv->button[i] = xf86SetIntOption(local->options, b, priv->button[i]);

			if (oldButton != priv->button[i])
				xf86Msg(X_CONFIG, "%s: button%d assigned to %d\n",
					dev->identifier, i+1, priv->button[i]);
		}
	}

	/* baud rate */
	{
		int val;
		val = xf86SetIntOption(local->options, "BaudRate", 0);

		switch(val)
		{
			case 38400:
				common->wcmLinkSpeed = 38400;
				break;
			case 19200:
				common->wcmLinkSpeed = 19200;
				break;
			case 9600:
				common->wcmLinkSpeed = 9600;
				break;
			default:
				xf86Msg(X_ERROR, "%s: Illegal speed value "
					"(must be 9600 or 19200 or 38400).",
					dev->identifier);
				break;
		}

		if (xf86Verbose && !(xf86SetBoolOption(local->options, "USB", 0)))
			xf86Msg(X_CONFIG, "%s: serial speed %u\n",
				dev->identifier, val);
	} /* baud rate */

	priv->speed = xf86SetRealOption(local->options, "Speed", DEFAULT_SPEED);
	if (priv->speed != DEFAULT_SPEED)
		xf86Msg(X_CONFIG, "%s: speed = %.3f\n", dev->identifier,
			priv->speed);

	priv->accel = xf86SetIntOption(local->options, "Accel", 0);
	if (priv->accel)
		xf86Msg(X_CONFIG, "%s: Accel = %d\n", dev->identifier,
			priv->accel);

	s = xf86FindOptionValue(local->options, "Twinview");
	if (s) xf86Msg(X_CONFIG, "%s: Twinview = %s\n", dev->identifier, s);
	if (s && xf86NameCmp(s, "none") == 0) 
		priv->twinview = TV_NONE;
	else if ((s && xf86NameCmp(s, "horizontal") == 0) ||
			(s && xf86NameCmp(s, "rightof") == 0)) 
		priv->twinview = TV_LEFT_RIGHT;
	else if ((s && xf86NameCmp(s, "vertical") == 0) ||
			(s && xf86NameCmp(s, "belowof") == 0)) 
		priv->twinview = TV_ABOVE_BELOW;
	else if (s && xf86NameCmp(s, "leftof") == 0) 
		priv->twinview = TV_RIGHT_LEFT;
	else if (s && xf86NameCmp(s, "aboveof") == 0) 
		priv->twinview = TV_BELOW_ABOVE;
	else if (s) 
	{
		xf86Msg(X_ERROR, "%s: invalid Twinview (should be none, vertical (belowof), "
			"horizontal (rightof), aboveof, or leftof). Using none.\n",
			dev->identifier);
		priv->twinview = TV_NONE;
	}

	/* mark the device configured */
	local->flags |= XI86_POINTER_CAPABLE | XI86_CONFIGURED;

	/* return the LocalDevice */
	return (local);

SetupProc_fail:
	if(area)
		xfree(area);
	if(tool)
		xfree(tool);
	if (common)
		xfree(common);
	if (priv)
		xfree(priv);
	if (local)
		xfree(local);
	return NULL;
}

#ifdef XFree86LOADER
static
#endif
InputDriverRec WACOM =
{
	1,             /* driver version */
	"wacom",       /* driver name */
	NULL,          /* identify */
	xf86WcmInit,   /* pre-init */
	xf86WcmUninit, /* un-init */
	NULL,          /* module */
	0              /* ref count */
};

/******************************************************************************
 * XFree86 V4 Dynamic Module Initialization
 *****************************************************************************/

#ifdef XFree86LOADER

/* xf86WcmUnplug - called when the module subsection is found in XF86Config */

static void xf86WcmUnplug(pointer p)
{
	ErrorF("xf86WcmUnplug\n");
}

/* xf86WcmPlug - called when the module subsection is found in XF86Config */

static pointer xf86WcmPlug(pointer module, pointer options, int* errmaj,
		int* errmin)
{
	xf86Msg(X_INFO, "Wacom driver level: %s\n",
		gWacomModule.identification + strlen("$Identification: "));
	xf86AddInputDriver(&WACOM, module, 0);
	return module;
}

static XF86ModuleVersionInfo xf86WcmVersionRec =
{
	"wacom",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
#ifdef WCM_XORG_XSERVER_1_6
	XORG_VERSION_CURRENT,
#else
	XF86_VERSION_CURRENT,
#endif
	1, 0, 0,
	ABI_CLASS_XINPUT,
	ABI_XINPUT_VERSION,
	MOD_CLASS_XINPUT,
	{0, 0, 0, 0}  /* signature, to be patched into the file by a tool */
};

#ifdef _X_EXPORT
    _X_EXPORT XF86ModuleData wacomModuleData =
#else
    XF86ModuleData wacomModuleData =
#endif
{
	&xf86WcmVersionRec,
	xf86WcmPlug,
	xf86WcmUnplug
};

#endif /* XFree86LOADER */
