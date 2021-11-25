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

/*
 * This driver is currently able to handle USB Wacom IV and V, serial ISDV4,
 * and bluetooth protocols.
 *
 * Wacom V protocol work done by Raph Levien <raph@gtk.org> and
 * Frédéric Lepied <lepied@xfree86.org>.
 *
 * Modified for Linux USB by MATSUMURA Namihiko,
 * Daniel Egger, Germany. <egger@suse.de>,
 * Frederic Lepied <lepied@xfree86.org>,
 * Brion Vibber <brion@pobox.com>,
 * Aaron Optimizer Digulla <digulla@hepe.com>,
 * Jonathan Layes <jonathan@layes.com>,
 * John Joganic <jej@j-arkadia.com>,
 * Magnus Vigerlöf <Magnus.Vigerlof@ipbo.se>,
 * Peter Hutterer <peter.hutterer@redhat.com>.
 */

#include <config.h>

#include "config-ver.h" /* BUILD_VERSION */

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "xf86Wacom.h"
#include <xf86_OSproc.h>
#include <exevents.h>           /* Needed for InitValuator/Proximity stuff */

#include <xserver-properties.h>
#include <X11/extensions/XKB.h>
#include <xkbsrv.h>

#ifndef XI86_SERVER_FD
#define XI86_SERVER_FD 0x20
#define XI86_DRV_CAP_SERVER_FD 0x01
#endif

static int wcmDevOpen(DeviceIntPtr pWcm);
static void wcmDevClose(WacomDevicePtr priv);

static void wcmKbdLedCallback(DeviceIntPtr di, LedCtrl * lcp)
{
}

static void wcmKbdCtrlCallback(DeviceIntPtr di, KeybdCtrl* ctrl)
{
}

/*****************************************************************************
 * wcmInitialToolSize --
 *    Initialize logical size and resolution for individual tool.
 ****************************************************************************/

TEST_NON_STATIC void
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

