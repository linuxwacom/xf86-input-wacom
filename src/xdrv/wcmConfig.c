/*
 * Copyright 1995-2002 by Frederic Lepied, France. <Lepied@XFree86.org>
 * Copyright 2002-2006 by Ping Cheng, Wacom. <pingc@wacom.com>
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
#include "wcmFilter.h"
#define WCMACTION_X11_DRIVER
#include "../include/wcmAction.h"

/* Max distance (0-63) at which a proximity-out event is generated for
 * cursor device (e.g. mouse). Default is half of the range.
 */
#define PROXOUT_DISTANCE	32
/* Hysteresis value for the above distance in 1/64 relative units, e.g.
 * if PROXOUT_DISTANCE == 32 and PROXOUT_HYSTERESIS == 8, this would mean
 * that proximity out is generated when distance is more than (for Intuos 
 * series and Cintiq 21UX) or less than (for Graphire series)
 * 32+(32*8/64) = 36 units (this is to produce a prompt relative movement), 
 * and proximity in is generated when distance is less than 32-(32*8/64) = 28 
 * units (this is to avoid prox in/out jittering).
 */
#define PROXOUT_HYSTERESIS	8

/*****************************************************************************
 * xf86WcmAllocate --
 ****************************************************************************/

LocalDevicePtr xf86WcmAllocate(char* name, int flag)
{
	LocalDevicePtr local;
	WacomDevicePtr priv;
	WacomCommonPtr common;
	int i;

	priv = (WacomDevicePtr) xalloc(sizeof(WacomDeviceRec));
	if (!priv)
		return NULL;

	common = (WacomCommonPtr) xalloc(sizeof(WacomCommonRec));
	if (!common)
	{
		xfree(priv);
		return NULL;
	}

	local = xf86AllocateInput(gWacomModule.v4.wcmDrv, 0);
	if (!local)
	{
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
	local->history_size  = 0;
	local->old_x = -1;
	local->old_y = -1;
	
	memset(priv,0,sizeof(*priv));
	priv->flags = flag;          /* various flags (device type, absolute, first touch...) */
	priv->oldX = -1;             /* previous X position */
	priv->oldY = -1;             /* previous Y position */
	priv->oldZ = -1;             /* previous pressure */
	priv->oldTiltX = -1;         /* previous tilt in x direction */
	priv->oldTiltY = -1;         /* previous tilt in y direction */
	priv->oldButtons = 0;        /* previous buttons state */
	priv->oldWheel = 0;          /* previous wheel */
	priv->topX = 0;              /* X top */
	priv->topY = 0;              /* Y top */
	priv->bottomX = 0;           /* X bottom */
	priv->bottomY = 0;           /* Y bottom */
	priv->factorX = 0.0;         /* X factor */
	priv->factorY = 0.0;         /* Y factor */
	priv->common = common;       /* common info pointer */
	priv->oldProximity = 0;      /* previous proximity */
	priv->serial = 0;            /* serial number */
	priv->screen_no = -1;        /* associated screen */
	priv->speed = DEFAULT_SPEED; /* rel. mode speed */
	priv->accel = 0;	     /* rel. mode acceleration */
	priv->nPressCtrl [0] = 0;    /* pressure curve x0 */
	priv->nPressCtrl [1] = 0;    /* pressure curve y0 */
	priv->nPressCtrl [2] = 100;  /* pressure curve x1 */
	priv->nPressCtrl [3] = 100;  /* pressure curve y1 */
	/* Pad by default emits keypresses, other devices emits buttons */
	for (i=0; i<MAX_MOUSE_BUTTONS/2; i++)
		priv->button[i] = IsPad (priv) ?
			(AC_BUTTON | (MAX_MOUSE_BUTTONS/2 + i + 1)) : (AC_BUTTON | (i + 1));
	for (i=MAX_MOUSE_BUTTONS/2; i<MAX_BUTTONS; i++)
		priv->button[i] = IsPad (priv) ?
			(AC_KEY | (XK_F1 + i-MAX_MOUSE_BUTTONS/2)) : (AC_BUTTON | (i + 1));
	priv->nbuttons = MAX_BUTTONS;      /* Default number of buttons */
	priv->naxes = 6;                   /* Default number of axes */
	priv->numScreen = screenInfo.numScreens; /* configured screens count */
	priv->currentScreen = 0;                 /* current screen in display */

	priv->twinview = TV_NONE;		/* not using twinview gfx */
	priv->tvoffsetX = 0;			/* none X edge offset for TwinView setup */
	priv->tvoffsetY = 0;			/* none Y edge offset for TwinView setup */
	for (i=0; i<4; i++)
		priv->tvResolution[i] = 0;	/* unconfigured twinview resolution */

	/* JEJ - throttle sampling code */
	priv->throttleValue = 0;
	priv->throttleStart = 0;
	priv->throttleLimit = -1;
	
	memset(common,0,sizeof(*common));
	memset(common->wcmChannel, 0, sizeof(common->wcmChannel));
	common->wcmDevice = "";                  /* device file name */
	common->wcmSuppress = DEFAULT_SUPPRESS;  /* transmit position if increment is superior */
	common->wcmFlags = RAW_FILTERING_FLAG;   /* various flags */
	common->wcmDevices = (LocalDevicePtr*) xalloc(sizeof(LocalDevicePtr));
	common->wcmDevices[0] = local;
	common->wcmNumDevices = 1;         /* number of devices */
	common->wcmMaxX = 0;               /* max X value */
	common->wcmMaxY = 0;               /* max Y value */
	common->wcmMaxZ = 0;               /* max Z value */
	common->wcmMaxDist = 0;            /* max distance value */
	common->wcmResolX = 0;             /* X resolution in points/inch */
	common->wcmResolY = 0;             /* Y resolution in points/inch */
	common->wcmMaxStripX = 4095;       /* Max fingerstrip X */
	common->wcmMaxStripY = 4095;       /* Max fingerstrip Y */
	common->wcmChannelCnt = 1;         /* number of channels */
	common->wcmProtocolLevel = 4;      /* protocol level */
	common->wcmThreshold = 0;       /* unconfigured threshold */
	common->wcmLinkSpeed = 9600;    /* serial link speed */
	common->wcmDevCls = &gWacomSerialDevice; /* device-specific functions */
	common->wcmModel = NULL;                 /* model-specific functions */
	common->wcmEraserID = 0;	/* eraser id associated with the stylus */
	common->wcmGimp = 1;	/* enabled (=1) to support Gimp when Xinerama is */
				/* enabled for multi-monitor desktop. */
				/* To calibrate Cintiq and ISDV4, it should be disabled (=0) */
	common->wcmMMonitor = 1;	/* enabled (=1) to support multi-monitor desktop. */
					/* disabled (=0) when user doesn't want to move the */
					/* cursor from one screen to another screen */
	common->wcmTPCButton = 0;	/* set Tablet PC button on/off, default is off */
	common->wcmRotate = ROTATE_NONE; /* default tablet rotation to off */
	common->wcmCursorProxoutDist = PROXOUT_DISTANCE;
				/* Max mouse distance for proxy-out max/256 units */
	common->wcmCursorProxoutHyst = PROXOUT_HYSTERESIS;
				/* Proxy-out distance hysteresis in max/256 units */
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
    
	DBG(1, ErrorF("xf86WcmUninit\n"));

	gWacomModule.DevProc(local->dev, DEVICE_OFF);

	/* free pressure curve */
	if (priv->pPressCurve)
		xfree(priv->pPressCurve);
    
	xfree(priv);
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
		DBG(2, ErrorF("xf86WcmInit wacom port share between"
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
		xfree(common->wcmDevices);
		xfree(common);
		common = priv->common = privMatch->common;
		common->wcmNumDevices++;
		common->wcmDevices = (LocalDevicePtr *)xrealloc(
				common->wcmDevices,
				sizeof(LocalDevicePtr) * common->wcmNumDevices);
		common->wcmDevices[common->wcmNumDevices - 1] = pLocal;
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

	gWacomModule.v4.wcmDrv = drv;

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
	else if (s && (xf86NameCmp(s, "cursor") == 0))
		local = xf86WcmAllocateCursor();
	else if (s && (xf86NameCmp(s, "eraser") == 0))
		local = xf86WcmAllocateEraser();
	else if (s && (xf86NameCmp(s, "pad") == 0))
		local = xf86WcmAllocatePad();
	else
	{
		xf86Msg(X_ERROR, "%s: No type or invalid type specified.\n"
				"Must be one of stylus, cursor, eraser, or pad\n",
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
    
	/* Serial Device is mandatory */
	common->wcmDevice = xf86FindOptionValue(local->options, "Device");

	if (!common->wcmDevice)
	{
		xf86Msg(X_ERROR, "%s: No Device specified.\n", dev->identifier);
		goto SetupProc_fail;
	}

	/* Lookup to see if there is another wacom device sharing
	 * the same serial line.
	 */
	localDevices = xf86FirstLocalDevice();
    
	for (; localDevices != NULL; localDevices = localDevices->next)
	{
		if (xf86WcmMatchDevice(localDevices,local))
		{
			common = priv->common;
			break;
		}
	}

	/* Process the common options. */
	xf86ProcessCommonOptions(local, local->options);

	/* Optional configuration */

	xf86Msg(X_CONFIG, "%s device is %s\n", dev->identifier,
			common->wcmDevice);

	gWacomModule.debugLevel = xf86SetIntOption(local->options,
		"DebugLevel", gWacomModule.debugLevel);
	if (gWacomModule.debugLevel > 0)
		xf86Msg(X_CONFIG, "WACOM: debug level set to %d\n",
			gWacomModule.debugLevel);

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

	/* pad reports absolute value and never moves the cursor */
	if (IsPad(priv)) 
		priv->flags |= ABSOLUTE_FLAG;

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
	}

	s = xf86FindOptionValue(local->options, "Rotate");

	if (s)
	{
		if (xf86NameCmp(s, "CW") == 0)
			common->wcmRotate=ROTATE_CW;
		else if (xf86NameCmp(s, "CCW") ==0)
			common->wcmRotate=ROTATE_CCW;
		xf86Msg(X_CONFIG, "WACOM: Rotation is set to %s\n", s);
	}

	common->wcmSuppress = xf86SetIntOption(local->options, "Suppress",
			common->wcmSuppress);
	if ((common->wcmSuppress != 0) && /* 0 disables suppression */
		(common->wcmSuppress > MAX_SUPPRESS ||
			common->wcmSuppress < DEFAULT_SUPPRESS))
	{
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

#ifdef LINUX_INPUT
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
	if (s && !IsCursor(priv)) 
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

	s = xf86FindOptionValue(local->options, "CursorProx");
	if (s && !IsCursor(priv))
	{
		int a,b;
		if ((sscanf(s,"%d,%d",&a,&b) != 2) ||
			(a < 0) || (a > 63) || (b < 0) || (b > 63))
			xf86Msg(X_CONFIG, "WACOM: CursorProx not valid\n");
		else
		{
			common->wcmCursorProxoutDist = a;
			common->wcmCursorProxoutHyst = b;
		}
	}

	/* Config Monitors' resoluiton in TwinView setup.
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

	common->wcmThreshold = xf86SetIntOption(local->options, "Threshold",
			common->wcmThreshold);
	if (common->wcmThreshold > 0)
		xf86Msg(X_CONFIG, "%s: threshold = %d\n", dev->identifier,
			common->wcmThreshold);

	common->wcmMaxX = xf86SetIntOption(local->options, "MaxX",
		common->wcmMaxX);
	if (common->wcmMaxX != 0)
		xf86Msg(X_CONFIG, "%s: max x = %d\n", dev->identifier,
			common->wcmMaxX);

	common->wcmMaxY = xf86SetIntOption(local->options, "MaxY",
		common->wcmMaxY);
	if (common->wcmMaxY != 0)
		xf86Msg(X_CONFIG, "%s: max y = %d\n", dev->identifier,
			common->wcmMaxY);

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
	if ( !common->wcmTPCButton )
	{
		common->wcmTPCButton = xf86SetBoolOption(local->options, "TPCButton", 0);
		if ( common->wcmTPCButton )
			xf86Msg(X_CONFIG, "%s: Tablet PC buttons on \n", common->wcmDevice);
	}

	/* Turn on/off Gimp support in a multimonitor setup */
	if ( !common->wcmGimp )
	{
		common->wcmGimp = xf86SetBoolOption(local->options, "Gimp", 1);
		if ( !common->wcmGimp )
			xf86Msg(X_CONFIG, "%s: Gimp multimonitor mapping isn't supported \n", common->wcmDevice);
	}

	/* Cursor stays in one monitor in a multimonitor setup */
	if ( !common->wcmMMonitor )
	{
		common->wcmMMonitor = xf86SetBoolOption(local->options, "MMonitor", 1);
		if ( !common->wcmMMonitor )
			xf86Msg(X_CONFIG, "%s: Cursor will stay in one monitor \n", common->wcmDevice);
	}


	for (i=0; i<MAX_BUTTONS; i++)
	{
		sprintf(b, "Button%d", i+1);
		s = xf86SetStrOption(local->options, b, NULL);
		if (s)
		{
			oldButton = priv->button[i];
			priv->button[i] = xf86WcmDecodeAction(dev->identifier, b, s);

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
	{
		priv->twinview = TV_NONE;
	}
	else if (s && xf86NameCmp(s, "horizontal") == 0) 
	{
		priv->twinview = TV_LEFT_RIGHT;
		/* default resolution */
		if(!priv->tvResolution[0])
		{
			priv->tvResolution[0] = screenInfo.screens[0]->width/2;
			priv->tvResolution[1] = screenInfo.screens[0]->height;
			priv->tvResolution[2] = priv->tvResolution[0];
			priv->tvResolution[3] = priv->tvResolution[1];
		}
	}
	else if (s && xf86NameCmp(s, "vertical") == 0) 
	{
		priv->twinview = TV_ABOVE_BELOW;
		/* default resolution */
		if(!priv->tvResolution[0])
		{
			priv->tvResolution[0] = screenInfo.screens[0]->width;
			priv->tvResolution[1] = screenInfo.screens[0]->height/2;
			priv->tvResolution[2] = priv->tvResolution[0];
			priv->tvResolution[3] = priv->tvResolution[1];
		}
	}
	else if (s) 
	{
		xf86Msg(X_ERROR, "%s: invalid Twinview (should be none, vertical or horizontal). Using none.\n",
			dev->identifier);
		priv->twinview = TV_NONE;
	}

	/* mark the device configured */
	local->flags |= XI86_POINTER_CAPABLE | XI86_CONFIGURED;

	/* return the LocalDevice */
	return (local);

SetupProc_fail:
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
	DBG(1, ErrorF("xf86WcmUnplug\n"));
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
	XF86_VERSION_CURRENT,
	1, 0, 0,
	ABI_CLASS_XINPUT,
	ABI_XINPUT_VERSION,
	MOD_CLASS_XINPUT,
	{0, 0, 0, 0}  /* signature, to be patched into the file by a tool */
};

XF86ModuleData wacomModuleData =
{
	&xf86WcmVersionRec,
	xf86WcmPlug,
	xf86WcmUnplug
};

#endif /* XFree86LOADER */
