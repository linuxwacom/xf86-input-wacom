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
static Bool wcmCheckSource(InputInfoPtr pInfo, dev_t min_maj)
{
	int match = 0;
	char* device;
	char* fsource = xf86CheckStrOption(pInfo->options, "_source", "");
	InputInfoPtr pDevices = xf86FirstLocalDevice();
	WacomCommonPtr pCommon = NULL;
	char* psource;

	for (; pDevices != NULL; pDevices = pDevices->next)
	{
		device = xf86CheckStrOption(pDevices->options, "Device", NULL);

		/* device can be NULL on some distros */
		if (!device || !strstr(pDevices->drv->driverName, "wacom"))
			continue;

		if (pInfo != pDevices)
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
			"Ignoring.\n", pInfo->name, pDevices->name);
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
int wcmIsDuplicate(char* device, InputInfoPtr pInfo)
{
	struct stat st;
	int isInUse = 0;
	char* lsource = xf86CheckStrOption(pInfo->options, "_source", "");

	/* always allow xorg.conf defined tools to be added */
	if (!strlen(lsource)) goto ret;

	if (stat(device, &st) == -1)
	{
		/* can not access major/minor to check device duplication */
		xf86Msg(X_ERROR, "%s: stat failed (%s). cannot check for duplicates.\n",
				pInfo->name, strerror(errno));

		/* older systems don't support the required ioctl.  let it pass */
		goto ret;
	}

	if (st.st_rdev)
	{
		/* device matches with another added port */
		if (wcmCheckSource(pInfo, st.st_rdev))
		{
			isInUse = 3;
			goto ret;
		}
	}
	else
	{
		/* major/minor can never be 0, right? */
		xf86Msg(X_ERROR, "%s: device opened with a major/minor of 0. "
			"Something was wrong.\n", pInfo->name);
		isInUse = 4;
	}
ret:
	return isInUse;
}

static struct
{
	const char* type;
	__u16 tool[3]; /* tool array is terminated by 0 */
} wcmType [] =
{
	{ "stylus", { BTN_TOOL_PEN,       0                  } },
	{ "eraser", { BTN_TOOL_RUBBER,    0                  } },
	{ "cursor", { BTN_TOOL_MOUSE,     0                  } },
	{ "touch",  { BTN_TOOL_DOUBLETAP, BTN_TOOL_FINGER, 0 } },
	{ "pad",    { BTN_FORWARD,        BTN_0,           0 } }
};

/* validate tool type for device/product */
Bool wcmIsAValidType(InputInfoPtr pInfo, const char* type)
{
	int j, k, ret = FALSE;
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;
	char* dsource = xf86CheckStrOption(pInfo->options, "_source", "");

	if (!type)
		return FALSE;

	/* walkthrough all types */
	for (j = 0; j < ARRAY_SIZE(wcmType); j++)
	{
		if (!strcmp(wcmType[j].type, type))
		{
			for (k = 0; wcmType[j].tool[k] && !ret; k++)
			{
				if (ISBITSET (common->wcmKeys, wcmType[j].tool[k]))
				{
					ret = TRUE;

					/* non GENERIC devices use BTN_TOOL_FINGER for pad */
					if (common->wcmProtocolLevel != WCM_PROTOCOL_GENERIC)
					{
						if (!strcmp(type, "touch") &&
							wcmType[j].tool[k] == BTN_TOOL_FINGER)
						    ret = FALSE;
					}
				}
				else if (!strlen(dsource)) /* an user defined type */
				{
					/* assume it is a valid type */
					SETBIT(common->wcmKeys, wcmType[j].tool[k]);
					ret = TRUE;
				}
			}
		}
	}
	return ret;
}

/* Choose valid types according to device ID. */
int wcmDeviceTypeKeys(InputInfoPtr pInfo)
{
	int ret = 1;
	WacomDevicePtr priv = pInfo->private;
	WacomCommonPtr common = priv->common;

	priv->common->tablet_id = common->wcmDevCls->ProbeKeys(pInfo);

	switch (priv->common->tablet_id)
	{
		case 0xCC:  /* CintiqV5 */
			priv->common->tablet_type = WCM_LCD;
			/* fall through */

		case 0xB8:  /* I4 */
		case 0xB9:  /* I4 */
		case 0xBA:  /* I4 */
		case 0xBB:  /* I4 */
		case 0xBC:  /* I4 */
		case 0xBD:  /* I4 */
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

		/* P4 display tablets */
		case 0x30:  /* PL400 */
		case 0x31:  /* PL500 */
		case 0x32:  /* PL600 */
		case 0x33:  /* PL600SX */
		case 0x34:  /* PL550 */
		case 0x35:  /* PL800 */
		case 0x37:  /* PL700 */
		case 0x38:  /* PL510 */
		case 0x39:  /* PL710 */
		case 0xC0:  /* DTF720 */
		case 0xC2:  /* DTF720a */
		case 0xC4:  /* DTF521 */
		case 0xC7:  /* DTU1931 */
		case 0xCE:  /* DTU2231 */
		case 0xF0:  /* DTU1631 */
			priv->common->tablet_type |= WCM_LCD;
			break;

		/* tablets support menu strips */
		case 0x3F:  /* CintiqV5 */
		case 0xC5:  /* CintiqV5 */
		case 0xC6:  /* CintiqV5 */
			priv->common->tablet_type = WCM_LCD;
			/* fall through */
		case 0xB0:  /* I3 */
		case 0xB1:  /* I3 */
		case 0xB2:  /* I3 */
		case 0xB3:  /* I3 */
		case 0xB4:  /* I3 */
		case 0xB5:  /* I3 */
		case 0xB7:  /* I3 */
			priv->common->tablet_type |= WCM_STRIP | WCM_ROTATION;
			break;

		case 0xE2: /* TPC with 2FGT */
		case 0xE3: /* TPC with 2FGT */
			priv->common->tablet_type = WCM_TPC;
			priv->common->tablet_type |= WCM_LCD;
			break;

		case 0x93: /* TPC with 1FGT */
		case 0x9A: /* TPC with 1FGT */
		case 0x90: /* TPC */
			priv->common->tablet_type |= WCM_TPC;
			priv->common->tablet_type |= WCM_LCD;
			break;

		case 0x9F:
			priv->common->tablet_type |= WCM_LCD;
			break;

		default:
			priv->common->tablet_type = WCM_PEN;
	}

	if (ISBITSET (common->wcmKeys, BTN_0) ||
			ISBITSET (common->wcmKeys, BTN_FORWARD))
	{
		priv->common->tablet_type |= WCM_PAD;
	}

	/* This handles both protocol 4 and 5 meanings of wcmKeys */
	if (common->wcmProtocolLevel == WCM_PROTOCOL_4)
	{
		/* TRIPLETAP means 2 finger touch */
		/* DOUBLETAP without TRIPLETAP means 1 finger touch */
		if (ISBITSET(common->wcmKeys, BTN_TOOL_TRIPLETAP))
			priv->common->tablet_type |= WCM_2FGT;
		else if (ISBITSET(common->wcmKeys, BTN_TOOL_DOUBLETAP))
			priv->common->tablet_type |= WCM_1FGT;
	}

	if (common->wcmProtocolLevel == WCM_PROTOCOL_GENERIC)
	{
		/* DOUBLETAP means 2 finger touch */
		/* FINGER without DOUBLETAP means 1 finger touch */
		if (ISBITSET(common->wcmKeys, BTN_TOOL_DOUBLETAP))
			priv->common->tablet_type |= WCM_2FGT;
		else if (ISBITSET(common->wcmKeys, BTN_TOOL_FINGER))
			priv->common->tablet_type |= WCM_1FGT;
	}

	return ret;
}

/**
 * Duplicate xf86 options, replace the "type" option with the given type
 * (and the name with "$name $type" and convert them to InputOption */
static InputOption *wcmOptionDupConvert(InputInfoPtr pInfo, const char* basename, const char *type)
{
	pointer original = pInfo->options;
	InputOption *iopts = NULL, *new;
	char *name;
	pointer options;
	int rc;

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 12
	options = xf86OptionListDuplicate(original);
#else
	{
		InputInfoRec dummy;

		memset(&dummy, 0, sizeof(dummy));
		xf86CollectInputOptions(&dummy, NULL, original);
		options = dummy.options;
	}
#endif

	rc = asprintf(&name, "%s %s", basename, type);
	if (rc == -1) /* if asprintf fails, strdup will probably too... */
		name = strdup("unknown");

	options = xf86ReplaceStrOption(options, "Type", type);
	options = xf86ReplaceStrOption(options, "Name", name);
	free(name);

	while(options)
	{
		new = calloc(1, sizeof(InputOption));

		new->key = xf86OptionName(options);
		new->value = xf86OptionValue(options);
		new->next = iopts;
		iopts = new;
		options = xf86NextOption(options);
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
static InputAttributes* wcmDuplicateAttributes(InputInfoPtr pInfo,
					       const char *type)
{
	int rc;
	InputAttributes *attr;
	attr = DuplicateInputAttributes(pInfo->attrs);
	rc = asprintf(&attr->product, "%s %s", attr->product, type);
	if (rc == -1)
		attr->product = NULL;
	return attr;
}
#endif

/**
 * Hotplug one device of the given type.
 * Device has the same options as the "parent" device, type is one of
 * erasor, stylus, pad, touch, cursor, etc.
 * Name of the new device is set automatically to "<device name> <type>".
 */
static void wcmHotplug(InputInfoPtr pInfo, const char* basename, const char *type)
{
	DeviceIntPtr dev; /* dummy */
	InputOption *input_options;
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 9
	InputAttributes *attrs = NULL;
#endif

	input_options = wcmOptionDupConvert(pInfo, basename, type);

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 11
	attrs = wcmDuplicateAttributes(pInfo, type);
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

void wcmHotplugOthers(InputInfoPtr pInfo, const char *basename)
{
	int i, skip = 1;
	char*		device;

        xf86Msg(X_INFO, "%s: hotplugging dependent devices.\n", pInfo->name);
	device = xf86SetStrOption(pInfo->options, "Device", NULL);
        /* same loop is used to init the first device, if we get here we
         * need to start at the second one */
	for (i = 0; i < ARRAY_SIZE(wcmType); i++)
	{
		if (wcmIsAValidType(pInfo, wcmType[i].type))
		{
			if (skip)
				skip = 0;
			else
				wcmHotplug(pInfo, basename, wcmType[i].type);
		}
	}
        xf86Msg(X_INFO, "%s: hotplugging completed.\n", pInfo->name);
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
int wcmNeedAutoHotplug(InputInfoPtr pInfo, const char **type)
{
	char *source = xf86CheckStrOption(pInfo->options, "_source", "");
	int i;

	if (*type) /* type specified, don't hotplug */
		return 0;

	if (strcmp(source, "server/hal") && strcmp(source, "server/udev"))
		return 0;

	/* no type specified, so we need to pick the first one applicable
	 * for our device */
	for (i = 0; i < ARRAY_SIZE(wcmType); i++)
	{
		if (wcmIsAValidType(pInfo, wcmType[i].type))
		{
			*type = strdup(wcmType[i].type);
			break;
		}
	}

	if (!*type)
		return 0;

	xf86Msg(X_INFO, "%s: type not specified, assuming '%s'.\n", pInfo->name, *type);
	xf86Msg(X_INFO, "%s: other types will be automatically added.\n", pInfo->name);

	pInfo->options = xf86AddNewOption(pInfo->options, "Type", *type);
	pInfo->options = xf86ReplaceStrOption(pInfo->options, "_source", "_driver/wacom");

	return 1;
}

int wcmParseOptions(InputInfoPtr pInfo, int hotplugged)
{
	WacomDevicePtr  priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr  common = priv->common;
	char            *s, b[12];
	int		i, oldButton;
	WacomToolPtr    tool = NULL;
	WacomToolAreaPtr area = NULL;

	/* Optional configuration */
	priv->debugLevel = xf86SetIntOption(pInfo->options,
					    "DebugLevel", priv->debugLevel);
	common->debugLevel = xf86SetIntOption(pInfo->options,
					      "CommonDBG", common->debugLevel);
	s = xf86SetStrOption(pInfo->options, "Mode", NULL);

	if (s && (xf86NameCmp(s, "absolute") == 0))
		set_absolute(pInfo, TRUE);
	else if (s && (xf86NameCmp(s, "relative") == 0))
		set_absolute(pInfo, FALSE);
	else
	{
		if (s)
			xf86Msg(X_ERROR, "%s: invalid Mode (should be absolute"
				" or relative). Using default.\n", pInfo->name);

		/* If Mode not specified or is invalid then rely on
		 * Type specific defaults from initialization.
		 */
	}

	/* Pad is always in absolute mode.
	 * The pad also defaults to wheel scrolling, unlike the pens
	 * (interesting effects happen on ArtPen and others with build-in
	 * wheels)
	 */
	if (IsPad(priv))
	{
		priv->wheelup = 4;
		priv->wheeldn = 5;
		set_absolute(pInfo, TRUE);
	}

	s = xf86SetStrOption(pInfo->options, "Rotate", NULL);

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
				pInfo->name, s);
			goto error;
		}
	}

	common->wcmSuppress = xf86SetIntOption(pInfo->options, "Suppress",
			common->wcmSuppress);
	if (common->wcmSuppress != 0) /* 0 disables suppression */
	{
		if (common->wcmSuppress > MAX_SUPPRESS)
			common->wcmSuppress = MAX_SUPPRESS;
		if (common->wcmSuppress < DEFAULT_SUPPRESS)
			common->wcmSuppress = DEFAULT_SUPPRESS;
	}

	if (xf86SetBoolOption(pInfo->options, "Tilt",
			(common->wcmFlags & TILT_REQUEST_FLAG)))
		common->wcmFlags |= TILT_REQUEST_FLAG;

	if (xf86SetBoolOption(pInfo->options, "RawFilter",
			(common->wcmFlags & RAW_FILTERING_FLAG)))
		common->wcmFlags |= RAW_FILTERING_FLAG;

	/* pressure curve takes control points x1,y1,x2,y2
	 * values in range from 0..100.
	 * Linear curve is 0,0,100,100
	 * Slightly depressed curve might be 5,0,100,95
	 * Slightly raised curve might be 0,5,95,100
	 */
	s = xf86SetStrOption(pInfo->options, "PressCurve", "0,0,100,100");
	if (s && (IsStylus(priv) || IsEraser(priv)))
	{
		int a,b,c,d;
		if ((sscanf(s,"%d,%d,%d,%d",&a,&b,&c,&d) != 4) ||
				!wcmCheckPressureCurveValues(a, b, c, d))
			xf86Msg(X_CONFIG, "%s: PressCurve not valid\n",
				pInfo->name);
		else
			wcmSetPressureCurve(priv,a,b,c,d);
	}

	if (IsCursor(priv))
	{
		common->wcmCursorProxoutDist = xf86SetIntOption(pInfo->options, "CursorProx", 0);
		if (common->wcmCursorProxoutDist < 0 || common->wcmCursorProxoutDist > 255)
			xf86Msg(X_CONFIG, "%s: CursorProx invalid %d \n",
				pInfo->name, common->wcmCursorProxoutDist);
	}

	if (xf86SetBoolOption(pInfo->options, "KeepShape", 0))
		priv->flags |= KEEP_SHAPE_FLAG;

	priv->topX = xf86SetIntOption(pInfo->options, "TopX", 0);
	priv->topY = xf86SetIntOption(pInfo->options, "TopY", 0);
	priv->bottomX = xf86SetIntOption(pInfo->options, "BottomX", 0);
	priv->bottomY = xf86SetIntOption(pInfo->options, "BottomY", 0);
	priv->serial = xf86SetIntOption(pInfo->options, "Serial", 0);

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

	common->wcmThreshold = xf86SetIntOption(pInfo->options, "Threshold",
			common->wcmThreshold);

	common->wcmMaxZ = xf86SetIntOption(pInfo->options, "MaxZ",
					   common->wcmMaxZ);
	if (xf86SetBoolOption(pInfo->options, "ButtonsOnly", 0))
		priv->flags |= BUTTONS_ONLY_FLAG;

	/* TPCButton on for Tablet PC by default */
	oldButton = xf86SetBoolOption(pInfo->options, "TPCButton",
					TabletHasFeature(common, WCM_TPC));

	if (hotplugged || IsStylus(priv))
		common->wcmTPCButton = oldButton;
	else if (oldButton != common->wcmTPCButton)
		xf86Msg(X_WARNING, "%s: TPCButton option can only be set "
			"by stylus.\n", pInfo->name);

	/* a single or double touch device */
	if (TabletHasFeature(common, WCM_1FGT) ||
	    TabletHasFeature(common, WCM_2FGT))
	{
		/* TouchDefault was off for all devices
		 * except when touch is supported */
		common->wcmTouchDefault = 1;

		oldButton = xf86SetBoolOption(pInfo->options, "Touch",
					common->wcmTouchDefault);

		if (hotplugged || IsTouch(priv))
			common->wcmTouch = oldButton;
		else if (oldButton != common->wcmTouch)
			xf86Msg(X_WARNING, "%s: Touch option can only be set "
				"by a touch tool.\n", pInfo->name);

		oldButton = xf86SetBoolOption(pInfo->options, "Capacity",
					common->wcmCapacityDefault);

		if (hotplugged || IsTouch(priv))
			common->wcmCapacity = oldButton;
		else if (oldButton != common->wcmCapacity)
			xf86Msg(X_WARNING, "%s: Touch Capacity option can only be"
				"set by a touch tool.\n", pInfo->name);
	}

	/* 2FG touch device */
	if (TabletHasFeature(common, WCM_2FGT))
	{
		/* GestureDefault was off for all devices
		 * except when multi-touch is supported */
		common->wcmGestureDefault = 1;

		oldButton = xf86SetBoolOption(pInfo->options, "Gesture",
					common->wcmGestureDefault);

		if (hotplugged || IsTouch(priv))
			common->wcmGesture = oldButton;
		else if (oldButton != common->wcmGesture)
			xf86Msg(X_WARNING, "%s: Touch gesture option can only "
				"be set by a touch tool.\n", pInfo->name);

		if ((common->wcmDevCls == &gWacomUSBDevice) &&
				TabletHasFeature(common, WCM_LCD) &&
				TabletHasFeature(common, WCM_2FGT)) {
			common->wcmGestureParameters.wcmZoomDistanceDefault = 30;
			common->wcmGestureParameters.wcmScrollDistanceDefault = 30;
			common->wcmGestureParameters.wcmTapTimeDefault = 250;
		}

		common->wcmGestureParameters.wcmZoomDistance =
			xf86SetIntOption(pInfo->options, "ZoomDistance",
			common->wcmGestureParameters.wcmZoomDistanceDefault);

		common->wcmGestureParameters.wcmScrollDistance =
			xf86SetIntOption(pInfo->options, "ScrollDistance",
			common->wcmGestureParameters.wcmScrollDistanceDefault);

		common->wcmGestureParameters.wcmTapTime =
			xf86SetIntOption(pInfo->options, "TapTime",
			common->wcmGestureParameters.wcmTapTimeDefault);
	}

	/* Swap stylus buttons 2 and 3 for Tablet PCs */
	if (TabletHasFeature(common, WCM_TPC) && IsStylus(priv))
	{
		priv->button[1] = 3;
		priv->button[2] = 2;
	}

	for (i=0; i<WCM_MAX_BUTTONS; i++)
	{
		sprintf(b, "Button%d", i+1);
		priv->button[i] = xf86SetIntOption(pInfo->options, b, priv->button[i]);
	}

	/* Now parse class-specific options */
	if (common->wcmDevCls->ParseOptions &&
	    !common->wcmDevCls->ParseOptions(pInfo))
		goto error;

	return 1;
error:
	free(area);
	free(tool);
	return 0;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
