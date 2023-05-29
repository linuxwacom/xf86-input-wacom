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
void wcmQueueHotplug(WacomDevicePtr priv, const char* name, const char *type, unsigned int serial)
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

static inline int
valuatorNumber(enum WacomAxisType which)
{
	int pos;

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
	case WACOM_AXIS_SCROLL_X: pos = 6; break;
	case WACOM_AXIS_SCROLL_Y: pos = 7; break;
		break;
	default:
		abort();
	}

	return pos;
}

static inline void
convertAxes(const WacomAxisData *axes, ValuatorMask *mask)
{
	for (enum WacomAxisType which = _WACOM_AXIS_LAST; which > 0; which >>= 1)
	{
		int value;
		Bool has_value = wcmAxisGet(axes, which, &value);
		int pos;

		if (!has_value)
			continue;

		/* Positions need to match wcmInitAxis */
		pos = valuatorNumber(which);
		valuator_mask_set(mask, pos, value);
	}
}

void wcmEmitProximity(WacomDevicePtr priv, bool is_proximity_in,
		      const WacomAxisData *axes)
{
	InputInfoPtr pInfo = priv->frontend;

	ValuatorMask *mask = priv->valuator_mask;
	valuator_mask_zero(mask);
	convertAxes(axes, mask);

	if (valuator_mask_num_valuators(mask))
		xf86PostProximityEventM(pInfo->dev, is_proximity_in, mask);
}

void wcmEmitMotion(WacomDevicePtr priv, bool is_absolute, const WacomAxisData *axes)
{
	InputInfoPtr pInfo = priv->frontend;

	ValuatorMask *mask = priv->valuator_mask;
	valuator_mask_zero(mask);
	convertAxes(axes, mask);

	if (valuator_mask_num_valuators(mask))
		xf86PostMotionEventM(pInfo->dev, is_absolute, mask);
}

void wcmEmitButton(WacomDevicePtr priv, bool is_absolute, int button, bool is_press, const WacomAxisData *axes)
{
	InputInfoPtr pInfo = priv->frontend;

	ValuatorMask *mask = priv->valuator_mask;
	valuator_mask_zero(mask);
	convertAxes(axes, mask);

	xf86PostButtonEventM(pInfo->dev, is_absolute, button, is_press, mask);
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
			label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_X);
			break;
		case WACOM_AXIS_Y:
			label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_Y);
			break;
		case WACOM_AXIS_PRESSURE:
			label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_PRESSURE);
			break;
		case WACOM_AXIS_TILT_X:
			label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_TILT_X);
			break;
		case WACOM_AXIS_TILT_Y:
			label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_TILT_Y);
			break;
		case WACOM_AXIS_STRIP_X:
		case WACOM_AXIS_STRIP_Y:
			break;
		case WACOM_AXIS_ROTATION:
			label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_RZ);
			break;
		case WACOM_AXIS_THROTTLE:
			label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_THROTTLE);
			break;
		case WACOM_AXIS_WHEEL:
		case WACOM_AXIS_RING:
			label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_WHEEL);
			break;
		case WACOM_AXIS_RING2:
			break;
		case WACOM_AXIS_SCROLL_X:
			label = XIGetKnownProperty(AXIS_LABEL_PROP_REL_HSCROLL);
			break;
		case WACOM_AXIS_SCROLL_Y:
			label = XIGetKnownProperty(AXIS_LABEL_PROP_REL_VSCROLL);
			break;

		default:
			abort();
	}

	index = valuatorNumber(type);
	InitValuatorAxisStruct(pInfo->dev, index,
			       label,
			       min, max, res, min_res, max_res,
			       Absolute);

	if (type == WACOM_AXIS_SCROLL_X)
		SetScrollValuator(pInfo->dev, index, SCROLL_TYPE_HORIZONTAL, PANSCROLL_INCREMENT, 0);
	else if (type == WACOM_AXIS_SCROLL_Y)
		SetScrollValuator(pInfo->dev, index, SCROLL_TYPE_VERTICAL, PANSCROLL_INCREMENT, 0);

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
			"type=%s flags=%u fd=%d what=%s\n",
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
	case WTYPE_INVALID:
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


