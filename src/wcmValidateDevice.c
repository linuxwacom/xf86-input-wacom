/*
 * Copyright 2009 - 2010 by Ping Cheng, Wacom. <pingc@wacom.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "xf86Wacom.h"
#include "wcmFilter.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* wcmCheckSource - Check if there is another source defined this device
 * before or not: don't add the tool by hal/udev if user has defined at least
 * one tool for the device in xorg.conf. One device can have multiple tools
 * with the same type to individualize tools with serial number or areas */
static Bool wcmCheckSource(LocalDevicePtr local, dev_t min_maj)
{
	int match = 0;
	char* device;
	char* fsource = xf86CheckStrOption(local->options, "_source", "");
	LocalDevicePtr pDevices = xf86FirstLocalDevice();
	WacomCommonPtr pCommon = NULL;
	char* psource;

	for (; pDevices != NULL; pDevices = pDevices->next)
	{
		device = xf86CheckStrOption(pDevices->options, "Device", NULL);

		/* device can be NULL on some distros */
		if (!device || !strstr(pDevices->drv->driverName, "wacom"))
			continue;

		if (local != pDevices)
		{
			psource = xf86CheckStrOption(pDevices->options, "_source", "");
			pCommon = ((WacomDevicePtr)pDevices->private)->common;
			if (pCommon->min_maj &&
				pCommon->min_maj == min_maj)
			{
				/* only add the new tool if the matching major/minor
				* was from the same source */
				if (strcmp(fsource, psource))
				{
					match = 1;
					break;
				}
			}
		}
	}
	if (match)
		xf86Msg(X_WARNING, "%s: device file already in use by %s. "
			"Ignoring.\n", local->name, pDevices->name);
	return match;
}

/* check if the device has been added.
 * Open the device and check it's major/minor, then compare this with every
 * other wacom device listed in the config. If they share the same
 * major/minor and the same source/type, fail.
 * This is to detect duplicate devices if a device was added once through
 * the xorg.conf and is then hotplugged through the server backend (HAL,
 * udev). In this case, the hotplugged one fails.
 */
int wcmIsDuplicate(char* device, LocalDevicePtr local)
{
	struct stat st;
	int isInUse = 0;
	char* lsource = xf86CheckStrOption(local->options, "_source", "");

	/* always allow xorg.conf defined tools to be added */
	if (!strlen(lsource)) goto ret;

	if (fstat(local->fd, &st) == -1)
	{
		/* can not access major/minor to check device duplication */
		xf86Msg(X_ERROR, "%s: stat failed (%s). cannot check for duplicates.\n",
				local->name, strerror(errno));

		/* older systems don't support the required ioctl.  let it pass */
		goto ret;
	}

	if (st.st_rdev)
	{
		/* device matches with another added port */
		if (wcmCheckSource(local, st.st_rdev))
		{
			isInUse = 3;
			goto ret;
		}
	}
	else
	{
		/* major/minor can never be 0, right? */
		xf86Msg(X_ERROR, "%s: device opened with a major/minor of 0. "
			"Something was wrong.\n", local->name);
		isInUse = 4;
	}
ret:
	return isInUse;
}

static struct
{
	const char* type;
	__u16 tool;
} wcmType [] =
{
	{ "stylus", BTN_TOOL_PEN       },
	{ "eraser", BTN_TOOL_RUBBER    },
	{ "cursor", BTN_TOOL_MOUSE     },
	{ "touch",  BTN_TOOL_DOUBLETAP },
	{ "pad",    BTN_TOOL_FINGER    }
};

/* validate tool type for device/product */
Bool wcmIsAValidType(LocalDevicePtr local, const char* type)
{
	int j, ret = FALSE;
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	char* dsource = xf86CheckStrOption(local->options, "_source", "");

	if (!type)
		return FALSE;

	/* walkthrough all types */
	for (j = 0; j < ARRAY_SIZE(wcmType); j++)
	{
		if (!strcmp(wcmType[j].type, type))
		{
			if (ISBITSET (common->wcmKeys, wcmType[j].tool))
			{
				ret = TRUE;
				break;
			}
			else if (!strlen(dsource)) /* an user defined type */
			{
				/* assume it is a valid type */
				SETBIT(common->wcmKeys, wcmType[j].tool);
				ret = TRUE;
				break;
			}
		}
	}
	return ret;
}

