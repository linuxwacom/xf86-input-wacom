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
#include <wacom-properties.h>
#include <X11/extensions/XKB.h>
#include <xkbsrv.h>

#include <stdbool.h>

#ifndef XI86_SERVER_FD
#define XI86_SERVER_FD 0x20
#define XI86_DRV_CAP_SERVER_FD 0x01
#endif

__attribute__((__format__(__printf__ , 2, 0)))
static void
log_sigsafe(WacomLogType type, const char *format, va_list args)
{
	MessageType xtype = (MessageType)type;
	LogVMessageVerbSigSafe(xtype, -1, format, args);
}

void
wcmLog(WacomDevicePtr priv, WacomLogType type, const char *format, ...)
{
	MessageType xtype = (MessageType)type;
	va_list args;

	va_start(args, format);
	xf86VIDrvMsgVerb(priv->frontend, xtype, 0, format, args);
	va_end(args);
}

void wcmLogSafe(WacomDevicePtr priv, WacomLogType type, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	log_sigsafe(type, format, args);
	va_end(args);
}

void wcmLogCommon(WacomCommonPtr common, WacomLogType type, const char *format, ...)
{
	MessageType xtype = (MessageType)type;
	va_list args;

	va_start(args, format);
	LogVMessageVerb(xtype, -1, format, args);
	va_end(args);
}

void wcmLogCommonSafe(WacomCommonPtr common, WacomLogType type, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	log_sigsafe(type, format, args);
	va_end(args);
}

void
wcmLogDebugDevice(WacomDevicePtr priv, int debug_level, const char *func, const char *format, ...)
{
	va_list args;

	LogMessageVerbSigSafe(X_INFO, -1, "%s (%d:%s): ", priv->name, debug_level, func);
	va_start(args, format);
	log_sigsafe(W_NONE, format, args);
	va_end(args);
}

void
wcmLogDebugCommon(WacomCommonPtr common, int debug_level, const char *func, const char *format, ...)
{
	va_list args;

	LogMessageVerbSigSafe(X_INFO, -1, "%s (%d:%s): ", common->device_path, debug_level, func);
	va_start(args, format);
	log_sigsafe(W_NONE, format, args);
	va_end(args);
}


char *wcmOptGetStr(WacomDevicePtr priv, const char *key, const char *default_value)
{
	InputInfoPtr pInfo = priv->frontend;
	return xf86SetStrOption(pInfo->options, key, default_value);
}

int wcmOptGetInt(WacomDevicePtr priv, const char *key, int default_value)
{
	InputInfoPtr pInfo = priv->frontend;
	return xf86SetIntOption(pInfo->options, key, default_value);
}

bool wcmOptGetBool(WacomDevicePtr priv, const char *key, bool default_value)
{
	InputInfoPtr pInfo = priv->frontend;
	return !!xf86SetBoolOption(pInfo->options, key, default_value);
}

/* Get the option of the given type, quietly (without logging) */
char *wcmOptCheckStr(WacomDevicePtr priv, const char *key, const char *default_value)
{
	InputInfoPtr pInfo = priv->frontend;
	return xf86CheckStrOption(pInfo->options, key, default_value);
}

int wcmOptCheckInt(WacomDevicePtr priv, const char *key, int default_value)
{
	InputInfoPtr pInfo = priv->frontend;
	return xf86CheckIntOption(pInfo->options, key, default_value);
}

bool wcmOptCheckBool(WacomDevicePtr priv, const char *key, bool default_value)
{
	InputInfoPtr pInfo = priv->frontend;
	return !!xf86CheckBoolOption(pInfo->options, key, default_value);
}

/* Change the option to the new value */
void wcmOptSetStr(WacomDevicePtr priv, const char *key, const char *value)
{
	InputInfoPtr pInfo = priv->frontend;
	pInfo->options = xf86ReplaceStrOption(pInfo->options, key, value);
}

void wcmOptSetInt(WacomDevicePtr priv, const char *key, int value)
{
	InputInfoPtr pInfo = priv->frontend;
	pInfo->options = xf86ReplaceIntOption(pInfo->options, key, value);
}

void wcmOptSetBool(WacomDevicePtr priv, const char *key, bool value)
{
	InputInfoPtr pInfo = priv->frontend;
	pInfo->options = xf86ReplaceIntOption(pInfo->options, key, value);
}