static void wcmInitAxis(DeviceIntPtr dev, int axis, Atom label, int min, int max, int res, int mode)
{
	int min_res, max_res;

	if (res != 0)
	{
		max_res = res;
		min_res = 0;
	} else
	{
		res = max_res = min_res = 1;
	}
	InitValuatorAxisStruct(dev, axis,
	                       label,
	                       min, max, res, min_res, max_res,
	                       mode
	);
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
static int wcmInitAxes(DeviceIntPtr pWcm)
{
	InputInfoPtr pInfo = (InputInfoPtr)pWcm->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;

	Atom label;
	int index;
	int min, max, res;
	int mode;

	/* first valuator: x */
	index = 0;
	label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_X);
	min = priv->topX;
	max = priv->bottomX;
	res = priv->resolX;
	mode = Absolute;

	wcmInitAxis(pInfo->dev, index, label, min, max, res, mode);


	/* second valuator: y */
	index = 1;
	label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_Y);
	min = priv->topY;
	max = priv->bottomY;
	res = priv->resolY;
	mode = Absolute;

	wcmInitAxis(pInfo->dev, index, label, min, max, res, mode);


	/* third valuator: pressure */
	index = 2;
	label = None;
	mode = Absolute;
	res = 0;
	min = 0;
	max = 1;

	if (!IsPad(priv))
	{
		label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_PRESSURE);
		max = priv->maxCurve;
	}

	wcmInitAxis(pInfo->dev, index, label, min, max, res, mode);


	/* fourth valuator: tilt-x, cursor:z-rotation, pad:strip-x */
	index = 3;
	label = None;
	mode = Absolute;
	res = 0;
	min = 0;
	max = 1;

	if (IsPen(priv))
	{
		label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_TILT_X),
		res = round(TILT_RES);
		min = TILT_MIN;
		max = TILT_MAX;
	}
	else if (IsCursor(priv))
	{
		label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_RZ);
		min = MIN_ROTATION;
		max = MIN_ROTATION + MAX_ROTATION_RANGE - 1;
	}
	else if (IsPad(priv) && TabletHasFeature(common, WCM_STRIP))
	{ /* XXX: what is this axis label? */
		max = common->wcmMaxStripX;
	}

	wcmInitAxis(pInfo->dev, index, label, min, max, res, mode);


	/* fifth valuator: tilt-y, cursor:throttle, pad:strip-y */
	index = 4;
	label = None;
	mode = Absolute;
	res = 0;
	min = 0;
	max = 1;

	if (IsPen(priv))
	{
		label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_TILT_Y);
		res = round(TILT_RES);
		min = TILT_MIN;
		max = TILT_MAX;
	}
	else if (IsCursor(priv))
	{
		label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_THROTTLE);
		min = -1023;
		max = 1023;
	}
	else if (IsPad(priv) && TabletHasFeature(common, WCM_STRIP))
	{ /* XXX: what is this axis label? */
		max = common->wcmMaxStripY;
	}

	wcmInitAxis(pInfo->dev, index, label, min, max, res, mode);


	/* sixth valuator: airbrush: abs-wheel, artpen: rotation, pad:abs-wheel */
	index = 5;
	label = None;
	mode = Absolute;
	res = 0;
	min = 0;
	max = 1;

	if (IsStylus(priv))
	{
		label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_WHEEL);
		max = MAX_ROTATION_RANGE + MIN_ROTATION - 1;
		min = MIN_ROTATION;
	}
	else if ((TabletHasFeature(common, WCM_RING)) && IsPad(priv))
	{
		/* Touch ring */
		label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_WHEEL);
		min = common->wcmMinRing;
		max = common->wcmMaxRing;
	}

	wcmInitAxis(pInfo->dev, index, label, min, max, res, mode);


	/* seventh valuator: abswheel2 */
	if ((TabletHasFeature(common, WCM_DUALRING)) && IsPad(priv))
	{
		/* XXX: what is this axis label? */
		index = 6;
		label = None;
		mode = Absolute;
		res = 1;

		min = common->wcmMinRing;
		max = common->wcmMaxRing;

		wcmInitAxis(pInfo->dev, index, label, min, max, res, mode);
	}

	return TRUE;
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

/*****************************************************************************
 * wcmDevInit --
 *    Set up the device's buttons, axes and keys
 ****************************************************************************/

