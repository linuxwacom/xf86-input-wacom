/*
 * Copyright 2009 - 2013 by Ping Cheng, Wacom. <pingc@wacom.com>
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

#include <config.h>

#include "xf86Wacom.h"
#include "wcmFilter.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

struct checkData {
	dev_t min_maj;
	const char *source;
};

static int checkSource(WacomDevicePtr other, void *data)
{
	char *device = wcmOptCheckStr(other, "Device", NULL);
	WacomCommonPtr pCommon;
	struct checkData *check = data;
	Bool match = FALSE;

	/* device can be NULL on some distros */
	if (!device)
		return -ENODEV;

	free(device);

	pCommon = other->common;
	if (pCommon->min_maj && pCommon->min_maj == check->min_maj)
	{
		char* source = wcmOptCheckStr(other, "_source", "");
		/* only add the new tool if the matching major/minor
		 * was from the same source */
		if (strcmp(check->source, source))
			match = TRUE;
		free(source);
	}

	return match ? 0 : -ENODEV;
}

/* wcmCheckSource - Check if there is another source defined this device
 * before or not: don't add the tool by hal/udev if user has defined at least
 * one tool for the device in xorg.conf. One device can have multiple tools
 * with the same type to individualize tools with serial number or areas */
static Bool wcmCheckSource(WacomDevicePtr priv, dev_t min_maj)
{
	int nmatch;
	struct checkData check = {
		.min_maj = min_maj,
		.source = wcmOptCheckStr(priv, "_source", ""),
	};

	nmatch = wcmForeachDevice(priv, checkSource, &check);
	if (nmatch > 0)
		wcmLog(priv, W_WARNING,
			    "device file already in use. Ignoring.\n");
	free((char*)check.source);
	return nmatch;
}

/* check if the device has been added.
 * Open the device and check it's major/minor, then compare this with every
 * other wacom device listed in the config. If they share the same
 * major/minor and the same source/type, fail.
 * This is to detect duplicate devices if a device was added once through
 * the xorg.conf and is then hotplugged through the server backend (HAL,
 * udev). In this case, the hotplugged one fails.
 */
int wcmIsDuplicate(const char* device, WacomDevicePtr priv)
{
	struct stat st;
	int isInUse = 0;
	char* lsource = wcmOptCheckStr(priv, "_source", NULL);

	/* always allow xorg.conf defined tools to be added */
	if (!lsource || !strlen(lsource)) goto ret;

	if (stat(device, &st) == -1)
	{
		/* can not access major/minor to check device duplication */
		wcmLog(priv, W_ERROR,
			    "stat failed (%s). cannot check for duplicates.\n",
			    strerror(errno));

		/* older systems don't support the required ioctl.  let it pass */
		goto ret;
	}

	if (st.st_rdev)
	{
		/* device matches with another added port */
		if (wcmCheckSource(priv, st.st_rdev))
		{
			isInUse = 3;
			goto ret;
		}
	}
	else
	{
		/* major/minor can never be 0, right? */
		wcmLog(priv, W_ERROR,
			    "device opened with a major/minor of 0. Something was wrong.\n");
		isInUse = 4;
	}
ret:
	free(lsource);
	return isInUse;
}

static struct
{
	const char* type;
	__u16 tool[5]; /* tool array is terminated by 0 */
} wcmType [] =
{
	{ "stylus", { BTN_TOOL_PEN,       0                  } },
	{ "eraser", { BTN_TOOL_RUBBER,    0                  } },
	{ "cursor", { BTN_TOOL_MOUSE,     0                  } },
	{ "touch",  { BTN_TOOL_DOUBLETAP, BTN_TOOL_FINGER, 0 } },
	{ "pad",    { BTN_FORWARD,        BTN_0,           KEY_CONTROLPANEL, KEY_ONSCREEN_KEYBOARD, 0 } }
};

