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
	InputInfoPtr pDevices = xf86FirstLocalDevice();

	for (; !match && pDevices != NULL; pDevices = pDevices->next)
	{
		char *device;
		WacomCommonPtr pCommon;

		if (pInfo == pDevices)
			continue;

		if (!strstr(pDevices->drv->driverName, "wacom"))
			continue;

		device = xf86CheckStrOption(pDevices->options, "Device", NULL);
		/* device can be NULL on some distros */
		if (!device)
			continue;
		free(device);

		pCommon = ((WacomDevicePtr)pDevices->private)->common;
		if (pCommon->min_maj && pCommon->min_maj == min_maj)
		{
			char* fsource = xf86CheckStrOption(pInfo->options, "_source", "");
			char* psource = xf86CheckStrOption(pDevices->options, "_source", "");

			/* only add the new tool if the matching major/minor
			* was from the same source */
			if (strcmp(fsource, psource))
				match = 1;
			free(fsource);
			free(psource);
		}
	}
	if (match)
		xf86IDrvMsg(pInfo, X_WARNING,
			    "device file already in use by %s. Ignoring.\n", pDevices->name);
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
int wcmIsDuplicate(const char* device, InputInfoPtr pInfo)
{
	struct stat st;
	int isInUse = 0;
	char* lsource = xf86CheckStrOption(pInfo->options, "_source", NULL);

	/* always allow xorg.conf defined tools to be added */
	if (!lsource || !strlen(lsource)) goto ret;

	if (stat(device, &st) == -1)
	{
		/* can not access major/minor to check device duplication */
		xf86IDrvMsg(pInfo, X_ERROR, "stat failed (%s). cannot check for duplicates.\n",
			    strerror(errno));

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
		xf86IDrvMsg(pInfo, X_ERROR,
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
Bool wcmIsAValidType(InputInfoPtr pInfo, const char* type)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;
	int i, j;
	char* dsource;
	Bool user_defined;

	if (!type)
	{
		xf86IDrvMsg(pInfo, X_ERROR, "No type specified\n");
		return FALSE;
	}

	dsource = xf86CheckStrOption(pInfo->options, "_source", NULL);
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
		xf86IDrvMsg(pInfo, X_ERROR, "type '%s' is not known to the driver\n", type);
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
		xf86IDrvMsg(pInfo, X_WARNING, "user-defined type '%s' may not be valid\n", type);
		return TRUE;
	}

	/* The driver is probably probing to see if this is a valid
	 * type to associate with this device for hotplug. Let the
	 * caller know it is invalid, but don't complain in the logs.
	 */
	return FALSE;
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
		case 0xF8:  /* Cintiq 24HDT */
		case 0xF4:  /* Cintiq 24HD */
			TabletSetFeature(priv->common, WCM_DUALRING);
			/* fall through */

		case 0x34D: /* MobileStudio Pro 13 */
		case 0x34E: /* MobileStudio Pro 16 */
		case 0x398: /* MobileStudio Pro 13 */
		case 0x399: /* MobileStudio Pro 16 */
		case 0x3AA: /* MobileStudio Pro 16 */
			TabletSetFeature(priv->common, WCM_LCD);
			/* fall through */

		case 0x357: /* Intuos Pro 2 M */
		case 0x358: /* Intuos Pro 2 L */
		case 0x360: /* Intuos Pro 2 M (Bluetooth) */
		case 0x36a: /* Intuos Pro 2 L (Bluetooth) */
		case 0x392: /* Intuos Pro 2 S */
		case 0x393: /* Intuos Pro 2 S (Bluetooth) */
		case 0x314: /* Intuos Pro S */
		case 0x315: /* Intuos Pro M */
		case 0x317: /* Intuos Pro L */
		case 0x26:  /* I5 */
		case 0x27:  /* I5 */
		case 0x28:  /* I5 */
		case 0x29:  /* I5 */
		case 0x2A:  /* I5 */
		case 0xB8:  /* I4 */
		case 0xB9:  /* I4 */
		case 0xBA:  /* I4 */
		case 0xBB:  /* I4 */
		case 0xBC:  /* I4 */
		case 0xBD:  /* I4 */
			TabletSetFeature(priv->common, WCM_ROTATION);
			/* fall through */

		/* tablets with touch ring */
		case 0x17:  /* BambooFun */
		case 0x18:  /* BambooFun */
			TabletSetFeature(priv->common, WCM_RING);
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
			TabletSetFeature(priv->common, WCM_DUALINPUT);
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
		case 0x3A:  /* DTI520 */
		case 0xC0:  /* DTF720 */
		case 0xC2:  /* DTF720a */
		case 0xC4:  /* DTF521 */
		case 0xC7:  /* DTU1931 */
		case 0xCE:  /* DTU2231 */
		case 0xF0:  /* DTU1631 */

		/* Wacom One display tablet */
		case 0x3A6: /* DTC133 */
			TabletSetFeature(priv->common, WCM_LCD);
			break;

		/* tablets support menu strips */
		case 0x3F:  /* CintiqV5 */
		case 0xC5:  /* CintiqV5 */
		case 0xC6:  /* CintiqV5 */
		case 0xCC:  /* CinitqV5 */
		case 0xFA:  /* Cintiq 22HD */
		case 0x5B:  /* Cintiq 22HDT Pen */
			TabletSetFeature(priv->common, WCM_LCD);
			/* fall through */
		case 0xB0:  /* I3 */
		case 0xB1:  /* I3 */
		case 0xB2:  /* I3 */
		case 0xB3:  /* I3 */
		case 0xB4:  /* I3 */
		case 0xB5:  /* I3 */
		case 0xB7:  /* I3 */
			TabletSetFeature(priv->common, WCM_STRIP | WCM_ROTATION);
			break;

		case 0x100: /* TPC with MT */
		case 0x101: /* TPC with MT */
		case 0x10D: /* TPC with MT */
		case 0x116: /* TPC with 1FGT */
		case 0x12C: /* TPC */
		case 0x4001: /* TPC with MT */
		case 0x4004: /* TPC with MT (no pen on Motion) */
		case 0x5000: /* TPC with MT */
		case 0x5002: /* TPC with MT */
		case 0xE2: /* TPC with 2FGT */
		case 0xE3: /* TPC with 2FGT */
		case 0xE5: /* TPC with MT */
		case 0xE6: /* TPC with 2FGT */
		case 0x93: /* TPC with 1FGT */
		case 0x9A: /* TPC with 1FGT */
		case 0xEC: /* TPC with 1FGT */
		case 0xED: /* TPC with 1FGT */
		case 0x90: /* TPC */
		case 0x97: /* TPC */
		case 0x9F: /* TPC */
		case 0xEF: /* TPC */
			TabletSetFeature(priv->common, WCM_TPC);
			break;

		case 0x304:/* Cintiq 13HD */
		case 0x307:/* Cintiq Companion Hybrid */
		case 0x30A:/* Cintiq Companion */
		case 0x325:/* Cintiq Companion 2 */
		case 0x32A:/* Cintiq 27QHD */
		case 0x32B:/* Cintiq 27QHDT Pen */
		case 0x333:/* Cintiq 13HDT Pen */
		case 0x34F:/* Cintiq Pro 13 FHD */
		case 0x350:/* Cintiq Pro 16 UHD */
		case 0x351:/* Cintiq Pro 24 */
		case 0x352:/* Cintiq Pro 32 */
		case 0x37C:/* Cintiq Pro 24 Pen-Only */
		case 0x390:/* Cintiq 16 */
		case 0x391:/* Cintiq 22 */
		case 0x396:/* DTK-1660E */
		case 0x3AE:/* Cintiq 16 */
		case 0x3B0:/* DTK-1660E */
			TabletSetFeature(priv->common, WCM_ROTATION);
			/* fall-through */

		case 0xF6: /* Cintiq 24HDT Touch */
		case 0x57: /* DTK2241 */
		case 0x59: /* DTH2242 Pen */
		case 0x5D: /* DTH2242 Touch */
		case 0x5E: /* Cintiq 22HDT Touch */
		case 0x309:/* Cintiq Companion Hybrid Touch */
		case 0x30C:/* Cintiq Companion Touch */
		case 0x326:/* Cintiq Companion 2 Touch */
		case 0x32C:/* Cintiq 27QHDT Touch */
		case 0x32F:/* DTU-1031X */
		case 0x335:/* Cintiq 13HDT Touch */
		case 0x336:/* DTU-1141 */
		case 0x343:/* DTK-1651 */
		case 0x34A:/* MobileStudio Pro 13 Touch */
		case 0x34B:/* MobileStudio Pro 16 Touch */
		case 0x353:/* Cintiq Pro 13 FHD Touch */
		case 0x354:/* Cintiq Pro 13 UHD Touch */
		case 0x355:/* Cintiq Pro 24 Touch */
		case 0x356:/* Cintiq Pro 32 Touch */
		case 0x359:/* DTU-1141B */
		case 0x35A:/* DTH-1152*/
		case 0x368:/* DTH-1152 Touch */
		case 0x382:/* DTK-2451 */
		case 0x37D:/* DTH-2452 */
		case 0x37E:/* DTH-2452 Touch */
		case 0x39A:/* MobileStudio Pro 13 Touch */
		case 0x39B:/* MobileStudio Pro 16 Touch */
		case 0x3AC:/* MobileStudio Pro 16 Touch */
			TabletSetFeature(priv->common, WCM_LCD);
			break;
	}

#ifdef INPUT_PROP_DIRECT
	{
		int rc;
		unsigned long prop[NBITS(INPUT_PROP_MAX)] = {0};

		rc = ioctl(pInfo->fd, EVIOCGPROP(sizeof(prop)), prop);
		if (rc >= 0 && ISBITSET(prop, INPUT_PROP_DIRECT))
			TabletSetFeature(priv->common, WCM_LCD);
	}
#endif
	if (ISBITSET(common->wcmKeys, BTN_TOOL_PEN))
		TabletSetFeature(priv->common, WCM_PEN);

	if (ISBITSET (common->wcmKeys, BTN_0) ||
			ISBITSET (common->wcmKeys, BTN_FORWARD))
	{
		TabletSetFeature(priv->common, WCM_PAD);
	}

	/* This handles both protocol 4 and 5 meanings of wcmKeys */
	if (common->wcmProtocolLevel == WCM_PROTOCOL_4)
	{
		/* TRIPLETAP means 2 finger touch */
		/* DOUBLETAP without TRIPLETAP means 1 finger touch */
		if (ISBITSET(common->wcmKeys, BTN_TOOL_TRIPLETAP))
			TabletSetFeature(priv->common, WCM_2FGT);
		else if (ISBITSET(common->wcmKeys, BTN_TOOL_DOUBLETAP))
			TabletSetFeature(priv->common, WCM_1FGT);
	}

	if (common->wcmProtocolLevel == WCM_PROTOCOL_GENERIC)
	{
		/* DOUBLETAP means 2 finger touch */
		/* FINGER without DOUBLETAP means 1 finger touch */
		if (ISBITSET(common->wcmKeys, BTN_TOOL_DOUBLETAP))
			TabletSetFeature(priv->common, WCM_2FGT);
		else if (ISBITSET(common->wcmKeys, BTN_TOOL_FINGER))
			TabletSetFeature(priv->common, WCM_1FGT);
	}

	return ret;
}

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) < 14
static InputOption*
input_option_new(InputOption *list, char *key, char *value)
{
	InputOption *new;

	new = calloc(1, sizeof(InputOption));
	new->key = strdup(key);
	new->value = strdup(value);
	new->next = list;
	return new;
}