/* Choose valid types according to device ID. */
int wcmDeviceTypeKeys(LocalDevicePtr local)
{
	int ret = 1;
	WacomDevicePtr priv = local->private;

	/* serial ISDV4 devices */
	priv->common->tablet_id = isdv4ProbeKeys(local);
	if (!priv->common->tablet_id) /* USB devices */
		priv->common->tablet_id = usbProbeKeys(local);

	switch (priv->common->tablet_id)
	{
		/* tablets with touch ring and rotation pen*/
		case 0xB8:  /* I4 */
		case 0xB9:  /* I4 */
		case 0xBA:  /* I4 */
		case 0xBB:  /* I4 */
			priv->common->tablet_type = WCM_ROTATION;
			/* fall through */

		/* tablets with touch ring */
		case 0x17:  /* BambooFun */
		case 0x18:  /* BambooFun */
			priv->common->tablet_type |= WCM_RING;
			break;

		/* tablets support dual input */
		case 0x20:  /* I1 */
		case 0x21:  /* I1 */
		case 0x22:  /* I1 */
		case 0x23:  /* I1 */
		case 0x24:  /* I1 */
		case 0x41:  /* I2 */
		case 0x42:  /* I2 */
		case 0x43:  /* I2 */
		case 0x44:  /* I2 */
		case 0x45:  /* I2 */
		case 0x47:  /* I2 */
			priv->common->tablet_type = WCM_DUALINPUT;
			break;

		/* tablets support menu strips */
		case 0x3F:  /* CintiqV5 */
		case 0xC5:  /* CintiqV5 */
		case 0xC6:  /* CintiqV5 */
			priv->common->tablet_type |= WCM_LCD;
			/* fall through */
		case 0xB0:  /* I3 */
		case 0xB1:  /* I3 */
		case 0xB2:  /* I3 */
		case 0xB3:  /* I3 */
		case 0xB4:  /* I3 */
		case 0xB5:  /* I3 */
		case 0xB7:  /* I3 */
			priv->common->tablet_type = WCM_STRIP | WCM_ROTATION;
			break;

		case 0xE2: /* TPC with 2FGT */
		case 0xE3: /* TPC with 2FGT */
			priv->common->tablet_type = WCM_TPC;
			priv->common->tablet_type |= WCM_LCD;
			/* fall through */
		case 0xD0:  /* Bamboo with 2FGT */
		case 0xD1:  /* Bamboo with 2FGT */
		case 0xD2:  /* Bamboo with 2FGT */
		case 0xD3:  /* Bamboo with 2FGT */
			priv->common->tablet_type |= WCM_2FGT;
			break;

		case 0x93: /* TPC with 1FGT */
		case 0x9A: /* TPC with 1FGT */
			priv->common->tablet_type = WCM_1FGT;
			/* fall through */
		case 0x90: /* TPC */
			priv->common->tablet_type |= WCM_TPC;
			priv->common->tablet_type |= WCM_LCD;
			break;

		case 0x9F:
			priv->common->tablet_type = WCM_1FGT;
			priv->common->tablet_type |= WCM_LCD;
			break;

		default:
			priv->common->tablet_type = WCM_PEN;
	}

	return ret;
}

/**
 * Duplicate xf86 options, replace the "type" option with the given type
 * (and the name with "$name $type" and convert them to InputOption */
static InputOption *wcmOptionDupConvert(LocalDevicePtr local, const char* basename, const char *type)
{
	pointer original = local->options;
	InputOption *iopts = NULL, *new;
	InputInfoRec dummy;
	char *name;

	memset(&dummy, 0, sizeof(dummy));
	xf86CollectInputOptions(&dummy, NULL, original);

	name = Xprintf("%s %s", basename, type);

	dummy.options = xf86ReplaceStrOption(dummy.options, "Type", type);
	dummy.options = xf86ReplaceStrOption(dummy.options, "Name", name);
	free(name);

	while(dummy.options)
	{
		new = calloc(1, sizeof(InputOption));

		new->key = xf86OptionName(dummy.options);
		new->value = xf86OptionValue(dummy.options);
		new->next = iopts;
		iopts = new;
		dummy.options = xf86NextOption(dummy.options);
	}
	return iopts;
}

static void wcmFreeInputOpts(InputOption* opts)
{
	InputOption *tmp = opts;
	while(opts)
	{
		tmp = opts->next;
		free(opts->key);
		free(opts->value);
		free(opts);
		opts = tmp;
	}
}

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 11
/**
 * Duplicate the attributes of the given device. "product" gets the type
 * appended, so a device of product "Wacom" will then have a product "Wacom
 * eraser", "Wacom cursor", etc.
 */
