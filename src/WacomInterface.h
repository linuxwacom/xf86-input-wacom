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

/* This header file declares the functions that need to be implemented to use
 * the driver. See xf86Wacom.c for the Xorg driver implementation.
 *
 * THIS IS NOT A STABLE API
 */

#include <config.h>

#ifndef __WACOM_INTERFACE_H
#define __WACOM_INTERFACE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct _WacomDeviceRec *WacomDevicePtr;
typedef struct _WacomCommonRec *WacomCommonPtr;

/* Identical to MessageType */
typedef enum {
      W_PROBED,                   /* Value was probed */
      W_CONFIG,                   /* Value was given in the config file */
      W_DEFAULT,                  /* Value is a default */
      W_CMDLINE,                  /* Value was given on the command line */
      W_NOTICE,                   /* Notice */
      W_ERROR,                    /* Error message */
      W_WARNING,                  /* Warning message */
      W_INFO,                     /* Informational message */
      W_NONE,                     /* No prefix */
      W_NOT_IMPLEMENTED,          /* Not implemented */
      W_DEBUG,                    /* Debug message */
      W_UNKNOWN = -1              /* unknown -- this must always be last */
} WacomLogType;

enum WacomAxisType {
	WACOM_AXIS_X		= (1 << 0),
	WACOM_AXIS_Y		= (1 << 1),
	WACOM_AXIS_PRESSURE	= (1 << 2),
	WACOM_AXIS_TILT_X	= (1 << 3),
	WACOM_AXIS_TILT_Y	= (1 << 4),
	WACOM_AXIS_STRIP_X	= (1 << 5),
	WACOM_AXIS_STRIP_Y	= (1 << 6),
	WACOM_AXIS_ROTATION	= (1 << 7), /* Cursor rotation only */
	WACOM_AXIS_THROTTLE	= (1 << 8),
	WACOM_AXIS_WHEEL	= (1 << 9), /* Artpen rotation or airbrush wheel */
	WACOM_AXIS_RING		= (1 << 10),
	WACOM_AXIS_RING2	= (1 << 11),
	WACOM_AXIS_SCROLL_X	= (1 << 12),
	WACOM_AXIS_SCROLL_Y	= (1 << 13),

	_WACOM_AXIS_LAST = WACOM_AXIS_SCROLL_Y,
};

typedef struct {
	uint32_t mask;
	int x, y;
	int pressure;
	int tilt_x, tilt_y;
	int strip_x, strip_y;
	int rotation;
	int throttle;
	int wheel;
	int ring, ring2;
	int scroll_x, scroll_y;
} WacomAxisData;


/**
 * General logging function for a device.
 */
__attribute__((__format__(__printf__ , 3, 4)))
void wcmLog(WacomDevicePtr priv, WacomLogType type, const char *format, ...);

/**
 * Same as the above but uses the sigsafe logging function if needed.
 */
__attribute__((__format__(__printf__ , 3, 4)))
void wcmLogSafe(WacomDevicePtr priv, WacomLogType type, const char *format, ...);

/**
 * General logging function for the driver.
 */
__attribute__((__format__(__printf__ , 3, 4)))
void wcmLogCommon(WacomCommonPtr priv, WacomLogType type, const char *format, ...);

/**
 * Same as the above but uses the sigsafe logging function if needed.
 */
__attribute__((__format__(__printf__ , 3, 4)))
void wcmLogCommonSafe(WacomCommonPtr priv, WacomLogType type, const char *format, ...);

/**
 * Identical to wcmLog but used for debug messages inside the driver for a
 * specific device. This must use a sigsafe path.
 */
__attribute__((__format__(__printf__ , 4, 5)))
void wcmLogDebugDevice(WacomDevicePtr priv, int debug_level, const char *func,
		       const char *format, ...);

/**
 * Identical to wcmLog but used for debug messages inside the driver from the
 * common shared path. This must use a sigsafe path.
 */
__attribute__((__format__(__printf__ , 4, 5)))
void wcmLogDebugCommon(WacomCommonPtr common, int debug_level, const char *func,
		       const char *format, ...);

/**
 * @retval 0 to abort the loop (counts as match)
 * @retval -ENODEV if the device is not a match
 * @retval 1 for success (counts as match).
 * @retval -errno for an error (to abort)
 */