static void
input_option_free_list(InputOption **opts)
{
	InputOption *tmp = *opts;
	while(*opts)
	{
		tmp = (*opts)->next;
		free((*opts)->key);
		free((*opts)->value);
		free((*opts));
		*opts = tmp;
	}
}
#endif

/**
 * Duplicate xf86 options, replace the "type" option with the given type
 * (and the name with "$name $type" and convert them to InputOption
 *
 * @param basename Kernel device name for this device
 * @param type Tool type (cursor, eraser, etc.)
 * @param serial Serial number this device should be bound to (-1 for "any")
 */
static InputOption *wcmOptionDupConvert(InputInfoPtr pInfo, const char* basename, const char *type, int serial)
{
	WacomDevicePtr priv = pInfo->private;
	WacomCommonPtr common = priv->common;
	pointer original = pInfo->options;
	WacomToolPtr ser = common->serials;
	InputOption *iopts = NULL;
	char *name;
	pointer options, o;
	int rc;

	options = xf86OptionListDuplicate(original);
	if (serial > -1)
	{
		while (ser->serial && ser->serial != serial)
			ser = ser->next;

		if (strlen(ser->name) > 0)
			rc = asprintf(&name, "%s %s %s", basename, ser->name, type);
		else
			rc = asprintf(&name, "%s %d %s", basename, ser->serial, type);
	}
	else
		rc = asprintf(&name, "%s %s", basename, type);

	if (rc == -1) /* if asprintf fails, strdup will probably too... */
		name = strdup("unknown");

	options = xf86ReplaceStrOption(options, "Type", type);
	options = xf86ReplaceStrOption(options, "Name", name);

	if (serial > -1)
		options = xf86ReplaceIntOption(options, "Serial", ser->serial);

	free(name);

	o = options;
	while(o)
	{
		iopts = input_option_new(iopts,
					 xf86OptionName(o),
					 xf86OptionValue(o));
		o = xf86NextOption(o);
	}
	xf86OptionListFree(options);
	return iopts;
}

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
	char *product;

	attr = DuplicateInputAttributes(pInfo->attrs);
	rc = asprintf(&product, "%s %s", attr->product, type);
	free(attr->product);
	attr->product = (rc != -1) ? product : NULL;
	return attr;
}

