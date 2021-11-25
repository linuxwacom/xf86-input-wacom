/*
 * Copyright 1995-2002 by Frederic Lepied, France. <Lepied@XFree86.org>
 * Copyright 2002-2013 by Ping Cheng, Wacom. <pingc@wacom.com>
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
#include <wacom-properties.h>

/*****************************************************************************
 * wcmAllocate --
 * Allocate the generic bits needed by any wacom device, regardless of type.
 ****************************************************************************/

WacomDevicePtr wcmAllocate(InputInfoPtr pInfo, const char *name)
{
	WacomDevicePtr   priv   = NULL;
	WacomCommonPtr   common = NULL;
	WacomToolPtr     tool   = NULL;
	int i;

	priv = calloc(1, sizeof(WacomDeviceRec));
	if (!priv)
		goto error;

	common = wcmNewCommon();
	if (!common)
		goto error;

	tool = calloc(1, sizeof(WacomTool));
	if(!tool)
		goto error;

	priv->next = NULL;
	priv->name = strdup(name);
	priv->pInfo = pInfo;
	priv->common = common;       /* common info pointer */
	priv->oldCursorHwProx = 0;   /* previous cursor hardware proximity */
	priv->maxCurve = FILTER_PRESSURE_RES;
	priv->nPressCtrl [0] = 0;    /* pressure curve x0 */
	priv->nPressCtrl [1] = 0;    /* pressure curve y0 */
	priv->nPressCtrl [2] = 100;  /* pressure curve x1 */
	priv->nPressCtrl [3] = 100;  /* pressure curve y1 */

	/* Default button and expresskey values, offset buttons 4 and higher
	 * by the 4 scroll buttons. */
	for (i=0; i<WCM_MAX_BUTTONS; i++)
		priv->button_default[i] = (i < 3) ? i + 1 : i + 5;

	priv->nbuttons = WCM_MAX_BUTTONS;       /* Default number of buttons */
	priv->wheel_default[WHEEL_REL_UP] = 5;
	priv->wheel_default[WHEEL_REL_DN] = 4;
	/* wheel events are set to 0, but the pad overwrites this default
	 * later in wcmParseOptions, when we have IsPad() available */
	priv->wheel_default[WHEEL_ABS_UP] = 0;
	priv->wheel_default[WHEEL_ABS_DN] = 0;
	priv->wheel_default[WHEEL2_ABS_UP] = 0;
	priv->wheel_default[WHEEL2_ABS_DN] = 0;
	priv->strip_default[STRIP_LEFT_UP] = 4;
	priv->strip_default[STRIP_LEFT_DN] = 5;
	priv->strip_default[STRIP_RIGHT_UP] = 4;
	priv->strip_default[STRIP_RIGHT_DN] = 5;
	priv->naxes = 6;			/* Default number of axes */

	common->wcmDevices = priv;

	/* tool */
	priv->tool = tool;
	common->wcmTool = tool;
	tool->next = NULL;          /* next tool in list */
	tool->device = pInfo;
	/* tool->typeid is set once we know the type - see wcmSetType */

	/* timers */
	priv->serial_timer = TimerSet(NULL, 0, 0, NULL, NULL);
	priv->tap_timer = TimerSet(NULL, 0, 0, NULL, NULL);
	priv->touch_timer = TimerSet(NULL, 0, 0, NULL, NULL);

	return priv;

error:
	free(tool);
	wcmFreeCommon(&common);
	if (priv) {
		free(priv->name);
	}
	free(priv);
	return NULL;
}

/*****************************************************************************
 * wcmFree --
 * Free the memory allocated by wcmAllocate
 ****************************************************************************/

static void wcmFree(WacomDevicePtr priv)
{
	if (!priv)
		return;

	TimerFree(priv->serial_timer);
	TimerFree(priv->tap_timer);
	TimerFree(priv->touch_timer);
	free(priv->tool);
	wcmFreeCommon(&priv->common);
	free(priv->name);
	free(priv);
}

