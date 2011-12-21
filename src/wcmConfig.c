/*
 * Copyright 1995-2002 by Frederic Lepied, France. <Lepied@XFree86.org>
 * Copyright 2002-2010 by Ping Cheng, Wacom. <pingc@wacom.com>
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
#include <wacom-properties.h>

/*****************************************************************************
 * wcmAllocate --
 * Allocate the generic bits needed by any wacom device, regardless of type.
 ****************************************************************************/

static int wcmAllocate(InputInfoPtr pInfo)
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

	pInfo->device_control = gWacomModule.DevProc;
	pInfo->read_input = gWacomModule.DevReadInput;
	pInfo->control_proc = gWacomModule.DevChangeControl;
	pInfo->switch_mode = gWacomModule.DevSwitchMode;
	pInfo->dev = NULL;
	pInfo->private = priv;

	priv->next = NULL;
	priv->pInfo = pInfo;
	priv->common = common;       /* common info pointer */
	priv->oldCursorHwProx = 0;   /* previous cursor hardware proximity */
	priv->nPressCtrl [0] = 0;    /* pressure curve x0 */
	priv->nPressCtrl [1] = 0;    /* pressure curve y0 */
	priv->nPressCtrl [2] = 100;  /* pressure curve x1 */
	priv->nPressCtrl [3] = 100;  /* pressure curve y1 */

	/* Default button and expresskey values, offset buttons 4 and higher
	 * by the 4 scroll buttons. */
	for (i=0; i<WCM_MAX_BUTTONS; i++)
		priv->button[i] = (i < 3) ? i + 1 : i + 5;

	priv->nbuttons = WCM_MAX_BUTTONS;		/* Default number of buttons */
	priv->relup = 5;			/* Default relative wheel up event */
	priv->reldn = 4;			/* Default relative wheel down event */
	/* wheel events are set to 0, but the pad overwrites this default
	 * later in wcmParseOptions, when we have IsPad() available */
	priv->wheelup = 0;			/* Default absolute wheel up event */
	priv->wheeldn = 0;			/* Default absolute wheel down event */
	priv->wheel2up = 0;                     /* Default absolute wheel2 up event */
	priv->wheel2dn = 0;                     /* Default absolute wheel2 down event */
	priv->striplup = 4;			/* Default left strip up event */
	priv->stripldn = 5;			/* Default left strip down event */
	priv->striprup = 4;			/* Default right strip up event */
	priv->striprdn = 5;			/* Default right strip down event */
	priv->naxes = 6;			/* Default number of axes */

	/* JEJ - throttle sampling code */
	priv->throttleLimit = -1;

	common->wcmDevices = priv;

	/* tool */
	priv->tool = tool;
	common->wcmTool = tool;
	tool->next = NULL;          /* next tool in list */
	tool->device = pInfo;
	/* tool->typeid is set once we know the type - see wcmSetType */

	/* timers */
	priv->serial_timer = TimerSet(NULL, 0, 0, NULL, NULL);

	return 1;

error:
	free(tool);
	wcmFreeCommon(&common);
	free(priv);
	return 0;
}

/*****************************************************************************
 * wcmFree --
 * Free the memory allocated by wcmAllocate
 ****************************************************************************/

static void wcmFree(InputInfoPtr pInfo)
{
	WacomDevicePtr priv = pInfo->private;

	if (!priv)
		return;

	TimerFree(priv->serial_timer);
	free(priv->tool);
	wcmFreeCommon(&priv->common);
	free(priv);

	pInfo->private = NULL;
}