static InputAttributes* wcmDuplicateAttributes(LocalDevicePtr local,
					       const char *type)
{
	InputAttributes *attr;
	attr = DuplicateInputAttributes(local->attrs);
	attr->product = Xprintf("%s %s", attr->product, type);
	return attr;
}
#endif

/**
 * Hotplug one device of the given type.
 * Device has the same options as the "parent" device, type is one of
 * erasor, stylus, pad, touch, cursor, etc.
 * Name of the new device is set automatically to "<device name> <type>".
 */
static void wcmHotplug(LocalDevicePtr local, const char* basename, const char *type)
{
	DeviceIntPtr dev; /* dummy */
	InputOption *input_options;
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 9
	InputAttributes *attrs = NULL;
#endif

	input_options = wcmOptionDupConvert(local, basename, type);

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 11
	attrs = wcmDuplicateAttributes(local, type);
#endif

	NewInputDeviceRequest(input_options,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 9
				attrs,
#endif
				&dev);
	wcmFreeInputOpts(input_options);

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 11
	FreeInputAttributes(attrs);
#endif
}

void wcmHotplugOthers(LocalDevicePtr local, const char *basename)
{
	int i, skip = 1;
	char*		device;

        xf86Msg(X_INFO, "%s: hotplugging dependent devices.\n", local->name);
	device = xf86SetStrOption(local->options, "Device", NULL);
        /* same loop is used to init the first device, if we get here we
         * need to start at the second one */
	for (i = 0; i < ARRAY_SIZE(wcmType); i++)
	{
		if (wcmIsAValidType(local, wcmType[i].type))
		{
			if (skip)
				skip = 0;
			else
				wcmHotplug(local, basename, wcmType[i].type);
		}
	}
        xf86Msg(X_INFO, "%s: hotplugging completed.\n", local->name);
}

/**
 * Return 1 if the device needs auto-hotplugging from within the driver.
 * This is the case if we don't get passed a "type" option (invalid in
 * xorg.conf configurations) and we come from HAL, udev or whatever future
 * config backend.
 *
 * This changes the source to _driver/wacom, all auto-hotplugged devices
 * will have the same source.
 */
int wcmNeedAutoHotplug(LocalDevicePtr local, const char **type)
{
	char *source = xf86CheckStrOption(local->options, "_source", "");
	int i;

	if (*type) /* type specified, don't hotplug */
		return 0;

	if (strcmp(source, "server/hal") && strcmp(source, "server/udev"))
		return 0;

	/* no type specified, so we need to pick the first one applicable
	 * for our device */
	for (i = 0; i < ARRAY_SIZE(wcmType); i++)
	{
		if (wcmIsAValidType(local, wcmType[i].type))
		{
			*type = strdup(wcmType[i].type);
			break;
		}
	}

	if (!*type)
		return 0;

	xf86Msg(X_INFO, "%s: type not specified, assuming '%s'.\n", local->name, *type);
	xf86Msg(X_INFO, "%s: other types will be automatically added.\n", local->name);

	local->options = xf86AddNewOption(local->options, "Type", *type);
	local->options = xf86ReplaceStrOption(local->options, "_source", "_driver/wacom");

	return 1;
}

