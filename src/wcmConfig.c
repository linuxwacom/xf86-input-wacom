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
#include "wcmFilter.h"

/*****************************************************************************
 * xf86WcmAllocate --
 ****************************************************************************/

#if XFREE86_V3
static Bool xf86WcmConfig(LocalDevicePtr* array, int inx, int max, LexPtr val);
#endif 

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

#if XFREE86_V4
	local = xf86AllocateInput(gWacomModule.v4.wcmDrv, 0);
#else
	local = (LocalDevicePtr) xalloc(sizeof(LocalDeviceRec));
#endif
	if (!local)
	{
		xfree(priv);
		xfree(common);
		return NULL;
	}

	local->name = name;
	local->flags = 0;
#if XFREE86_V3
	local->device_config = xf86WcmConfig;
#endif 
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
	for (i=0; i<16; i++)
		priv->button[i] = i+1; /* button i value */
	priv->numScreen = screenInfo.numScreens; /* configured screens count */
	priv->currentScreen = 0;                 /* current screen in display */

	priv->dscaleX = 1.0;			/* dual screen scale X factor */
	priv->dscaleY = 1.0;			/* dual screen scale Y factor */
	priv->doffsetX = 0;			/* dual screen offset X */
	priv->doffsetY = 0;			/* dual screen offset Y */
	priv->twinview = TV_NONE;		/* not using twinview gfx */
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
	common->wcmResolX = 0;             /* X resolution in points/inch */
	common->wcmResolY = 0;             /* Y resolution in points/inch */
	common->wcmChannelCnt = 1;             /* number of channels */
	common->wcmProtocolLevel = 4;          /* protocol level */
	common->wcmThreshold = 0;       /* unconfigured threshold */
	common->wcmInitialized = FALSE; /* device is not initialized */
	common->wcmLinkSpeed = 9600;    /* serial link speed */
	common->wcmDevCls = &gWacomSerialDevice; /* device-specific functions */
	common->wcmModel = NULL;                 /* model-specific functions */
	common->wcmGimp = 1;		/* enabled (=1) to support Gimp when Xinerama Enabled in multi-monitor desktop. Needs to be disabled (=0) for Cintiq calibration */
	common->wcmMMonitor = 1;	/* enabled (=1) to support multi-monitors by default. disabled (=0) when user doesn't want to move from one screen to the other */
	common->wcmTPCButton = 0;       /* set Tablet PC button on/off default is off */
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
	LocalDevicePtr local = xf86WcmAllocate(XI_ERASER,
			ABSOLUTE_FLAG|ERASER_ID);

	if (local)
		local->type_name = "Wacom Eraser";

	return local;
}


/******************************************************************************
 * XFree86 V3 Module Configuration
 *****************************************************************************/

#if XFREE86_V3

#define CURSOR_SECTION_NAME "wacomcursor"
#define STYLUS_SECTION_NAME "wacomstylus"
#define ERASER_SECTION_NAME "wacomeraser"

#define PORT  1
#define DEVICENAME 2
#define THE_MODE 3
#define SUPPRESS 4
#define DEBUG_LEVEL     5
#define TILT_MODE 6
#define HISTORY_SIZE 7
#define ALWAYS_CORE 8
#define KEEP_SHAPE 9
#define TOP_X  10
#define TOP_Y  11
#define BOTTOM_X 12
#define BOTTOM_Y 13
#define SERIAL  14
#define BAUD_RATE 15
#define THRESHOLD 16
#define MAX_X  17
#define MAX_Y  18
#define MAX_Z  19
#define RESOLUTION_X 20
#define RESOLUTION_Y 21
#define RESOLUTION_Z 22
#define USB  23
#define SCREEN_NO 24
#define BUTTONS_ONLY 25
#define TWINVIEW 26