static int wcmDevInit(DeviceIntPtr pWcm)
{
	InputInfoPtr pInfo = (InputInfoPtr)pWcm->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common =	priv->common;
	unsigned char butmap[WCM_MAX_BUTTONS+1];
	int nbaxes, nbbuttons, nbkeys;
	int loop;
        Atom btn_labels[WCM_MAX_BUTTONS] = {0};
        Atom axis_labels[MAX_VALUATORS] = {0};

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
	nbkeys = nbbuttons;         /* Same number of keys since any button may be
	                             * configured as an either mouse button or key */

	DBG(10, priv,
		"(%s) %d buttons, %d keys, %d axes\n",
		pInfo->type_name,
		nbbuttons, nbkeys, nbaxes);

	for(loop=1; loop<=nbbuttons; loop++)
		butmap[loop] = loop;

	/* FIXME: button labels would be nice */
	if (InitButtonClassDeviceStruct(pInfo->dev, nbbuttons,
					btn_labels,
					butmap) == FALSE)
	{
		xf86IDrvMsg(pInfo, X_ERROR, "unable to allocate Button class device\n");
		return FALSE;
	}

	if (InitFocusClassDeviceStruct(pInfo->dev) == FALSE)
	{
		xf86IDrvMsg(pInfo, X_ERROR, "unable to init Focus class device\n");
		return FALSE;
	}

	if (InitPtrFeedbackClassDeviceStruct(pInfo->dev,
		wcmDevControlProc) == FALSE)
	{
		xf86IDrvMsg(pInfo, X_ERROR, "unable to init ptr feedback\n");
		return FALSE;
	}

	if (InitProximityClassDeviceStruct(pInfo->dev) == FALSE)
	{
			xf86IDrvMsg(pInfo, X_ERROR, "unable to init proximity class device\n");
			return FALSE;
	}

	/* axis_labels is just zeros, we set up each valuator with the
	 * correct property later */
	if (InitValuatorClassDeviceStruct(pInfo->dev, nbaxes,
					  axis_labels,
					  GetMotionHistorySize(),
					  (is_absolute(priv) ?  Absolute : Relative) | OutOfProximity) == FALSE)
	{
		xf86IDrvMsg(pInfo, X_ERROR, "unable to allocate Valuator class device\n");
		return FALSE;
	}


	if (!InitKeyboardDeviceStruct(pInfo->dev, NULL, NULL, wcmKbdCtrlCallback)) {
		xf86IDrvMsg(pInfo, X_ERROR, "unable to init kbd device struct\n");
		return FALSE;
	}
	if(InitLedFeedbackClassDeviceStruct (pInfo->dev, wcmKbdLedCallback) == FALSE) {
		xf86IDrvMsg(pInfo, X_ERROR, "unable to init led feedback device struct\n");
		return FALSE;
	}

	if (IsTouch(priv)) {
		if (!InitTouchClassDeviceStruct(pInfo->dev, common->wcmMaxContacts,
						TabletHasFeature(common, WCM_LCD) ? XIDirectTouch : XIDependentTouch,
						2))
		{
			xf86IDrvMsg(pInfo, X_ERROR, "Unable to init touch class device struct!\n");
			return FALSE;
		}
		priv->common->touch_mask = valuator_mask_new(2);
	}

	if (!IsPad(priv))
	{
		wcmInitialToolSize(priv);
	}

	if (!wcmInitAxes(pWcm))
		return FALSE;

	wcmInitActions(priv);
	InitWcmDeviceProperties(priv);

	return TRUE;
}

/*****************************************************************************
 * wcmOpen --
 ****************************************************************************/

int wcmOpen(WacomDevicePtr priv)
{
	InputInfoPtr pInfo = priv->pInfo;
	WacomCommonPtr common = priv->common;

	DBG(1, priv, "opening device file\n");

	pInfo->fd = xf86OpenSerial(pInfo->options);
	if (pInfo->fd < 0)
	{
		int saved_errno = errno;
		xf86IDrvMsg(pInfo, X_ERROR, "Error opening %s (%s)\n",
			common->device_path, strerror(errno));
		return -saved_errno;
	}

	return pInfo->fd;
}

/*****************************************************************************
 * wcmClose --
 ****************************************************************************/

void wcmClose(WacomDevicePtr priv)
{
	InputInfoPtr pInfo = priv->pInfo;

	DBG(1, priv, "closing device file\n");

	if (pInfo->fd > -1 && !(pInfo->flags & XI86_SERVER_FD)) {
		xf86CloseSerial(pInfo->fd);
		pInfo->fd = -1;
	}
}

/*****************************************************************************
 * wcmDevOpen --
 *    Open the physical device and init information structs.
 ****************************************************************************/

static int wcmDevOpen(DeviceIntPtr pWcm)
{
	InputInfoPtr pInfo = (InputInfoPtr)pWcm->public.devicePrivate;
	WacomDevicePtr priv = pInfo->private;
	WacomCommonPtr common = priv->common;
	WacomModelPtr model = common->wcmModel;
	struct stat st;

	DBG(10, priv, "\n");

	/* If fd management is done by the server, skip common fd handling */
	if (pInfo->flags & XI86_SERVER_FD)
		goto got_fd;

	/* open file, if not already open */
	if (common->fd_refs == 0)
	{
		if (!common->device_path) {
			DBG(1, priv, "Missing common device path\n");
			return FALSE;
		}
		if (wcmOpen(priv) < 0)
			return FALSE;

		if (fstat(pInfo->fd, &st) == -1)
		{
			/* can not access major/minor */
			DBG(1, priv, "stat failed (%s).\n", strerror(errno));

			/* older systems don't support the required ioctl.
			 * So, we have to let it pass */
			common->min_maj = 0;
		}
		else
			common->min_maj = st.st_rdev;
		common->fd = pInfo->fd;
		common->fd_refs = 1;
	}

	/* Grab the common descriptor, if it's available */
	if (pInfo->fd < 0)
	{
		pInfo->fd = common->fd;
		common->fd_refs++;
	}

got_fd:
	/* start the tablet data */
	if (model->Start && (model->Start(priv) != Success))
		return !Success;

	return TRUE;
}