static int wcmSetType(InputInfoPtr pInfo, const char *type)
{
	WacomDevicePtr priv = pInfo->private;

	if (!type)
		goto invalid;

	if (xf86NameCmp(type, "stylus") == 0)
	{
		priv->flags = ABSOLUTE_FLAG|STYLUS_ID;
		pInfo->type_name = WACOM_PROP_XI_TYPE_STYLUS;
	} else if (xf86NameCmp(type, "touch") == 0)
	{
		int flags = TOUCH_ID;

		if (TabletHasFeature(priv->common, WCM_LCD))
			flags |= ABSOLUTE_FLAG;

		priv->flags = flags;
		pInfo->type_name = WACOM_PROP_XI_TYPE_TOUCH;
	} else if (xf86NameCmp(type, "cursor") == 0)
	{
		priv->flags = CURSOR_ID;
		pInfo->type_name = WACOM_PROP_XI_TYPE_CURSOR;
	} else if (xf86NameCmp(type, "eraser") == 0)
	{
		priv->flags = ABSOLUTE_FLAG|ERASER_ID;
		pInfo->type_name = WACOM_PROP_XI_TYPE_ERASER;
	} else if (xf86NameCmp(type, "pad") == 0)
	{
		priv->flags = ABSOLUTE_FLAG|PAD_ID;
		pInfo->type_name = WACOM_PROP_XI_TYPE_PAD;
	} else
		goto invalid;

	/* Set the device id of the "last seen" device on this tool */
	priv->old_device_id = wcmGetPhyDeviceID(priv);

	if (!priv->tool)
		return 0;

	priv->tool->typeid = DEVICE_ID(priv->flags); /* tool type (stylus/touch/eraser/cursor/pad) */

	return 1;

invalid:
	xf86Msg(X_ERROR, "%s: No type or invalid type specified.\n"
			 "Must be one of stylus, touch, cursor, eraser, or pad\n",
			 pInfo->name);
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

/* 
 * Be sure to set vmin appropriately for your device's protocol. You want to
 * read a full packet before returning
 *
 * JEJ - Actually, anything other than 1 is probably a bad idea since packet
 * errors can occur.  When that happens, bytes are read individually until it
 * starts making sense again.
 */

static char *default_options[] =
{
	"StopBits",    "1",
	"DataBits",    "8",
	"Parity",      "None",
	"Vmin",        "1",
	"Vtime",       "10",
	"FlowControl", "Xoff",
	NULL
};

/* wcmUninit - called when the device is no longer needed. */

static void wcmUninit(InputDriverPtr drv, InputInfoPtr pInfo, int flags)
{
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	WacomDevicePtr dev;
	WacomDevicePtr *prev;
	WacomCommonPtr common = priv->common;

	if (!priv)
		goto out;

	DBG(1, priv, "\n");

	/* Server 1.10 will UnInit all devices for us */
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) < 12
	if (priv->isParent)
	{
		/* HAL removal sees the parent device removed first. */
		WacomDevicePtr next;
		dev = priv->common->wcmDevices;

		xf86Msg(X_INFO, "%s: removing automatically added devices.\n",
			pInfo->name);

		while(dev)
		{
			next = dev->next;
			if (!dev->isParent)
			{
				xf86Msg(X_INFO, "%s: removing dependent device '%s'\n",
					pInfo->name, dev->pInfo->name);
				DeleteInputDeviceRequest(dev->pInfo->dev);
			}
			dev = next;
		}

		free(pInfo->name);
		pInfo->name = NULL;
	}
#endif

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
	wcmFree(pInfo);
	xf86DeleteInput(pInfo, 0);
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
static Bool wcmMatchDevice(InputInfoPtr pLocal, WacomCommonPtr *common_return)
{
	WacomDevicePtr priv = (WacomDevicePtr)pLocal->private;
	WacomCommonPtr common = priv->common;
	InputInfoPtr pMatch = xf86FirstLocalDevice();

	*common_return = common;

	if (!common->device_path)
		return 0;

	for (; pMatch != NULL; pMatch = pMatch->next)
	{
		WacomDevicePtr privMatch = (WacomDevicePtr)pMatch->private;

		if ((pLocal != pMatch) &&
				strstr(pMatch->drv->driverName, "wacom") &&
				!strcmp(privMatch->common->device_path, common->device_path))
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
wcmDetectDeviceClass(const InputInfoPtr pInfo)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;

	if (common->wcmDevCls)
		return TRUE;

	/* Bluetooth is also considered as USB */
	if (gWacomISDV4Device.Detect(pInfo))
		common->wcmDevCls = &gWacomISDV4Device;
	else if (gWacomUSBDevice.Detect(pInfo))
		common->wcmDevCls = &gWacomUSBDevice;
	else
		xf86Msg(X_ERROR, "%s: cannot identify device class.\n", pInfo->name);

	return (common->wcmDevCls != NULL);
}

static Bool
wcmInitModel(InputInfoPtr pInfo)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;
	char id[BUFFER_SIZE];
	float version;

	/* Initialize the tablet */
	if(common->wcmDevCls->Init(pInfo, id, &version) != Success ||
		wcmInitTablet(pInfo, id, version) != Success)
		return FALSE;

	return TRUE;
}

/**
 * Link the touch tool to the pen of the same device
 * so we can arbitrate the events when posting them.
 */
static void wcmLinkTouchAndPen(InputInfoPtr pInfo)
{
	WacomDevicePtr priv = pInfo->private;
	WacomCommonPtr common = priv->common;
	InputInfoPtr device = xf86FirstLocalDevice();
	WacomCommonPtr tmpcommon = NULL;
	WacomDevicePtr tmppriv = NULL;
	Bool touch_device_assigned = FALSE;

	/* Lookup to find the associated pen and touch */
	for (; device != NULL; device = device->next)
	{
		if (!strcmp(device->drv->driverName, "wacom"))
		{
			tmppriv = (WacomDevicePtr) device->private;
			tmpcommon = tmppriv->common;
			touch_device_assigned = (common->wcmTouchDevice ||
						tmpcommon->wcmTouchDevice);

			/* skip the same tool or already linked devices */
			if ((tmppriv == priv) || touch_device_assigned)
				continue;

			if (tmpcommon->tablet_id == common->tablet_id)
			{
				if (IsTouch(tmppriv) && IsPen(priv))
					common->wcmTouchDevice = tmppriv;
				else if (IsTouch(priv) && IsPen(tmppriv))
					tmpcommon->wcmTouchDevice = priv;

				if (common->wcmTouchDevice ||
						tmpcommon->wcmTouchDevice)
				{
					TabletSetFeature(common, WCM_PENTOUCH);
					TabletSetFeature(tmpcommon, WCM_PENTOUCH);
				}
			}
		}
	}
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
static int wcmIsHotpluggedDevice(InputInfoPtr pInfo)
{
	char *source = xf86CheckStrOption(pInfo->options, "_source", "");
	return !strcmp(source, "_driver/wacom");
}

/* wcmPreInit - called for each input devices with the driver set to
 * "wacom" */
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) < 12
static int NewWcmPreInit(InputDriverPtr drv, InputInfoPtr pInfo, int flags);

static InputInfoPtr wcmPreInit(InputDriverPtr drv, IDevPtr dev, int flags)
{
	InputInfoPtr pInfo = NULL;

	if (!(pInfo = xf86AllocateInput(drv, 0)))
		return NULL;

	pInfo->conf_idev = dev;
	pInfo->name = dev->identifier;

	/* Force default port options to exist because the init
	 * phase is based on those values.
	 */
	xf86CollectInputOptions(pInfo, (const char**)default_options, NULL);
	xf86ProcessCommonOptions(pInfo, pInfo->options);

	if (NewWcmPreInit(drv, pInfo, flags) == Success) {
		pInfo->flags |= XI86_CONFIGURED;
		return pInfo;
	} else {
		xf86DeleteInput(pInfo, 0);
		return NULL;
	}
}

static int NewWcmPreInit(InputDriverPtr drv, InputInfoPtr pInfo, int flags)
#else
static int wcmPreInit(InputDriverPtr drv, InputInfoPtr pInfo, int flags)
#endif
{
	WacomDevicePtr priv = NULL;
	WacomCommonPtr common = NULL;
	const char*	type;
	char*		device, *oldname;
	int		need_hotplug = 0, is_dependent = 0;

	gWacomModule.wcmDrv = drv;

	device = xf86SetStrOption(pInfo->options, "Device", NULL);
	type = xf86SetStrOption(pInfo->options, "Type", NULL);

	/*
	   Init process:
	   - allocate the generic struct used by all device types.
	   - if no device is given, auto-probe for one (find a wacom device
	     in /dev/input/event?
	   - open the device file
	   - probe the device
	   - remove duplicate devices if needed
	   - set the device type
	   - hotplug dependent devices if needed
	 */

	if (!wcmAllocate(pInfo))
		goto SetupProc_fail;

	if (!device && !(device = wcmEventAutoDevProbe(pInfo)))
		goto SetupProc_fail;

	priv = (WacomDevicePtr) pInfo->private;
	priv->common->device_path = device;
	priv->name = pInfo->name;
	priv->debugLevel = xf86SetIntOption(pInfo->options,
					    "DebugLevel", priv->debugLevel);

	/* check if the same device file has been added already */
	if (wcmIsDuplicate(device, pInfo))
		goto SetupProc_fail;

	if (wcmOpen(pInfo) != Success)
		goto SetupProc_fail;

	/* Try to guess whether it's USB or ISDV4 */
	if (!wcmDetectDeviceClass(pInfo))
		goto SetupProc_fail;

	/* check if this is the first tool on the port */
	if (!wcmMatchDevice(pInfo, &common))
		/* initialize supported keys with the first tool on the port */
		wcmDeviceTypeKeys(pInfo);

	common->debugLevel = xf86SetIntOption(pInfo->options,
					      "CommonDBG", common->debugLevel);
	oldname = pInfo->name;

	if (wcmIsHotpluggedDevice(pInfo))
		is_dependent = 1;
	else if ((need_hotplug = wcmNeedAutoHotplug(pInfo, &type)))
	{
		/* we need subdevices, change the name so all of them have a
		   type. */
		char *new_name;
		if (asprintf(&new_name, "%s %s", pInfo->name, type) == -1)
			new_name = strdup(pInfo->name);
		pInfo->name = priv->name = new_name;
	}

	/* check if the type is valid for those don't need hotplug */
	if(!need_hotplug && !wcmIsAValidType(pInfo, type))
		goto SetupProc_fail;

	if (!wcmSetType(pInfo, type))
		goto SetupProc_fail;

	if (!wcmPreInitParseOptions(pInfo, need_hotplug, is_dependent))
		goto SetupProc_fail;

	if (!wcmInitModel(pInfo))
		goto SetupProc_fail;

	if (!wcmPostInitParseOptions(pInfo, need_hotplug, is_dependent))
		goto SetupProc_fail;

	if (need_hotplug)
	{
		priv->isParent = 1;
		wcmHotplugOthers(pInfo, oldname);
	}

	if (pInfo->fd != -1)
	{
		close(pInfo->fd);
		pInfo->fd = -1;
	}

	/* only link them once per port. We need to try for both pen and touch
	 * since we do not know which tool (touch or pen) will be added first.
	 */
	if (IsTouch(priv) || (IsPen(priv) && !common->wcmTouchDevice))
		wcmLinkTouchAndPen(pInfo);

	return Success;

SetupProc_fail:
	/* restart the device list from the next one */
	if (common && priv)
		common->wcmDevices = priv->next;

	if (pInfo->fd != -1)
	{
		close(pInfo->fd);
		pInfo->fd = -1;
	}

	return BadMatch;
}

InputDriverRec WACOM =
{
	1,             /* driver version */
	"wacom",       /* driver name */
	NULL,          /* identify */
	wcmPreInit,    /* pre-init */
	wcmUninit, /* un-init */
	NULL,          /* module */
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 12
	default_options
#endif
};


/* wcmUnplug - Uninitialize the device */

static void wcmUnplug(pointer p)
{
}

/* wcmPlug - called by the module loader */

static pointer wcmPlug(pointer module, pointer options, int* errmaj,
		int* errmin)
{
	xf86AddInputDriver(&WACOM, module, 0);
	return module;
}

static XF86ModuleVersionInfo wcmVersionRec =
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
	&wcmVersionRec,
	wcmPlug,
	wcmUnplug
};

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