TEST_NON_STATIC int
wcmSetType(WacomDevicePtr priv, const char *type)
{
	InputInfoPtr pInfo = priv->pInfo;

	if (!type)
		goto invalid;

	if (strcasecmp(type, "stylus") == 0)
	{
		priv->flags = ABSOLUTE_FLAG|STYLUS_ID;
		pInfo->type_name = WACOM_PROP_XI_TYPE_STYLUS;
	} else if (strcasecmp(type, "touch") == 0)
	{
		int flags = TOUCH_ID;

		if (TabletHasFeature(priv->common, WCM_LCD))
			flags |= ABSOLUTE_FLAG;

		priv->flags = flags;
		pInfo->type_name = WACOM_PROP_XI_TYPE_TOUCH;
	} else if (strcasecmp(type, "cursor") == 0)
	{
		priv->flags = CURSOR_ID;
		pInfo->type_name = WACOM_PROP_XI_TYPE_CURSOR;
	} else if (strcasecmp(type, "eraser") == 0)
	{
		priv->flags = ABSOLUTE_FLAG|ERASER_ID;
		pInfo->type_name = WACOM_PROP_XI_TYPE_ERASER;
	} else if (strcasecmp(type, "pad") == 0)
	{
		priv->flags = ABSOLUTE_FLAG|PAD_ID;
		pInfo->type_name = WACOM_PROP_XI_TYPE_PAD;
	} else
		goto invalid;

	/* Set the device id of the "last seen" device on this tool */
	priv->oldState.device_id = wcmGetPhyDeviceID(priv);

	if (!priv->tool)
		return 0;

	priv->tool->typeid = DEVICE_ID(priv->flags); /* tool type (stylus/touch/eraser/cursor/pad) */

	return 1;

invalid:
	xf86IDrvMsg(pInfo, X_ERROR,
		    "No type or invalid type specified.\n"
		    "Must be one of stylus, touch, cursor, eraser, or pad\n");

	return 0;
}

int wcmGetPhyDeviceID(WacomDevicePtr priv)
{
	if (IsStylus(priv))
		return STYLUS_DEVICE_ID;
	else if (IsEraser(priv))
		return ERASER_DEVICE_ID;
	else if (IsCursor(priv))
		return CURSOR_DEVICE_ID;
	else if (IsTouch(priv))
		return TOUCH_DEVICE_ID;
	else
		return PAD_DEVICE_ID;
}

void wcmUnInit(WacomDevicePtr priv)
{
	WacomDevicePtr dev;
	WacomDevicePtr *prev;
	WacomCommonPtr common;

	if (!priv)
		goto out;

	common = priv->common;

	DBG(1, priv, "\n");

	wcmRemoveActive(priv);

	if (priv->tool)
	{
		WacomToolPtr *prev_tool = &common->wcmTool;
		WacomToolPtr tool = *prev_tool;
		while (tool)
		{
			if (tool == priv->tool)
			{
				*prev_tool = tool->next;
				break;
			}
			prev_tool = &tool->next;
			tool = tool->next;
		}
	}

	prev = &common->wcmDevices;
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

out:
	wcmFree(priv);
}

static void wcmEnableDisableTool(WacomDevicePtr priv, Bool enable)
{
	WacomToolPtr	tool	= priv->tool;

	tool->enabled = enable;
}

void wcmEnableTool(WacomDevicePtr priv)
{
	wcmEnableDisableTool(priv, TRUE);
}
void wcmDisableTool(WacomDevicePtr priv)
{
	wcmEnableDisableTool(priv, FALSE);
}

/**
 * Unlink the touch tool from the pen of the same device
 */
void wcmUnlinkTouchAndPen(WacomDevicePtr priv)
{
	WacomCommonPtr common = priv->common;
	InputInfoPtr device = xf86FirstLocalDevice();
	WacomCommonPtr tmpcommon = NULL;
	WacomDevicePtr tmppriv = NULL;
	Bool touch_device = FALSE;

	if (!TabletHasFeature(common, WCM_PENTOUCH))
		return;

	/* Lookup to find the associated pen and touch */
	for (; device != NULL; device = device->next)
	{
		if (!strcmp(device->drv->driverName, "wacom"))
		{
			tmppriv = (WacomDevicePtr) device->private;
			tmpcommon = tmppriv->common;
			touch_device = (common->wcmTouchDevice ||
						tmpcommon->wcmTouchDevice);

			/* skip the same tool or unlinked devices */
			if ((tmppriv == priv) || !touch_device)
				continue;

			if (tmpcommon->tablet_id == common->tablet_id)
			{
				common->wcmTouchDevice = NULL;
				tmpcommon->wcmTouchDevice = NULL;
				common->tablet_type &= ~WCM_PENTOUCH;
				tmpcommon->tablet_type &= ~WCM_PENTOUCH;
				return;
			}
		}
	}
}