/**
 * This struct contains the necessary info for hotplugging a device later.
 * Memory must be freed after use.
 */
typedef struct {
	InputOption *input_options;
	InputAttributes *attrs;
} WacomHotplugInfo;

/**
 * Actually hotplug the device. This function is called by the server when
 * the WorkProcs are processed.
 *
 * @param client The server client. unused
 * @param closure A pointer to a struct WcmHotplugInfo containing the
 * necessary information to create a new device.
 * @return TRUE to remove this function from the server's work queue.
 */
static Bool
wcmHotplugDevice(ClientPtr client, pointer closure )
{
	WacomHotplugInfo *hotplug_info = closure;
	DeviceIntPtr dev; /* dummy */

#if HAVE_THREADED_INPUT
	input_lock();
#endif

	NewInputDeviceRequest(hotplug_info->input_options,
			      hotplug_info->attrs,
			      &dev);
#if HAVE_THREADED_INPUT
	input_unlock();
#endif

	input_option_free_list(&hotplug_info->input_options);

	FreeInputAttributes(hotplug_info->attrs);
	free(hotplug_info);

	return TRUE;
}

/**
 * Queue the hotplug for one tool/device of the given type.
 * Device has the same options as the "parent" device, type is one of
 * erasor, stylus, pad, touch, cursor, etc.
 * Name of the new device is set automatically to "<device name> <type>".
 *
 * Note that we don't actually hotplug the device here. We store the
 * information needed to hotplug the device later and then queue the
 * hotplug. The server will come back and call the @ref wcmHotplugDevice
 * later.
 *
 * @param pInfo The parent device
 * @param basename The base name for the device (type will be appended)
 * @param type Type name for this tool
 * @param serial Serial number this device should be bound to (-1 for "any")
 */