/* validate tool type for device/product */
Bool wcmIsAValidType(WacomDevicePtr priv, const char* type)
{
	WacomCommonPtr common = priv->common;
	size_t i, j;
	char* dsource;
	Bool user_defined;

	if (!type)
	{
		wcmLog(priv, W_ERROR, "No type specified\n");
		return FALSE;
	}

	dsource = wcmOptCheckStr(priv, "_source", NULL);
	user_defined = !dsource || !strlen(dsource);
	free(dsource);

	for (i = 0; i < ARRAY_SIZE(wcmType); i++)
	{
		if (strcmp(wcmType[i].type, type) == 0)
		{
			break;
		}
	}

	if (i >= ARRAY_SIZE(wcmType))
	{
		/* No type with the given name was found. */
		wcmLog(priv, W_ERROR, "type '%s' is not known to the driver\n", type);
		return FALSE;
	}

	for (j = 0; wcmType[i].tool[j]; j++)
	{
		/* Check if the device has this tool among its declared
		 * keys. If it does, then this is (generally) a valid
		 * type.
		 *
		 * Non-generic devices are an exception to this rule.
		 * In particular, they set BTN_TOOL_FINGER on the
		 * pad device. This means that we need to ignore
		 * BTN_TOOL_FINGER when checking if "touch" is a
		 * valid type since that would be a false-positive.
		 * Note that "touch" can still be detected by another
		 * key, e.g. BTN_TOOL_DOUBLETAP.
		 */
		Bool valid = ISBITSET (common->wcmKeys, wcmType[i].tool[j]);
		Bool bypass = common->wcmProtocolLevel != WCM_PROTOCOL_GENERIC &&
			      wcmType[i].tool[j] == BTN_TOOL_FINGER &&
		              strcmp(type, "touch") == 0;

		if (valid && !bypass) {
			return TRUE;
		}
	}

	if (user_defined)
	{
		/* This device does not appear to be the right type, but
		 * maybe the user knows better than us...
		 */
		SETBIT(common->wcmKeys, wcmType[i].tool[0]);
		wcmLog(priv, W_WARNING, "user-defined type '%s' may not be valid\n", type);
		return TRUE;
	}

	/* The driver is probably probing to see if this is a valid
	 * type to associate with this device for hotplug. Let the
	 * caller know it is invalid, but don't complain in the logs.
	 */
	return FALSE;
}

static void
wcmAddHotpluggedDevice(WacomDevicePtr priv, const char *basename, const char *type,
		       WacomToolPtr ser)
{
	char *name;
	int rc;

	if (ser == NULL)
		rc = asprintf(&name, "%s %s", basename, type);
	else if (strlen(ser->name) > 0)
		rc = asprintf(&name, "%s %s %s", basename, ser->name, type);
	else
		rc = asprintf(&name, "%s %u %s", basename, ser->serial, type);

	if (rc == -1)
		return;

	wcmQueueHotplug(priv, name, type, ser ? ser->serial : UINT_MAX);

	free(name);
}

/**
 * Attempt to hotplug a tool with a given type.
 *
 * If the tool does not claim to support the given type, ignore the
 * request. Otherwise, verify that it is actually valid and then
 * queue the hotplug.
 *
 * @param priv The parent device
 * @param ser The tool pointer
 * @param basename The kernel device name
 * @param idflag Tool ID_XXXX flag to try to hotplug
 * @param type Type name for this tool
 */
static void wcmTryHotplugSerialType(WacomDevicePtr priv, WacomToolPtr ser, const char *basename, int idflag, const char *type)
{
	if (!(ser->typeid & idflag)) {
		// No need to print an error message. The device doesn't
		// claim to support this type so there's no problem.
		return;
	}

	if (!wcmIsAValidType(priv, type)) {
		wcmLog(priv, W_ERROR, "invalid device type '%s'.\n", type);
		return;
	}

	wcmAddHotpluggedDevice(priv, basename, type, ser);
}

/**
 * Hotplug all serial numbers configured on this device.
 *
 * @param priv The parent device
 * @param basename The kernel device name
 */
