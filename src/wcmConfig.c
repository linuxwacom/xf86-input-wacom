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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "xf86Wacom.h"
#include "wcmFilter.h"
#include <sys/stat.h>
#include <fcntl.h>

extern Bool xf86WcmIsWacomDevice (char* fname);
extern int wcmIsAValidType(LocalDevicePtr local, const char* type);
extern int wcmIsDuplicate(char* device, LocalDevicePtr local);
extern int wcmNeedAutoHotplug(LocalDevicePtr local, const char **type);
extern int wcmAutoProbeDevice(LocalDevicePtr local);
extern int wcmParseOptions(LocalDevicePtr local);
extern void wcmHotplugOthers(LocalDevicePtr local);

static int xf86WcmAllocate(LocalDevicePtr local, char* name, int flag);

/*****************************************************************************
 * xf86WcmAllocate --
 ****************************************************************************/

static int xf86WcmAllocate(LocalDevicePtr local, char* type_name, int flag)
{
	WacomDevicePtr   priv   = NULL;
	WacomCommonPtr   common = NULL;
	WacomToolPtr     tool   = NULL;
	WacomToolAreaPtr area   = NULL;
	int i, j;

	priv = xcalloc(1, sizeof(WacomDeviceRec));
	if (!priv)
		goto error;

	common = xcalloc(1, sizeof(WacomCommonRec));
	if (!common)
		goto error;

	tool = xcalloc(1, sizeof(WacomTool));
	if(!tool)
		goto error;

	area = xcalloc(1, sizeof(WacomToolArea));
	if (!area)
		goto error;

	local->type_name = type_name;
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
	priv->nPressCtrl [0] = 0;    /* pressure curve x0 */
	priv->nPressCtrl [1] = 0;    /* pressure curve y0 */
	priv->nPressCtrl [2] = 100;  /* pressure curve x1 */
	priv->nPressCtrl [3] = 100;  /* pressure curve y1 */

	/* Default button and expresskey values */
	for (i=0; i<WCM_MAX_BUTTONS; i++)
		priv->button[i] = (AC_BUTTON | (i + 1));

	for (i=0; i<WCM_MAX_BUTTONS; i++)
		for (j=0; j<256; j++)
			priv->keys[i][j] = 0;

	priv->nbuttons = WCM_MAX_BUTTONS;		/* Default number of buttons */
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
	common->min_maj = 0;			 /* device major and minor */
	common->wcmFlags = RAW_FILTERING_FLAG;   /* various flags */
	common->wcmDevices = priv;
	common->npadkeys = 0;		   /* Default number of pad keys */
	common->wcmProtocolLevel = 4;      /* protocol level */
	common->wcmThreshold = 0;       /* unconfigured threshold */
	common->wcmISDV4Speed = 38400;  /* serial ISDV4 link speed */
	common->debugLevel = 0;         /* shared debug level can only 
					 * be changed though xsetwacom */

	common->wcmDevCls = &gWacomUSBDevice; /* device-specific functions */
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
	common->wcmTouchResolX = 0;        /* touch X resolution in points/mm */
	common->wcmTouchResolY = 0;        /* touch Y resolution in points/mm */
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

	return 1;

error:
	xfree(area);
	xfree(tool);
	xfree(common);
	xfree(priv);
	return 0;
}