static void wcmQueueHotplug(InputInfoPtr pInfo, const char* basename, const char *type, int serial)
{
	WacomHotplugInfo *hotplug_info;

	hotplug_info = calloc(1, sizeof(WacomHotplugInfo));

	if (!hotplug_info)
	{
		xf86IDrvMsg(pInfo, X_ERROR, "OOM, cannot hotplug dependent devices\n");
		return;
	}

	hotplug_info->input_options = wcmOptionDupConvert(pInfo, basename, type, serial);
	hotplug_info->attrs = wcmDuplicateAttributes(pInfo, type);
	QueueWorkProc(wcmHotplugDevice, serverClient, hotplug_info);
}

/**
 * Attempt to hotplug a tool with a given type.
 *
 * If the tool does not claim to support the given type, ignore the
 * request. Otherwise, verify that it is actually valid and then
 * queue the hotplug.
 *
 * @param pInfo The parent device
 * @param ser The tool pointer
 * @param basename The kernel device name
 * @param idflag Tool ID_XXXX flag to try to hotplug
 * @param type Type name for this tool
 */
static void wcmTryHotplugSerialType(InputInfoPtr pInfo, WacomToolPtr ser, const char *basename, int idflag, const char *type)
{
	if (!(ser->typeid & idflag)) {
		// No need to print an error message. The device doesn't
		// claim to support this type so there's no problem.
		return;
	}

	if (!wcmIsAValidType(pInfo, type)) {
		xf86IDrvMsg(pInfo, X_ERROR, "invalid device type '%s'.\n", type);
		return;
	}

	wcmQueueHotplug(pInfo, basename, type, ser->serial);
}