typedef int (*WacomDeviceCallback)(WacomDevicePtr priv, void *data);

/**
 * Returns the number of devices processed or a negative errno
 */
int wcmForeachDevice(WacomDevicePtr priv, WacomDeviceCallback func, void *data);

/* Open the device with the right serial parmeters */
int wcmOpen(WacomDevicePtr priv);

/* Close the device */
void wcmClose(WacomDevicePtr priv);

int wcmGetFd(WacomDevicePtr priv);
void wcmSetFd(WacomDevicePtr priv, int fd);
/* Update the driver implementation's name, if any */
void wcmSetName(WacomDevicePtr priv, const char *name);

uint32_t wcmTimeInMillis(void);

static inline void wcmAxisSet(WacomAxisData *data,
			      enum WacomAxisType which, int value)
{
	data->mask |= which;
	switch (which){
	case WACOM_AXIS_X: data->x = value; break;
	case WACOM_AXIS_Y: data->y = value; break;
	case WACOM_AXIS_PRESSURE: data->pressure = value; break;
	case WACOM_AXIS_TILT_X: data->tilt_x = value; break;
	case WACOM_AXIS_TILT_Y: data->tilt_y = value; break;
	case WACOM_AXIS_STRIP_X: data->strip_x = value; break;
	case WACOM_AXIS_STRIP_Y: data->strip_y = value; break;
	case WACOM_AXIS_ROTATION: data->rotation = value; break;
	case WACOM_AXIS_THROTTLE: data->wheel = value; break;
	case WACOM_AXIS_WHEEL: data->wheel = value; break;
	case WACOM_AXIS_RING: data->ring = value; break;
	case WACOM_AXIS_RING2: data->ring2 = value; break;
	case WACOM_AXIS_SCROLL_X: data->scroll_x = value; break;
	case WACOM_AXIS_SCROLL_Y: data->scroll_y = value; break;
	default:
		abort();
	}
}

static inline bool wcmAxisGet(const WacomAxisData *data,
			      enum WacomAxisType which, int *value_out)
{
	if (!(data->mask & which))
		return FALSE;

	switch (which){
	case WACOM_AXIS_X: *value_out = data->x; break;
	case WACOM_AXIS_Y: *value_out = data->y; break;
	case WACOM_AXIS_PRESSURE: *value_out = data->pressure; break;
	case WACOM_AXIS_TILT_X: *value_out = data->tilt_x; break;
	case WACOM_AXIS_TILT_Y: *value_out = data->tilt_y; break;
	case WACOM_AXIS_STRIP_X: *value_out = data->strip_x; break;
	case WACOM_AXIS_STRIP_Y: *value_out = data->strip_y; break;
	case WACOM_AXIS_ROTATION: *value_out = data->rotation; break;
	case WACOM_AXIS_THROTTLE: *value_out = data->wheel; break;
	case WACOM_AXIS_WHEEL: *value_out = data->wheel; break;
	case WACOM_AXIS_RING: *value_out = data->ring; break;
	case WACOM_AXIS_RING2: *value_out = data->ring2; break;
	case WACOM_AXIS_SCROLL_X: *value_out = data->scroll_x; break;
	case WACOM_AXIS_SCROLL_Y: *value_out = data->scroll_y; break;
	default:
		abort();
	}
	return TRUE;
}

static inline void wcmAxisValue(const WacomAxisData *data,
			       enum WacomAxisType which,
			       char *buf, size_t len)
{
	int val = 0;
	int rc;

	if (!wcmAxisGet(data, which, &val)) {
		rc = snprintf(buf, len, "N/A");
		assert(rc > 0 && (size_t)rc < len);
		return;
	}
	rc = snprintf(buf, len, "%d", val);
	assert(rc > 0 && (size_t)rc < len);
}

static inline const char* wcmAxisName(enum WacomAxisType which)
{
	switch (which){
	case WACOM_AXIS_X: return "x";
	case WACOM_AXIS_Y: return "y";
	case WACOM_AXIS_PRESSURE: return "pressure";
	case WACOM_AXIS_TILT_X: return "tilt-x";
	case WACOM_AXIS_TILT_Y: return "tilt-y";
	case WACOM_AXIS_STRIP_X: return "strip-x";
	case WACOM_AXIS_STRIP_Y: return "strip-y";
	case WACOM_AXIS_ROTATION: return "rotation";
	case WACOM_AXIS_THROTTLE: return "throttle";
	case WACOM_AXIS_WHEEL: return "wheel";
	case WACOM_AXIS_RING: return "ring";
	case WACOM_AXIS_RING2: return "ring2";
	case WACOM_AXIS_SCROLL_X: return "scroll-x";
	case WACOM_AXIS_SCROLL_Y: return "scroll-y";
	default:
		abort();
	}
}

