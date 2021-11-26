/*
 * Copyright 2021 Red Hat, Inc.
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

/* THIS IS NOT STABLE API, USE WITHIN THIS REPO ONLY */

#include "config.h"
#include "wacom-device.h"
#include "wacom-private.h"
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <gio/gio.h>


#include "xf86Wacom.h"

struct _WacomOptions {
	GObject parent_instance;
	GHashTable *ht;
};

struct _WacomDevice {
	GObject parent_instance;
	WacomDriver *driver;

	guint id; /* incrementing ID */

	WacomOptions *options;
	int fd;
	char *name;
	char *path;

	enum WacomToolType type;

	gboolean enabled;

	WacomDevicePtr priv;

	int nbuttons;
	int naxes;
	int ntouches;
	gboolean has_keys;
	gboolean is_absolute;
	gboolean is_direct_touch;

	guint axis_mask;
	WacomAxis axes[32]; /* ffs of the type is the axis index */

	GIOChannel *channel;
	guint watch;
};

G_DEFINE_TYPE (WacomOptions, wacom_options, G_TYPE_OBJECT)
G_DEFINE_TYPE (WacomDevice, wacom_device, G_TYPE_OBJECT)
G_DEFINE_BOXED_TYPE (WacomAxis, wacom_axis, wacom_axis_copy, wacom_axis_free)

WacomOptions *wacom_options_new(const char *key, ...)
{
	g_autoptr(WacomOptions) opts = g_object_new(WACOM_TYPE_OPTIONS, NULL);
	GHashTable *ht = opts->ht;
	va_list args;
	const char *k, *v;

	va_start(args, key);
	k = key;
	while (k) {
		v = va_arg(args, const char *);
		g_return_val_if_fail(v != NULL, NULL);
		g_hash_table_replace(ht, g_strdup(k), g_strdup(v));
		k = va_arg(args, const char *);
	}
	va_end(args);

	return g_steal_pointer(&opts);
}

static void copy_options(gpointer key, gpointer value, gpointer user_data)
{
	WacomOptions *dup = user_data;
	wacom_options_set(dup, key, value);
}

WacomOptions *wacom_options_duplicate(WacomOptions *opts)
{
	WacomOptions *dup = wacom_options_new(NULL, NULL);
	g_hash_table_foreach(opts->ht, copy_options, dup);
	return dup;
}

const char *wacom_options_get(WacomOptions *opts, const char *key)
{
	g_return_val_if_fail(key != NULL, NULL);

	g_autofree char *lower = g_ascii_strdown(key, -1);
	return g_hash_table_lookup(opts->ht, lower);
}

void wacom_options_set(WacomOptions *opts, const char *key, const char *value)
{
	g_return_if_fail(key != NULL);
	g_return_if_fail(value != NULL);
	g_hash_table_replace(opts->ht, g_ascii_strdown(key, -1), g_strdup(value));
}

static void
wacom_options_finalize(GObject *gobject)
{
	WacomOptions *opts = WACOM_OPTIONS(gobject);
	g_hash_table_destroy(opts->ht);
	G_OBJECT_CLASS (wacom_options_parent_class)->finalize (gobject);
}

static void
wacom_options_class_init(WacomOptionsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->finalize = wacom_options_finalize;
}

static void
wacom_options_init(WacomOptions *self)
{
	g_autoptr(GHashTable) ht = g_hash_table_new_full(g_str_hash, g_str_equal,
							 g_free, g_free);

	/* Pretend this is a udev device */
	g_hash_table_replace(ht, g_strdup("_source"), g_strdup("server/udev"));

	self->ht = g_steal_pointer(&ht);
}

enum {
	PROP_0,
	PROP_NAME,
	PROP_ENABLED,
	PROP_PATH,

	N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

enum {
	SIGNAL_KEY,
	SIGNAL_BUTTON,
	SIGNAL_MOTION,
	SIGNAL_TOUCH,
	SIGNAL_PROXIMITY,

