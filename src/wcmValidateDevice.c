/*
 * Copyright 2009 by Ping Cheng, Wacom. <pingc@wacom.com>
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
#include <linux/serial.h>

#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))


Bool wcmIsAValidType(const char *type, unsigned long* keys);
int wcmNeedAutoHotplug(LocalDevicePtr local, const char **type,
		unsigned long* keys);
void wcmHotplugOthers(LocalDevicePtr local, unsigned long* keys);
int wcmAutoProbeDevice(LocalDevicePtr local);
int wcmParseOptions(LocalDevicePtr local, unsigned long* keys);
int wcmIsDuplicate(char* device, LocalDevicePtr local);
int wcmDeviceTypeKeys(LocalDevicePtr local, unsigned long* keys);

/* xf86WcmCheckSource - Check if there is another source defined this device
 * before or not: don't add the tool by hal/udev if user has defined at least
 * one tool for the device in xorg.conf. One device can have multiple tools
 * with the same type to individualize tools with serial number or areas */
static Bool xf86WcmCheckSource(LocalDevicePtr local, dev_t min_maj)
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
 */
int wcmIsDuplicate(char* device, LocalDevicePtr local)
{
	struct stat st;
	int isInUse = 0;
	char* lsource = xf86CheckStrOption(local->options, "_source", "");

	local->fd = -1;

	/* always allow xorg.conf defined tools to be added */
	if (!strlen(lsource)) goto ret;

	/* open the port */
        SYSCALL(local->fd = open(device, O_RDONLY, 0));
	if (local->fd < 0)
	{
		/* can not open the device */
		xf86Msg(X_ERROR, "%s: Unable to open Wacom device \"%s\".\n",
			local->name, device);
		isInUse = 1;
		goto ret;
	}

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
		if (xf86WcmCheckSource(local, st.st_rdev))
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
	if (local->fd >= 0)
	{
		close(local->fd);
		local->fd = -1;
	}
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
Bool wcmIsAValidType(const char* type, unsigned long* keys)
{
	int j, ret = FALSE;

	if (!type)
		return FALSE;

	/* walkthrough all types */
	for (j = 0; j < ARRAY_SIZE(wcmType); j++)
	{
		if (!strcmp(wcmType[j].type, type))
			if (ISBITSET (keys, wcmType[j].tool))
			{
				ret = TRUE;
				break;
			}
	}
	return ret;
}

/* Choose valid types according to device ID */
int wcmDeviceTypeKeys(LocalDevicePtr local, unsigned long* keys)
{
	int ret = 1, i;
	int fd = -1, id = 0;
	char* device, *stopstring;
	char* str = strstr(local->name, "WACf");
	struct serial_struct tmp;

	device = xf86SetStrOption(local->options, "Device", NULL);

	SYSCALL(fd = open(device, O_RDONLY));
	if (fd < 0)
	{
		xf86Msg(X_WARNING, "%s: failed to open %s in "
			"wcmDeviceTypeKeys.\n", local->name, device);
		return 0;
	}

	/* we have tried memset. it doesn't work */
	for (i=0; i<NBITS(KEY_MAX); i++)
		keys[i] = 0;

	/* serial ISDV4 devices */
	if (ioctl(fd, TIOCGSERIAL, &tmp) == 0)
	{
		if (str) /* id in name */
		{
			str = str + 4;
			if (str)
				id = (int)strtol(str, &stopstring, 16);

		}
		else /* id in file sys/class/tty/%str/device/id */
		{
			FILE *file;
			char sysfs_id[256];
			str = strstr(device, "ttyS");
			snprintf(sysfs_id, sizeof(sysfs_id),
				"/sys/class/tty/%s/device/id", str);
			file = fopen(sysfs_id, "r");

			/* return true since it falls to default */
			if (file)
			{
				/* make sure we fall to default */
				if (fscanf(file, "WACf%x\n", &id) <= 0)
					id = 0;

				fclose(file);
			}
		}

		/* default to penabled */
		keys[LONG(BTN_TOOL_PEN)] |= BIT(BTN_TOOL_PEN);
		keys[LONG(BTN_TOOL_RUBBER)] |= BIT(BTN_TOOL_RUBBER);

		/* id < 0x008 are only penabled */
		if (id > 0x007)
		{
			keys[LONG(BTN_TOOL_DOUBLETAP)] |= BIT(BTN_TOOL_DOUBLETAP);
			if (id > 0x0a)
				keys[LONG(BTN_TOOL_TRIPLETAP)] |= BIT(BTN_TOOL_TRIPLETAP);
		}

		/* no pen 2FGT */
		if (id == 0x010)
		{
			keys[LONG(BTN_TOOL_PEN)] &= ~BIT(BTN_TOOL_PEN);
			keys[LONG(BTN_TOOL_RUBBER)] &= ~BIT(BTN_TOOL_RUBBER);
		}
	}
	else /* USB devices */
	{
		/* test if the tool is defined in the kernel */
		if (ioctl(fd, EVIOCGBIT(EV_KEY, (sizeof(unsigned long)
			 * NBITS(KEY_MAX))), keys) < 0)
		{
			xf86Msg(X_ERROR, "%s: wcmDeviceTypeKeys unable to "
				"ioctl USB key bits.\n", local->name);
			ret = 0;
		}
	}
	close(fd);
	return ret;
}

/**
 * Duplicate xf86 options, replace the "type" option with the given type
 * (and the name with "$name $type" and convert them to InputOption */
static InputOption *wcmOptionDupConvert(LocalDevicePtr local, const char *type)
{
	pointer original = local->options;
	InputOption *iopts = NULL, *new;
	InputInfoRec dummy;
	char *name;

	memset(&dummy, 0, sizeof(dummy));
	xf86CollectInputOptions(&dummy, NULL, original);

	name = xcalloc(strlen(local->name) + strlen(type) + 2, 1);
	sprintf(name, "%s %s", local->name, type);

	dummy.options = xf86ReplaceStrOption(dummy.options, "Type", type);
	dummy.options = xf86ReplaceStrOption(dummy.options, "Name", name);
	xfree(name);

	while(dummy.options)
	{
		new = xcalloc(1, sizeof(InputOption));

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
		xfree(opts->key);
		xfree(opts->value);
		xfree(opts);
		opts = tmp;
	}
}

/**
 * Hotplug one device of the given type.
 * Device has the same options as the "parent" device, type is one of
 * erasor, stylus, pad, touch, cursor, etc.
 * Name of the new device is set automatically to "<device name> <type>".
 */
static void wcmHotplug(LocalDevicePtr local, const char *type)
{
	DeviceIntPtr dev; /* dummy */
	InputOption *input_options;

	input_options = wcmOptionDupConvert(local, type);

	NewInputDeviceRequest(input_options,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 9
				NULL,
#endif
				&dev);
	wcmFreeInputOpts(input_options);
}

void wcmHotplugOthers(LocalDevicePtr local, unsigned long* keys)
{
	int i, skip = 1;
	char*		device;

        xf86Msg(X_INFO, "%s: hotplugging dependent devices.\n", local->name);
	device = xf86SetStrOption(local->options, "Device", NULL);
        /* same loop is used to init the first device, if we get here we
         * need to start at the second one */
	for (i = 0; i < ARRAY_SIZE(wcmType); i++)
	{
		if (wcmIsAValidType(wcmType[i].type, keys))
		{
			if (skip)
				skip = 0;
			else
				wcmHotplug(local, wcmType[i].type);
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
int wcmNeedAutoHotplug(LocalDevicePtr local, const char **type,
		unsigned long* keys)
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
		if (wcmIsAValidType(wcmType[i].type, keys))
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

int wcmParseOptions(LocalDevicePtr local, unsigned long* keys)
{
	WacomDevicePtr  priv = (WacomDevicePtr)local->private;
	WacomCommonPtr  common = priv->common;
	char            *s, b[12];
	int		i, oldButton;
	WacomToolPtr    tool = NULL;
	WacomToolAreaPtr area = NULL;

	/* Optional configuration */
	priv->debugLevel = xf86SetIntOption(local->options,
					    "DebugLevel", priv->debugLevel);
	common->debugLevel = xf86SetIntOption(local->options,
					      "CommonDBG", common->debugLevel);
	s = xf86SetStrOption(local->options, "Mode", NULL);

	if (s && (xf86NameCmp(s, "absolute") == 0))
		priv->flags |= ABSOLUTE_FLAG;
	else if (s && (xf86NameCmp(s, "relative") == 0))
		priv->flags &= ~ABSOLUTE_FLAG;
	else
	{
		if (s)
			xf86Msg(X_ERROR, "%s: invalid Mode (should be absolute"
				" or relative). Using default.\n", local->name);

		/* If Mode not specified or is invalid then rely on
		 * Type specific defaults from initialization.
		 *
		 * If Mode default is hardware specific then handle here:
		 *
		 * touch Types are initilized to Absolute.
		 * Bamboo P&T touch pads need to change default to Relative.
		 */
		if (IsTouch(priv) &&
		    (common->tablet_id >= 0xd0) && (common->tablet_id <= 0xd3))
			priv->flags &= ~ABSOLUTE_FLAG;
	}

	/* Pad is always in relative mode when it's a core device.
	 * Always in absolute mode when it is not a core device.
	 */
	if (IsPad(priv))
		xf86WcmSetPadCoreMode(local);

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
	{
		common->wcmFlags |= TILT_REQUEST_FLAG;
	}

	if (xf86SetBoolOption(local->options, "RawFilter",
			(common->wcmFlags & RAW_FILTERING_FLAG)))
	{
		common->wcmFlags |= RAW_FILTERING_FLAG;
	}

	/* pressure curve takes control points x1,y1,x2,y2
	 * values in range from 0..100.
	 * Linear curve is 0,0,100,100
	 * Slightly depressed curve might be 5,0,100,95
	 * Slightly raised curve might be 0,5,95,100
	 */
	s = xf86SetStrOption(local->options, "PressCurve", NULL);
	if (s && !IsCursor(priv) && !IsTouch(priv))
	{
		int a,b,c,d;
		if ((sscanf(s,"%d,%d,%d,%d",&a,&b,&c,&d) != 4) ||
			(a < 0) || (a > 100) || (b < 0) || (b > 100) ||
			(c < 0) || (c > 100) || (d < 0) || (d > 100))
			xf86Msg(X_CONFIG, "%s: PressCurve not valid\n",
				local->name);
		else
		{
			wcmSetPressureCurve(priv,a,b,c,d);
		}
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

	common->wcmScaling = 0;

	common->wcmThreshold = xf86SetIntOption(local->options, "Threshold",
			common->wcmThreshold);
	if (!IsTouch(priv))
		common->wcmMaxX = xf86SetIntOption(local->options, "MaxX",
					 common->wcmMaxX);
	else
		common->wcmMaxTouchX = xf86SetIntOption(local->options, "MaxX",
					 common->wcmMaxTouchX);


	if (!IsTouch(priv))
		common->wcmMaxY = xf86SetIntOption(local->options, "MaxY",
					 common->wcmMaxY);
	else
		common->wcmMaxTouchY = xf86SetIntOption(local->options, "MaxY",
					 common->wcmMaxTouchY);

	common->wcmMaxZ = xf86SetIntOption(local->options, "MaxZ",
					   common->wcmMaxZ);
	common->wcmUserResolX = xf86SetIntOption(local->options, "ResolutionX",
						 common->wcmUserResolX);
	common->wcmUserResolY = xf86SetIntOption(local->options, "ResolutionY",
						 common->wcmUserResolY);
	common->wcmUserResolZ = xf86SetIntOption(local->options, "ResolutionZ",
						 common->wcmUserResolZ);
	if (xf86SetBoolOption(local->options, "ButtonsOnly", 0))
		priv->flags |= BUTTONS_ONLY_FLAG;

	/* Tablet PC button applied to the whole tablet. Not just one tool */
	if ( priv->flags & STYLUS_ID )
		common->wcmTPCButton = xf86SetBoolOption(local->options,
							 "TPCButton",
							 common->wcmTPCButtonDefault);

	/* a single touch device */
	if (ISBITSET (keys, BTN_TOOL_DOUBLETAP))
	{
		/* TouchDefault was off for all devices
		 * except when touch is supported */
		common->wcmTouchDefault = 1;
	}

	/* 2FG touch device */
	if (ISBITSET (keys, BTN_TOOL_TRIPLETAP))
	{
		/* GestureDefault was off for all devices
		 * except when multi-touch is supported */
		common->wcmGestureDefault = 1;
	}

	/* check if touch was turned off in xorg.conf */
	common->wcmTouch = xf86SetBoolOption(local->options, "Touch",
		common->wcmTouchDefault);

	/* Touch gesture applies to the whole tablet */
	common->wcmGesture = xf86SetBoolOption(local->options, "Gesture",
			common->wcmGestureDefault);

	/* Touch capacity applies to the whole tablet */
	common->wcmCapacity = xf86SetBoolOption(local->options, "Capacity", common->wcmCapacityDefault);

	/* Mouse cursor stays in one monitor in a multimonitor setup */
	if ( !priv->wcmMMonitor )
		priv->wcmMMonitor = xf86SetBoolOption(local->options, "MMonitor", 1);

	for (i=0; i<WCM_MAX_BUTTONS; i++)
	{
		sprintf(b, "Button%d", i+1);
		s = xf86SetStrOption(local->options, b, NULL);
		if (s)
		{
			oldButton = priv->button[i];
			priv->button[i] = xf86SetIntOption(local->options, b, priv->button[i]);
		}
	}

	if (common->wcmForceDevice == DEVICE_ISDV4)
        {
		int val;
		val = xf86SetIntOption(local->options, "BaudRate", 38400);

		switch(val)
		{
			case 38400:
			case 19200:
				common->wcmISDV4Speed = val;
				break;
			default:
				xf86Msg(X_ERROR, "%s: Illegal speed value "
					"(must be 19200 or 38400).",
					local->name);
				break;
		}
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

	return 1;
error:
	xfree(area);
	xfree(tool);
	return 0;
}

int wcmAutoProbeDevice(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	WacomCommonPtr common =  priv->common;

	if ((!common->wcmDevice || !strcmp (common->wcmDevice, "auto-dev")))
	{
		common->wcmFlags |= AUTODEV_FLAG;
		if (! (common->wcmDevice = wcmEventAutoDevProbe (local)))
		{
			xf86Msg(X_ERROR, "%s: unable to probe device\n",
				local->name);
			return 0;
		}
	}
	return 1;
}
/* vim: set noexpandtab shiftwidth=8: */