/**
 * Splits a wacom device name into its constituent pieces. For instance,
 * "Wacom Intuos Pro Finger touch" would be split into "Wacom Intuos Pro"
 * (the base kernel device name), "Finger" (an descriptor of the specific
 * event interface), and "touch" (a suffix added by this driver to indicate
 * the specific tool).
 */
static void wcmSplitName(char* devicename, char *basename, char *subdevice, char *tool, size_t len)
{
	char *name = strdupa(devicename);
	char *a, *b;

	*basename = *subdevice = *tool = '\0';

	a = strrchr(name, ' ');
	if (a)
	{
		*a = '\0';
		b = strrchr(name, ' ');

		while (b)
		{
			if (!strcmp(b, " Pen") || !strcmp(b, " Finger") ||
			    !strcmp(b, " Pad") || !strcmp(b, " Touch"))
			{
				*b = '\0';
				strncpy(subdevice, b+1, len-1);
				subdevice[len-1] = '\0';
				b = strrchr(name, ' ');
			}
			else
				b = NULL;
		}
		strncat(tool, a+1, len-1);
	}
	strncat(basename, name, len-1);
}

/**
 * Determines if two input devices represent independent parts (stylus,
 * eraser, pad) of the same underlying device. If the 'logical_only'
 * flag is set, the function will only return true if the two devices
 * are represented by the same logical device (i.e., share the same
 * input device node). Otherwise, the function will attempt to determine
 * if the two devices are part of the same physical tablet, such as
 * when a tablet reports 'pen' and 'touch' through separate device
 * nodes.
 */
static Bool wcmIsSiblingDevice(InputInfoPtr a, InputInfoPtr b, Bool logical_only)
{
	WacomDevicePtr privA = (WacomDevicePtr)a->private;
	WacomDevicePtr privB = (WacomDevicePtr)b->private;

	if (strcmp(a->drv->driverName, "wacom") || strcmp(b->drv->driverName, "wacom"))
		return FALSE;

	if (privA == privB)
		return FALSE;

	if (DEVICE_ID(privA->flags) == DEVICE_ID(privB->flags))
		return FALSE;

	if (!strcmp(privA->common->device_path, privB->common->device_path))
		return TRUE;

	if (!logical_only)
	{
		// TODO: Udev might provide more accurate data, but this should
		// be good enough in practice.
		const int len = 50;
		char baseA[len], subA[len], toolA[len];
		char baseB[len], subB[len], toolB[len];
		wcmSplitName(privA->name, baseA, subA, toolA, len);
		wcmSplitName(privB->name, baseB, subB, toolB, len);

		if (strcmp(baseA, baseB))
		{
			// Fallback for (arbitrary) static xorg.conf device names
			return (privA->common->tablet_id == privB->common->tablet_id);
		}

		if (strlen(subA) != 0 && strlen(subB) != 0)
			return TRUE;
	}

	return FALSE;
}

/* wcmMatchDevice - locate matching device and merge common structure. If an
 * already initialized device shares the same device file and driver, remove
 * the new device's "common" struct and point to the one of the already
 * existing one instead.
 * Then add the new device to the now-shared common struct.
 *
 * Returns 1 on a found match or 0 otherwise.
 * Common_return is set to the common struct in use by this device.
 */
static Bool wcmMatchDevice(WacomDevicePtr priv, WacomCommonPtr *common_return)
{
	InputInfoPtr pLocal = priv->pInfo;
	WacomCommonPtr common = priv->common;
	InputInfoPtr pMatch = xf86FirstLocalDevice();

	*common_return = common;

	if (!common->device_path)
		return 0;

	for (; pMatch != NULL; pMatch = pMatch->next)
	{
		WacomDevicePtr privMatch = (WacomDevicePtr)pMatch->private;

		if (wcmIsSiblingDevice(pLocal, pMatch, TRUE))
		{
			DBG(2, priv, "port share between %s and %s\n",
					pLocal->name, pMatch->name);
			/* FIXME: we loose the common->wcmTool here but it
			 * gets re-added during wcmParseOptions. This is
			 * currently required by the code, adding the tool
			 * again here means we trigger the duplicate tool
			 * detection */
			wcmFreeCommon(&priv->common);
			priv->common = wcmRefCommon(privMatch->common);
			priv->next = priv->common->wcmDevices;
			priv->common->wcmDevices = priv;
			*common_return = priv->common;
			return 1;
		}
	}
	return 0;
}