int wcmParseOptions(LocalDevicePtr local, int hotplugged)
{
	WacomDevicePtr  priv = (WacomDevicePtr)local->private;
	WacomCommonPtr  common = priv->common;
	char            *s, b[12];
	int		i, oldButton, baud;
	WacomToolPtr    tool = NULL;
	WacomToolAreaPtr area = NULL;

	/* Optional configuration */
	priv->debugLevel = xf86SetIntOption(local->options,
					    "DebugLevel", priv->debugLevel);
	common->debugLevel = xf86SetIntOption(local->options,
					      "CommonDBG", common->debugLevel);
	s = xf86SetStrOption(local->options, "Mode", NULL);

	if (s && (xf86NameCmp(s, "absolute") == 0))
		set_absolute(local, TRUE);
	else if (s && (xf86NameCmp(s, "relative") == 0))
		set_absolute(local, FALSE);
	else
	{
		if (s)
			xf86Msg(X_ERROR, "%s: invalid Mode (should be absolute"
				" or relative). Using default.\n", local->name);

		/* If Mode not specified or is invalid then rely on
		 * Type specific defaults from initialization.
		 */
	}

	/* Pad is always in relative mode.
	 * The pad also defaults to wheel scrolling, unlike the pens
	 * (interesting effects happen on ArtPen and others with build-in
	 * wheels)
	 */
	if (IsPad(priv))
	{
		priv->wheelup = 4;
		priv->wheeldn = 5;
		set_absolute(local, FALSE);
	}

	/* Store original local Core flag so it can be changed later */
	if (local->flags & (XI86_ALWAYS_CORE | XI86_CORE_POINTER))
		priv->flags |= COREEVENT_FLAG;

	s = xf86SetStrOption(local->options, "Rotate", NULL);

	if (s)
	{
		if (xf86NameCmp(s, "CW") == 0)
			common->wcmRotate=ROTATE_CW;
		else if (xf86NameCmp(s, "CCW") ==0)
			common->wcmRotate=ROTATE_CCW;
		else if (xf86NameCmp(s, "HALF") ==0)
			common->wcmRotate=ROTATE_HALF;
		else if (xf86NameCmp(s, "NONE") !=0)
		{
			xf86Msg(X_ERROR, "%s: invalid Rotate option '%s'.\n",
				local->name, s);
			goto error;
		}
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

	if (xf86SetBoolOption(local->options, "Tilt",
			(common->wcmFlags & TILT_REQUEST_FLAG)))
		common->wcmFlags |= TILT_REQUEST_FLAG;

	if (xf86SetBoolOption(local->options, "RawFilter",
			(common->wcmFlags & RAW_FILTERING_FLAG)))
		common->wcmFlags |= RAW_FILTERING_FLAG;

	/* pressure curve takes control points x1,y1,x2,y2
	 * values in range from 0..100.
	 * Linear curve is 0,0,100,100
	 * Slightly depressed curve might be 5,0,100,95
	 * Slightly raised curve might be 0,5,95,100
	 */
	s = xf86SetStrOption(local->options, "PressCurve", "0,0,100,100");
	if (s && (IsStylus(priv) || IsEraser(priv)))
	{
		int a,b,c,d;
		if ((sscanf(s,"%d,%d,%d,%d",&a,&b,&c,&d) != 4) ||
				!wcmCheckPressureCurveValues(a, b, c, d))
			xf86Msg(X_CONFIG, "%s: PressCurve not valid\n",
				local->name);
		else
			wcmSetPressureCurve(priv,a,b,c,d);
	}

	if (IsCursor(priv))
	{
		common->wcmCursorProxoutDist = xf86SetIntOption(local->options, "CursorProx", 0);
		if (common->wcmCursorProxoutDist < 0 || common->wcmCursorProxoutDist > 255)
			xf86Msg(X_CONFIG, "%s: CursorProx invalid %d \n",
				local->name, common->wcmCursorProxoutDist);
	}

	/* Configure Monitors' resoluiton in TwinView setup.
	 * The value is in the form of "1024x768,1280x1024"
	 * for a desktop of monitor 1 at 1024x768 and
	 * monitor 2 at 1280x1024
	 */
	s = xf86SetStrOption(local->options, "TVResolution", NULL);
	if (s)
	{
		int a,b,c,d;
		if ((sscanf(s,"%dx%d,%dx%d",&a,&b,&c,&d) != 4) ||
			(a <= 0) || (b <= 0) || (c <= 0) || (d <= 0))
			xf86Msg(X_CONFIG, "%s: TVResolution not valid\n",
				local->name);
		else
		{
			priv->tvResolution[0] = a;
			priv->tvResolution[1] = b;
			priv->tvResolution[2] = c;
			priv->tvResolution[3] = d;
		}
	}

	priv->screen_no = xf86SetIntOption(local->options, "ScreenNo", -1);

	if (xf86SetBoolOption(local->options, "KeepShape", 0))
		priv->flags |= KEEP_SHAPE_FLAG;

	priv->topX = xf86SetIntOption(local->options, "TopX", 0);
	priv->topY = xf86SetIntOption(local->options, "TopY", 0);
	priv->bottomX = xf86SetIntOption(local->options, "BottomX", 0);
	priv->bottomY = xf86SetIntOption(local->options, "BottomY", 0);
	priv->serial = xf86SetIntOption(local->options, "Serial", 0);

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

			free(tool);
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

	common->wcmMaxZ = xf86SetIntOption(local->options, "MaxZ",
					   common->wcmMaxZ);
	if (xf86SetBoolOption(local->options, "ButtonsOnly", 0))
		priv->flags |= BUTTONS_ONLY_FLAG;

	/* TPCButton on for Tablet PC by default */
	if (TabletHasFeature(common, WCM_TPC))
		common->wcmTPCButtonDefault = 1;

	oldButton = xf86SetBoolOption(local->options, "TPCButton",
					common->wcmTPCButtonDefault);

	if (hotplugged || IsStylus(priv))
		common->wcmTPCButton = oldButton;
	else if (oldButton != common->wcmTPCButton)
		xf86Msg(X_WARNING, "%s: TPCButton option can only be set "
			"by stylus.\n", local->name);

	/* a single touch device */
	if (ISBITSET (common->wcmKeys, BTN_TOOL_DOUBLETAP))
	{
		/* TouchDefault was off for all devices
		 * except when touch is supported */
		common->wcmTouchDefault = 1;

		oldButton = xf86SetBoolOption(local->options, "Touch",
					common->wcmTouchDefault);

		if (hotplugged || IsTouch(priv))
			common->wcmTouch = oldButton;
		else if (oldButton != common->wcmTouch)
			xf86Msg(X_WARNING, "%s: Touch option can only be set "
				"by a touch tool.\n", local->name);

		oldButton = xf86SetBoolOption(local->options, "Capacity",
					common->wcmCapacityDefault);

		if (hotplugged || IsTouch(priv))
			common->wcmCapacity = oldButton;
		else if (oldButton != common->wcmCapacity)
			xf86Msg(X_WARNING, "%s: Touch Capacity option can only be"
				"set by a touch tool.\n", local->name);
	}

	/* 2FG touch device */
	if (ISBITSET (common->wcmKeys, BTN_TOOL_TRIPLETAP))
	{
		/* GestureDefault was off for all devices
		 * except when multi-touch is supported */
		common->wcmGestureDefault = 1;

		oldButton = xf86SetBoolOption(local->options, "Gesture",
					common->wcmGestureDefault);

		if (hotplugged || IsTouch(priv))
			common->wcmGesture = oldButton;
		else if (oldButton != common->wcmGesture)
			xf86Msg(X_WARNING, "%s: Touch gesture option can only "
				"be set by a touch tool.\n", local->name);

		if ((common->wcmDevCls == &gWacomUSBDevice) &&
				TabletHasFeature(common, WCM_LCD) &&
				TabletHasFeature(common, WCM_2FGT)) {
			common->wcmGestureParameters.wcmZoomDistanceDefault = 30;
			common->wcmGestureParameters.wcmScrollDistanceDefault = 30;
			common->wcmGestureParameters.wcmTapTimeDefault = 250;
		}

		common->wcmGestureParameters.wcmZoomDistance =
			xf86SetIntOption(local->options, "ZoomDistance",
			common->wcmGestureParameters.wcmZoomDistanceDefault);

		common->wcmGestureParameters.wcmScrollDistance =
			xf86SetIntOption(local->options, "ScrollDistance",
			common->wcmGestureParameters.wcmScrollDistanceDefault);

		common->wcmGestureParameters.wcmTapTime =
			xf86SetIntOption(local->options, "TapTime",
			common->wcmGestureParameters.wcmTapTimeDefault);
	}

	/* Mouse cursor stays in one monitor in a multimonitor setup */
	if ( !priv->wcmMMonitor )
		priv->wcmMMonitor = xf86SetBoolOption(local->options, "MMonitor", 1);

	for (i=0; i<WCM_MAX_BUTTONS; i++)
	{
		sprintf(b, "Button%d", i+1);
		priv->button[i] = xf86SetIntOption(local->options, b, priv->button[i]);
	}

	baud = xf86SetIntOption(local->options, "BaudRate", 38400);

	switch (baud)
	{
		case 38400:
		case 19200:
			common->wcmISDV4Speed = baud;
			break;
		default:
			xf86Msg(X_ERROR, "%s: Illegal speed value "
					"(must be 19200 or 38400).",
					local->name);
			break;
	}

	s = xf86SetStrOption(local->options, "Twinview", NULL);
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
			local->name);
		priv->twinview = TV_NONE;
	}

	if (s && priv->twinview != TV_NONE)
		priv->numScreen = 2;
	else
		priv->numScreen = screenInfo.numScreens;

	return 1;
error:
	free(area);
	free(tool);
	return 0;
}

int wcmAutoProbeDevice(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	WacomCommonPtr common =  priv->common;

	if ((!common->device_path || !strcmp (common->device_path, "auto-dev")))
	{
		common->wcmFlags |= AUTODEV_FLAG;
		if (! (common->device_path = wcmEventAutoDevProbe (local)))
		{
			xf86Msg(X_ERROR, "%s: unable to probe device\n",
				local->name);
			return 0;
		}
	}
	return 1;
}
/* vim: set noexpandtab shiftwidth=8: */