#ifdef ENABLE_TESTS
#include "wacom-test-suite.h"

#define MAX_VALUATORS 36

struct _ValuatorMask {
	int8_t last_bit;            /* highest bit set in mask */
	int8_t has_unaccelerated;
	uint8_t mask[(MAX_VALUATORS + 7) / 8];
	double valuators[MAX_VALUATORS];    /* valuator data */
	double unaccelerated[MAX_VALUATORS];    /* valuator data */
};

// The following functions are copied from https://gitlab.freedesktop.org/xorg/xserver/-/blob/master/dix/inpututils.c

int
CountBits(const uint8_t * mask, int len)
{
	int i;
	int ret = 0;

	for (i = 0; i < len; i++)
		if (BitIsOn(mask, i))
			ret++;

	return ret;
}


/**
 * Alloc a valuator mask large enough for num_valuators.
 */
ValuatorMask *
valuator_mask_new(int num_valuators)
{
	/* alloc a fixed size mask for now and ignore num_valuators. in the
	 * flying-car future, when we can dynamically alloc the masks and are
	 * not constrained by signals, we can start using num_valuators */
	ValuatorMask *mask = calloc(1, sizeof(ValuatorMask));

	if (mask == NULL)
		return NULL;

	mask->last_bit = -1;
	return mask;
}

/**
 * Reset mask to zero.
 */
void
valuator_mask_zero(ValuatorMask *mask)
{
	memset(mask, 0, sizeof(*mask));
	mask->last_bit = -1;
}

/**
 * Returns the current size of the mask (i.e. the highest number of
 * valuators currently set + 1).
 */
int
valuator_mask_size(const ValuatorMask *mask)
{
	return mask->last_bit + 1;
}

/**
 * Returns the number of valuators set in the given mask.
 */
int
valuator_mask_num_valuators(const ValuatorMask *mask)
{
	return CountBits(mask->mask, min(mask->last_bit + 1, MAX_VALUATORS));
}

/**
 * Return true if the valuator is set in the mask, or false otherwise.
 */
int
valuator_mask_isset(const ValuatorMask *mask, int valuator)
{
	return mask->last_bit >= valuator && BitIsOn(mask->mask, valuator);
}

static inline void
_valuator_mask_set_double(ValuatorMask *mask, int valuator, double data)
{
	mask->last_bit = max(valuator, mask->last_bit);
	SetBit(mask->mask, valuator);
	mask->valuators[valuator] = data;
}

/**
 * Set the valuator to the given floating-point data.
 */
void
valuator_mask_set_double(ValuatorMask *mask, int valuator, double data)
{
	if (mask->has_unaccelerated) {
		wcmLog(null, 0, "Do not mix valuator types, zero mask first\n");
	}
	_valuator_mask_set_double(mask, valuator, data);
}

/**
 * Set the valuator to the given integer data.
 */
void
valuator_mask_set(ValuatorMask *mask, int valuator, int data)
{
	valuator_mask_set_double(mask, valuator, data);
}

/**
 * Return the requested valuator value as a double. If the mask bit is not
 * set for the given valuator, the returned value is undefined.
 */
	double
valuator_mask_get_double(const ValuatorMask *mask, int valuator)
{
	return mask->valuators[valuator];
}

/**
 * Return the requested valuator value as an integer, rounding towards zero.
 * If the mask bit is not set for the given valuator, the returned value is
 * undefined.
 */
int
valuator_mask_get(const ValuatorMask *mask, int valuator)
{
	return trunc(valuator_mask_get_double(mask, valuator));
}