/**
 * Detect the device's device class. We only support two classes right now,
 * USB and ISDV4. Let each class try to detect the type by checking what's
 * behind the fd.
 */
static Bool
wcmDetectDeviceClass(WacomDevicePtr priv)
{
	InputInfoPtr pInfo = priv->pInfo;
	WacomCommonPtr common = priv->common;

	if (common->wcmDevCls)
		return TRUE;

	/* Bluetooth is also considered as USB */
	if (gWacomISDV4Device.Detect(priv))
		common->wcmDevCls = &gWacomISDV4Device;
	else if (gWacomUSBDevice.Detect(priv))
		common->wcmDevCls = &gWacomUSBDevice;
	else
		xf86IDrvMsg(pInfo, X_ERROR, "cannot identify device class.\n");

	return (common->wcmDevCls != NULL);
}

static Bool
wcmInitModel(WacomDevicePtr priv)
{
	WacomCommonPtr common = priv->common;

	/* Initialize the tablet */
	if(common->wcmDevCls->Init(priv) != Success ||
		wcmInitTablet(priv) != Success)
		return FALSE;

	return TRUE;
}

/**
 * Lookup to find the associated pen and touch for the same device.
 * Store touch tool in wcmTouchDevice for pen and touch, respectively,
 * of the same device. Update TabletFeature to indicate it is a hybrid
 * of touch and pen.
 *
 * @return True if found a touch tool for hybrid devices.
 * false otherwise.
 */
static Bool wcmLinkTouchAndPen(WacomDevicePtr priv)
{
	InputInfoPtr pInfo = priv->pInfo;
	WacomCommonPtr common = priv->common;
	InputInfoPtr device = xf86FirstLocalDevice();
	WacomCommonPtr tmpcommon = NULL;
	WacomDevicePtr tmppriv = NULL;

	if (IsPad(priv))
	{
		DBG(4, priv, "No need to link up pad devices.\n");
		return FALSE;
	}

	/* Lookup to find the associated pen and touch */
	for (; device != NULL; device = device->next)
	{
		if (!wcmIsSiblingDevice(pInfo, device, FALSE))
			continue;

		tmppriv = (WacomDevicePtr) device->private;
		tmpcommon = tmppriv->common;

		DBG(4, priv, "Considering link with %s...\n", tmppriv->name);

		/* already linked devices */
		if (tmpcommon->wcmTouchDevice)
		{
			DBG(4, priv, "A link is already in place. Ignoring.\n");
			continue;
		}

		if (IsTouch(tmppriv))
		{
			common->wcmTouchDevice = tmppriv;
			tmpcommon->wcmTouchDevice = tmppriv;
		}
		else if (IsTouch(priv))
		{
			common->wcmTouchDevice = priv;
			tmpcommon->wcmTouchDevice = priv;
		}
		else
		{
			DBG(4, priv, "A link is not necessary. Ignoring.\n");
		}

		if ((common->wcmTouchDevice && IsTablet(priv)) ||
			(tmpcommon->wcmTouchDevice && IsTablet(tmppriv)))
		{
			TabletSetFeature(common, WCM_PENTOUCH);
			TabletSetFeature(tmpcommon, WCM_PENTOUCH);
		}

		if (common->wcmTouchDevice)
		{
			DBG(4, priv, "Link created!\n");
			return TRUE;
		}
	}
	DBG(4, priv, "No suitable device to link with found.\n");
	return FALSE;
}

/**
 * Check if this device was hotplugged by the driver by checking the _source
 * option.
 *
 * Must be called before wcmNeedAutoHotplug()
 *
 * @return True if the source for this device is the wacom driver itself or
 * false otherwise.
 */
static int wcmIsHotpluggedDevice(WacomDevicePtr priv)
{
	InputInfoPtr pInfo = priv->pInfo;
	char *source = xf86CheckStrOption(pInfo->options, "_source", "");
	int matches = (strcmp(source, "_driver/wacom") == 0);
	free(source);
	return matches;
}