static void wcmHotplugSerials(WacomDevicePtr priv, const char *basename)
{
	WacomCommonPtr  common = priv->common;
	WacomToolPtr    ser = common->serials;

	while (ser)
	{
		wcmLog(priv, W_INFO, "hotplugging serial %u.\n", ser->serial);
		wcmTryHotplugSerialType(priv, ser, basename, STYLUS_ID, "stylus");
		wcmTryHotplugSerialType(priv, ser, basename, ERASER_ID, "eraser");
		wcmTryHotplugSerialType(priv, ser, basename, CURSOR_ID, "cursor");

		ser = ser->next;
	}
}

void wcmHotplugOthers(WacomDevicePtr priv, const char *basename)
{
	Bool skip = TRUE;

	wcmLog(priv, W_INFO, "hotplugging dependent devices.\n");

        /* same loop is used to init the first device, if we get here we
         * need to start at the second one */
	for (size_t i = 0; i < ARRAY_SIZE(wcmType); i++)
	{
		if (wcmIsAValidType(priv, wcmType[i].type))
		{
			if (skip)
				skip = FALSE;
			else
				wcmAddHotpluggedDevice(priv, basename, wcmType[i].type, NULL);
		}
	}

	wcmHotplugSerials(priv, basename);

	wcmLog(priv, W_INFO, "hotplugging completed.\n");
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
int wcmNeedAutoHotplug(WacomDevicePtr priv, char **type)
{
	char *source = wcmOptCheckStr(priv, "_source", NULL);
	int rc = 0;

	if (*type) /* type specified, don't hotplug */
		goto out;

	if (!source) /* xorg.conf device, don't auto-pick type */
		goto out;

	if (source && strcmp(source, "server/hal") && strcmp(source, "server/udev"))
		goto out;

	/* no type specified, so we need to pick the first one applicable
	 * for our device */
	for (size_t i = 0; i < ARRAY_SIZE(wcmType); i++)
	{
		if (wcmIsAValidType(priv, wcmType[i].type))
		{
			free(*type);
			*type = strdup(wcmType[i].type);
			break;
		}
	}

	if (!*type) {
		wcmLog(priv, W_ERROR, "No valid type found for this device.\n");
		goto out;
	}

	wcmLog(priv, W_INFO, "type not specified, assuming '%s'.\n", *type);
	wcmLog(priv, W_INFO, "other types will be automatically added.\n");

	/* Note: wcmIsHotpluggedDevice() relies on this */
	wcmOptSetStr(priv, "Type", *type);
	wcmOptSetStr(priv, "_source", "_driver/wacom");

	rc = 1;

	free(source);
out:
	return rc;
}

int wcmParseSerials (WacomDevicePtr priv)
{
	WacomCommonPtr  common = priv->common;
	char            *s;

	if (common->serials)
	{
		return 0; /*Parse has been already done*/
	}

	s = wcmOptGetStr(priv, "ToolSerials", NULL);
	if (s) /*Dont parse again, if the commons have values already*/
	{
		char* tok = strtok(s, ";");
		while (tok != NULL)
		{
			int serial, nmatch;
			char type[strlen(tok) + 1];
			char name[strlen(tok) + 1];
			WacomToolPtr ser = calloc(1, sizeof(WacomTool));

			if (ser == NULL)
				return 1;

			nmatch = sscanf(tok,"%d,%[a-z],%[A-Za-z ]",&serial, type, name);

			if (nmatch < 1)
			{
				wcmLog(priv, W_ERROR, "%s is invalid serial string.\n", tok);
				free(ser);
				return 1;
			}

			if (nmatch >= 1)
			{
				wcmLog(priv, W_CONFIG, "Tool serial %d found.\n", serial);

				ser->serial = serial;

				ser->typeid = STYLUS_ID | ERASER_ID; /*Default to both tools*/
			}

			if (nmatch >= 2)
			{
				wcmLog(priv, W_CONFIG, "Tool %d has type %s.\n", serial, type);
				if ((strcmp(type, "pen") == 0) || (strcmp(type, "airbrush") == 0))
					ser->typeid = STYLUS_ID | ERASER_ID;
				else if (strcmp(type, "artpen") == 0)
					ser->typeid = STYLUS_ID;
				else if (strcmp(type, "cursor") == 0)
					ser->typeid = CURSOR_ID;
				else    wcmLog(priv, W_CONFIG, "Invalid type %s, defaulting to pen.\n", type);
			}

			if (nmatch == 3)
			{
				wcmLog(priv, W_CONFIG, "Tool %d is named %s.\n", serial, name);
				ser->name = strdup(name);
			}
			else ser->name = strdup(""); /*no name yet*/

			if (common->serials == NULL)
				common->serials = ser;
			else
			{
				WacomToolPtr tool = common->serials;
				while (tool->next)
					tool = tool->next;
				tool->next = ser;
			}

			tok = strtok(NULL,";");
		}
	}
	return 0;
}

/**
 * Parse the pre-init options for this device. Most useful for options
 * needed to properly init a device (baud rate for example).
 *
 * Note that parameters is_primary and is_dependent are mutually exclusive,
 * though both may be false in the case of an xorg.conf device.
 *
 * @param is_primary True if the device is the parent device for
 * hotplugging, False if the device is a depent or xorg.conf device.
 * @param is_hotplugged True if the device is a dependent device, FALSE
 * otherwise.
 * @retval True on success or False otherwise.
 */
Bool wcmPreInitParseOptions(WacomDevicePtr priv, Bool is_primary,
			    Bool is_dependent)
{
	WacomCommonPtr  common = priv->common;
	char            *s;
	int		i;
	WacomToolPtr    tool = NULL;
	int		tpc_button_is_on;

	/* Optional configuration */
	s = wcmOptGetStr(priv, "Mode", NULL);

	if (s && (strcasecmp(s, "absolute") == 0))
		set_absolute(priv, TRUE);
	else if (s && (strcasecmp(s, "relative") == 0))
		set_absolute(priv, FALSE);
	else
	{
		if (s)
			wcmLog(priv, W_ERROR,
				"invalid Mode (should be absolute"
				" or relative). Using default.\n");

		/* If Mode not specified or is invalid then rely on
		 * Type specific defaults from initialization.
		 */
	}

	free(s);

	/* Pad is always in absolute mode.
	 * The pad also defaults to wheel scrolling, unlike the pens
	 * (interesting effects happen on ArtPen and others with build-in
	 * wheels)
	 */
	if (IsPad(priv))
	{
		priv->wheel_default[WHEEL_ABS_UP] = priv->wheel_default[WHEEL2_ABS_UP] = 4;
		priv->wheel_default[WHEEL_ABS_DN] = priv->wheel_default[WHEEL2_ABS_DN] = 5;
		set_absolute(priv, TRUE);
	}

	s = wcmOptGetStr(priv, "Rotate", NULL);

	if (s)
	{
		int rotation = ROTATE_NONE;

		if (strcasecmp(s, "CW") == 0)
			rotation = ROTATE_CW;
		else if (strcasecmp(s, "CCW") ==0)
			rotation = ROTATE_CCW;
		else if (strcasecmp(s, "HALF") ==0)
			rotation = ROTATE_HALF;
		else if (strcasecmp(s, "NONE") !=0)
		{
			wcmLog(priv, W_ERROR, "invalid Rotate option '%s'.\n", s);
			goto error;
		}

		if (is_dependent && rotation != common->wcmRotate)
			wcmLog(priv, W_INFO, "ignoring rotation of dependent device\n");
		else
			wcmRotateTablet(priv, rotation);
		free(s);
	}

	common->wcmRawSample = wcmOptGetInt(priv, "RawSample",
			common->wcmRawSample);
	if (common->wcmRawSample < 1 || common->wcmRawSample > MAX_SAMPLES)
	{
		wcmLog(priv, W_ERROR,
			    "RawSample setting '%d' out of range [1..%d]. Using default.\n",
			    common->wcmRawSample, MAX_SAMPLES);
		common->wcmRawSample = DEFAULT_SAMPLES;
	}

	common->wcmSuppress = wcmOptGetInt(priv, "Suppress",
			common->wcmSuppress);
	if (common->wcmSuppress != 0) /* 0 disables suppression */
	{
		if (common->wcmSuppress > MAX_SUPPRESS)
			common->wcmSuppress = MAX_SUPPRESS;
		if (common->wcmSuppress < DEFAULT_SUPPRESS)
			common->wcmSuppress = DEFAULT_SUPPRESS;
	}

	/* pressure curve takes control points x1,y1,x2,y2
	 * values in range from 0..100.
	 * Linear curve is 0,0,100,100
	 * Slightly depressed curve might be 5,0,100,95
	 * Slightly raised curve might be 0,5,95,100
	 */
	s = wcmOptGetStr(priv, "PressCurve", "0,0,100,100");
	if (s && (IsPen(priv) || IsTouch(priv)))
	{
		int a,b,c,d;
		if ((sscanf(s,"%d,%d,%d,%d",&a,&b,&c,&d) != 4) ||
				!wcmCheckPressureCurveValues(a, b, c, d))
			wcmLog(priv, W_CONFIG, "PressCurve not valid\n");
		else
			wcmSetPressureCurve(priv,a,b,c,d);
	}
	free(s);

	if (wcmOptGetBool(priv, "Pressure2K", 0)) {
		wcmLog(priv, W_CONFIG, "Using 2K pressure levels\n");
		priv->maxCurve = 2048;
	}

	/*Serials of tools we want hotpluged*/
	if (wcmParseSerials (priv) != 0)
		goto error;

	if (IsTablet(priv))
	{
		const char *prop = IsCursor(priv) ? "CursorProx" : "StylusProx";
		priv->wcmProxoutDist = wcmOptGetInt(priv, prop, 0);
		if (priv->wcmProxoutDist < 0 ||
				priv->wcmProxoutDist > common->wcmMaxDist)
			wcmLog(priv, W_CONFIG, "%s invalid %d \n",
				    prop, priv->wcmProxoutDist);
		priv->wcmSurfaceDist = -1;
	}

	priv->topX = wcmOptGetInt(priv, "TopX", 0);
	priv->topY = wcmOptGetInt(priv, "TopY", 0);
	priv->bottomX = wcmOptGetInt(priv, "BottomX", 0);
	priv->bottomY = wcmOptGetInt(priv, "BottomY", 0);
	priv->serial = wcmOptGetInt(priv, "Serial", 0);

	tool = priv->tool;
	tool->serial = priv->serial;

	common->wcmPanscrollThreshold = wcmOptGetInt(priv, "PanScrollThreshold",
			common->wcmPanscrollThreshold);

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
			wcmLog(priv, W_ERROR,
				    "already have a tool with type/serial %d/%u.\n",
				    tool->typeid, tool->serial);
			goto error;
		} else /* No match on existing tool/serial, add tool to the end of the list */
		{
			assert(common->wcmTool != NULL);
			toollist = common->wcmTool;
			while(toollist->next)
				toollist = toollist->next;
			toollist->next = tool;
		}
	}

	common->wcmThreshold = wcmOptGetInt(priv, "Threshold",
			common->wcmThreshold);

	if (wcmOptGetBool(priv, "ButtonsOnly", 0))
		priv->flags |= BUTTONS_ONLY_FLAG;

	/* TPCButton on for Tablet PC by default */
	tpc_button_is_on = wcmOptGetBool(priv, "TPCButton",
					TabletHasFeature(common, WCM_TPC));

	if (is_primary || IsStylus(priv))
		common->wcmTPCButton = tpc_button_is_on;
	else if (tpc_button_is_on != common->wcmTPCButton)
		wcmLog(priv, W_WARNING, "TPCButton option can only be set by stylus.\n");

	/* a single or double touch device */
	if (TabletHasFeature(common, WCM_1FGT) ||
	    TabletHasFeature(common, WCM_2FGT))
	{
		int touch_is_on;

		/* TouchDefault was off for all devices
		 * except when touch is supported */
		common->wcmTouchDefault = 1;

		touch_is_on = wcmOptGetBool(priv, "Touch",
						common->wcmTouchDefault);

		if (is_primary || IsTouch(priv))
			common->wcmTouch = touch_is_on;
		else if (touch_is_on != common->wcmTouch)
			wcmLog(priv, W_WARNING,
				    "Touch option can only be set by a touch tool.\n");

		if (TabletHasFeature(common, WCM_1FGT))
			common->wcmMaxContacts = 1;
		else
			common->wcmMaxContacts = 2;
	}

	/* 2FG touch device */
	if (TabletHasFeature(common, WCM_2FGT))
	{
		int gesture_is_on;
		Bool gesture_default = TabletHasFeature(priv->common, WCM_LCD) ? FALSE : TRUE;

		gesture_is_on = wcmOptGetBool(priv, "Gesture",
					    gesture_default);

		if (is_primary || IsTouch(priv))
			common->wcmGesture = gesture_is_on;
		else if (gesture_is_on != common->wcmGesture)
			wcmLog(priv, W_WARNING,
				    "Touch gesture option can only be set by a touch tool.\n");

		common->wcmGestureParameters.wcmTapTime =
			wcmOptGetInt(priv, "TapTime",
			common->wcmGestureParameters.wcmTapTime);
	}

	if (IsStylus(priv) || IsEraser(priv)) {
		common->wcmPressureRecalibration
			= wcmOptGetBool(priv,
					    "PressureRecalibration", 1);
	}

	/* Swap stylus buttons 2 and 3 for Tablet PCs */
	if (TabletHasFeature(common, WCM_TPC) && IsStylus(priv))
	{
		priv->button_default[1] = 3;
		priv->button_default[2] = 2;
	}

	for (i=0; i<WCM_MAX_BUTTONS; i++)
	{
		char b[20];
		sprintf(b, "Button%d", i+1);
		priv->button_default[i] = wcmOptGetInt(priv, b, priv->button_default[i]);
	}

	/* Now parse class-specific options */
	if (common->wcmDevCls->ParseOptions &&
	    !common->wcmDevCls->ParseOptions(priv))
		goto error;

	return TRUE;