struct _WacomTimer {
	OsTimerPtr timer;
	WacomTimerCallback func;
	void *userdata;
};

static CARD32 xserverTimerFunc(OsTimerPtr ostimer, CARD32 time, void *arg)
{
	WacomTimerPtr timer = arg;

	return timer->func(timer, (uint32_t)time, timer->userdata);
}

WacomTimerPtr wcmTimerNew(void)
{
	uint32_t flags = 0; /* relative */
	WacomTimerPtr timer = calloc(1, sizeof(*timer));

	timer->timer = TimerSet(timer->timer, flags, 0, NULL, NULL);

	return timer;
}

void wcmTimerFree(WacomTimerPtr timer)
{
	TimerCancel(timer->timer);
	TimerFree(timer->timer);
	free(timer);
}

void wcmTimerCancel(WacomTimerPtr timer)
{
	TimerCancel(timer->timer);
}

void wcmTimerSet(WacomTimerPtr timer, uint32_t millis, WacomTimerCallback func, void *userdata)
{
	uint32_t flags = 0; /* relative */

	timer->func = func;
	timer->userdata = userdata;
	TimerSet(timer->timer, flags, millis, xserverTimerFunc, timer);
}


/**
 * Duplicate xf86 options, replace the "type" option with the given type
 * (and the name with "$name $type" and convert them to InputOption
 *
 * @param basename Kernel device name for this device
 * @param type Tool type (cursor, eraser, etc.)
 * @param serial Serial number this device should be bound to (-1 for "any")
 */