static int wcmReady(WacomDevicePtr priv)
{
#ifdef DEBUG
	InputInfoPtr pInfo = priv->pInfo;
#endif
	int n = xf86WaitForInput(pInfo->fd, 0);
	if (n < 0) {
		int saved_errno = errno;
		xf86IDrvMsg(pInfo, X_ERROR, "select error: %s\n", strerror(errno));
		return -saved_errno;
	} else {
		DBG(10, priv, "%d numbers of data\n", n);
	}
	return n;
}

/*****************************************************************************
 * wcmDevReadInput --
 *   Read the device on IO signal
 ****************************************************************************/

static void wcmDevReadInput(InputInfoPtr pInfo)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	int loop=0;
	#define MAX_READ_LOOPS 10

	/* move data until we exhaust the device */
	for (loop=0; loop < MAX_READ_LOOPS; ++loop)
	{
		int rc;

		/* verify that there is still data in pipe */
		if (wcmReady(priv) <= 0)
			break;

		/* dispatch */
		if ((rc = wcmReadPacket(priv)) < 0) {
			LogMessageVerbSigSafe(X_ERROR, 0,
					      "%s: Error reading wacom device : %s\n", pInfo->name, strerror(-rc));
			if (rc == -ENODEV)
				xf86RemoveEnabledDevice(pInfo);
			break;
		}
	}

#ifdef DEBUG
	/* report how well we're doing */
	if (loop > 0)
	{
		if (loop >= MAX_READ_LOOPS)
			DBG(1, priv, "Can't keep up!!!\n");
		else
			DBG(10, priv, "Read (%d)\n",loop);
	}
#endif
}

static int wcmDevChangeControl(InputInfoPtr pInfo, xDeviceCtl * control)
{
#ifdef DEBUG
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	DBG(3, priv, "\n");
#endif
	return Success;
}

/*****************************************************************************
 * wcmDevControlProc --
 ****************************************************************************/

void wcmDevControlProc(DeviceIntPtr device, PtrCtrl* ctrl)
{
#ifdef DEBUG
	InputInfoPtr pInfo = (InputInfoPtr)device->public.devicePrivate;
	WacomDevicePtr priv = pInfo->private;

	DBG(4, priv, "called\n");
#endif
	return;
}

/*****************************************************************************
 * wcmDevClose --
 ****************************************************************************/

static void wcmDevClose(WacomDevicePtr priv)
{
	InputInfoPtr pInfo = priv->pInfo;
	WacomCommonPtr common = priv->common;

	/* If fd management is done by the server, skip common fd handling */
	if (pInfo->flags & XI86_SERVER_FD)
		return;

	DBG(4, priv, "Wacom number of open devices = %d\n", common->fd_refs);

	if (pInfo->fd >= 0)
	{
		if (!--common->fd_refs)
			wcmClose(priv);
		pInfo->fd = -1;
	}
}

/*****************************************************************************
 * wcmDevProc --
 *   Handle the initialization, etc. of a wacom tablet. Called by the server
 *   once with DEVICE_INIT when the device becomes available, then
 *   DEVICE_ON/DEVICE_OFF possibly multiple times as the device is enabled
 *   and disabled. DEVICE_CLOSE is called before removal of the device.
 ****************************************************************************/