#if !defined(sun) || defined(i386)
static SymTabRec WcmTab[] =
{
	{ ENDSUBSECTION, "endsubsection" },
	{ PORT,          "port" },
	{ DEVICENAME,    "devicename" },
	{ THE_MODE,      "mode" },
	{ SUPPRESS,      "suppress" },
	{ DEBUG_LEVEL,   "debuglevel" },
	{ TILT_MODE,     "tiltmode" },
	{ HISTORY_SIZE,  "historysize" },
	{ ALWAYS_CORE,   "alwayscore" },
	{ KEEP_SHAPE,    "keepshape" },
	{ TOP_X,         "topx" },
	{ TOP_Y,         "topy" },
	{ BOTTOM_X,      "bottomx" },
	{ BOTTOM_Y,      "bottomy" },
	{ SERIAL,        "serial" },
	{ BAUD_RATE,     "baudrate" },
	{ THRESHOLD,     "threshold" },
	{ MAX_X,         "maxx" },
	{ MAX_Y,         "maxy" },
	{ MAX_Z,         "maxz" },
	{ RESOLUTION_X,  "resolutionx" },
	{ RESOLUTION_Y,  "resolutiony" },
	{ RESOLUTION_Z,  "resolutionz" },
	{ USB,           "usb" },
	{ SCREEN_NO,     "screenno" },
	{ BUTTONS_ONLY,  "buttonsonly" },
	{ TWINVIEW,      "twinview" },
	{ -1, "" }
};

#define RELATIVE 1
#define ABSOLUTE 2

static SymTabRec ModeTabRec[] =
{
	{ RELATIVE,      "relative" },
	{ ABSOLUTE,      "absolute" },
	{ -1,  "" }
};

static SymTabRec TwinViewTabRec[] = {
  { TV_NONE,		"none" },
  { TV_ABOVE_BELOW,	"vertical" },
  { TV_LEFT_RIGHT,	"horizontal" },
  { -1,			"" }
};

#endif /* not sun or i386 */

/* xf86WcmMatchDevice - Find another device with same port. */

static Bool xf86WcmMatchDevice(LocalDevicePtr pMatch, LocalDevicePtr pDev,
	const char* name)
{
	WacomDevicePtr privMatch = (WacomDevicePtr)(pMatch->private);
	WacomDevicePtr priv= (WacomDevicePtr)(pDev->private);
	WacomCommonPtr common = priv->common;

	if ((pMatch->device_config == xf86WcmConfig) &&
		(!strcmp(privMatch->common->wcmDevice, name)))
	{
		DBG(2, ErrorF("xf86WcmConfig wacom port share between "
			"%s and %s\n", pDev->name, pMatch->name));
		privMatch->common->wcmHasEraser |= common->wcmHasEraser;
		xfree(common->wcmDevices);
		xfree(common);
		common = priv->common = privMatch->common;
		common->wcmNumDevices++;
		common->wcmDevices = (LocalDevicePtr*)xrealloc(
			common->wcmDevices,
			sizeof(LocalDevicePtr) * common->wcmNumDevices);
		common->wcmDevices[common->wcmNumDevices - 1] = pDev;
		return 1;
	}
	return 0;
}

static void xf86WcmParseMode(LocalDevicePtr dev, LexPtr val, int token)
{
	WacomDevicePtr priv = (WacomDevicePtr)(dev->private);
	switch (token)
	{
		case ABSOLUTE:
			priv->flags = priv->flags | ABSOLUTE_FLAG;
			break;
		case RELATIVE:
			priv->flags = priv->flags & ~ABSOLUTE_FLAG; 
			break;
		default:
			xf86ConfigError("Illegal Mode type");
			break;
	}
}