static InputOption *wcmOptionDupConvert(WacomDevicePtr priv, const char* name, const char *type, int serial)
{
	WacomCommonPtr common = priv->common;
	InputInfoPtr pInfo = priv->frontend;
	pointer original = pInfo->options;
	WacomToolPtr ser = common->serials;
	InputOption *iopts = NULL;
	pointer options, o;

	options = xf86OptionListDuplicate(original);
	options = xf86ReplaceStrOption(options, "Type", type);
	options = xf86ReplaceStrOption(options, "Name", name);

	if (serial > -1)
		options = xf86ReplaceIntOption(options, "Serial", ser->serial);

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
static InputAttributes* wcmDuplicateAttributes(WacomDevicePtr priv,
					       const char *type)
{
	InputInfoPtr pInfo = priv->frontend;
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
 * @param priv The parent device
 * @param basename The base name for the device (type will be appended)
 * @param type Type name for this tool
 * @param serial Serial number this device should be bound to (-1 for "any")
 */
void wcmQueueHotplug(WacomDevicePtr priv, const char* name, const char *type, int serial)
{
	WacomHotplugInfo *hotplug_info;

	hotplug_info = calloc(1, sizeof(WacomHotplugInfo));

	if (!hotplug_info)
	{
		wcmLog(priv, W_ERROR, "OOM, cannot hotplug dependent devices\n");
		return;
	}

	hotplug_info->input_options = wcmOptionDupConvert(priv, name, type, serial);
	hotplug_info->attrs = wcmDuplicateAttributes(priv, type);
	QueueWorkProc(wcmHotplugDevice, serverClient, hotplug_info);
}

/*****************************************************************************
 * Event helpers
 ****************************************************************************/
void wcmEmitKeycode(WacomDevicePtr priv, int keycode, int state)
{
	InputInfoPtr pInfo = priv->frontend;
	DeviceIntPtr keydev = pInfo->dev;

	xf86PostKeyboardEvent (keydev, keycode, state);
}

static inline void
convertAxes(const WacomAxisData *axes, int *first_out, int *num_out, int valuators[7])
{
	int first = 7;
	int last = -1;

	memset(valuators, 0, 7 * sizeof(valuators[0]));

	for (enum WacomAxisType which = _WACOM_AXIS_LAST; which > 0; which >>= 1)
	{
		int value;
		Bool has_value = wcmAxisGet(axes, which, &value);
		int pos;

		if (!has_value)
			continue;

		/* Positions need to match wcmInitAxis */
		switch (which){
		case WACOM_AXIS_X: pos = 0; break;
		case WACOM_AXIS_Y: pos = 1; break;
		case WACOM_AXIS_PRESSURE: pos = 2; break;
		case WACOM_AXIS_TILT_X: pos = 3; break;
		case WACOM_AXIS_TILT_Y: pos = 4; break;
		case WACOM_AXIS_STRIP_X: pos = 3; break;
		case WACOM_AXIS_STRIP_Y: pos = 4; break;
		case WACOM_AXIS_ROTATION: pos = 3; break;
		case WACOM_AXIS_THROTTLE: pos = 4; break;
		case WACOM_AXIS_WHEEL: pos = 5; break;
		case WACOM_AXIS_RING: pos = 5; break;
		case WACOM_AXIS_RING2: pos = 6; break;
			break;
		default:
			abort();
		}

		first = min(first, pos);
		last = max(last, pos);
		valuators[pos] = value;
	}

	if (last < 0)
		first = 0;
	*first_out = first;
	*num_out = last - first + 1;
}

void wcmEmitProximity(WacomDevicePtr priv, bool is_proximity_in,
		      const WacomAxisData *axes)
{
	InputInfoPtr pInfo = priv->frontend;
	int valuators[7];
	int first_val, num_vals;

	convertAxes(axes, &first_val, &num_vals, valuators);

	xf86PostProximityEventP(pInfo->dev, is_proximity_in, first_val, num_vals, valuators);
}

void wcmEmitMotion(WacomDevicePtr priv, bool is_absolute, const WacomAxisData *axes)
{
	InputInfoPtr pInfo = priv->frontend;
	int valuators[7];
	int first_val, num_vals;

	convertAxes(axes, &first_val, &num_vals, valuators);

	xf86PostMotionEventP(pInfo->dev, is_absolute, first_val, num_vals, valuators);
}

void wcmEmitButton(WacomDevicePtr priv, bool is_absolute, int button, bool is_press, const WacomAxisData *axes)
{
	InputInfoPtr pInfo = priv->frontend;
	int valuators[7];
	int first_val, num_vals;

	convertAxes(axes, &first_val, &num_vals, valuators);

	xf86PostButtonEventP(pInfo->dev, is_absolute, button, is_press, first_val, num_vals, valuators);
}

void wcmEmitTouch(WacomDevicePtr priv, int type, unsigned int touchid, int x, int y)
{
	InputInfoPtr pInfo = priv->frontend;
	/* FIXME: this should be part of this interface here */
	ValuatorMask *mask = priv->common->touch_mask;

	valuator_mask_set(mask, 0, x);
	valuator_mask_set(mask, 1, y);

	xf86PostTouchEvent(pInfo->dev, touchid, type, 0, mask);
}

void wcmNotifyEvdev(WacomDevicePtr priv, const struct input_event *event)
{
	/* NOOP */
}


void wcmInitAxis(WacomDevicePtr priv, enum WacomAxisType type,
			int min, int max, int res)
{

	InputInfoPtr pInfo = priv->frontend;
	Atom label = None;
	int min_res, max_res;
	int index;

	if (res != 0)
	{
		max_res = res;
		min_res = 0;
	} else
	{
		res = max_res = min_res = 1;
	}

	switch (type) {
		case WACOM_AXIS_X:
			index = 0;
			label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_X);
			break;
		case WACOM_AXIS_Y:
			index = 1;
			label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_Y);
			break;
		case WACOM_AXIS_PRESSURE:
			index = 2;
			label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_PRESSURE);
			break;
		case WACOM_AXIS_TILT_X:
			index = 3;
			label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_TILT_X);
			break;
		case WACOM_AXIS_TILT_Y:
			index = 4;
			label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_TILT_Y);
			break;
		case WACOM_AXIS_STRIP_X:
			index = 3;
			break;
		case WACOM_AXIS_STRIP_Y:
			index = 4;
			break;
		case WACOM_AXIS_ROTATION:
			index = 3;
			label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_RZ);
			break;
		case WACOM_AXIS_THROTTLE:
			index = 4;
			label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_THROTTLE);
			break;
		case WACOM_AXIS_WHEEL:
		case WACOM_AXIS_RING:
			index = 5;
			label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_WHEEL);
			break;
		case WACOM_AXIS_RING2:
			index = 6;
			break;
		default:
			abort();
	}

	InitValuatorAxisStruct(pInfo->dev, index,
	                       label,
	                       min, max, res, min_res, max_res,
	                       Absolute);
}