static int xf86WcmAllocateByType(LocalDevicePtr local, const char *type)
{
	int rc = 0;

	if (!type)
	{
		xf86Msg(X_ERROR, "%s: No type or invalid type specified.\n"
				"Must be one of stylus, touch, cursor, eraser, or pad\n",
				local->name);
		return rc;
	}

	if (xf86NameCmp(type, "stylus") == 0)
		rc = xf86WcmAllocate(local, XI_STYLUS, ABSOLUTE_FLAG|STYLUS_ID);
	else if (xf86NameCmp(type, "touch") == 0)
		rc = xf86WcmAllocate(local, XI_TOUCH, ABSOLUTE_FLAG|TOUCH_ID);
	else if (xf86NameCmp(type, "cursor") == 0)
		rc = xf86WcmAllocate(local, XI_CURSOR, CURSOR_ID);
	else if (xf86NameCmp(type, "eraser") == 0)
		rc = xf86WcmAllocate(local, XI_ERASER, ABSOLUTE_FLAG|ERASER_ID);
	else if (xf86NameCmp(type, "pad") == 0)
		rc = xf86WcmAllocate(local, XI_PAD, PAD_ID);

	return rc;
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
	WacomDevicePtr dev;
	WacomDevicePtr *prev;

	DBG(1, priv->debugLevel, ErrorF("xf86WcmUninit\n"));

	if (priv->isParent)
	{
		/* HAL removal sees the parent device removed first. */
		WacomDevicePtr next;
		dev = priv->common->wcmDevices;

		xf86Msg(X_INFO, "%s: removing automatically added devices.\n",
			local->name);

		while(dev)
		{
			next = dev->next;
			if (!dev->isParent && dev->uniq == priv->uniq)
			{
				xf86Msg(X_INFO, "%s: removing dependent device '%s'\n",
					local->name, dev->local->name);
				DeleteInputDeviceRequest(dev->local->dev);
			}
			dev = next;
		}
	}

	prev = &priv->common->wcmDevices;
	dev = *prev;
	while(dev)
	{
		if (dev == priv)
		{
			*prev = dev->next;
			break;
		}
		prev = &dev->next;
		dev = dev->next;
	}

	/* free pressure curve */
	xfree(priv->pPressCurve);

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
		strstr(pMatch->drv->driverName, "wacom") &&
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

/* xf86WcmInit - called for each input devices with the driver set to
 * "wacom" */
static LocalDevicePtr xf86WcmInit(InputDriverPtr drv, IDevPtr dev, int flags)
{
	LocalDevicePtr local = NULL;
	WacomDevicePtr priv = NULL;
	WacomCommonPtr common = NULL;
	const char*	type;
	char*		device;
	static int	numberWacom = 0;
	int		need_hotplug = 0;

	gWacomModule.wcmDrv = drv;

	if (!(local = xf86AllocateInput(drv, 0)))
		goto SetupProc_fail;

	local->conf_idev = dev;
	local->name = dev->identifier;

	/* Force default port options to exist because the init
	 * phase is based on those values.
	 */
	xf86CollectInputOptions(local, default_options, NULL);

	device = xf86SetStrOption(local->options, "Device", NULL);
	type = xf86FindOptionValue(local->options, "Type");
	need_hotplug = wcmNeedAutoHotplug(local, &type);

	/* leave the undefined for auto-dev (if enabled) to deal with */
	if(device)
	{
		/* check if the type is valid for the device */
		if(!wcmIsAValidType(local, type))
			goto SetupProc_fail;

		/* check if the device has been added */
		if (wcmIsDuplicate(device, local))
			goto SetupProc_fail;
	}

	if (!xf86WcmAllocateByType(local, type))
		goto SetupProc_fail;

	priv = (WacomDevicePtr) local->private;
	common = priv->common;

	common->wcmDevice = device;

	/* Auto-probe the device if required, otherwise just noop. */
	if (numberWacom)
		if (!wcmAutoProbeDevice(local))
			goto SetupProc_fail;

	/* Lookup to see if there is another wacom device sharing the same port */
	if (common->wcmDevice)
	{
		LocalDevicePtr localDevices = xf86FirstLocalDevice();
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
	if (!wcmParseOptions(local))
		goto SetupProc_fail;

	/* mark the device configured */
	local->flags |= XI86_POINTER_CAPABLE | XI86_CONFIGURED;

	/* keep a local count so we know if "auto-dev" is necessary or not */
	numberWacom++;

	if (need_hotplug)
	{
		priv->isParent = 1;
		wcmHotplugOthers(local);
	}

	/* return the LocalDevice */
	return (local);

SetupProc_fail:
	xfree(common);
	xfree(priv);
	if (local)
	{
		local->private = NULL;
		xf86DeleteInput(local, 0);
	}

	return NULL;
}

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


/* xf86WcmUnplug - Uninitialize the device */

static void xf86WcmUnplug(pointer p)
{
}

/* xf86WcmPlug - called by the module loader */

static pointer xf86WcmPlug(pointer module, pointer options, int* errmaj,
		int* errmin)
{
	xf86AddInputDriver(&WACOM, module, 0);
	return module;
}

static XF86ModuleVersionInfo xf86WcmVersionRec =
{
	"wacom",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XORG_VERSION_CURRENT,
	PACKAGE_VERSION_MAJOR, PACKAGE_VERSION_MINOR, PACKAGE_VERSION_PATCHLEVEL,
	ABI_CLASS_XINPUT,
	ABI_XINPUT_VERSION,
	MOD_CLASS_XINPUT,
	{0, 0, 0, 0}  /* signature, to be patched into the file by a tool */
};

_X_EXPORT XF86ModuleData wacomModuleData =
{
	&xf86WcmVersionRec,
	xf86WcmPlug,
	xf86WcmUnplug
};

/* vim: set noexpandtab shiftwidth=8: */