TEST_CASE(test_convert_axes)
{
	WacomAxisData axes = {0};
	ValuatorMask *mask = valuator_mask_new(8);

	convertAxes(&axes, mask);
	assert(valuator_mask_num_valuators(mask) == 0);
	assert(valuator_mask_size(mask) == 0);
	for (size_t i = 0; i< 9; i++)
		assert(!valuator_mask_isset(mask, i));

	memset(&axes, 0, sizeof(axes));
	valuator_mask_zero(mask);

	/* Check conversion for single value with first_valuator != 0 */
	wcmAxisSet(&axes, WACOM_AXIS_PRESSURE, 1); /* pos 2 */
	convertAxes(&axes, mask);
	assert(valuator_mask_num_valuators(mask) == 1);
	assert(valuator_mask_size(mask) == 3);
	assert(!valuator_mask_isset(mask, 0));
	assert(!valuator_mask_isset(mask, 1));
	assert(valuator_mask_isset(mask, 2));
	assert(valuator_mask_get(mask, 2) == 1);
	assert(!valuator_mask_isset(mask, 3));
	assert(!valuator_mask_isset(mask, 4));
	assert(!valuator_mask_isset(mask, 5));
	assert(!valuator_mask_isset(mask, 6));
	assert(!valuator_mask_isset(mask, 7));
	assert(!valuator_mask_isset(mask, 8));

	memset(&axes, 0, sizeof(axes));
	valuator_mask_zero(mask);

	/* Check conversion for gaps with first_valuator != 0 */
	wcmAxisSet(&axes, WACOM_AXIS_PRESSURE, 1); /* pos 2 */
	wcmAxisSet(&axes, WACOM_AXIS_WHEEL, 2); /* pos 5 */

 	convertAxes(&axes, mask);
	assert(valuator_mask_num_valuators(mask) == 2);
	assert(valuator_mask_size(mask) == 6);
	assert(!valuator_mask_isset(mask, 0));
	assert(!valuator_mask_isset(mask, 1));
	assert(valuator_mask_isset(mask, 2));
	assert(valuator_mask_get(mask, 2) == 1);
	assert(!valuator_mask_isset(mask, 3));
	assert(!valuator_mask_isset(mask, 4));
	assert(valuator_mask_isset(mask, 5));
	assert(valuator_mask_get(mask, 5) == 2);
	assert(!valuator_mask_isset(mask, 6));
	assert(!valuator_mask_isset(mask, 7));

	memset(&axes, 0, sizeof(axes));
	valuator_mask_zero(mask);

	/* Check conversion for valuators with duplicate uses. Note that this
	 * test is implementation-dependent: the loop in convertAxes decides
	 */
	wcmAxisSet(&axes, WACOM_AXIS_PRESSURE, 1); /* pos 2 */
	wcmAxisSet(&axes, WACOM_AXIS_STRIP_X, 20); /* pos 3 */
	wcmAxisSet(&axes, WACOM_AXIS_STRIP_Y, 21); /* pos 4 */
	wcmAxisSet(&axes, WACOM_AXIS_TILT_X, 10); /* also pos 3 */
	wcmAxisSet(&axes, WACOM_AXIS_TILT_Y, 11); /* also pos 4 */
	wcmAxisSet(&axes, WACOM_AXIS_RING, 3); /* pos 5 */
	wcmAxisSet(&axes, WACOM_AXIS_WHEEL, 2); /* also pos 5 */

	convertAxes(&axes, mask);
	assert(valuator_mask_num_valuators(mask) == 4);
	assert(valuator_mask_size(mask) == 6);
	assert(!valuator_mask_isset(mask, 0));
	assert(!valuator_mask_isset(mask, 1));
	assert(valuator_mask_isset(mask, 2));
	assert(valuator_mask_get(mask, 2) == 1);
	assert(valuator_mask_isset(mask, 3));
	assert(valuator_mask_get(mask, 3) == 10);
	assert(valuator_mask_isset(mask, 4));
	assert(valuator_mask_get(mask, 4) == 11);
	assert(valuator_mask_isset(mask, 5));
	assert(valuator_mask_get(mask, 5) == 2);
	assert(!valuator_mask_isset(mask, 6));
	assert(!valuator_mask_isset(mask, 7));
	assert(!valuator_mask_isset(mask, 8));

	free(mask);
}

#endif

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