bool wcmInitButtons(WacomDevicePtr priv, unsigned int nbuttons)
{
	InputInfoPtr pInfo = priv->frontend;
	unsigned char butmap[WCM_MAX_BUTTONS+1];
	/* FIXME: button labels would be nice */
	Atom btn_labels[WCM_MAX_BUTTONS] = {0};

	for(unsigned char loop=1; loop<=nbuttons; loop++)
		butmap[loop] = loop;

	return InitButtonClassDeviceStruct(pInfo->dev, nbuttons,
					   btn_labels,
					   butmap);
}

static void wcmKbdLedCallback(DeviceIntPtr di, LedCtrl * lcp) { }
static void wcmKbdCtrlCallback(DeviceIntPtr di, KeybdCtrl* ctrl) { }

bool wcmInitKeyboard(WacomDevicePtr priv)
{
	InputInfoPtr pInfo = priv->frontend;
	return InitFocusClassDeviceStruct(pInfo->dev) &&
		InitKeyboardDeviceStruct(pInfo->dev, NULL, NULL, wcmKbdCtrlCallback) &&
		InitLedFeedbackClassDeviceStruct (pInfo->dev, wcmKbdLedCallback);
}

static void wcmDevControlProc(DeviceIntPtr device, PtrCtrl* ctrl) { }

bool wcmInitPointer(WacomDevicePtr priv, int naxes, bool is_absolute)
{
	InputInfoPtr pInfo = priv->frontend;
        Atom axis_labels[MAX_VALUATORS] = {0};
	int mode = is_absolute ? Absolute : Relative;

	/* axis_labels is just zeros, we set up each valuator with the
	 * correct property later */

	return InitPtrFeedbackClassDeviceStruct(pInfo->dev, wcmDevControlProc) &&
		InitProximityClassDeviceStruct(pInfo->dev) &&
		InitValuatorClassDeviceStruct(pInfo->dev, naxes,
					      axis_labels,
					      GetMotionHistorySize(),
					      mode | OutOfProximity);
}

bool wcmInitTouch(WacomDevicePtr priv, int ntouches, bool is_direct_touch)
{
	InputInfoPtr pInfo = priv->frontend;
	WacomCommonPtr common = priv->common;

	/* Ugly. This needs to be allocated outside the event processing path,
	 * it should really be part of the driver layer but for that we'd need
	 * yet another struct to handle (or a server ABI change).
	 * Let's just allocate here and live with it
	 */
	priv->common->touch_mask = valuator_mask_new(2);

	return InitTouchClassDeviceStruct(pInfo->dev, common->wcmMaxContacts,
					  is_direct_touch ? XIDirectTouch : XIDependentTouch,
					  2);
}

int wcmForeachDevice(WacomDevicePtr priv, WacomDeviceCallback func, void *data)
{
	InputInfoPtr pInfo = priv->frontend;
	InputInfoPtr pOther;
	int nmatch = 0;

	for (pOther = xf86FirstLocalDevice(); pOther; pOther = pOther->next)
	{
		WacomDevicePtr pPriv;
		int rc;

		if (pInfo == pOther || !strstr(pOther->drv->driverName, "wacom"))
			continue;

		pPriv = pOther->private;
		rc = func(pPriv, data);
		if (rc == -ENODEV)
			continue;
		if (rc < 0)
			return -rc;
		nmatch += 1; /* zero counts as matched */
		if (rc == 0)
			break;
	}

	return nmatch;
}

int wcmGetFd(WacomDevicePtr priv)
{
	InputInfoPtr pInfo = priv->frontend;
	return pInfo->fd;
}

void wcmSetFd(WacomDevicePtr priv, int fd)
{
	InputInfoPtr pInfo = priv->frontend;
	pInfo->fd = fd;
}

void wcmSetName(WacomDevicePtr priv, const char *name)
{
	InputInfoPtr pInfo = priv->frontend;

	free(pInfo->name);
	pInfo->name = strdup(name);
}

uint32_t wcmTimeInMillis(void)
{
	return GetTimeInMillis();
}

/*****************************************************************************
 * wcmOpen --
 ****************************************************************************/

int wcmOpen(WacomDevicePtr priv)
{
	InputInfoPtr pInfo = priv->frontend;
	WacomCommonPtr common = priv->common;
	int fd;

	DBG(1, priv, "opening device file\n");

	fd = xf86OpenSerial(pInfo->options);
	if (fd < 0)
	{
		int saved_errno = errno;
		wcmLog(priv, W_ERROR, "Error opening %s (%s)\n",
			common->device_path, strerror(errno));
		return -saved_errno;
	}

	return fd;
}