	SIGNAL_LOGMSG, /* A log message from the driver */
	SIGNAL_DBGMSG, /* A debug message from the driver */

	SIGNAL_READ_ERROR,

	LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = { 0 };

WacomDevice*
wacom_device_new(WacomDriver *driver,
		 const char *name,
		 WacomOptions *options)
{
	static guint deviceid = 0;
	WacomDevice *device = g_object_new(WACOM_TYPE_DEVICE, NULL);
	WacomDevicePtr priv = wcmAllocate(device, name);

	device->id = deviceid++;
	device->driver = g_object_ref(driver);
	device->priv = priv;
	device->name = g_strdup(name);
	device->options = wacom_options_duplicate(options);

	wacom_driver_add_device(driver, device);

	return device;
}

void
wacom_device_remove(WacomDevice *device)
{
	wcmUnInit(device->priv);
	wacom_driver_remove_device(device->driver, device);
}

gboolean
wacom_device_preinit(WacomDevice *device)
{
	gboolean rc = wcmPreInit(device->priv) == Success;

	device->type = (enum WacomToolType)device->priv->type;

	return rc;
}

gboolean
wacom_device_setup(WacomDevice *device)
{
	return wcmDevInit(device->priv);
}

static gboolean read_device(GIOChannel *source, GIOCondition condition, gpointer data)
{
	WacomDevice *device = data;
	int rc;

	do {
		rc = wcmReadPacket(device->priv);
	} while (rc > 0);

	return rc == 0;
}

gboolean
wacom_device_enable(WacomDevice *device)
{

	g_return_val_if_fail(!device->enabled, true);

	if (!wcmDevOpen(device->priv) || ! wcmDevStart(device->priv))
		return false;

	device->channel = g_io_channel_unix_new(device->fd);
	device->watch = g_io_add_watch(device->channel, G_IO_IN,
				       read_device, device);

	device->enabled = true;
	g_object_notify_by_pspec(G_OBJECT(device), obj_properties[PROP_ENABLED]);

	return true;
}

void
wacom_device_disable(WacomDevice *device)
{
	g_return_if_fail(device->enabled);

	wcmDevStop(device->priv);
	wcmDevClose(device->priv);

	device->enabled = false;
	g_object_notify_by_pspec(G_OBJECT(device), obj_properties[PROP_ENABLED]);
}

WacomDriver *wacom_device_get_driver(WacomDevice *device)
{
	return device->driver;
}
void *wacom_device_get_impl(WacomDevice *device)
{
	return device->priv;
}

/****************** Driver layer *****************/

int
wcmGetFd(WacomDevicePtr priv)
{
	WacomDevice *device = priv->frontend;
	return device->fd;
}

void
wcmSetFd(WacomDevicePtr priv, int fd)
{
	WacomDevice *device = priv->frontend;
	device->fd = fd;
}

void
wcmSetName(WacomDevicePtr priv, const char *name)
{
	WacomDevice *device = priv->frontend;
	g_free(device->name);
	device->name = g_strdup(name);
}

__attribute__((__format__(__printf__ , 3, 0)))
static void wcmLogDevice(WacomDevicePtr priv, WacomLogType type,
			 const char *format, va_list args)
{
	WacomDevice *device = priv->frontend;
	const char *prefix = ".";
	g_autofree char *str = NULL;

	switch (type) {
	case W_PROBED:		prefix = "p"; break;
	case W_CONFIG:		prefix = "c"; break;
	case W_DEFAULT:		prefix = "d"; break;
	case W_CMDLINE:		prefix = "c"; break;
	case W_NOTICE:		prefix = "N"; break;
	case W_ERROR:		prefix = "E"; break;
	case W_WARNING:		prefix = "W"; break;
	case W_INFO:		prefix = "I"; break;
	case W_NONE:		prefix = "-"; break;
	case W_NOT_IMPLEMENTED:	prefix = "N/I"; break;
	case W_DEBUG:		prefix = "D"; break;
	case W_UNKNOWN:		prefix = "?"; break;
	}

	str = g_strdup_vprintf(format, args);
	g_signal_emit(device, signals[SIGNAL_LOGMSG], 0, prefix, str);
}

void wcmLog(WacomDevicePtr priv, WacomLogType type, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	wcmLogDevice(priv, type, format, args);
	va_end(args);
}

/* identical to wcmLog, we don't need sigsafe logging */
void wcmLogSafe(WacomDevicePtr priv, WacomLogType type, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	wcmLogDevice(priv, type, format, args);
	va_end(args);
}

void
wcmLogDebugDevice(WacomDevicePtr priv, int debug_level, const char *func, const char *format, ...)
{
	WacomDevice *device = priv->frontend;
	g_autofree char *str = NULL;
	va_list args;

	va_start(args, format);
	str = g_strdup_vprintf(format, args);
	va_end(args);

	g_signal_emit(device, signals[SIGNAL_DBGMSG], 0, debug_level, func, str);
}

void wcmLogCommon(WacomCommonPtr common, WacomLogType type, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	/* We just log the common ones through the first device in the list,
	 * probably good enough */
	wcmLogDevice(common->wcmDevices, type, format, args);
	va_end(args);
}

/* identical to wcmLogCommon, we don't need sigsafe logging */
void wcmLogCommonSafe(WacomCommonPtr common, WacomLogType type, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	/* We just log the common ones through the first device in the list,
	 * probably good enough */
	wcmLogDevice(common->wcmDevices, type, format, args);
	va_end(args);
}

void
wcmLogDebugCommon(WacomCommonPtr common, int debug_level, const char *func, const char *format, ...)
{
	/* We just log the common ones through the first device in the list,
	 * probably good enough */
	WacomDevice *device = common->wcmDevices->frontend;
	g_autofree char *str = NULL;
	va_list args;

	va_start(args, format);
	str = g_strdup_vprintf(format, args);
	va_end(args);

	g_signal_emit(device, signals[SIGNAL_DBGMSG], 0, debug_level, func, str);
}

char *wcmOptGetStr(WacomDevicePtr priv, const char *key, const char *default_value)
{
	WacomDevice *dev = priv->frontend;
	WacomOptions *opts = dev->options;
	const char *value = wacom_options_get(opts, key);

	if (!value)
		value = default_value;

	return g_strdup(value);
}

int wcmOptGetInt(WacomDevicePtr priv, const char *key, int default_value)
{
	char buf[16];
	snprintf(buf, sizeof(buf), "%d", default_value);
	char *str = wcmOptGetStr(priv, key, buf);
	int value = default_value;
	if (str)
		value = atoi(str);
	g_free(str);
	return value;
}

bool wcmOptGetBool(WacomDevicePtr priv, const char *key, bool default_value)
{
	char *str = wcmOptGetStr(priv, key, default_value ? "true" : "false");
	bool is_true = false;

	if (g_str_equal(str, "true") ||
	    g_str_equal(str, "yes") ||
	    g_str_equal(str, "on") ||
	    g_str_equal(str, "1"))
		is_true = true;
	g_free(str);
	return is_true;
}

/* Get the option of the given type, quietly (without logging) */
char *wcmOptCheckStr(WacomDevicePtr priv, const char *key, const char *default_value)
{
	return wcmOptGetStr(priv, key, default_value);
}

int wcmOptCheckInt(WacomDevicePtr priv, const char *key, int default_value)
{
	return wcmOptGetInt(priv, key, default_value);
}

bool wcmOptCheckBool(WacomDevicePtr priv, const char *key, bool default_value)
{
	return wcmOptGetBool(priv, key, default_value);
}

/* Change the option to the new value */
void wcmOptSetStr(WacomDevicePtr priv, const char *key, const char *value)
{
	WacomDevice *device = priv->frontend;
	WacomOptions *opts = device->options;

	wacom_options_set(opts, key, value);
}

void wcmOptSetInt(WacomDevicePtr priv, const char *key, int value)
{
	char buf[64];
	snprintf(buf, sizeof(buf), "%d", value);
	wcmOptSetStr(priv, key, buf);
}

void wcmOptSetBool(WacomDevicePtr priv, const char *key, bool value)
{
	wcmOptSetStr(priv, key, value ? "true" : "false");
}

void wcmEmitKeycode(WacomDevicePtr priv, int keycode, int state)
{
	WacomDevice *device = priv->frontend;
	g_signal_emit(device, signals[SIGNAL_KEY], 0, keycode, state);
}

void wcmEmitProximity(WacomDevicePtr priv, bool is_proximity_in,
		      const WacomAxisData *axes)
{
	WacomDevice *device = priv->frontend;
	g_signal_emit(device, signals[SIGNAL_PROXIMITY], 0, is_proximity_in, axes);
}

void wcmEmitMotion(WacomDevicePtr priv, bool is_absolute, const WacomAxisData *axes)
{
	WacomDevice *device = priv->frontend;
	g_signal_emit(device, signals[SIGNAL_MOTION], 0, is_absolute, axes);
}

void wcmEmitButton(WacomDevicePtr priv, bool is_absolute, int button, bool is_press, const WacomAxisData *axes)
{
	WacomDevice *device = priv->frontend;
	g_signal_emit(device, signals[SIGNAL_BUTTON], 0, is_absolute, button, is_press, axes);
}

void wcmEmitTouch(WacomDevicePtr priv, int type, unsigned int touchid, int x, int y)
{
	WacomDevice *device = priv->frontend;
	enum WacomTouchState state;

	switch (type) {
	case XI_TouchBegin: state = WACOM_TOUCH_BEGIN; break;
	case XI_TouchUpdate: state = WACOM_TOUCH_UPDATE; break;
	case XI_TouchEnd: state = WACOM_TOUCH_END; break;
	default:
			  abort();
	}
	g_signal_emit(device, signals[SIGNAL_TOUCH], 0, state, touchid, x, y);
}

void wcmInitAxis(WacomDevicePtr priv, enum WacomAxisType type,
			int min, int max, int res)
{
	WacomDevice *device = priv->frontend;
	WacomAxis ax = {
		.type = (enum WacomEventAxis)type,
		.min = min,
		.max = max,
		.res = res,
	};
	device->axes[ffs(type)] = ax;
	device->axis_mask |= (enum WacomEventAxis)type;
}

bool wcmInitButtons(WacomDevicePtr priv, unsigned int nbuttons)
{
	WacomDevice *device = priv->frontend;
	device->nbuttons = nbuttons;
	return true;
}

bool wcmInitKeyboard(WacomDevicePtr priv)
{
	WacomDevice *device = priv->frontend;
	device->has_keys = true;
	return true;
}

bool wcmInitPointer(WacomDevicePtr priv, int naxes, bool is_absolute)
{
	WacomDevice *device = priv->frontend;
	device->naxes = naxes;
	device->is_absolute = is_absolute;
	return true;
}

bool wcmInitTouch(WacomDevicePtr priv, int ntouches, bool is_direct_touch)
{
	WacomDevice *device = priv->frontend;
	device->ntouches = ntouches;
	device->is_direct_touch = is_direct_touch;
	return true;
}

guint wacom_device_get_id(WacomDevice *device)
{
	return device->id;
}

const char *wacom_device_get_name(WacomDevice *device)
{
	return device->name;
}

enum WacomToolType wacom_device_get_tool_type(WacomDevice *device)
{
	return device->type;
}

const WacomAxis *wacom_device_get_axis(WacomDevice *device,
				       enum WacomEventAxis which)
{
	g_return_val_if_fail(which <= WACOM_RING2, NULL);