static int wcmDevProc(DeviceIntPtr pWcm, int what)
{
	InputInfoPtr pInfo = (InputInfoPtr)pWcm->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	Status rc = !Success;

	DBG(2, priv, "BEGIN dev=%p priv=%p "
			"type=%s flags=%d fd=%d what=%s\n",
			(void *)pWcm, (void *)priv,
			pInfo->type_name,
			priv->flags, pInfo ? pInfo->fd : -1,
			(what == DEVICE_INIT) ? "INIT" :
			(what == DEVICE_OFF) ? "OFF" :
			(what == DEVICE_ON) ? "ON" :
			(what == DEVICE_CLOSE) ? "CLOSE" : "???");

	switch (what)
	{
		case DEVICE_INIT:
			if (!wcmDevInit(pWcm))
				goto out;
			break;

		case DEVICE_ON:
			if (!wcmDevOpen(pWcm))
				goto out;
			wcmEnableTool(priv);
			xf86AddEnabledDevice(pInfo);
			pWcm->public.on = TRUE;
			break;

		case DEVICE_OFF:
			TimerCancel(priv->tap_timer);
			TimerCancel(priv->serial_timer);
			TimerCancel(priv->touch_timer);
			wcmDisableTool(priv);
			wcmUnlinkTouchAndPen(priv);
			if (pInfo->fd >= 0)
			{
				xf86RemoveEnabledDevice(pInfo);
				wcmDevClose(priv);
			}
			pWcm->public.on = FALSE;
			break;
		case DEVICE_CLOSE:
			break;
#if ABI_XINPUT_VERSION >= SET_ABI_VERSION(19, 1)
		case DEVICE_ABORT:
			break;
#endif
		default:
			xf86IDrvMsg(pInfo, X_ERROR,
				    "invalid mode=%d. This is an X server bug.\n", what);
			goto out;
	} /* end switch */

	rc = Success;

out:
	if (rc != Success)
		DBG(1, priv, "Failed during %d\n", what);
	return rc;
}

static int wcmDevSwitchMode(ClientPtr client, DeviceIntPtr dev, int mode)
{
	InputInfoPtr pInfo = (InputInfoPtr)dev->public.devicePrivate;
#ifdef DEBUG
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;

	DBG(3, priv, "dev=%p mode=%d\n",
		(void *)dev, mode);
#endif
	/* Share this call with sendAButton in wcmCommon.c */
	return wcmDevSwitchModeCall(priv, mode);
}

static int preInit(InputDriverPtr drv, InputInfoPtr pInfo, int flags)
{
	WacomDevicePtr priv = NULL;

	pInfo->device_control = wcmDevProc;
	pInfo->read_input = wcmDevReadInput;
	pInfo->control_proc = wcmDevChangeControl;
	pInfo->switch_mode = wcmDevSwitchMode;
	pInfo->dev = NULL;

	if (!(priv = wcmAllocate(pInfo, pInfo->name)))
		return BadAlloc;

	pInfo->private = priv;

	return wcmPreInit(priv);
}

static void unInit(InputDriverPtr drv, InputInfoPtr pInfo, int flags)
{
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;

	wcmUnInit(priv);
	pInfo->private = NULL;
	xf86DeleteInput(pInfo, 0);
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
	"StopBits",    "1",
	"DataBits",    "8",
	"Parity",      "None",
	"Vmin",        "1",
	"Vtime",       "10",
	"FlowControl", "Xoff",
	NULL
};

static InputDriverRec WACOM =
{
	1,             /* driver version */
	"wacom",       /* driver name */
	NULL,          /* identify */
	preInit,       /* pre-init */
	unInit,        /* un-init */
	NULL,          /* module */
	default_options,
	XI86_DRV_CAP_SERVER_FD,
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

	xf86Msg(X_INFO, "Build version: " BUILD_VERSION "\n");
	usbListModels();

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