/*****************************************************************************
 * wcmClose --
 ****************************************************************************/

void wcmClose(WacomDevicePtr priv)
{
	InputInfoPtr pInfo = priv->frontend;

	DBG(1, priv, "closing device file\n");

	if (pInfo->fd > -1 && !(pInfo->flags & XI86_SERVER_FD)) {
		xf86CloseSerial(pInfo->fd);
		pInfo->fd = -1;
	}
}

static int wcmReady(WacomDevicePtr priv)
{
	InputInfoPtr pInfo = priv->frontend;
	int n = xf86WaitForInput(pInfo->fd, 0);
	if (n < 0) {
		int saved_errno = errno;
		wcmLog(priv, W_ERROR, "select error: %s\n", strerror(errno));
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
			wcmLogSafe(priv, W_ERROR,
			       "%s: Error reading wacom device : %s\n", priv->name, strerror(-rc));
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
			if (!wcmDevInit(priv))
				goto out;
			InitWcmDeviceProperties(priv);
			break;

		case DEVICE_ON:
			/* If fd management is done by the server, skip common fd handling */
			if ((pInfo->flags & XI86_SERVER_FD) == 0 && !wcmDevOpen(priv))
				goto out;
			if (!wcmDevStart(priv))
				goto out;
			xf86AddEnabledDevice(pInfo);
			pWcm->public.on = TRUE;
			break;

		case DEVICE_OFF:
			wcmDevStop(priv);
			if (pInfo->fd >= 0)
			{
				xf86RemoveEnabledDevice(pInfo);
				/* If fd management is done by the server, skip common fd handling */
				if ((pInfo->flags & XI86_SERVER_FD) == 0)
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
			wcmLog(priv, W_ERROR,
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
	Bool is_absolute = TRUE;
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;

	DBG(3, priv, "dev=%p mode=%d\n", (void *)dev, mode);

	if (mode != Absolute) {
		if (mode != Relative)
			return XI_BadMode;
		is_absolute = FALSE;
	}

	return wcmDevSwitchModeCall(priv, is_absolute) ? Success : XI_BadMode;
}

static int preInit(InputDriverPtr drv, InputInfoPtr pInfo, int flags)
{
	WacomDevicePtr priv = NULL;
	Status status;

	pInfo->device_control = wcmDevProc;
	pInfo->read_input = wcmDevReadInput;
	pInfo->control_proc = wcmDevChangeControl;
	pInfo->switch_mode = wcmDevSwitchMode;
	pInfo->dev = NULL;

	if (!(priv = wcmAllocate(pInfo, pInfo->name)))
		return BadAlloc;

	pInfo->private = priv;

	if ((status = wcmPreInit(priv)) != Success)
		return status;

	switch (priv->type) {
	case WTYPE_STYLUS:	pInfo->type_name = WACOM_PROP_XI_TYPE_STYLUS; break;
	case WTYPE_ERASER:	pInfo->type_name = WACOM_PROP_XI_TYPE_ERASER; break;
	case WTYPE_CURSOR:	pInfo->type_name = WACOM_PROP_XI_TYPE_CURSOR; break;
	case WTYPE_PAD:		pInfo->type_name = WACOM_PROP_XI_TYPE_PAD; break;
	case WTYPE_TOUCH:	pInfo->type_name = WACOM_PROP_XI_TYPE_TOUCH; break;
	default:
		xf86IDrvMsg(pInfo, X_ERROR,
		       "No type or invalid type specified.\n"
		       "Must be one of stylus, touch, cursor, eraser, or pad\n");
		return BadValue;
	}

	return Success;
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

static void usbListModels(void)
{
	const char *wmodels[512];
	size_t nmodels;
	SymTabRec models[512 + 1] = {0};

	nmodels = wcmListModels(wmodels, ARRAY_SIZE(models));

	for (size_t i = 0; i < min(nmodels, ARRAY_SIZE(models)); i++)
	{
		models[i].token = i;
		models[i].name = wmodels[i];
	}

	models[nmodels].name = NULL;

	xf86PrintChipsets("wacom",
			  "Driver for Wacom graphics tablets",
			  models);
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