static inline void wcmAxisDump(const WacomAxisData *data, char *buf, size_t len)
{
	uint32_t mask = data->mask;
	const char *prefix = "";

	assert(len > 0);
	buf[0] = '\0';
	for (uint32_t flag = 0x1; flag <= _WACOM_AXIS_LAST; flag <<= 1) {
		const char *name = wcmAxisName(flag);
		char value[32];
		int rc;

		if ((mask & flag) == 0)
			continue;

		wcmAxisValue(data, flag, value, sizeof(value));

		rc = snprintf(buf, len, "%s%s: %s", prefix, name, value);
		assert(rc > 0 && (size_t)rc < len);
		buf += rc;
		len -= rc;
		prefix = ", ";
	}
}

void wcmInitAxis(WacomDevicePtr priv, enum WacomAxisType type, int min, int max, int res);
bool wcmInitButtons(WacomDevicePtr priv, unsigned int nbuttons);
bool wcmInitKeyboard(WacomDevicePtr priv);
bool wcmInitPointer(WacomDevicePtr priv, int naxes, bool is_absolute);
bool wcmInitTouch(WacomDevicePtr priv, int ntouches, bool is_direct_touch);


void wcmEmitKeycode(WacomDevicePtr priv, int keycode, int state);
void wcmEmitMotion(WacomDevicePtr priv, bool is_absolute, const WacomAxisData *axes);
void wcmEmitButton(WacomDevicePtr priv, bool is_absolute, int button, bool is_press,
		   const WacomAxisData *axes);
void wcmEmitProximity(WacomDevicePtr priv, bool is_proximity_in,
		      const WacomAxisData *axes);
void wcmEmitTouch(WacomDevicePtr priv, int type, unsigned int touchid, int x, int y);


struct input_event;
void wcmNotifyEvdev(WacomDevicePtr priv, const struct input_event *event);

/**
 * Queue the addition of a new device with the device's name, type and
 * the serial, if any. Otherwise the device should be a copy of priv.
 *
 * This is a **queuing** function, the device must be hotplugged when the
 * frontend is idle later.
 */
void wcmQueueHotplug(WacomDevicePtr priv, const char *name,
		     const char *type, unsigned int serial);

/* X server interface emulations */

/* Get the option of the given type */
char *wcmOptGetStr(WacomDevicePtr priv, const char *key, const char *default_value);
int wcmOptGetInt(WacomDevicePtr priv, const char *key, int default_value);
bool wcmOptGetBool(WacomDevicePtr priv, const char *key, bool default_value);

/* Get the option of the given type, quietly (without logging) */
char *wcmOptCheckStr(WacomDevicePtr priv, const char *key, const char *default_value);
int wcmOptCheckInt(WacomDevicePtr priv, const char *key, int default_value);
bool wcmOptCheckBool(WacomDevicePtr priv, const char *key, bool default_value);

/* Change the option to the new value */
void wcmOptSetStr(WacomDevicePtr priv, const char *key, const char *value);
void wcmOptSetInt(WacomDevicePtr priv, const char *key, int value);
void wcmOptSetBool(WacomDevicePtr priv, const char *key, bool value);

typedef struct _WacomTimer *WacomTimerPtr;

/* Return the new (relative) time in millis to set the timer for or 0 */
typedef uint32_t (*WacomTimerCallback)(WacomTimerPtr timer, uint32_t millis, void *userdata);

WacomTimerPtr wcmTimerNew(void);
void wcmTimerFree(WacomTimerPtr timer);
void wcmTimerCancel(WacomTimerPtr timer);
void wcmTimerSet(WacomTimerPtr timer, uint32_t millis, /* reltime */
		 WacomTimerCallback func, void *userdata);


void wcmUpdateRotationProperty(WacomDevicePtr priv);
void wcmUpdateSerialProperty(WacomDevicePtr priv);
void wcmUpdateHWTouchProperty(WacomDevicePtr priv);

#endif

