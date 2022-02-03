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

#ifdef ENABLE_TESTS
#include "wacom-test-suite.h"
#endif

/*****************************************************************************
 * wcmAllocate --
 * Allocate the generic bits needed by any wacom device, regardless of type.
 ****************************************************************************/

WacomDevicePtr wcmAllocate(void *frontend, const char *name)
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
	priv->frontend = frontend;
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
	tool->device = priv;
	/* tool->typeid is set once we know the type - see wcmSetType */

	/* timers */
	priv->serial_timer = wcmTimerNew();
	priv->tap_timer = wcmTimerNew();
	priv->touch_timer = wcmTimerNew();

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

	wcmTimerFree(priv->serial_timer);
	wcmTimerFree(priv->tap_timer);
	wcmTimerFree(priv->touch_timer);
	free(priv->tool);
	wcmFreeCommon(&priv->common);
	free(priv->name);
	free(priv);
}

static Bool
wcmSetFlags(WacomDevicePtr priv, WacomType type)
{
	int flags = 0;

	switch (type)
	{
		case WTYPE_STYLUS:
			flags = ABSOLUTE_FLAG|STYLUS_ID;
			break;
		case WTYPE_TOUCH:
			flags = TOUCH_ID;
			if (TabletHasFeature(priv->common, WCM_LCD))
				flags |= ABSOLUTE_FLAG;
			break;
		case WTYPE_CURSOR:
			flags = CURSOR_ID;
			break;
		case WTYPE_ERASER:
			flags = ABSOLUTE_FLAG|ERASER_ID;
			break;
		case WTYPE_PAD:
			flags = ABSOLUTE_FLAG|PAD_ID;
			break;
		default:
			goto invalid;
	}

	priv->flags = flags;

	/* Set the device id of the "last seen" device on this tool */
	priv->oldState.device_id = wcmGetPhyDeviceID(priv);

	if (!priv->tool)
		return FALSE;

	priv->tool->typeid = DEVICE_ID(flags); /* tool type (stylus/touch/eraser/cursor/pad) */

	return TRUE;

invalid:
	wcmLog(priv, W_ERROR,
		    "No type or invalid type specified.\n"
		    "Must be one of stylus, touch, cursor, eraser, or pad\n");

	return FALSE;
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

static int unlinkDevice(WacomDevicePtr tmppriv, /* tmppriv is the iterated device */
			void *data)
{
	WacomDevicePtr priv = data; /* our device is the data argument */
	WacomCommonPtr common = priv->common;
	WacomCommonPtr tmpcommon = tmppriv->common;
	Bool touch_device = (common->wcmTouchDevice || tmpcommon->wcmTouchDevice);

	if (!touch_device || tmpcommon->tablet_id != common->tablet_id)
		return -ENODEV;

	common->wcmTouchDevice = NULL;
	tmpcommon->wcmTouchDevice = NULL;
	common->tablet_type &= ~WCM_PENTOUCH;
	tmpcommon->tablet_type &= ~WCM_PENTOUCH;
	return 0;
}


/**
 * Unlink the touch tool from the pen of the same device
 */
void wcmUnlinkTouchAndPen(WacomDevicePtr priv)
{
	WacomCommonPtr common = priv->common;

	if (!TabletHasFeature(common, WCM_PENTOUCH))
		return;

	wcmForeachDevice(priv, unlinkDevice, priv);
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
static Bool wcmIsSiblingDevice(WacomDevicePtr privA,
			       WacomDevicePtr privB, Bool logical_only)
{
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


static int matchDevice(WacomDevicePtr privMatch, void *data)
{
	WacomDevicePtr priv = data;

	if (!wcmIsSiblingDevice(priv, privMatch, TRUE))
		return -ENODEV;

	DBG(2, priv, "port share between %s and %s\n",
	    priv->name, privMatch->name);
	/* FIXME: we loose the common->wcmTool here but it
	 * gets re-added during wcmParseOptions. This is
	 * currently required by the code, adding the tool
	 * again here means we trigger the duplicate tool
	 * detection */
	wcmFreeCommon(&priv->common);
	priv->common = wcmRefCommon(privMatch->common);
	priv->next = priv->common->wcmDevices;
	priv->common->wcmDevices = priv;

	return 0;
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
	WacomCommonPtr common = priv->common;

	*common_return = common;

	if (!common->device_path)
		return 0;

	/* If a match is found, priv->common has been replaced */
	if (wcmForeachDevice(priv, matchDevice, priv) == 0)
		*common_return = priv->common;
	return 0;
}

/**
 * Detect the device's device class. We now only support USB so this is
 * somewhat superfluous and should be refactored.
 */
static Bool
wcmDetectDeviceClass(WacomDevicePtr priv)
{
	WacomCommonPtr common = priv->common;
	WacomHWClass *classes[] = {
		WacomGetClassUSB(),
	};

	if (common->wcmDevCls)
		return TRUE;

	for (size_t i = 0; i < ARRAY_SIZE(classes); i++) {
		WacomHWClass *cls = classes[i];
		if (cls && cls->Detect(priv)) {
			common->wcmDevCls = cls;
			return TRUE;
		}
	}

	wcmLog(priv, W_ERROR, "cannot identify device class.\n");

	return FALSE;
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

static int linkDevice(WacomDevicePtr tmppriv, /* the device iterated on */
		      void *data) /* our device */
{
	WacomDevicePtr priv = data;
	WacomCommonPtr common = priv->common;
	WacomCommonPtr tmpcommon = tmppriv->common;

	if (!wcmIsSiblingDevice(priv, tmppriv, FALSE))
		return -ENODEV;

	DBG(4, priv, "Considering link with %s...\n", tmppriv->name);

	/* already linked devices */
	if (tmpcommon->wcmTouchDevice)
	{
		DBG(4, priv, "A link is already in place. Ignoring.\n");
		return -ENODEV;
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
		return 0;
	}

	return -ENODEV;
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
	if (IsPad(priv))
	{
		DBG(4, priv, "No need to link up pad devices.\n");
		return FALSE;
	}

	if (wcmForeachDevice(priv, linkDevice, priv) <= 0)
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
	char *source = wcmOptCheckStr(priv, "_source", "");
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
				wcmLog(priv, W_PROBED, "probed device is %s (waited %d msec)\n", fname, wait);
				wcmOptSetStr(priv, "Device", fname);

				/* this assumes there is only one Wacom device on the system */
				return wcmOptCheckStr(priv, "Device", NULL);
			}
		}
		wait += 100;
		wcmLog(priv, W_ERROR, "waiting 100 msec (total %dms) for device to become ready\n", wait);
		usleep(100*1000);
	}
	wcmLog(priv, W_ERROR,
		    "no Wacom event device found (checked %d nodes, waited %d msec)\n", i + 1, wait);
	wcmLog(priv, W_ERROR, "unable to probe device\n");
	return NULL;
}

/*****************************************************************************
 * wcmInitialToolSize --
 *    Initialize logical size and resolution for individual tool.
 ****************************************************************************/

static void
wcmInitialToolSize(WacomDevicePtr priv)
{
	WacomCommonPtr common = priv->common;

	/* assign max and resolution here since we don't get them during
	 * the configuration stage */
	if (IsTouch(priv))
	{
		priv->maxX = common->wcmMaxTouchX;
		priv->maxY = common->wcmMaxTouchY;
		priv->resolX = common->wcmTouchResolX;
		priv->resolY = common->wcmTouchResolY;
	}
	else
	{
		priv->minX = common->wcmMinX;
		priv->minY = common->wcmMinY;
		priv->maxX = common->wcmMaxX;
		priv->maxY = common->wcmMaxY;
		priv->resolX = common->wcmResolX;
		priv->resolY = common->wcmResolY;
	}

	if (!priv->topX)
		priv->topX = priv->minX;
	if (!priv->topY)
		priv->topY = priv->minY;
	if (!priv->bottomX)
		priv->bottomX = priv->maxX;
	if (!priv->bottomY)
		priv->bottomY = priv->maxY;

	return;
}

static void wcmInitActions(WacomDevicePtr priv)
{
	int i;

	for (i = 0; i < priv->nbuttons; i++)
		wcmResetButtonAction(priv, i);

	if (IsPad(priv)) {
		for (i = 0; i < 4; i++)
			wcmResetStripAction(priv, i);
	}

	if (IsPad(priv) || IsCursor(priv))
	{
		for (i = 0; i < 6; i++)
			wcmResetWheelAction(priv, i);
	}
}

static inline WacomType getType(const char *type)
{
	WacomType wtype = WTYPE_INVALID;

	if (strcasecmp(type, "stylus") == 0)
		wtype = WTYPE_STYLUS;
	else if (strcasecmp(type, "touch") == 0)
		wtype = WTYPE_TOUCH;
	else if (strcasecmp(type, "cursor") == 0)
		wtype = WTYPE_CURSOR;
	else if (strcasecmp(type, "eraser") == 0)
		wtype = WTYPE_ERASER;
	else if (strcasecmp(type, "pad") == 0)
		wtype = WTYPE_PAD;

	return wtype;
}

/* wcmPreInit - called for each input devices with the driver set to
 * "wacom" */
int wcmPreInit(WacomDevicePtr priv)
{
	WacomCommonPtr common = NULL;
	char		*type = NULL, *device = NULL;
	char		*oldname = NULL;
	int		need_hotplug = 0, is_dependent = 0;
	int		fd = -1;

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

	device = wcmOptGetStr(priv, "Device", NULL);
	type = wcmOptGetStr(priv, "Type", NULL);
	if (!device && !(device = wcmEventAutoDevProbe(priv)))
		goto SetupProc_fail;

	priv->common->device_path = device;
	priv->debugLevel = wcmOptGetInt(priv,
					    "DebugLevel", priv->debugLevel);

	/* check if the same device file has been added already */
	if (wcmIsDuplicate(device, priv))
		goto SetupProc_fail;

	if ((fd = wcmOpen(priv)) < 0)
		goto SetupProc_fail;
	wcmSetFd(priv, fd);

	if (!wcmDetectDeviceClass(priv))
		goto SetupProc_fail;

	/* check if this is the first tool on the port */
	if (!wcmMatchDevice(priv, &common))
		/* initialize supported keys with the first tool on the port */
		wcmDeviceTypeKeys(priv);

	common->debugLevel = wcmOptGetInt(priv,
					      "CommonDBG", common->debugLevel);
	oldname = strdup(priv->name);

	if (wcmIsHotpluggedDevice(priv))
		is_dependent = 1;
	else if ((need_hotplug = wcmNeedAutoHotplug(priv, &type)))
	{
		/* we need subdevices, change the name so all of them have a
		   type. */
		char *new_name;
		if (asprintf(&new_name, "%s %s", priv->name, type) == -1)
			new_name = strdup(priv->name);
		free(priv->name);
		priv->name = new_name;
		wcmSetName(priv, new_name);
	}

	/* check if the type is valid for those don't need hotplug */
	if(!need_hotplug && !wcmIsAValidType(priv, type)) {
		wcmLog(priv, W_ERROR, "Invalid type '%s' for this device.\n", type);
		goto SetupProc_fail;
	}

	priv->type = getType(type);

	if (!wcmSetFlags(priv, priv->type))
		goto SetupProc_fail;

	if (!wcmPreInitParseOptions(priv, need_hotplug, is_dependent))
		goto SetupProc_fail;

	if (!wcmInitModel(priv))
		goto SetupProc_fail;

	if (!wcmPostInitParseOptions(priv, need_hotplug, is_dependent))
		goto SetupProc_fail;

	if (!IsPad(priv))
		wcmInitialToolSize(priv);

	wcmInitActions(priv);

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


/*****************************************************************************
 * wcmDevOpen --
 *    Open the physical device and init information structs.
 ****************************************************************************/

int wcmDevOpen(WacomDevicePtr priv)
{
	WacomCommonPtr common = priv->common;
	struct stat st;

	DBG(10, priv, "\n");

	/* open file, if not already open */
	if (common->fd_refs == 0)
	{
		int fd = -1;

		if (!common->device_path) {
			DBG(1, priv, "Missing common device path\n");
			return FALSE;
		}
		if ((fd = wcmOpen(priv)) < 0)
			return FALSE;

		if (fstat(fd, &st) == -1)
		{
			/* can not access major/minor */
			DBG(1, priv, "stat failed (%s).\n", strerror(errno));

			/* older systems don't support the required ioctl.
			 * So, we have to let it pass */
			common->min_maj = 0;
		}
		else
			common->min_maj = st.st_rdev;
		common->fd = fd;
		common->fd_refs = 1;
	}

	/* Grab the common descriptor, if it's available */
	if (wcmGetFd(priv) < 0)
	{
		wcmSetFd(priv, common->fd);
		common->fd_refs++;
	}

	return TRUE;
}

/**
 * Initialize the device axes with their proper attributes.
 *
 * For each axis on the device, we need to provide X with its attributes
 * so that its values can be interpreted properly. To support older X
 * servers without axis labels, each axis index has a de-facto meaning.
 * Any de-facto defined axis index left unused is initialized with default
 * attributes.
 */
static int wcmInitAxes(WacomDevicePtr priv)
{
	WacomCommonPtr common = priv->common;
	int min, max, res;

	/* first valuator: x */
	min = priv->topX;
	max = priv->bottomX;
	res = priv->resolX;
	wcmInitAxis(priv, WACOM_AXIS_X, min, max, res);
	/* We need to store this so we can scale into the right range if top
	 * x/y changes later */
	priv->valuatorMinX = min;
	priv->valuatorMaxX = max;


	/* second valuator: y */
	min = priv->topY;
	max = priv->bottomY;
	res = priv->resolY;
	wcmInitAxis(priv, WACOM_AXIS_Y, min, max, res);
	/* We need to store this so we can scale into the right range if top
	 * x/y changes later */
	priv->valuatorMinY = min;
	priv->valuatorMaxY = max;

	/* third valuator: pressure */
	if (!IsPad(priv))
	{
		res = 0;
		min = 0;
		max = priv->maxCurve;
		wcmInitAxis(priv, WACOM_AXIS_PRESSURE, min, max, res);
	}
	/* FIXME: how to set up this axis on the pad? */

	/* fourth valuator: tilt-x, cursor:z-rotation, pad:strip-x */
	res = 0;
	if (IsPen(priv))
	{
		res = round(TILT_RES);
		min = TILT_MIN;
		max = TILT_MAX;
		wcmInitAxis(priv, WACOM_AXIS_TILT_X, min, max, res);
	}
	else if (IsCursor(priv))
	{
		min = MIN_ROTATION;
		max = MIN_ROTATION + MAX_ROTATION_RANGE - 1;
		wcmInitAxis(priv, WACOM_AXIS_ROTATION, min, max, res);
	}
	else if (IsPad(priv) && TabletHasFeature(common, WCM_STRIP))
	{
		min = 0;
		max = common->wcmMaxStripX;
		wcmInitAxis(priv, WACOM_AXIS_STRIP_X, min, max, res);
	}

	/* fifth valuator: tilt-y, cursor:throttle, pad:strip-y */
	res = 0;
	if (IsPen(priv))
	{
		res = round(TILT_RES);
		min = TILT_MIN;
		max = TILT_MAX;
		wcmInitAxis(priv, WACOM_AXIS_TILT_Y, min, max, res);
	}
	else if (IsCursor(priv))
	{
		min = -1023;
		max = 1023;
		wcmInitAxis(priv, WACOM_AXIS_THROTTLE, min, max, res);
	}
	else if (IsPad(priv) && TabletHasFeature(common, WCM_STRIP))
	{
		min = 0;
		max = common->wcmMaxStripY;
		wcmInitAxis(priv, WACOM_AXIS_STRIP_Y, min, max, res);
	}

	/* sixth valuator: airbrush: abs-wheel, artpen: rotation, pad:abs-wheel */
	res = 0;
	if (IsStylus(priv))
	{
		max = MAX_ROTATION_RANGE + MIN_ROTATION - 1;
		min = MIN_ROTATION;
		wcmInitAxis(priv, WACOM_AXIS_WHEEL, min, max, res);
	}
	else if ((TabletHasFeature(common, WCM_RING)) && IsPad(priv))
	{
		/* Touch ring */
		min = common->wcmMinRing;
		max = common->wcmMaxRing;
		wcmInitAxis(priv, WACOM_AXIS_RING, min, max, res);
	}

	/* seventh valuator: abswheel2 */
	if ((TabletHasFeature(common, WCM_DUALRING)) && IsPad(priv))
	{
		res = 0;
		min = common->wcmMinRing;
		max = common->wcmMaxRing;
		wcmInitAxis(priv, WACOM_AXIS_RING2, min, max, res);
	}

	return TRUE;
}

Bool wcmDevInit(WacomDevicePtr priv)
{
	WacomCommonPtr common =	priv->common;
	int nbaxes, nbbuttons;

	/* Detect tablet configuration, if possible */
	if (priv->common->wcmModel->DetectConfig)
		priv->common->wcmModel->DetectConfig (priv);

	nbaxes = priv->naxes;       /* X, Y, Pressure, Tilt-X, Tilt-Y, Wheel */
	if (!nbaxes || nbaxes > 7)
		nbaxes = priv->naxes = 7;
	nbbuttons = priv->nbuttons; /* Use actual number of buttons, if possible */

	if (IsPad(priv) && TabletHasFeature(priv->common, WCM_DUALRING))
		nbaxes = priv->naxes = nbaxes + 1; /* ABS wheel 2 */

	/* if more than 3 buttons, offset by the four scroll buttons,
	 * otherwise, alloc 7 buttons for scroll wheel. */
	nbbuttons = min(max(nbbuttons + 4, 7), WCM_MAX_BUTTONS);

	DBG(10, priv,
		"(type %d) %d buttons, %d axes\n",
		priv->type, nbbuttons, nbaxes);

	if (!wcmInitButtons(priv, nbbuttons))
	{
		wcmLog(priv, W_ERROR, "unable to allocate Button class device\n");
		return FALSE;
	}

	if (!wcmInitKeyboard(priv))
	{
		wcmLog(priv, W_ERROR, "unable to init Focus class device\n");
		return FALSE;
	}

	if (!wcmInitPointer(priv, nbaxes, is_absolute(priv) ? Absolute : Relative))
	{
		wcmLog(priv, W_ERROR, "unable to init Pointer class device\n");
		return FALSE;
	}

	if (IsTouch(priv)) {
		if (!wcmInitTouch(priv, common->wcmMaxContacts,
				  TabletHasFeature(common, WCM_LCD)))
		{
			wcmLog(priv, W_ERROR, "Unable to init touch class device struct!\n");
			return FALSE;
		}
	}

	if (!wcmInitAxes(priv))
		return FALSE;

	return TRUE;
}

Bool wcmDevStart(WacomDevicePtr priv)
{
	WacomCommonPtr common = priv->common;
	WacomModelPtr model = common->wcmModel;

	/* start the tablet data */
	if (model->Start && (model->Start(priv) != Success))
		return FALSE;

	wcmEnableTool(priv);

	return TRUE;
}

void wcmDevStop(WacomDevicePtr priv)
{

	wcmTimerCancel(priv->tap_timer);
	wcmTimerCancel(priv->serial_timer);
	wcmTimerCancel(priv->touch_timer);
	wcmDisableTool(priv);
	wcmUnlinkTouchAndPen(priv);
}

/*****************************************************************************
 * wcmDevClose --
 ****************************************************************************/

void wcmDevClose(WacomDevicePtr priv)
{
	WacomCommonPtr common = priv->common;

	DBG(4, priv, "Wacom number of open devices = %d\n", common->fd_refs);

	if (wcmGetFd(priv) >= 0)
	{
		if (!--common->fd_refs)
			wcmClose(priv);
		wcmSetFd(priv, -1);
	}
}

#ifdef ENABLE_TESTS

/**
 * After a call to wcmInitialToolSize, the min/max and resolution must be
 * set up correctly.
 *
 * wcmInitialToolSize takes the data from the common rec, so test that the
 * priv has all the values of the common.
 */
TEST_CASE(test_initial_size)
{
	WacomDeviceRec priv = {0};
	WacomCommonRec common = {0};

	/* pin to some numbers */
	int xres = 1920, yres = 1600;
	int minx, maxx = 2 * xres, miny, maxy = 2 * yres;

	priv.common = &common;

	/* FIXME: we currently assume min of 0 in the driver. we cannot cope
	 * with non-zero devices */
	minx = miny = 0;

	common.wcmMaxX = maxx;
	common.wcmMaxY = maxy;
	common.wcmResolX = xres;
	common.wcmResolY = yres;

	wcmInitialToolSize(&priv);

	assert(priv.topX == minx);
	assert(priv.topY == miny);
	assert(priv.bottomX == maxx);
	assert(priv.bottomY == maxy);
	assert(priv.resolX == xres);
	assert(priv.resolY == yres);

	/* Same thing for a touch-enabled device */
	memset(&common, 0, sizeof(common));

	priv.flags = TOUCH_ID;
	assert(IsTouch(&priv));

	common.wcmMaxTouchX = maxx;
	common.wcmMaxTouchY = maxy;
	common.wcmTouchResolX = xres;
	common.wcmTouchResolY = yres;

	wcmInitialToolSize(&priv);

	assert(priv.topX == minx);
	assert(priv.topY == miny);
	assert(priv.bottomX == maxx);
	assert(priv.bottomY == maxy);
	assert(priv.resolX == xres);
	assert(priv.resolY == yres);

}

TEST_CASE(test_set_type)
{
	InputInfoRec info = {0};
	WacomDeviceRec priv = {0};
	WacomTool tool = {0};
	WacomCommonRec common = {0};
	int rc;

#define reset(_info, _priv, _tool, _common) \
	memset(&(_info), 0, sizeof(_info)); \
	memset(&(_priv), 0, sizeof(_priv)); \
	memset(&(_tool), 0, sizeof(_tool)); \
	(_priv).frontend = &(_info); \
	(_info).private = &(_priv); \
	(_priv).tool = &(_tool); \
	(_priv).common = &(_common);


	reset(info, priv, tool, common);
	rc = wcmSetFlags(&priv, WTYPE_STYLUS);
	assert(rc == 1);
	assert(is_absolute(&priv));
	assert(IsStylus(&priv));
	assert(!IsTouch(&priv));
	assert(!IsEraser(&priv));
	assert(!IsCursor(&priv));
	assert(!IsPad(&priv));

	reset(info, priv, tool, common);
	rc = wcmSetFlags(&priv, WTYPE_TOUCH);
	assert(rc == 1);
	/* only some touch screens are absolute */
	assert(!is_absolute(&priv));
	assert(!IsStylus(&priv));
	assert(IsTouch(&priv));
	assert(!IsEraser(&priv));
	assert(!IsCursor(&priv));
	assert(!IsPad(&priv));

	reset(info, priv, tool, common);
	rc = wcmSetFlags(&priv, WTYPE_ERASER);
	assert(rc == 1);
	assert(is_absolute(&priv));
	assert(!IsStylus(&priv));
	assert(!IsTouch(&priv));
	assert(IsEraser(&priv));
	assert(!IsCursor(&priv));
	assert(!IsPad(&priv));

	reset(info, priv, tool, common);
	rc = wcmSetFlags(&priv, WTYPE_CURSOR);
	assert(rc == 1);
	assert(!is_absolute(&priv));
	assert(!IsStylus(&priv));
	assert(!IsTouch(&priv));
	assert(!IsEraser(&priv));
	assert(IsCursor(&priv));
	assert(!IsPad(&priv));

	reset(info, priv, tool, common);
	rc = wcmSetFlags(&priv, WTYPE_PAD);
	assert(rc == 1);
	assert(is_absolute(&priv));
	assert(!IsStylus(&priv));
	assert(!IsTouch(&priv));
	assert(!IsEraser(&priv));
	assert(!IsCursor(&priv));
	assert(IsPad(&priv));

	reset(info, priv, tool, common);
	rc = wcmSetFlags(&priv, WTYPE_INVALID);
	assert(rc == 0);

#undef reset
}

TEST_CASE(test_flag_set)
{
	int i;
	unsigned int flags = 0;

	for (i = 0; i < sizeof(flags); i++)
	{
		int mask = 1 << i;
		flags = 0;

		assert(!MaskIsSet(flags, mask));
		MaskSet(flags, mask);
		assert(flags != 0);
		assert(MaskIsSet(flags, mask));
		MaskClear(flags, mask);
		assert(!MaskIsSet(flags, mask));
		assert(flags == 0);
	}
}

#endif

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