/**
 * Hotplug all serial numbers configured on this device.
 *
 * @param pInfo The parent device
 * @param basename The kernel device name
 */
static void wcmHotplugSerials(InputInfoPtr pInfo, const char *basename)
{
	WacomDevicePtr  priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr  common = priv->common;
	WacomToolPtr    ser = common->serials;

	while (ser)
	{
		xf86IDrvMsg(pInfo, X_INFO, "hotplugging serial %d.\n", ser->serial);
		wcmTryHotplugSerialType(pInfo, ser, basename, STYLUS_ID, "stylus");
		wcmTryHotplugSerialType(pInfo, ser, basename, ERASER_ID, "eraser");
		wcmTryHotplugSerialType(pInfo, ser, basename, CURSOR_ID, "cursor");

		ser = ser->next;
	}
}

void wcmHotplugOthers(InputInfoPtr pInfo, const char *basename)
{
	int i, skip = 1;

	xf86IDrvMsg(pInfo, X_INFO, "hotplugging dependent devices.\n");

        /* same loop is used to init the first device, if we get here we
         * need to start at the second one */
	for (i = 0; i < ARRAY_SIZE(wcmType); i++)
	{
		if (wcmIsAValidType(pInfo, wcmType[i].type))
		{
			if (skip)
				skip = 0;
			else
				wcmQueueHotplug(pInfo, basename, wcmType[i].type, -1);
		}
	}

	wcmHotplugSerials(pInfo, basename);

        xf86IDrvMsg(pInfo, X_INFO, "hotplugging completed.\n");
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
int wcmNeedAutoHotplug(InputInfoPtr pInfo, char **type)
{
	char *source = xf86CheckStrOption(pInfo->options, "_source", NULL);
	int i;
	int rc = 0;

	if (*type) /* type specified, don't hotplug */
		goto out;

	if (!source) /* xorg.conf device, don't auto-pick type */
		goto out;

	if (source && strcmp(source, "server/hal") && strcmp(source, "server/udev"))
		goto out;

	/* no type specified, so we need to pick the first one applicable
	 * for our device */
	for (i = 0; i < ARRAY_SIZE(wcmType); i++)
	{
		if (wcmIsAValidType(pInfo, wcmType[i].type))
		{
			free(*type);
			*type = strdup(wcmType[i].type);
			break;
		}
	}

	if (!*type) {
		xf86IDrvMsg(pInfo, X_ERROR, "No valid type found for this device.\n");
		goto out;
	}

	xf86IDrvMsg(pInfo, X_INFO, "type not specified, assuming '%s'.\n", *type);
	xf86IDrvMsg(pInfo, X_INFO, "other types will be automatically added.\n");

	/* Note: wcmIsHotpluggedDevice() relies on this */
	pInfo->options = xf86AddNewOption(pInfo->options, "Type", *type);
	pInfo->options = xf86ReplaceStrOption(pInfo->options, "_source", "_driver/wacom");

	rc = 1;

	free(source);
out:
	return rc;
}

int wcmParseSerials (InputInfoPtr pInfo)
{
	WacomDevicePtr  priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr  common = priv->common;
	char            *s;

	if (common->serials)
	{
		return 0; /*Parse has been already done*/
	}

	s = xf86SetStrOption(pInfo->options, "ToolSerials", NULL);
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
				xf86IDrvMsg(pInfo, X_ERROR, "%s is invalid serial string.\n", tok);
				free(ser);
				return 1;
			}

			if (nmatch >= 1)
			{
				xf86IDrvMsg(pInfo, X_CONFIG, "Tool serial %d found.\n", serial);

				ser->serial = serial;

				ser->typeid = STYLUS_ID | ERASER_ID; /*Default to both tools*/
			}

			if (nmatch >= 2)
			{
				xf86IDrvMsg(pInfo, X_CONFIG, "Tool %d has type %s.\n", serial, type);
				if ((strcmp(type, "pen") == 0) || (strcmp(type, "airbrush") == 0))
					ser->typeid = STYLUS_ID | ERASER_ID;
				else if (strcmp(type, "artpen") == 0)
					ser->typeid = STYLUS_ID;
				else if (strcmp(type, "cursor") == 0)
					ser->typeid = CURSOR_ID;
				else    xf86IDrvMsg(pInfo, X_CONFIG, "Invalid type %s, defaulting to pen.\n", type);
			}

			if (nmatch == 3)
			{
				xf86IDrvMsg(pInfo, X_CONFIG, "Tool %d is named %s.\n", serial, name);
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
Bool wcmPreInitParseOptions(InputInfoPtr pInfo, Bool is_primary,
			    Bool is_dependent)
{
	WacomDevicePtr  priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr  common = priv->common;
	char            *s;
	int		i;
	WacomToolPtr    tool = NULL;
	int		tpc_button_is_on;

	/* Optional configuration */
	s = xf86SetStrOption(pInfo->options, "Mode", NULL);

	if (s && (xf86NameCmp(s, "absolute") == 0))
		set_absolute(pInfo, TRUE);
	else if (s && (xf86NameCmp(s, "relative") == 0))
		set_absolute(pInfo, FALSE);
	else
	{
		if (s)
			xf86IDrvMsg(pInfo, X_ERROR, "invalid Mode (should be absolute"
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
		set_absolute(pInfo, TRUE);
	}

	s = xf86SetStrOption(pInfo->options, "Rotate", NULL);

	if (s)
	{
		int rotation = ROTATE_NONE;

		if (xf86NameCmp(s, "CW") == 0)
			rotation = ROTATE_CW;
		else if (xf86NameCmp(s, "CCW") ==0)
			rotation = ROTATE_CCW;
		else if (xf86NameCmp(s, "HALF") ==0)
			rotation = ROTATE_HALF;
		else if (xf86NameCmp(s, "NONE") !=0)
		{
			xf86IDrvMsg(pInfo, X_ERROR, "invalid Rotate option '%s'.\n", s);
			goto error;
		}

		if (is_dependent && rotation != common->wcmRotate)
			xf86IDrvMsg(pInfo, X_INFO, "ignoring rotation of dependent device\n");
		else
			wcmRotateTablet(pInfo, rotation);
		free(s);
	}

	common->wcmRawSample = xf86SetIntOption(pInfo->options, "RawSample",
			common->wcmRawSample);
	if (common->wcmRawSample < 1 || common->wcmRawSample > MAX_SAMPLES)
	{
		xf86IDrvMsg(pInfo, X_ERROR,
			    "RawSample setting '%d' out of range [1..%d]. Using default.\n",
			    common->wcmRawSample, MAX_SAMPLES);
		common->wcmRawSample = DEFAULT_SAMPLES;
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

	/* pressure curve takes control points x1,y1,x2,y2
	 * values in range from 0..100.
	 * Linear curve is 0,0,100,100
	 * Slightly depressed curve might be 5,0,100,95
	 * Slightly raised curve might be 0,5,95,100
	 */
	s = xf86SetStrOption(pInfo->options, "PressCurve", "0,0,100,100");
	if (s && (IsPen(priv) || IsTouch(priv)))
	{
		int a,b,c,d;
		if ((sscanf(s,"%d,%d,%d,%d",&a,&b,&c,&d) != 4) ||
				!wcmCheckPressureCurveValues(a, b, c, d))
			xf86IDrvMsg(pInfo, X_CONFIG, "PressCurve not valid\n");
		else
			wcmSetPressureCurve(priv,a,b,c,d);
	}
	free(s);

	if (xf86SetBoolOption(pInfo->options, "Pressure2K", 0)) {
		xf86IDrvMsg(pInfo, X_CONFIG, "Using 2K pressure levels\n");
		priv->maxCurve = 2048;
	}

	/*Serials of tools we want hotpluged*/
	if (wcmParseSerials (pInfo) != 0)
		goto error;

	if (IsTablet(priv))
	{
		const char *prop = IsCursor(priv) ? "CursorProx" : "StylusProx";
		priv->wcmProxoutDist = xf86SetIntOption(pInfo->options, prop, 0);
		if (priv->wcmProxoutDist < 0 ||
				priv->wcmProxoutDist > common->wcmMaxDist)
			xf86IDrvMsg(pInfo, X_CONFIG, "%s invalid %d \n",
				    prop, priv->wcmProxoutDist);
		priv->wcmSurfaceDist = -1;
	}

	priv->topX = xf86SetIntOption(pInfo->options, "TopX", 0);
	priv->topY = xf86SetIntOption(pInfo->options, "TopY", 0);
	priv->bottomX = xf86SetIntOption(pInfo->options, "BottomX", 0);
	priv->bottomY = xf86SetIntOption(pInfo->options, "BottomY", 0);
	priv->serial = xf86SetIntOption(pInfo->options, "Serial", 0);

	tool = priv->tool;
	tool->serial = priv->serial;

	common->wcmPanscrollThreshold = xf86SetIntOption(pInfo->options, "PanScrollThreshold",
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
			xf86IDrvMsg(pInfo, X_ERROR, "already have a tool with type/serial %d/%d.\n",
				    tool->typeid, tool->serial);
			goto error;
		} else /* No match on existing tool/serial, add tool to the end of the list */
		{
			toollist = common->wcmTool;
			while(toollist->next)
				toollist = toollist->next;
			toollist->next = tool;
		}
	}

	common->wcmThreshold = xf86SetIntOption(pInfo->options, "Threshold",
			common->wcmThreshold);

	if (xf86SetBoolOption(pInfo->options, "ButtonsOnly", 0))
		priv->flags |= BUTTONS_ONLY_FLAG;

	/* TPCButton on for Tablet PC by default */
	tpc_button_is_on = xf86SetBoolOption(pInfo->options, "TPCButton",
					TabletHasFeature(common, WCM_TPC));

	if (is_primary || IsStylus(priv))
		common->wcmTPCButton = tpc_button_is_on;
	else if (tpc_button_is_on != common->wcmTPCButton)
		xf86IDrvMsg(pInfo, X_WARNING, "TPCButton option can only be set by stylus.\n");

	/* a single or double touch device */
	if (TabletHasFeature(common, WCM_1FGT) ||
	    TabletHasFeature(common, WCM_2FGT))
	{
		int touch_is_on;

		/* TouchDefault was off for all devices
		 * except when touch is supported */
		common->wcmTouchDefault = 1;

		touch_is_on = xf86SetBoolOption(pInfo->options, "Touch",
						common->wcmTouchDefault);

		if (is_primary || IsTouch(priv))
			common->wcmTouch = touch_is_on;
		else if (touch_is_on != common->wcmTouch)
			xf86IDrvMsg(pInfo, X_WARNING,
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

		gesture_is_on = xf86SetBoolOption(pInfo->options, "Gesture",
					    gesture_default);

		if (is_primary || IsTouch(priv))
			common->wcmGesture = gesture_is_on;
		else if (gesture_is_on != common->wcmGesture)
			xf86IDrvMsg(pInfo, X_WARNING,
				    "Touch gesture option can only be set by a touch tool.\n");

		common->wcmGestureParameters.wcmTapTime =
			xf86SetIntOption(pInfo->options, "TapTime",
			common->wcmGestureParameters.wcmTapTime);
	}

	if (IsStylus(priv) || IsEraser(priv)) {
		common->wcmPressureRecalibration
			= xf86SetBoolOption(pInfo->options,
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
		char b[12];
		sprintf(b, "Button%d", i+1);
		priv->button_default[i] = xf86SetIntOption(pInfo->options, b, priv->button_default[i]);
	}

	/* Now parse class-specific options */
	if (common->wcmDevCls->ParseOptions &&
	    !common->wcmDevCls->ParseOptions(pInfo))
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
Bool wcmPostInitParseOptions(InputInfoPtr pInfo, Bool is_primary,
			     Bool is_dependent)
{
	WacomDevicePtr  priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr  common = priv->common;

	common->wcmMaxZ = xf86SetIntOption(pInfo->options, "MaxZ",
					   common->wcmMaxZ);

	/* 2FG touch device */
	if (TabletHasFeature(common, WCM_2FGT) && IsTouch(priv))
	{
		int x_res = common->wcmTouchResolX ? common->wcmTouchResolX : WCM_DEFAULT_MM_XRES;
		int y_res = common->wcmTouchResolY ? common->wcmTouchResolY : WCM_DEFAULT_MM_YRES;
		int zoom_distance = WCM_ZOOM_DISTANCE_MM * x_res / 1000;
		int scroll_distance = WCM_SCROLL_DISTANCE_MM * y_res / 1000;

		common->wcmGestureParameters.wcmZoomDistance =
			xf86SetIntOption(pInfo->options, "ZoomDistance",
					 zoom_distance);

		common->wcmGestureParameters.wcmScrollDistance =
			xf86SetIntOption(pInfo->options, "ScrollDistance",
					 scroll_distance);
	}


	return TRUE;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