	return (device->axis_mask & which) ? &device->axes[ffs(which)] : NULL;
}

int wacom_device_get_num_buttons(WacomDevice *device)
{
	return device->nbuttons;
}
gboolean wacom_device_has_keys(WacomDevice *device)
{
	return device->has_keys;
}
int wacom_device_get_num_touches(WacomDevice *device)
{
	return device->ntouches;
}
gboolean wacom_device_is_direct_touch(WacomDevice *device)
{
	return device->is_direct_touch;
}
gboolean wacom_device_is_absolute(WacomDevice *device)
{
	return device->is_absolute;
}
int wacom_device_get_num_axes(WacomDevice *device)
{
	return device->naxes;
}

int wcmOpen(WacomDevicePtr priv)
{
	WacomDevice *device = priv->frontend;
	const char *path = wacom_options_get(device->options, "device");
	int fd;

	if (!path) {
		wcmLog(priv, W_ERROR, "Error opending device, no option 'device'\n");
		return -ENODEV;
	}

	fd = open(path, O_RDONLY|O_NONBLOCK);

	g_free(device->path);
	device->path = strdup(path);
	g_object_notify_by_pspec(G_OBJECT(device), obj_properties[PROP_PATH]);

	return fd != -1 ? fd : -errno;
}


void wcmClose(WacomDevicePtr priv)
{
	WacomDevice *device = priv->frontend;

	close(device->fd);
	device->fd = -1;
}

/* FIXME: the bits we emulate in the driver don't depend on timers, so we just
 * have a stub timer implementation that does nothing */
WacomTimerPtr wcmTimerNew(void)
{
	return calloc(1, 1);
}

void wcmTimerFree(WacomTimerPtr timer)
{
	g_free(timer);
}

void wcmTimerCancel(WacomTimerPtr timer)
{
}

void wcmTimerSet(WacomTimerPtr timer, uint32_t millis, WacomTimerCallback func, void *userdata)
{
}

void wcmUpdateSerialProperty(WacomDevicePtr priv) {}
void wcmUpdateHWTouchProperty(WacomDevicePtr priv) {}
void wcmUpdateRotationProperty(WacomDevicePtr priv) {}

struct hotplug {
	WacomDriver *driver;
	char *name;
	WacomOptions *opts;
};

static gboolean hotplugDevice(gpointer data)
{
	struct hotplug *hotplug = data;
	g_autoptr(WacomDevice) new_device = wacom_device_new(hotplug->driver, hotplug->name, hotplug->opts);

	g_object_unref(hotplug->driver);
	g_object_unref(hotplug->opts);
	g_free(hotplug->name);
	g_free(hotplug);

	g_return_val_if_fail(new_device != NULL, FALSE); /* silence 'unused variable device' compiler warning */

	/* WacomDriver holds the device ref */
	return FALSE;
}

void wcmQueueHotplug(WacomDevicePtr priv, const char* name, const char *type, int serial)
{
	WacomDevice *device = priv->frontend;
	struct hotplug *hotplug = g_new0(struct hotplug, 1);
	WacomOptions *new_opts = wacom_options_duplicate(device->options);
	char buf[64] = {0};

	wacom_options_set(new_opts, "Type", type);
	if (serial > -1) {
		snprintf(buf, sizeof(buf), "0x%x", serial);
		wacom_options_set(new_opts, "Serial", buf);
	}

	hotplug->name = g_strdup(name);
	hotplug->opts = g_steal_pointer(&new_opts);
	hotplug->driver = g_object_ref(device->driver);

	g_idle_add(hotplugDevice, hotplug);
}

uint32_t wcmTimeInMillis(void)
{
	return (uint32_t)g_get_monotonic_time();
}

/****************** GObject boilerplate *****************/

static void
wacom_device_dispose(GObject *gobject)
{
	G_OBJECT_CLASS (wacom_device_parent_class)->dispose (gobject);
}

static void
wacom_device_finalize(GObject *gobject)
{
	WacomDevice *device = WACOM_DEVICE(gobject);
	g_free(device->path);
	g_object_unref(device->driver);
	G_OBJECT_CLASS (wacom_device_parent_class)->finalize (gobject);
}

static void
wacom_device_get_property(GObject *object, guint property_id,
			  GValue *value, GParamSpec *pspec)
{
	WacomDevice *self = WACOM_DEVICE(object);

	switch (property_id) {
		case PROP_NAME:
			g_value_set_string(value, self->name);
			break;
		case PROP_ENABLED:
			g_value_set_boolean(value, self->enabled);
			break;
		case PROP_PATH:
			g_value_set_string(value, self->path);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
			break;
	}
}

static void
wacom_device_class_init(WacomDeviceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->dispose = wacom_device_dispose;
	object_class->finalize = wacom_device_finalize;
	object_class->get_property = wacom_device_get_property;

	/**
	 * WacomDevice:name
	 */
	obj_properties[PROP_NAME] =
		g_param_spec_string("name",
				    "Device name",
				    "The device's name",
				    NULL, /* default value */
				    G_PARAM_READABLE);
	g_object_class_install_property(object_class, PROP_NAME,
					obj_properties[PROP_NAME]);

	/**
	 * WacomDevice:enabled
	 */
	obj_properties[PROP_ENABLED] =
		g_param_spec_boolean("enabled",
				    "Enabled indicator",
				    "Indicates if the device is enabled",
				    FALSE,
				    G_PARAM_READABLE);
	g_object_class_install_property(object_class, PROP_ENABLED,
					obj_properties[PROP_ENABLED]);

	/**
	 * WacomDevice:path
	 */
	obj_properties[PROP_PATH] =
		g_param_spec_string("path",
				    "Device path",
				    "The device's event node",
				    NULL /* default value */,
				    G_PARAM_READABLE);
	g_object_class_install_property(object_class, PROP_PATH,
					obj_properties[PROP_PATH]);

	/**
	 * WacomDevice::keycode:
	 * @device: the device that sent the event
	 * @keycode: the keycode
	 * @is_press: true for down, false for up
	 *
	 * The keycode signal is emitted whenever the device sends a key event
	 */
	signals[SIGNAL_KEY] =
		g_signal_new("keycode",
			     G_TYPE_FROM_CLASS(klass),
			     G_SIGNAL_RUN_FIRST,
			     0, NULL, NULL, NULL, G_TYPE_NONE,
			     /* key, is_press */
			     2, G_TYPE_UINT, G_TYPE_BOOLEAN);

	/**
	 * WacomDevice::button:
	 * @device: the device that sent the event
	 * @is_absolute: true if the axis values are absolute
	 * @button: the 1-indexed button number
	 * @is_press: true for down, false for up
	 * @axes: a WacomEventData pointer
	 *
	 * The button signal is emitted whenever the device sends a button
	 * event
	 */
	signals[SIGNAL_BUTTON] =
		g_signal_new("button",
			     G_TYPE_FROM_CLASS(klass),
			     G_SIGNAL_RUN_FIRST,
			     0, NULL, NULL, NULL, G_TYPE_NONE,
			     /* is_absolute, button, is_press, axes */
			     4, G_TYPE_BOOLEAN, G_TYPE_UINT,
			     G_TYPE_BOOLEAN, G_TYPE_POINTER);

	/**
	 * WacomDevice::motion:
	 * @device: the device that sent the event
	 * @is_absolute: true if the axis values are absolute
	 * @axes: a WacomEventData pointer
	 *
	 * The motion signal is emitted whenever the device changes position
	 */
	signals[SIGNAL_MOTION] =
		g_signal_new("motion",
			     G_TYPE_FROM_CLASS(klass),
			     G_SIGNAL_RUN_FIRST,
			     0, NULL, NULL, NULL, G_TYPE_NONE,
			     /* is_absolute, axes */
			     2, G_TYPE_BOOLEAN, G_TYPE_POINTER);

	/**
	 * WacomDevice::touch:
	 * @device: the device that sent the event
	 * @type: one of WacomTouchState
	 * @touchid: an unsigned integer with the touch ID of this touch
	 * @x: x position
	 * @y: y position
	 *
	 * The touch signal is emitted whenever a touch starts/moves/stops
	 */
	signals[SIGNAL_TOUCH] =
		g_signal_new("touch",
			     G_TYPE_FROM_CLASS(klass),
			     G_SIGNAL_RUN_FIRST,
			     0, NULL, NULL, NULL, G_TYPE_NONE,
			     /* type, touchid, x, y */
			     3, G_TYPE_INT, G_TYPE_UINT, G_TYPE_INT, G_TYPE_INT);

	/**
	 * WacomDevice::proximity:
	 * @device: the device that sent the event
	 * @is_prox_in: true for proximity in, false for proxiity out
	 * @axes: a WacomEventData pointer
	 *
	 * The proximity signal is emitted whenever the device enters of
	 * leaves proximity
	 */
	signals[SIGNAL_PROXIMITY] =
		g_signal_new("proximity",
			     G_TYPE_FROM_CLASS(klass),
			     G_SIGNAL_RUN_FIRST,
			     0, NULL, NULL, NULL, G_TYPE_NONE,
			     /* is_prox_in, axes */
			     2, G_TYPE_BOOLEAN, G_TYPE_POINTER);

	/**
	 * WacomDevice::log-message:
	 * @device: the device that sent the event
	 * @type: a string to use as prefix
	 * @msg: the log message
	 *
	 * The log-message signal is emitted whenever the device is trying to
	 * write a message to the regular log
	 */
	signals[SIGNAL_LOGMSG] =
		g_signal_new("log-message",
			     G_TYPE_FROM_CLASS(klass),
			     G_SIGNAL_RUN_FIRST,
			     0, NULL, NULL, NULL, G_TYPE_NONE,
			     /* type, string */
			     2, G_TYPE_STRING, G_TYPE_STRING);


	/**
	 * WacomDevice::debug-message:
	 * @device: the device that sent the event
	 * @level: the numerical debug level for this message
	 * @func: function where the debug message was triggered
	 * @msg: the debug message
	 *
	 * The debug-message signal is emitted whenever the device is trying to
	 * write a debug messag
	 */
	signals[SIGNAL_DBGMSG] =
		g_signal_new("debug-message",
			     G_TYPE_FROM_CLASS(klass),
			     G_SIGNAL_RUN_FIRST,
			     0, NULL, NULL, NULL, G_TYPE_NONE,
			     /* level, func, string */
			     3, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING);

	/**
	 * WacomDevice::read-error:
	 * @device: the device that sent the event
	 * @errno: the errno that was detected
	 *
	 * The read-error signal is emitted whenever that was an error reading
	 * the device.
	 */
	signals[SIGNAL_READ_ERROR] =
		g_signal_new("read-error",
			     G_TYPE_FROM_CLASS(klass),
			     G_SIGNAL_RUN_FIRST,
			     0, NULL, NULL, NULL, G_TYPE_NONE,
			     /* errno */
			     1, G_TYPE_INT);
}

static void
wacom_device_init(WacomDevice *self)
{
}

WacomAxis* wacom_axis_copy(const WacomAxis *axis)
{
	WacomAxis *new_axis = malloc(sizeof(*axis));
	memcpy(new_axis, axis, sizeof(*axis));
	return new_axis;
}

void wacom_axis_free(WacomAxis *axis)
{
	free(axis);
}