static void xf86WcmParseToken(LocalDevicePtr dev, LexPtr val, int token)
{
	WacomDevicePtr priv = (WacomDevicePtr)(dev->private);
	WacomCommonPtr common = priv->common;
	int mtoken;

	switch(token)
	{
		case DEVICENAME:
			if (xf86GetToken(NULL) != STRING)
				xf86ConfigError("Option string expected");
			dev->name = strdup(val->str);
			if (xf86Verbose)
				ErrorF("%s Wacom X device name is %s\n",
					XCONFIG_GIVEN, dev->name);
			break;     

		case THE_MODE:
			mtoken = xf86GetToken(ModeTabRec);
			if ((mtoken==EOF)||(mtoken==STRING)||(mtoken==NUMBER)) 
				xf86ConfigError("Mode type token expected");
			else
				xf86WcmParseMode(dev,val,mtoken);
			break;

		case SUPPRESS:
			if (xf86GetToken(NULL) != NUMBER)
				xf86ConfigError("Option number expected");
			common->wcmSuppress = val->num;
			if ((common->wcmSuppress > MAX_SUPPRESS || 
				common->wcmSuppress < DEFAULT_SUPPRESS) &&
				(common->wcmSuppress != 0) /* disabled */)
			{
				common->wcmSuppress = DEFAULT_SUPPRESS;
			}
			if (xf86Verbose)
				ErrorF("%s Wacom suppress value is %d\n",
					XCONFIG_GIVEN, common->wcmSuppress);
			break;

		case DEBUG_LEVEL:
			if (xf86GetToken(NULL) != NUMBER)
				xf86ConfigError("Option number expected");
			gWacomModule.debugLevel = val->num;
			if (xf86Verbose)
			{
#if DEBUG
				ErrorF("%s Wacom debug level sets to %d\n",
					XCONFIG_GIVEN, gWacomModule.debugLevel);
#else
				ErrorF("%s Wacom debug level not sets to %d "
					"because debugging is not compiled\n",
					XCONFIG_GIVEN, gWacomModule.debugLevel);
#endif
			}
			break;

		case TILT_MODE:
			common->wcmFlags |= TILT_REQUEST_FLAG;
			break;

		case HISTORY_SIZE:
			if (xf86GetToken(NULL) != NUMBER)
				xf86ConfigError("Option number expected");
			dev->history_size = val->num;
			if (xf86Verbose)
				ErrorF("%s Wacom Motion history size is %d\n",
					XCONFIG_GIVEN, dev->history_size);
			break;

		case ALWAYS_CORE:
			xf86AlwaysCore(dev, TRUE);
			if (xf86Verbose)
				ErrorF("%s Wacom device always stays core "
					"pointer\n", XCONFIG_GIVEN);
			break;

		case KEEP_SHAPE:
			priv->flags |= KEEP_SHAPE_FLAG;
			if (xf86Verbose)
				ErrorF("%s Wacom keeps shape\n", XCONFIG_GIVEN);
			break;

		case TOP_X:
			if (xf86GetToken(NULL) != NUMBER)
				xf86ConfigError("Option number expected");
			priv->topX = val->num;
			if (xf86Verbose)
				ErrorF("%s Wacom top x = %d\n",
					XCONFIG_GIVEN, priv->topX);
			break;

		case TOP_Y:
			if (xf86GetToken(NULL) != NUMBER)
				xf86ConfigError("Option number expected");
			priv->topY = val->num;
			if (xf86Verbose)
				ErrorF("%s Wacom top y = %d\n",
					XCONFIG_GIVEN, priv->topY);
			break;

		case BOTTOM_X:
			if (xf86GetToken(NULL) != NUMBER)
				xf86ConfigError("Option number expected");
			priv->bottomX = val->num;
			if (xf86Verbose)
				ErrorF("%s Wacom bottom x = %d\n",
					XCONFIG_GIVEN, priv->bottomX);
			break;

		case BOTTOM_Y:
			if (xf86GetToken(NULL) != NUMBER)
				xf86ConfigError("Option number expected");
			priv->bottomY = val->num;
			if (xf86Verbose)
				ErrorF("%s Wacom bottom y = %d\n",
					XCONFIG_GIVEN, priv->bottomY);
			break;

		case SERIAL:
			if (xf86GetToken(NULL) != NUMBER)
				xf86ConfigError("Option number expected");
			priv->serial = val->num;
			if (xf86Verbose)
				ErrorF("%s Wacom serial number = %u\n",
					XCONFIG_GIVEN, priv->serial);
			break;

		case BAUD_RATE:
			if (xf86GetToken(NULL) != NUMBER)
				xf86ConfigError("Option number expected");
			switch(val->num)
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
					xf86ConfigError("Illegal speed value");
					break;
			}
			if (xf86Verbose)
				ErrorF("%s Wacom baud rate of %u\n",
						XCONFIG_GIVEN, val->num);
			break;

		case THRESHOLD:
			if (xf86GetToken(NULL) != STRING)
				xf86ConfigError("Option string expected");

			common->wcmThreshold = atoi(val->str);

			if (xf86Verbose)
				ErrorF("%s Wacom pressure threshold for "
					"button 1 = %d\n", XCONFIG_GIVEN,
					common->wcmThreshold);
			break;

		case MAX_X:
			if (xf86GetToken(NULL) != NUMBER)
				xf86ConfigError("Option number expected");
			common->wcmMaxX = val->num;
			if (xf86Verbose)
				ErrorF("%s Wacom max x = %d\n", XCONFIG_GIVEN,
					common->wcmMaxX);
			break;

		case MAX_Y:
			if (xf86GetToken(NULL) != NUMBER)
				xf86ConfigError("Option number expected");
			common->wcmMaxY = val->num;
			if (xf86Verbose)
				ErrorF("%s Wacom max y = %d\n", XCONFIG_GIVEN,
					common->wcmMaxY);
			break;

		case MAX_Z:
			if (xf86GetToken(NULL) != NUMBER)
				xf86ConfigError("Option number expected");
			common->wcmMaxZ = val->num;
			if (xf86Verbose)
				ErrorF("%s Wacom max z = %d\n", XCONFIG_GIVEN,
					common->wcmMaxZ);
			break;

		case RESOLUTION_X:
			if (xf86GetToken(NULL) != NUMBER)
				xf86ConfigError("Option number expected");
			common->wcmUserResolX = val->num;
			if (xf86Verbose)
				ErrorF("%s Wacom resolution x = %d\n",
					XCONFIG_GIVEN, common->wcmUserResolX);
			break;

		case RESOLUTION_Y:
			if (xf86GetToken(NULL) != NUMBER)
				xf86ConfigError("Option number expected");
			common->wcmUserResolY = val->num;
			if (xf86Verbose)
				ErrorF("%s Wacom resolution y = %d\n",
					XCONFIG_GIVEN, common->wcmUserResolY);
			break;

		case RESOLUTION_Z:
			if (xf86GetToken(NULL) != NUMBER)
				xf86ConfigError("Option number expected");
			common->wcmUserResolZ = val->num;
			if (xf86Verbose)
				ErrorF("%s Wacom resolution z = %d\n",
					XCONFIG_GIVEN, common->wcmUserResolZ);
			break;

		case USB:
#ifdef LINUX_INPUT
			ErrorF("%s Wacom reading USB link\n", XCONFIG_GIVEN);
#else
			ErrorF("The USB version of the driver isn't available "
				"for your platform\n");
#endif
			break;

		case SCREEN_NO:
			if (xf86GetToken(NULL) != NUMBER)
				xf86ConfigError("Option number expected");
			priv->screen_no = val->num;
			if (xf86Verbose)
				ErrorF("%s Wacom attached screen = %d\n",
					XCONFIG_GIVEN, priv->screen_no);
			break;

		case BUTTONS_ONLY:
			priv->flags |= BUTTONS_ONLY_FLAG;
			if (xf86Verbose)
				ErrorF("%s Wacom set buttons only\n",
					XCONFIG_GIVEN);
			break;

		case TWINVIEW:
			mtoken = xf86GetToken(TwinViewTabRec);
			if ((mtoken == EOF) || (mtoken == STRING) || (mtoken == NUMBER))
				xf86ConfigError("TwinView type token expected");
			else {
				switch (mtoken) {
					case TV_ABOVE_BELOW:
						priv->twinview = mtoken;
						priv->dscaleX = 1.0;
						priv->dscaleY = 2.0;
						priv->doffsetX = 0;
						priv->doffsetY = 0;
						break;
					case TV_LEFT_RIGHT:
						priv->twinview = mtoken;
						priv->dscaleX = 2.0;
						priv->dscaleY = 1.0;
						priv->doffsetX = 0;
						priv->doffsetY = 0;
						break;
					case TV_NONE:
						priv-twinview = TV_NONE;
						priv->dscaleX = 1.0;
						priv->dscaleY = 1.0;
						priv->doffsetX = 0;
						priv->doffsetY = 0;
						break;
					default:
						xf86ConfigError("Illegal TwinView type");
				}
			}
			break;

		case EOF:
			FatalError("Unexpected EOF (missing EndSubSection)");
			break;

		default:
			xf86ConfigError("Wacom subsection keyword expected");
			break;
	}
}