error:
	return FALSE;
}

/* The values were based on trial and error with a 3rd-gen Bamboo */
#define WCM_DEFAULT_MM_XRES           (27.8 * 1000)
#define WCM_DEFAULT_MM_YRES           (44.5 * 1000)
#define WCM_ZOOM_DISTANCE_MM          6.5
#define WCM_SCROLL_DISTANCE_MM        1.8

/**
 * Parse post-init options for this device. Useful for overriding HW
 * specific options computed during init phase (HW distances for example).
 *
 * Note that parameters is_primary and is_dependent are mutually exclusive,
 * though both may be false in the case of an xorg.conf device.
 *
 * @param is_primary True if the device is the parent device for
 * hotplugging, False if the device is a depent or xorg.conf device.
 * @param is_hotplugged True if the device is a dependent device, FALSE
 * otherwise.
 * @retval True on success or False otherwise.
 */
Bool wcmPostInitParseOptions(WacomDevicePtr priv, Bool is_primary,
			     Bool is_dependent)
{
	WacomCommonPtr  common = priv->common;

	common->wcmMaxZ = wcmOptGetInt(priv, "MaxZ",
					   common->wcmMaxZ);

	/* 2FG touch device */
	if (TabletHasFeature(common, WCM_2FGT) && IsTouch(priv))
	{
		int x_res = common->wcmTouchResolX ? common->wcmTouchResolX : WCM_DEFAULT_MM_XRES;
		int y_res = common->wcmTouchResolY ? common->wcmTouchResolY : WCM_DEFAULT_MM_YRES;
		int zoom_distance = WCM_ZOOM_DISTANCE_MM * x_res / 1000;
		int scroll_distance = WCM_SCROLL_DISTANCE_MM * y_res / 1000;

		common->wcmGestureParameters.wcmZoomDistance =
			wcmOptGetInt(priv, "ZoomDistance",
					 zoom_distance);

		common->wcmGestureParameters.wcmScrollDistance =
			wcmOptGetInt(priv, "ScrollDistance",
					 scroll_distance);
	}


	return TRUE;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