static Bool wcmIsWacomDevice (char* fname)
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
		case LENOVO_VENDOR_ID:
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
char *wcmEventAutoDevProbe (WacomDevicePtr priv)
{
	InputInfoPtr pInfo = priv->pInfo;
	/* We are trying to find the right eventX device */
	int i = 0, wait = 0;
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
				xf86IDrvMsg(pInfo, X_PROBED,
					    "probed device is %s (waited %d msec)\n", fname, wait);
				xf86ReplaceStrOption(pInfo->options, "Device", fname);

				/* this assumes there is only one Wacom device on the system */
				return xf86CheckStrOption(pInfo->options, "Device", NULL);
			}
		}
		wait += 100;
		xf86IDrvMsg(pInfo, X_ERROR, "waiting 100 msec (total %dms) for device to become ready\n", wait);
		usleep(100*1000);
	}
	xf86IDrvMsg(pInfo, X_ERROR,
		    "no Wacom event device found (checked %d nodes, waited %d msec)\n", i + 1, wait);
	xf86IDrvMsg(pInfo, X_ERROR, "unable to probe device\n");
	return NULL;
}


/* wcmPreInit - called for each input devices with the driver set to
 * "wacom" */
int wcmPreInit(WacomDevicePtr priv)
{
	InputInfoPtr   pInfo = priv->pInfo;
	WacomCommonPtr common = NULL;
	char		*type = NULL, *device = NULL;
	char		*oldname = NULL;
	int		need_hotplug = 0, is_dependent = 0;

	/*
	   Init process:
	   - if no device is given, auto-probe for one (find a wacom device
	     in /dev/input/event?
	   - open the device file
	   - probe the device
	   - remove duplicate devices if needed
	   - set the device type
	   - hotplug dependent devices if needed
	 */

	device = xf86SetStrOption(pInfo->options, "Device", NULL);
	type = xf86SetStrOption(pInfo->options, "Type", NULL);
	if (!device && !(device = wcmEventAutoDevProbe(priv)))
		goto SetupProc_fail;

	priv->common->device_path = device;
	priv->debugLevel = xf86SetIntOption(pInfo->options,
					    "DebugLevel", priv->debugLevel);

	/* check if the same device file has been added already */
	if (wcmIsDuplicate(device, priv))
		goto SetupProc_fail;

	if (wcmOpen(priv) < 0)
		goto SetupProc_fail;

	/* Try to guess whether it's USB or ISDV4 */
	if (!wcmDetectDeviceClass(priv))
		goto SetupProc_fail;

	/* check if this is the first tool on the port */
	if (!wcmMatchDevice(priv, &common))
		/* initialize supported keys with the first tool on the port */
		wcmDeviceTypeKeys(priv);

	common->debugLevel = xf86SetIntOption(pInfo->options,
					      "CommonDBG", common->debugLevel);
	oldname = strdup(pInfo->name);

	if (wcmIsHotpluggedDevice(priv))
		is_dependent = 1;
	else if ((need_hotplug = wcmNeedAutoHotplug(priv, &type)))
	{
		/* we need subdevices, change the name so all of them have a
		   type. */
		char *new_name;
		if (asprintf(&new_name, "%s %s", pInfo->name, type) == -1)
			new_name = strdup(pInfo->name);
		free(pInfo->name);
		free(priv->name);
		pInfo->name = new_name;
		priv->name = strdup(new_name);
	}

	/* check if the type is valid for those don't need hotplug */
	if(!need_hotplug && !wcmIsAValidType(priv, type)) {
		xf86IDrvMsg(pInfo, X_ERROR, "Invalid type '%s' for this device.\n", type);
		goto SetupProc_fail;
	}

	if (!wcmSetType(priv, type))
		goto SetupProc_fail;

	if (!wcmPreInitParseOptions(priv, need_hotplug, is_dependent))
		goto SetupProc_fail;

	if (!wcmInitModel(priv))
		goto SetupProc_fail;

	if (!wcmPostInitParseOptions(priv, need_hotplug, is_dependent))
		goto SetupProc_fail;

	if (need_hotplug)
	{
		priv->isParent = 1;
		wcmHotplugOthers(priv, oldname);
	}

	wcmClose(priv);

	/* only link them once per port. We need to try for both tablet tool
	 * and touch since we do not know which tool will be added first.
	 */
	if (IsTouch(priv) || (IsTablet(priv) && !common->wcmTouchDevice))
		wcmLinkTouchAndPen(priv);

	free(type);
	free(oldname);

	return Success;

SetupProc_fail:
	/* restart the device list from the next one */
	if (common && priv)
		common->wcmDevices = priv->next;

	wcmClose(priv);
	free(type);
	free(oldname);
	return BadMatch;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