/* xf86WcmConfig - Configure the device. */

static Bool xf86WcmConfig(LocalDevicePtr* array, int inx, int max, LexPtr val)
{
	LocalDevicePtr dev = array[inx];
	WacomDevicePtr priv = (WacomDevicePtr)(dev->private);
	WacomCommonPtr common = priv->common;
	int token;
  
	DBG(1, ErrorF("xf86WcmConfig\n"));

	/* Parse port option */

	if (xf86GetToken(WcmTab) != PORT)
	{
		xf86ConfigError("PORT option must be the first option "
			"of a Wacom SubSection");
	}
  
	if (xf86GetToken(NULL) != STRING)
		xf86ConfigError("Option string expected");
	else
	{
		int loop;
  
		/* try to find another wacom device which share the same port */
		for(loop=0; loop<max; loop++)
		{
			if (loop == inx)
				continue;
			if (xf86WcmMatchDevice(array[loop],dev,val->str))
			{
				common = priv->common;
				break;
			}
		} /* next device */

		if (loop == max)
		{
			common->wcmDevice = strdup(val->str);
			if (xf86Verbose)
				ErrorF("%s Wacom port is %s\n", XCONFIG_GIVEN,
					common->wcmDevice);
		}
	}

	while ((token = xf86GetToken(WcmTab)) != ENDSUBSECTION)
	{
		xf86WcmParseToken(dev,val,token);
	}
  
	DBG(1, ErrorF("xf86WcmConfig name=%s\n", common->wcmDevice));
  
	return Success;
}

