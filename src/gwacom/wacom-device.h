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

#pragma once

#include <stdint.h>
#include <glib.h>
#include <glib-object.h>

#include "wacom-driver.h"

G_BEGIN_DECLS

#define WACOM_TYPE_OPTIONS (wacom_options_get_type())
G_DECLARE_FINAL_TYPE (WacomOptions, wacom_options, WACOM, OPTIONS, GObject);

/**
 * wacom_options_new: (skip)
 * @key: (nullable): The first key, followed by a const char * value, followed
 * by other key/valuy pairs, terminated with a NULL key.
 *
 * Create a new set of configuration options in key-value format, matching the
 * xorg.conf options the driver supports. Parsing of the varargs stops when
 * the next key is NULL.
 *
 * Use g_object_unref() to free the returned object.
 *
 * Returns: (transfer full): a new WacomOptions object
 */
__attribute__((sentinel))
WacomOptions *wacom_options_new(const char *key, ...);

/**
 * wacom_options_duplicate:
 *
 * Returns: (transfer full):  a deep copy of the options given as argument.
 */
WacomOptions *wacom_options_duplicate(WacomOptions *opts);

/**
 * wacom_options_get:
 *
 * Return the value for the given key or NULL if the key does not exist.
 */
const char *wacom_options_get(WacomOptions *opts, const char *key);

/**
 * wacom_options_set:
 *
 * Add or replace the option with the given string value.
 */
void wacom_options_set(WacomOptions *opts, const char *key, const char *value);

/***
 * wacom_options_list_keys:
 *
 * Returns: (element-type utf8) (transfer full): The List of keys in these
 * options
 */
GSList *wacom_options_list_keys(WacomOptions *opts);


#define WACOM_TYPE_DEVICE (wacom_device_get_type())
G_DECLARE_FINAL_TYPE (WacomDevice, wacom_device, WACOM, DEVICE, GObject)

typedef enum {
	WTOOL_INVALID = 0,
	WTOOL_STYLUS,
	WTOOL_ERASER,
	WTOOL_CURSOR,
	WTOOL_PAD,
	WTOOL_TOUCH,
} WacomToolType;

typedef enum {
	WTOUCH_BEGIN,
	WTOUCH_UPDATE,
	WTOUCH_END,
} WacomTouchState;

typedef enum {
	WAXIS_X		= (1 << 0),
	WAXIS_Y		= (1 << 1),
	WAXIS_PRESSURE	= (1 << 2),
	WAXIS_TILT_X	= (1 << 3),
	WAXIS_TILT_Y	= (1 << 4),
	WAXIS_STRIP_X	= (1 << 5),
	WAXIS_STRIP_Y	= (1 << 6),
	WAXIS_ROTATION	= (1 << 7),
	WAXIS_THROTTLE	= (1 << 8),
	WAXIS_WHEEL	= (1 << 9),
	WAXIS_RING	= (1 << 10),
	WAXIS_RING2	= (1 << 11),

	_WAXIS_LAST = WAXIS_RING2,
} WacomEventAxis;

/* The pointer argument to all the event signals. If the mask is set for
 * a given axis, that value contains the current state of the axis */
typedef struct {
	uint32_t mask; /* bitmask of WacomEventAxis */
	int x, y;
	int pressure;
	int tilt_x, tilt_y;
	int strip_x, strip_y;
	int rotation;
	int throttle;
	int wheel;
	int ring, ring2;
} WacomEventData;

#define WACOM_TYPE_EVENT_DATA (wacom_event_data_get_type())
GType wacom_event_data_get_type(void);
WacomEventData *wacom_event_data_copy(const WacomEventData *data);
void wacom_event_data_free(WacomEventData *data);

#define WACOM_TYPE_AXIS (wacom_axis_get_type())

typedef struct {
	WacomEventAxis type;
	int min, max;
	int res;
} WacomAxis;

GType wacom_axis_get_type(void);
WacomAxis* wacom_axis_copy(const WacomAxis *axis);
void wacom_axis_free(WacomAxis *axis);

/**
 * wacom_device_new:
 *
 * Note that the driver may rename the device, the name is not guaranteed to
 * be the device's name.
 *
 * If options does not contain a key "Device" with the path to the event node,
 * the driver will auto-detect the event node during PreInit.
 *
 * Use wacom_device_preinit() to pre-init the device and verify it can be
 * used. Then call wacom_device_setup() to initialize the device with the
 * right axes, followed by wacom_device_enable() to have it process events.
 *
 * You should connect to the log-message and debug-message signals before
 * calling wacom_device_preinit().
 *
 * Use g_object_unref() to free the returned object.
 *
 * @driver The driver to assign to
 * @param name (trqnsfer none) The device name
 * @param options (transfer none) The device name
 *
 * Returns: (transfer full): a new allocated device witha  refcount of at
 * least 1
 */
WacomDevice *wacom_device_new(WacomDriver *driver,
			      const char *name,
			      WacomOptions *options);

void wacom_device_remove(WacomDevice *device);

gboolean wacom_device_preinit(WacomDevice *device);
gboolean wacom_device_setup(WacomDevice *device);
gboolean wacom_device_enable(WacomDevice *device);
void wacom_device_disable(WacomDevice *device);

/**
 * wacom_device_get_id:
 *
 * A numeric value assigned to the device by this wrapper library during
 * wacom_device_new(). This value serves the same purpose as the X11 device
 * ID.
 */
guint wacom_device_get_id(WacomDevice *device);
const char *wacom_device_get_name(WacomDevice *device);
WacomToolType wacom_device_get_tool_type(WacomDevice *device);

/**
 * wacom_device_get_options:
 *
 * Returns: (transfer none): the options applied to this device
 */
WacomOptions *wacom_device_get_options(WacomDevice *device);

/**
 * wacom_device_set_runtime_option:
 *
 * Some options like button actions are runtime-only and cannot be set through.
 * WacomOptionx (which maps to the xorg.conf support of the driver).
 * This is a hack to set some of those options, however the options
 * and values supported are very specific to the implementation.
 */
void wacom_device_set_runtime_option(WacomDevice *device, const char *name, const char *value);

/* The following getters are only available after wacom_device_setup() */

int wacom_device_get_num_buttons(WacomDevice *device);
gboolean wacom_device_has_keys(WacomDevice *device);
int wacom_device_get_num_touches(WacomDevice *device);
gboolean wacom_device_is_direct_touch(WacomDevice *device);
gboolean wacom_device_is_absolute(WacomDevice *device);
int wacom_device_get_num_axes(WacomDevice *device);

/**
 * wacom_device_get_axis:
 *
 * This function is only available after wacom_device_setup()
 *
 * Returns: (transfer none): the axis of this device or NULL for an invalid
 * index.
 */
const WacomAxis* wacom_device_get_axis(WacomDevice *device,
				       WacomEventAxis which);

G_END_DECLS