DeviceAssocRec wacom_stylus_assoc =
{
	STYLUS_SECTION_NAME,        /* config_section_name */
	xf86WcmAllocateStylus       /* device_allocate */
};

DeviceAssocRec wacom_cursor_assoc =
{
	CURSOR_SECTION_NAME,        /* config_section_name */
	xf86WcmAllocateCursor       /* device_allocate */
};

DeviceAssocRec wacom_eraser_assoc =
{
	ERASER_SECTION_NAME,        /* config_section_name */
	xf86WcmAllocateEraser       /* device_allocate */
};

/******************************************************************************
 * XFree86 V3 Dynamic Module Initialization
 *****************************************************************************/

#ifdef DYNAMIC_MODULE

/* module entry point */

#ifndef DLSYM_BUG
#define init_xf86Wacom init_module
#endif

int init_xf86Wacom(unsigned long server_version)
{
	xf86AddDeviceAssoc(&wacom_stylus_assoc);
	xf86AddDeviceAssoc(&wacom_cursor_assoc);
	xf86AddDeviceAssoc(&wacom_eraser_assoc);

	if (server_version != XF86_VERSION_CURRENT)
	{
		ErrorF("Warning: Wacom module compiled for version%s\n",
				XF86_VERSION);
		return 0;
	}

	return 1;
}

#endif /* DYNAMIC_MODULE */

/******************************************************************************
 * XFree86 V4 Module Configuration
 *****************************************************************************/

#elif XFREE86_V4

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

	if ((pLocal != pMatch) &&
		(pMatch->device_control == gWacomModule.DevProc) &&
		!strcmp(privMatch->common->wcmDevice, common->wcmDevice))
	{
		DBG(2, ErrorF("xf86WcmInit wacom port share between"
				" %s and %s\n", pLocal->name, pMatch->name));
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

static InputInfoPtr xf86WcmInit(InputDriverPtr drv, IDevPtr dev, int flags)
{
	LocalDevicePtr local = NULL;
	LocalDevicePtr fakeLocal = NULL;
	WacomDevicePtr priv = NULL;
	WacomCommonPtr common = NULL;
	char* 		s, b[10];
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
	else
	{
		xf86Msg(X_ERROR, "%s: No type or invalid type specified.\n"
				"Must be one of stylus, cursor or eraser\n",
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

	xf86Msg(X_CONFIG, "%s serial device is %s\n", dev->identifier,
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

	common->wcmRotate=ROTATE_NONE;

	s = xf86FindOptionValue(local->options, "Rotate");

	if (s)
	{
		if (xf86NameCmp(s, "CW") == 0)
			common->wcmRotate=ROTATE_CW;
		else if (xf86NameCmp(s, "CCW") ==0)
			common->wcmRotate=ROTATE_CCW;
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
		/* best effort attempt at loading the wacom and evdev
		 * kernel modules */
		(void)xf86LoadKernelModule("wacom");
		(void)xf86LoadKernelModule("evdev");
    
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
	if (s)
	{
		int a,b,c,d;
		if ((sscanf(s,"%d,%d,%d,%d",&a,&b,&c,&d) != 4) ||
			(a < 0) || (a > 100) || (b < 0) || (b > 100) ||
			(c < 0) || (c > 100) || (d < 0) || (d > 100))
			xf86Msg(X_CONFIG, "WACOM: PressCurve not valid\n");
		else
		{
			xf86WcmSetPressureCurve(priv,a,b,c,d);
			xf86Msg(X_CONFIG, "WACOM: PressCurve %d,%d %d,%d\n",
				a,b,c,d);
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

	common->wcmTPCButton = xf86SetBoolOption(local->options, "TPCButton", 0);
	if ( common->wcmTPCButton )
		xf86Msg(X_CONFIG, "Tablet PC buttons on \n");

	for (i=0; i<16; i++)
	{
		sprintf(b, "Button%d", i+1);
		oldButton = priv->button[i];
		priv->button[i] = xf86SetIntOption(local->options, b, priv->button[i]);
		if (oldButton != priv->button[i])
		{
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

		if (xf86Verbose)
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
		priv->dscaleX = 1.0;
		priv->dscaleY = 1.0;
		priv->doffsetX = 0;
		priv->doffsetY = 0;
	}
	else if (s && xf86NameCmp(s, "horizontal") == 0) 
	{
		priv->twinview = TV_LEFT_RIGHT;
		priv->dscaleX = 2.0;
		priv->dscaleY = 1.0;
		priv->doffsetX = 0;
		priv->doffsetY = 0;
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
		priv->dscaleX = 1.0;
		priv->dscaleY = 2.0;
		priv->doffsetX = 0;
		priv->doffsetY = 0;
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
		priv->dscaleX = 1.0;
		priv->dscaleY = 1.0;
		priv->doffsetX = 0;
		priv->doffsetY = 0;
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

#endif /* XFREE86_V4 */

