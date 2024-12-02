/*
 * Copyright 2009 - 2010 Red Hat, Inc.
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

#include "config.h"

#include "config-ver.h" /* BUILD_VERSION */

#include <wacom-properties.h>
#include <wacom-util.h>
#include "Xwacom.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <math.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xinerama.h>
#include <X11/XKBlib.h>

#define TRACE(...) \
	if (verbose) fprintf(stderr, "... " __VA_ARGS__)

#define safe_realloc(p, n, s) \
	((size_t)-1 / (n) < (s) ? NULL : realloc((p), (n)*(s)))

#define EXIT_INVALID_USAGE 2

static int verbose = False;

enum printformat {
	FORMAT_DEFAULT,
	FORMAT_XORG_CONF,
	FORMAT_SHELL
};

enum prop_flags {
	PROP_FLAG_BOOLEAN = 1,
	PROP_FLAG_READONLY = 2,
	PROP_FLAG_WRITEONLY = 4,
	PROP_FLAG_INVERTED = 8, /* only valid with PROP_FLAG_BOOLEAN */
	PROP_FLAG_OUTPUT = 16,
};


/**
 * How this works:
 * Each parameter supported by xsetwacom has a struct param_t in the global
 * parameters[] array.
 * For 'standard' parameters that just modify a property, the prop_* fields
 * are set to the matching property, format and offset. The get() function
 * then handles the retrieval  of the property, the set() function handles
 * the modification of the property.
 *
 * For parameters that need more than just triggering a property, the
 * set_func and get_func point to the matching function to modify that
 * particular parameter. example are the ButtonX parameters that call
 * XSetDeviceButtonMapping instead of triggering a property.
 *
 * device_name is filled in automatically and just used to pass around the
 * device name (since the XDevice* doesn't store this info). printformat is
 * a flag that changes the output required, so that the -c and -s
 * commandline arguments work.
 */

typedef struct _param
{
	const char *name;	/* param name as specified by the user */
	const char *x11name;    /* param name used in xorg.conf */
	const char *desc;	/* description */
	const char *prop_name;	/* property name */
	const int prop_format;	/* property format */
	const unsigned int prop_offset;	/* offset (index) into the property values */
	const int arg_count;   /* extra number of items after first one */
	const unsigned int prop_flags;
	int (*set_func)(Display *dpy, XDevice *dev, struct _param *param, int argc, char **argv); /* handler function, if appropriate */
	int (*get_func)(Display *dpy, XDevice *dev, struct _param *param, int argc, char **argv); /* handler function for getting, if appropriate */

	/* filled in by get() */
	char *device_name;
	enum printformat printformat;
} param_t;


/* get_func/set_func calls for special parameters */
static int map_actions(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static int set_mode(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static int get_mode(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static int get_map(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static int set_rotate(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static int get_rotate(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static int set_xydefault(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static int get_all(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static int get_param(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static int set_output(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);

/* NOTE: When removing or changing a parameter name, add to
 * deprecated_parameters.
 */
static param_t parameters[] =
{
	{
		.name = "Area",
		.x11name = "Area",
		.desc = "Valid tablet area in device coordinates. ",
		.prop_name = WACOM_PROP_TABLET_AREA,
		.prop_format = 32,
		.prop_offset = 0,
		.arg_count = 4,
	},
	{
		.name = "Button",
		.desc = "X11 event to which the given button should be mapped. ",
		.prop_name = WACOM_PROP_BUTTON_ACTIONS,
		.arg_count = 1,
		.set_func = map_actions,
		.get_func = get_map,
	},
	{
		.name = "ToolDebugLevel",
		.x11name = "DebugLevel",
		.desc = "Level of debugging trace for individual tools "
		"(default is 0 [off]). ",
		.prop_name = WACOM_PROP_DEBUGLEVELS,
		.prop_format = 8,
		.prop_offset = 0,
		.arg_count = 1,
	},
	{
		.name = "TabletDebugLevel",
		.x11name = "CommonDBG",
		.desc = "Level of debugging statements applied to shared "
		"code paths between all tools "
		"associated with the same tablet (default is 0 [off]). ",
		.prop_name = WACOM_PROP_DEBUGLEVELS,
		.prop_format = 8,
		.prop_offset = 1,
		.arg_count = 1,
	},
	{
		.name = "Suppress",
		.x11name = "Suppress",
		.desc = "Number of points trimmed (default is 2). ",
		.prop_name = WACOM_PROP_SAMPLE,
		.prop_format = 32,
		.prop_offset = 0,
		.arg_count = 1,
	},
	{
		.name = "RawSample",
		.x11name = "RawSample",
		.desc = "Number of raw data used to filter the points "
		"(default is 4). ",
		.prop_name = WACOM_PROP_SAMPLE,
		.prop_format = 32,
		.prop_offset = 1,
		.arg_count = 1,
	},
	{
		.name = "PressureCurve",
		.x11name = "PressCurve",
		.desc = "Bezier curve for pressure (default is 0 0 100 100 [linear]). ",
		.prop_name = WACOM_PROP_PRESSURECURVE,
		.prop_format = 32,
		.prop_offset = 0,
		.arg_count = 4,
	},
	{
		.name = "Mode",
		.x11name = "Mode",
		.desc = "Switches cursor movement mode (default is absolute). ",
		.arg_count = 1,
		.set_func = set_mode,
		.get_func = get_mode,
	},
	{
		.name = "TabletPCButton",
		.x11name = "TPCButton",
		.desc = "Turns on/off Tablet PC buttons "
		"(default is off for regular tablets, "
		"on for Tablet PC). ",
		.prop_name = WACOM_PROP_HOVER,
		.prop_format = 8,
		.prop_offset = 0,
		.arg_count = 1,
		.prop_flags = PROP_FLAG_BOOLEAN | PROP_FLAG_INVERTED
	},
	{
		.name = "Touch",
		.x11name = "Touch",
		.desc = "Turns on/off Touch events (default is on). ",
		.prop_name = WACOM_PROP_TOUCH,
		.prop_format = 8,
		.prop_offset = 0,
		.arg_count = 1,
		.prop_flags = PROP_FLAG_BOOLEAN
	},
	{
		.name = "HWTouchSwitchState",
		.desc = "Touch events turned on/off by hardware switch. ",
		.prop_name = WACOM_PROP_HARDWARE_TOUCH,
		.prop_format = 8,
		.prop_offset = 0,
		.arg_count = 1,
		.prop_flags = PROP_FLAG_READONLY | PROP_FLAG_BOOLEAN
	},
	{
		.name = "Gesture",
		.x11name = "Gesture",
		.desc = "Turns on/off multi-touch gesture events "
		"(default is on). ",
		.prop_name = WACOM_PROP_ENABLE_GESTURE,
		.prop_format = 8,
		.prop_offset = 0,
		.arg_count = 1,
		.prop_flags = PROP_FLAG_BOOLEAN
	},
	{
		.name = "ZoomDistance",
		.x11name = "ZoomDistance",
		.desc = "Minimum distance for a zoom gesture "
		"(default is 50). ",
		.prop_name = WACOM_PROP_GESTURE_PARAMETERS,
		.prop_format = 32,
		.prop_offset = 0,
		.arg_count = 1,
	},
	{
		.name = "ScrollDistance",
		.x11name = "ScrollDistance",
		.desc = "Minimum motion before sending a scroll gesture "
		"(default is 20). ",
		.prop_name = WACOM_PROP_GESTURE_PARAMETERS,
		.prop_format = 32,
		.prop_offset = 1,
		.arg_count = 1,
	},
	{
		.name = "TapTime",
		.x11name = "TapTime",
		.desc = "Minimum time between taps for a right click "
		"(default is 250). ",
		.prop_name = WACOM_PROP_GESTURE_PARAMETERS,
		.prop_format = 32,
		.prop_offset = 2,
		.arg_count = 1,
	},
	{
		.name = "CursorProximity",
		.x11name = "CursorProx",
		.desc = "Sets cursor distance for proximity-out "
		"in distance from the tablet "
		"(default is 10 for Intuos series, "
		"42 for Graphire series). ",
		.prop_name = WACOM_PROP_PROXIMITY_THRESHOLD,
		.prop_format = 32,
		.prop_offset = 0,
		.arg_count = 1,
	},
	{
		.name = "Rotate",
		.x11name = "Rotate",
		.desc = "Sets the rotation of the tablet. "
		"Values = none, cw, ccw, half (default is none). ",
		.prop_name = WACOM_PROP_ROTATION,
		.set_func = set_rotate,
		.get_func = get_rotate,
		.arg_count = 1,
	},
	{
		.name = "RelWheelUp",
		.desc = "X11 event to which relative wheel up should be mapped. ",
		.prop_name = WACOM_PROP_WHEELBUTTONS,
		.prop_format = 8,
		.prop_offset = 0,
		.arg_count = 0,
		.set_func = map_actions,
		.get_func = get_map,
	},
	{
		.name = "RelWheelDown",
		.desc = "X11 event to which relative wheel down should be mapped. ",
		.prop_name = WACOM_PROP_WHEELBUTTONS,
		.prop_format = 8,
		.prop_offset = 1,
		.arg_count = 0,
		.set_func = map_actions,
		.get_func = get_map,
	},
	{
		.name = "AbsWheelUp",
		.desc = "X11 event to which absolute wheel up should be mapped. ",
		.prop_name = WACOM_PROP_WHEELBUTTONS,
		.prop_format = 8,
		.prop_offset = 2,
		.arg_count = 0,
		.set_func = map_actions,
		.get_func = get_map,
	},
	{
		.name = "AbsWheelDown",
		.desc = "X11 event to which absolute wheel down should be mapped. ",
		.prop_name = WACOM_PROP_WHEELBUTTONS,
		.prop_format = 8,
		.prop_offset = 3,
		.arg_count = 0,
		.set_func = map_actions,
		.get_func = get_map,
	},
	{
		.name = "AbsWheel2Up",
		.desc = "X11 event to which absolute wheel up should be mapped. ",
		.prop_name = WACOM_PROP_WHEELBUTTONS,
		.prop_format = 8,
		.prop_offset = 4,
		.arg_count = 0,
		.set_func = map_actions,
		.get_func = get_map,
	},
	{
		.name = "AbsWheel2Down",
		.desc = "X11 event to which absolute wheel down should be mapped. ",
		.prop_name = WACOM_PROP_WHEELBUTTONS,
		.prop_format = 8,
		.prop_offset = 5,
		.arg_count = 0,
		.set_func = map_actions,
		.get_func = get_map,
	},
	{
		.name = "StripLeftUp",
		.desc = "X11 event to which left strip up should be mapped. ",
		.prop_name = WACOM_PROP_STRIPBUTTONS,
		.prop_format = 8,
		.prop_offset = 0,
		.arg_count = 0,
		.set_func = map_actions,
		.get_func = get_map,
	},
	{
		.name = "StripLeftDown",
		.desc = "X11 event to which left strip down should be mapped. ",
		.prop_name = WACOM_PROP_STRIPBUTTONS,
		.prop_format = 8,
		.prop_offset = 1,
		.arg_count = 0,
		.set_func = map_actions,
		.get_func = get_map,
	},
	{
		.name = "StripRightUp",
		.desc = "X11 event to which right strip up should be mapped. ",
		.prop_name = WACOM_PROP_STRIPBUTTONS,
		.prop_format = 8,
		.prop_offset = 2,
		.arg_count = 0,
		.set_func = map_actions,
		.get_func = get_map,
	},
	{
		.name = "StripRightDown",
		.desc = "X11 event to which right strip down should be mapped. ",
		.prop_name = WACOM_PROP_STRIPBUTTONS,
		.prop_format = 8,
		.prop_offset = 3,
		.arg_count = 0,
		.set_func = map_actions,
		.get_func = get_map,
	},
	{
		.name = "Threshold",
		.x11name = "Threshold",
		.desc = "Sets tip/eraser pressure threshold "
		"(default is 27). ",
		.prop_name = WACOM_PROP_PRESSURE_THRESHOLD,
		.prop_format = 32,
		.prop_offset = 0,
		.arg_count = 1,
	},
	{
		.name = "ResetArea",
		.desc = "Resets the bounding coordinates to default in tablet units. ",
		.prop_name = WACOM_PROP_TABLET_AREA,
		.prop_format = 32,
		.prop_offset = 0,
		.arg_count = 0,
		.prop_flags = PROP_FLAG_WRITEONLY,
		.set_func = set_xydefault,
	},
	{
		.name = "ToolType",
		.desc = "Returns the tool type of the associated device. ",
		.prop_name = WACOM_PROP_TOOL_TYPE,
		.prop_format = 32,
		.prop_offset = 0,
		.arg_count = 1,
		.prop_flags = PROP_FLAG_READONLY
	},
	{
		.name = "ToolSerial",
		.desc = "Returns the serial number of the current device in proximity.",
		.prop_name = WACOM_PROP_SERIALIDS,
		.prop_format = 32,
		.prop_offset = 3,
		.arg_count = 1,
		.prop_flags = PROP_FLAG_READONLY
	},
	{
		.name = "ToolID",
		.desc = "Returns the tool ID of the current tool in proximity.",
		.prop_name = WACOM_PROP_SERIALIDS,
		.prop_format = 32,
		.prop_offset = 4,
		.arg_count = 1,
		.prop_flags = PROP_FLAG_READONLY
	},
	{
		.name = "ToolSerialPrevious",
		.desc = "Returns the serial number of the previous device in proximity.",
		.prop_name = WACOM_PROP_SERIALIDS,
		.prop_format = 32,
		.prop_offset = 1,
		.arg_count = 1,
		.prop_flags = PROP_FLAG_READONLY
	},
	{
		.name = "BindToSerial",
		.x11name = "Serial",
		.desc = "Binds this device to the serial number.",
		.prop_name = WACOM_PROP_SERIAL_BIND,
		.prop_format = 32,
		.prop_offset = 0,
		.arg_count = 1,
	},
	{
		.name = "TabletID",
		.desc = "Returns the tablet ID of the associated device. ",
		.prop_name = WACOM_PROP_SERIALIDS,
		.prop_format = 32,
		.prop_offset = 0,
		.arg_count = 1,
		.prop_flags = PROP_FLAG_READONLY
	},
	{
		.name = "PressureRecalibration",
		.x11name = "PressureRecalibration",
		.desc = "Turns on/off Tablet pressure recalibration",
		.prop_name = WACOM_PROP_PRESSURE_RECAL,
		.prop_format = 8,
		.prop_offset = 0,
		.arg_count = 1,
		.prop_flags = PROP_FLAG_BOOLEAN
	},
	{
		.name = "PanScrollThreshold",
		.x11name = "PanScrollThreshold",
		.desc = "Adjusts distance required for pan actions to generate a scroll event",
		.prop_name = WACOM_PROP_PANSCROLL_THRESHOLD,
		.prop_format = 32,
		.prop_offset = 0,
		.arg_count = 1,
	},
	{
		.name = "MapToOutput",
		.desc = "Map the device to the given output. ",
		.set_func = set_output,
		.arg_count = 1,
		.prop_flags = PROP_FLAG_WRITEONLY | PROP_FLAG_OUTPUT,
	},
	{
		.name = "DeJitterEnable",
		.desc = "Enable or disable de-jittering (default is off).",
		.prop_name = WACOM_PROP_DEJITTER_ENABLED,
		.prop_format = 8,
		.prop_offset = 0,
		.arg_count = 1,
		.prop_flags = PROP_FLAG_BOOLEAN,
	},
	{
		.name = "DeJitterThreshold",
		.desc = "Set the spatial threshold for de-jittering (default is 150).",
		.prop_name = WACOM_PROP_DEJITTER_THRESHOLD,
		.prop_format = 32,
		.prop_offset = 0,
		.arg_count = 1,
	},
	{
		.name = "DeJitterTimeThreshold",
		.desc = "Set the time threshold (in ms) for de-jittering (default is 100).",
		.prop_name = WACOM_PROP_DEJITTER_TIME_THRESHOLD,
		.prop_format = 32,
		.prop_offset = 0,
		.arg_count = 1,
	},
	{
		.name = "all",
		.desc = "Get value for all parameters. ",
		.get_func = get_all,
		.prop_flags = PROP_FLAG_READONLY,
	},
	{.name = NULL}
};

/**
 * Deprecated parameters and their respective replacements.
 */
static struct deprecated
{
	const char *name;
	const char *replacement;
} deprecated_parameters[] =
{
	{"Button",	"Button"}, /* this covers Button1-32 */
	{"TopX",	"Area"},
	{"TopY",	"Area"},
	{"BottomX",	"Area"},
	{"BottomY",	"Area"},
	{"GetTabletID", "TabletID"},
	{"DebugLevel",	"ToolDebugLevel"},
	{"CommonDBG",	"TabletDebugLevel"},
	{"Serial",	"BindToSerial"},
	{"PressCurve",	"PressureCurve"},
	{"TPCButton",	"TabletPCButton"},
	{"CursorProx",	"CursorProximity"},
	{"xyDefault",	"ResetArea"},
	{"ClickForce",	"Threshold"},
	{"RawFilter",   NULL},
	{"Capacity",	NULL},
	{NULL,		NULL}
};

/**
 * Check if name is deprecated and print out a warning if it is.
 *
 * @return True if deprecated, False otherwise.
 */
static Bool
is_deprecated_parameter(const char *name)
{
	struct deprecated *d;
	Bool is_deprecated = False;

	/* all others */
	for (d = deprecated_parameters; d->name; d++)
	{
		if (strncmp(name, d->name, strlen(d->name)) == 0)
		{
			is_deprecated = True;
			break;
		}
	}

	if (is_deprecated)
	{
		fprintf(stderr, "Parameter '%s' is no longer in use. ", name);
		if (d->replacement != NULL)
			fprintf(stderr, "It was replaced with '%s'.\n", d->replacement);
		else
			fprintf(stderr, "Its use has been deprecated.\n");
	}

	return is_deprecated;

}

struct modifier {
	const char *name;
	const char *converted;
};

static struct modifier modifiers[] = {
	{"ctrl", "Control_L"},
	{"ctl", "Control_L"},
	{"control", "Control_L"},
	{"lctrl", "Control_L"},
	{"rctrl", "Control_R"},

	{"meta", "Meta_L"},
	{"lmeta", "Meta_L"},
	{"rmeta", "Meta_R"},

	{"alt", "Alt_L"},
	{"lalt", "Alt_L"},
	{"ralt", "Alt_R"},

	{"shift", "Shift_L"},
	{"lshift", "Shift_L"},
	{"rshift", "Shift_R"},

	{"super", "Super_L"},
	{"lsuper", "Super_L"},
	{"rsuper", "Super_R"},

	{"hyper", "Hyper_L"},
	{"lhyper", "Hyper_L"},
	{"rhyper", "Hyper_R"},

	{"altgr", "ISO_Level3_Shift"},

	{ NULL, NULL }
};

static struct modifier specialkeys[] = {
	{"f1", "F1"}, {"f2", "F2"}, {"f3", "F3"},
	{"f4", "F4"}, {"f5", "F5"}, {"f6", "F6"},
	{"f7", "F7"}, {"f8", "F8"}, {"f9", "F9"},
	{"f10", "F10"}, {"f11", "F11"}, {"f12", "F12"},
	{"f13", "F13"}, {"f14", "F14"}, {"f15", "F15"},
	{"f16", "F16"}, {"f17", "F17"}, {"f18", "F18"},
	{"f19", "F19"}, {"f20", "F20"}, {"f21", "F21"},
	{"f22", "F22"}, {"f23", "F23"}, {"f24", "F24"},
	{"f25", "F25"}, {"f26", "F26"}, {"f27", "F27"},
	{"f28", "F28"}, {"f29", "F29"}, {"f30", "F30"},
	{"f31", "F31"}, {"f32", "F32"}, {"f33", "F33"},
	{"f34", "F34"}, {"f35", "F35"},

	{"esc", "Escape"}, {"Esc", "Escape"},

	{"up", "Up"}, {"down", "Down"},
	{"left", "Left"}, {"right", "Right"},

	{"backspace", "BackSpace"}, {"Backspace", "BackSpace"},

	{"return", "Return"},

	{"tab", "Tab"},

	{"PgUp", "Prior"}, {"PgDn", "Next"},

	{"del", "Delete"}, {"home", "Home"},
	{"end", "End"},

	{"insert", "Insert"},

	{"`", "quoteleft"},
	{"-", "minus"},
	{"=", "equal"},
	{"[", "bracketleft"},
	{"]", "bracketright"},
	{"\\", "backslash"},
	{";", "semicolon"},
	{"'", "apostrophe"},
	{",", "comma"},
	{".", "period"},
	{"/", "slash"},

	{"~", "asciitilde"},
	{"!", "exclam"},
	{"@", "at"},
	{"#", "numbersign"},
	{"$", "dollar"},
	{"%", "percent"},
	{"^", "asciicircum"},
	{"&", "ampersand"},
	{"*", "asterisk"},
	{"(", "parenleft"},
	{")", "parenright"},
	{"_", "underscore"},
	{"+", "plus"},
	{"{", "braceleft"},
	{"}", "braceright"},
	{"|", "bar"},
	{":", "colon"},
	{"\"", "quotedbl"},
	{"<", "less"},
	{">", "greater"},
	{"?", "question"},

	{ NULL, NULL }
};

static param_t* find_parameter(char *name)
{
	param_t *param = NULL;

	for (param = parameters; param->name; param++)
		if (strcasecmp(name, param->name) == 0)
			break;
	return param->name ? param : NULL;
}

static void print_value(param_t *param, const char *msg, ...) __attribute__((format(printf, 2, 3)));
static void print_value(param_t *param, const char *msg, ...)
{
	va_list va_args;
	va_start(va_args, msg);
	switch(param->printformat)
	{
		case FORMAT_XORG_CONF:
			if (param->prop_flags & PROP_FLAG_READONLY)
				break;
			if (!param->x11name)
				break;
			printf("Option \"%s\" \"", param->x11name);
			vprintf(msg, va_args);
			printf("\"\n");
			break;
		case FORMAT_SHELL:
			if (param->prop_flags & PROP_FLAG_READONLY)
				break;
			printf("xsetwacom set \"%s\" \"%s\" \"",
					param->device_name, param->name);
			vprintf(msg, va_args);
			printf("\"\n");
			break;
		case FORMAT_DEFAULT:
		default:
			vprintf(msg, va_args);
			printf("\n");
			break;
	}

	va_end(va_args);
}

static void print_button_value(param_t *param, int n, const char *msg, ...) __attribute__((format(printf, 3, 4)));
static void print_button_value(param_t *param, int n, const char *msg, ...)
{
	va_list va_args;
	va_start(va_args, msg);

	n++; /* Property is 0-indexed, X buttons are 1-indexed */
	switch(param->printformat)
	{
		case FORMAT_XORG_CONF:
			printf("Option \"%s%d\" \"", param->name, n);
			vprintf(msg, va_args);
			printf("\"\n");
			break;
		case FORMAT_SHELL:
			printf("xsetwacom set \"%s\" \"%s\" \"%d\" \"",
					param->device_name, param->name, n);
			vprintf(msg, va_args);
			printf("\"\n");
			break;
		case FORMAT_DEFAULT:
		default:
			vprintf(msg, va_args);
			printf("\n");
			break;
	}

	va_end(va_args);
}

static void usage(void)
{
	printf(
	"Usage: xsetwacom [options] [command [arguments...]]\n"
	"Options:\n"
	" -h, --help                 - usage\n"
	" -v, --verbose              - verbose output\n"
	" -V, --version              - version info\n"
	" -d, --display \"display\"    - override default display\n"
	" -s, --shell                - generate shell commands for 'get'\n"
	" -x, --xconf                - generate xorg.conf lines for 'get'\n");

	printf(
	"\nCommands:\n"
	" --list devices             - display detected devices\n"
	" --list parameters          - display supported parameters\n"
	" --list modifiers           - display supported modifier and specific keys for keystrokes\n"
	" --set \"device name\" parameter [values...] - set device parameter by name\n"
	" --get \"device name\" parameter [param...]  - get current device parameter(s) value by name\n");
}


static void version(void)
{
	printf(BUILD_VERSION "\n");
}

static XDevice* find_device(Display *display, char *name)
{
	XDeviceInfo	*devices;
	XDeviceInfo	*found = NULL;
	XDevice		*dev = NULL;
	int		i, num_devices;
	int		len = strlen(name);
	Bool		is_id = True;
	XID		id = -1;

	for(i = 0; i < len; i++)
	{
		if (!isdigit(name[i]))
		{
			is_id = False;
			break;
		}
	}

	if (is_id)
		id = atoi(name);

	devices = XListInputDevices(display, &num_devices);

	for(i = 0; i < num_devices; i++)
	{
		TRACE("Checking device '%s' (%lu).\n", devices[i].name, devices[i].id);

		if (((devices[i].use >= IsXExtensionDevice)) &&
			((!is_id && strcmp(devices[i].name, name) == 0) ||
			 (is_id && devices[i].id == id)))
		{
			if (found) {
				fprintf(stderr, "Warning: There are multiple devices named \"%s\".\n"
						"To ensure the correct one is selected, please use "
						"the device ID instead.\n\n", name);
				found = NULL;
				break;
			} else {
				found = &devices[i];
			}
		}
	}

	if (found)
	{
		TRACE("Device '%s' (%lu) found.\n", found->name, found->id);
		dev = XOpenDevice(display, found->id);
	}

	XFreeDeviceList(devices);

	return dev;
}

/* Return True if the given device has the property, or False otherwise */
static Bool test_property(Display *dpy, XDevice* dev, Atom prop)
{
	int nprops_return;
	Atom *properties;
	int found = False;

	/* if no property is required, return success */
	if (prop == None)
		return True;

	properties = XListDeviceProperties(dpy, dev, &nprops_return);

	while(nprops_return--)
	{
		if (properties[nprops_return] == prop)
		{
			found = True;
			break;
		}
	}

	XFree(properties);
	return found;
}

static void list_one_device(Display *dpy, XDeviceInfo *info)
{
	static unsigned int wacom_prop = 0;
	int		natoms;
	Atom		*atoms;
	XDevice		*dev;
	char		*type_name = NULL;

	if (!wacom_prop)
		wacom_prop = XInternAtom(dpy, "Wacom Tool Type", True);

	dev = XOpenDevice(dpy, info->id);
	if (!dev)
		return;

	atoms = XListDeviceProperties(dpy, dev, &natoms);
	if (natoms)
	{
		int j;
		for (j = 0; j < natoms; j++)
			if (atoms[j] == wacom_prop)
				break;

		if (j < natoms)
		{
			unsigned char	*data;
			Atom		type;
			int		format;
			unsigned long	nitems, bytes_after;

			XGetDeviceProperty(dpy, dev, wacom_prop, 0, 1,
					   False, AnyPropertyType, &type,
					   &format, &nitems, &bytes_after,
					   &data);
			if (nitems)
			{
				type_name = XGetAtomName(dpy, *(Atom*)data);
				printf("%-32s	id: %lu	type: %-10s\n",
						info->name, info->id,
						type_name);
			}

			XFree(data);
		} else {
			TRACE("'%s' (%lu) is not a wacom device.\n", info->name, info->id);
		}

		XFree(atoms);
	}

	XCloseDevice(dpy, dev);
	XFree(type_name);
}

static void list_devices(Display *dpy)
{
	XDeviceInfo	*info;
	int		ndevices, i;
	Atom		wacom_prop;


	wacom_prop = XInternAtom(dpy, "Wacom Tool Type", True);
	if (wacom_prop == None)
		return;

	info = XListInputDevices(dpy, &ndevices);

	for (i = 0; i < ndevices; i++)
	{
		if (info[i].use == IsXPointer || info[i].use == IsXKeyboard)
			continue;

		TRACE("Found device '%s' (%lu).\n", info[i].name, info[i].id);
		list_one_device(dpy, &info[i]);
	}

	XFreeDeviceList(info);
}


static void list_param(Display *dpy)
{
	param_t *param = parameters;

	while(param->name)
	{
		printf("%-16s - %16s\n", param->name, param->desc);
		param++;
	}
}

static void list_mod(Display *dpy)
{
	struct modifier *m = modifiers;

	printf("%zu modifiers are supported:\n", ARRAY_SIZE(modifiers) - 1);
	while(m->name)
		printf("	%s\n", m++->name);

	printf("\n%zu specialkeys are supported:\n", ARRAY_SIZE(specialkeys) - 1);
	m = specialkeys;
	while(m->name)
		printf("	%s\n", m++->name);

	printf("Keys not listed above can be specified via their KeySyms."
		" See the man page for details.\n");
}

static int list(Display *dpy, int argc, char **argv)
{
	TRACE("'list' requested.\n");
	if (argc == 0)
		list_devices(dpy);
	else if (strcmp(argv[0], "dev") == 0 ||
		 strcmp(argv[0], "devices") == 0)
		list_devices(dpy);
	else if (strcmp(argv[0], "param") == 0 ||
		 strcmp(argv[0], "parameters") == 0)
		list_param(dpy);
	else if (strcmp(argv[0], "mod") == 0 ||
		 strcmp(argv[0], "modifiers") == 0)
		list_mod(dpy);
	else {
		fprintf(stderr, "unknown argument to list.\n");
		return EXIT_INVALID_USAGE;
	}
	return EXIT_SUCCESS;
}

/**
 * Convert a list of random special keys to strings that can be passed into
 * XStringToKeysym
 * @param special A special key, e.g. a modifier or one of the keys in
 * specialkeys.
 * @return The X Keysym representing specialkey.
 */
static const char *convert_specialkey(const char *specialkey)
{
	struct modifier *m = modifiers;

	while(m->name && strcasecmp(specialkey, m->name))
		m++;

	if (!m->name)
	{
		m = specialkeys;
		while(m->name && strcasecmp(specialkey, m->name))
			m++;
	}

	return m->converted ? m->converted : specialkey;
}

/**
 * @param keysym An X Keysym
 * @return nonzero if the given keysym is a modifier (as per the modifiers
 * list) or zero otherwise.
 */
static int is_modifier(const char* keysym)
{
	struct modifier *m = modifiers;

	while(m->name)
	{
		if (strcmp(keysym, m->converted) == 0)
			break;
		m++;
	}

	return (m->name != NULL);
}

static int special_map_keystrokes(Display *dpy, int argc, char **argv, unsigned long *ndata, unsigned long* data, const size_t size);
static int special_map_button(Display *dpy, int argc, char **argv, unsigned long *ndata, unsigned long* data, const size_t size);
static int special_map_core(Display *dpy, int argc, char **argv, unsigned long *ndata, unsigned long *data, const size_t size);
static int special_map_modetoggle(Display *dpy, int argc, char **argv, unsigned long *ndata, unsigned long *data, const size_t size);
static int special_map_displaytoggle(Display *dpy, int argc, char **argv, unsigned long *ndata, unsigned long *data, const size_t size);
static int special_map_panscroll(Display *dpy, int argc, char **argv, unsigned long *ndata, unsigned long *data, const size_t size);

/* Valid keywords for the --set ButtonX options */
static struct keywords {
	const char *keyword;
	int (*func)(Display*, int, char **, unsigned long*, unsigned long *, const size_t size);
} keywords[] = {
	{"key", special_map_keystrokes},
	{"button", special_map_button},
	{"core", special_map_core},
	{"modetoggle", special_map_modetoggle},
	{"displaytoggle", special_map_displaytoggle},
	{"pan", special_map_panscroll},
	{ NULL, NULL }
};

/* the "core" keyword isn't supported anymore, we just have this here to
   tell people that. */
static int special_map_core(Display *dpy, int argc, char **argv, unsigned long *ndata, unsigned long *data, const size_t size)
{
	static int once_only = 1;
	if (once_only)
	{
		fprintf(stderr, "Note: The \"core\" keyword is not supported anymore and "
			"will be ignored.\n");
		once_only = 0;
	}
	return 0;
}

static int special_map_modetoggle(Display *dpy, int argc, char **argv, unsigned long *ndata, unsigned long *data, const size_t size)
{
	if (*ndata + 1 > size) {
		fprintf(stderr, "Insufficient space to store all commands.\n");
		return 0;
	}
	data[*ndata] = AC_MODETOGGLE;

	*ndata += 1;

	return 0;
}

/* the "displaytoggle" keyword isn't supported anymore, we just have this
   here to tell people that. */
static int special_map_displaytoggle(Display *dpy, int argc, char **argv, unsigned long *ndata, unsigned long *data, const size_t size)
{
	static int once_only = 1;
	if (once_only)
	{
		fprintf(stderr, "Note: The \"displaytoggle\" keyword is not supported "
			"anymore and will be ignored.\n");
		once_only = 0;
	}
	return 0;
}

static int special_map_panscroll(Display *dpy, int argc, char **argv, unsigned long *ndata, unsigned long *data, const size_t size)
{
	if (*ndata + 1 > size) {
		fprintf(stderr, "Insufficient space to store all commands.\n");
		return 0;
	}
	data[*ndata] = AC_PANSCROLL;

	*ndata += 1;

	return 0;
}

static inline int is_valid_keyword(const char *keyword)
{
	struct keywords *kw = keywords;

	while(kw->keyword)
	{
		if (strcmp(keyword, kw->keyword) == 0)
			return 1;
		kw++;
	}
	return 0;
}

static int special_map_button(Display *dpy, int argc, char **argv, unsigned long *ndata, unsigned long *data, const size_t size)
{
	int nitems = 0;
	int i;

	for (i = 0; i < argc; i++)
	{
		char *btn = argv[i];
		int button;
		int need_press = 0, need_release = 0;

		if (strlen(btn) > 1)
		{
			if (is_valid_keyword(btn))
				break;

			switch (btn[0])
			{
				case '+': need_press = 1; break;
				case '-': need_release= 1; break;
				default:
					  need_press = need_release = 1;
					  break;
			}
		} else
			need_press = need_release = 1;

		if (sscanf(btn, "%d", &button) != 1)
			break;


		TRACE("Button map %d [%s,%s]\n", abs(button),
				need_press ?  "press" : "",
				need_release ?  "release" : "");

		if (need_press && need_release && *ndata + nitems + 2 > size) {
			fprintf(stderr, "Insufficient space to store all commands.\n");
			break;
		}
		else if ((need_press || need_release) && *ndata + nitems + 1 > size) {
			fprintf(stderr, "Insufficient space to store all commands.\n");
			break;
		}

		if (need_press)
			data[*ndata + nitems++] = AC_BUTTON | AC_KEYBTNPRESS | abs(button);
		if (need_release)
			data[*ndata + nitems++] = AC_BUTTON | abs(button);
	}

	*ndata += nitems;
	return i;
}

/**
 * Return the effective group number for a given keycode and requested
 * global group number. If the group number is in-range for the keycode,
 * the returned value will be identical. If the group number is out of
 * range, the returned value will be wrapped, clamped, or redirected as
 * necessary.
 */
static int keycode_effective_group(XkbDescPtr xkb, int keycode, int group)
{
	unsigned char info = XkbKeyGroupInfo(xkb, keycode);
	unsigned char action = XkbOutOfRangeGroupInfo(info);
	int n = XkbKeyNumGroups(xkb, keycode);

	if (n <= 0)
		return 0;
	else if (group < n && group >= 0)
		return group;
	else if (action == XkbRedirectIntoRange)
		return XkbOutOfRangeGroupNumber(info);
	else if (action == XkbClampIntoRange)
		return group < 1 ? 0 : group-1;
	else /* if XkbWrapIntoRange */
		return group % n;
}

/**
 * Return the first keycode to have the required keysym. The effective
 * group number is used durring scanning to ensure that an appropriate
 * keycode is returned even if the active group does not define all
 * keycodes.
 *
 *  TODOs:
 *  - for keysyms not on level 0, return the keycodes for the modifiers as
 *    well
 */
static int keysym_to_keycode(Display *dpy, KeySym sym)
{
	static XkbDescPtr xkb = NULL;
	XkbStateRec state;
	int kc = 0;

	if (!xkb)
		xkb = XkbGetMap(dpy, XkbAllComponentsMask, XkbUseCoreKbd);
	if (!xkb) {
		fprintf(stderr, "Warning: failed to query keyboard map\n");
		goto out;
	}
	XkbGetState(dpy, XkbUseCoreKbd, &state);

	for (kc = xkb->min_key_code; kc <= xkb->max_key_code; kc++)
	{
		int group = keycode_effective_group(xkb, kc, state.group);
		int i;

		for (i = 0; i < XkbKeyGroupWidth(xkb, kc, group); i++)
			if (XkbKeycodeToKeysym(dpy, kc, group, i) == sym)
				goto out;
	}

out:
	return kc;
}

/*
   Map gibberish like "ctrl alt f2" into the matching AC_KEY values.
   Returns 1 on success or 0 otherwise.
 */
static int special_map_keystrokes(Display *dpy, int argc, char **argv, unsigned long *ndata, unsigned long* data, const size_t size)
{
	int i;
	int nitems = 0;

	for (i = 0; i < argc; i++)
	{
		KeySym ks;
		KeyCode kc;
		int need_press = 0, need_release = 0;
		const char *key = argv[i];

		if (strlen(key) > 1)
		{
			if (is_valid_keyword(key))
				break;

			switch(key[0])
			{
				case '+':
					need_press = 1;
					key++;
					break;
				case '-':
					need_release = 1;
					key++;
					break;
				default:
					need_press = need_release = 1;
					break;
			}

			/* Function keys must be uppercased */
			if (strlen(key) > 1)
				key = convert_specialkey(key);

			if (!key || !XStringToKeysym(key))
			{
				fprintf(stderr, "Invalid key '%s'.\n", argv[i]);
				break;
			}

			if (is_modifier(key) && (need_press && need_release))
				need_release = 0;

		} else
			need_press = need_release = 1;


		ks = XStringToKeysym(key);
		if (ks == 0) {
			key = convert_specialkey(key);
			ks = XStringToKeysym(key);
			if (ks == 0)
				fprintf(stderr, "Warning: unable to map '%s' to a keysym.\n", key);
		}

		kc = keysym_to_keycode(dpy, ks);
		if (kc == 0)
			fprintf(stderr, "Warning: unable to map '%s' to a keycode.\n", key);

		if (need_press && need_release && *ndata + nitems + 2 > size) {
			fprintf(stderr, "Insufficient space to store all commands.\n");
			break;
		}
		else if ((need_press || need_release) && *ndata + nitems + 1 > size) {
			fprintf(stderr, "Insufficient space to store all commands.\n");
			break;
		}

		if (need_press)
			data[*ndata + nitems++] = AC_KEY | AC_KEYBTNPRESS | kc;
		if (need_release)
			data[*ndata + nitems++] = AC_KEY | kc;

		TRACE("Key map %lu (%d, '%s') [%s,%s]\n", ks, kc,
				XKeysymToString(ks),
				need_press ?  "press" : "",
				need_release ?  "release" : "");

	}

	*ndata += nitems;
	return i;
}

/* Join a bunch of argv into a string, then split up again at the spaces
 * into words. Returns a char** array sized *nwords, each element pointing
 * to s strdup'd string.
 * Memory to be freed by the caller.
 */
static char** strjoinsplit(int argc, char **argv, int *nwords)
{
	char **words = NULL;
	int i;

	*nwords = 0;
	for (i = 0; i < argc; i++) {
		char *tok = strtok(argv[i], " ");
		while (tok) {
			char **p = safe_realloc(words, *nwords+1, sizeof(char*));
			if (!p) {
				fprintf(stderr, "Unable to reallocate memory.\n");
				return words;
			}

			words = p;
			words[*nwords] = strdup(tok);
			if (!words[*nwords]) {
				fprintf(stderr, "Unable to allocate memory.\n");
				return words;
			}

			(*nwords)++;
			tok = strtok(NULL, " ");
		}
	}

	return words;
}

/**
 * This function parses the given strings to produce a list of actions that
 * the driver can carry out. We first combine the strings and then split
 * on spaces to produce a wordlist. Begining with the first word, we let each
 * registered keyword parser try to parse the string; if one succeeds in
 * parsing a portion, we jump ahead to the first word it could not parse
 * and repeat the process. Each parser builds up the list of actions with
 * those commands it can interpret.
 *
 * @param dpy   X11 display to query
 * @param argc  Length of argv
 * @param argv  String data to be parsed
 * @param data  Parsed action data
 * @return 'true' if the whole string was parsed sucessfully, else 'false'
 */
static Bool parse_actions(Display *dpy, int argc, char **argv, unsigned long* data, unsigned long *nitems, const size_t size)
{
	int  i = 0;
	int  nwords = 0;
	char **words = NULL;
	int n;

	/* translate cmdline commands */
	words = strjoinsplit(argc, argv, &nwords);

	if (nwords==1 && sscanf(words[0], "%d", &n) == 1)
	{ /* Mangle "simple" button maps into proper actions */
		char *nargv[1];

		for (i =  0; i < nwords; i++)
			free(words[i]);
		free(words);
		nargv[0] = alloca(32);
		sprintf(nargv[0], "button +%d", n);
		words = strjoinsplit(1, nargv, &nwords);
	}

	for (i = 0; i < nwords && *nitems < size; i++)
	{
		int j = 0;
		int keyword_found = 0;

		while (keywords[j].keyword && i < nwords && *nitems < size)
		{
			if (strcasecmp(words[i], keywords[j].keyword) == 0)
			{
				int parsed = keywords[j].func(dpy, nwords - i - 1,
							  &words[i + 1],
							  nitems, data, size);
				i += parsed;
				keyword_found = 1;

				if (parsed)
				{
					/* restart with first keyword */
					j = 0;
					continue;
				}
			}
			j++;
		}

		if (!keyword_found)
		{
			fprintf(stderr, "Cannot parse keyword '%s' at position %d\n", words[i], i+1);
			free(words);
			return False;
		}
	}

	for (i = 0; i < nwords; i++)
		free(words[i]);
	free(words);

	return True;
}

/**
 * Maps sub-properties (e.g. the 3rd button in WACOM_PROP_BUTTON_ACTIONS)
 * to actions. This function leverages the several available parsing
 * functions to convert plain-text descriptions into a list of actions
 * the driver can understand.
 *
 * Once we have a list of actions, we can store it in the appropriate
 * child property. Action atoms should be pre-created by the server,
 * so it is an error if one does not exist. To reset the action to its
 * default, we can have the parent point to '0' (which is a special
 * signal to the Wacom driver).
 *
 * @param  dpy         X display we want to query
 * @param  dev         X device we want to modify
 * @param  btnact_prop Parent property
 * @param  offset      Offset into the parent's list of child properties
 * @param  argc        Number of command line arguments we've been passed
 * @param  argv        Command line arguments we need to parse
 */
static int special_map_property(Display *dpy, XDevice *dev, Atom btnact_prop, unsigned int offset, int argc, char **argv)
{
	unsigned long *data, *btnact_data;
	Atom type, prop = 0;
	int format;
	unsigned long btnact_nitems, bytes_after;
	unsigned long nitems = 0;
	int status = EXIT_SUCCESS;

	data = calloc(256, sizeof(unsigned long));
	if (!parse_actions(dpy, argc, argv, data, &nitems, 256)) {
		status = EXIT_FAILURE;
		goto out;
	}

	/* obtain the button actions Atom */
	XGetDeviceProperty(dpy, dev, btnact_prop, 0, 100, False,
				AnyPropertyType, &type, &format, &btnact_nitems,
				&bytes_after, (unsigned char**)&btnact_data);

	if (offset >= btnact_nitems)
	{
		fprintf(stderr, "Invalid offset into %s property.\n", XGetAtomName(dpy, btnact_prop));
		status = EXIT_INVALID_USAGE;
		goto out2;
	}

	if (format != 32 || type != XA_ATOM)
	{
		fprintf(stderr, "Property '%s' in an unexpected format. This is a bug.\n",
		        XGetAtomName(dpy, btnact_prop));
		status = EXIT_FAILURE;
		goto out2;
	}

	/* set or unset the property */
	prop = btnact_data[offset];
	if (!prop)
	{
		/* The subproperty at the given offset is set to 'None',
		 * meaning the device does not support its meaning. E.g.
		 * buttons 4-7 are 'None' since the device doesn't have
		 * physical buttons relating to them.
		 */
		fprintf(stderr, "Unsupported offset into '%s' property.\n",
			XGetAtomName(dpy, btnact_prop));
		status = EXIT_INVALID_USAGE;
		goto out2;
	}

	if (nitems > 0)
	{ /* Setting an existing property */
		/* FIXME: the property containing the key sequence must be
		 * set before updating the button action properties */
		XChangeDeviceProperty(dpy, dev, prop, XA_INTEGER, 32,
					PropModeReplace,
					(unsigned char*)data, nitems);

		XChangeDeviceProperty(dpy, dev, btnact_prop, XA_ATOM, 32,
						PropModeReplace,
						(unsigned char*)btnact_data,
						btnact_nitems);
	}
	else if (prop)
	{ /* Unsetting a property that exists */
		btnact_data[offset] = 0;

		XChangeDeviceProperty(dpy, dev, btnact_prop, XA_ATOM, 32,
					PropModeReplace,
					(unsigned char*)btnact_data,
					btnact_nitems);
	}

	XFlush(dpy);
out2:
	XFree(btnact_data);
out:
	free(data);
	return status;
}

/**
 * Maps "actions" to certain properties. Actions allow for complex tasks to
 * be performed when the driver recieves certain events. For example you
 * could have an action of "key +alt f2" to open the run-application dialog
 * in Gnome, or "button 4 4 4 4 4" to have applications scroll by 5 lines
 * instead of 1.
 *
 * Buttons, wheels, and strips all support actions. Note that button actions
 * require the button to modify as the first argument to this function.
 *
 * @param dpy   X11 display to query
 * @param dev   Device to modify
 * @param param Info about parameter to modify
 * @param argc  Size of argv
 * @param argv  Arguments to parse
 */
static int map_actions(Display *dpy, XDevice *dev, param_t* param, int argc, char **argv)
{
	Atom action_prop;
	unsigned int offset = param->prop_offset;

	TRACE("Mapping %s for device %lu.\n", param->name, dev->device_id);

	action_prop = XInternAtom(dpy, param->prop_name, True);
	if (!action_prop)
	{
		fprintf(stderr, "Unable to locate property '%s'\n", param->prop_name);
		return EXIT_FAILURE;
	}

	if (argc < param->arg_count)
	{
		fprintf(stderr, "Too few arguments provided.\n");
		return EXIT_INVALID_USAGE;
	}

	if (strcmp(param->prop_name, WACOM_PROP_BUTTON_ACTIONS) == 0)
	{
		if (sscanf(argv[0], "%u", &offset) != 1 || offset <= 0)
		{
			fprintf(stderr, "'%s' is not a valid button number.\n", argv[0]);
			return EXIT_INVALID_USAGE;
		}

		offset--;        /* Property is 0-indexed, X buttons are 1-indexed */
		argc--;          /* Trim off the target button argument */
		argv = &argv[1]; /* ... ditto ... */
	}

	return special_map_property(dpy, dev, action_prop, offset, argc, argv);
}

static int set_xydefault(Display *dpy, XDevice *dev, param_t* param, int argc, char **argv)
{
	Atom prop, type;
	int format;
	unsigned char* data = NULL;
	unsigned long nitems, bytes_after;
	long *ldata;
	int status = EXIT_SUCCESS;

	if (argc != param->arg_count)
	{
		fprintf(stderr, "'%s' requires exactly %d value(s).\n", param->name,
			param->arg_count);
		return EXIT_INVALID_USAGE;
	}

	prop = XInternAtom(dpy, param->prop_name, True);
	if (!prop)
	{
		fprintf(stderr, "Property for '%s' not available.\n",
			param->name);
		return EXIT_FAILURE;
	}

	XGetDeviceProperty(dpy, dev, prop, 0, 1000, False, AnyPropertyType,
				&type, &format, &nitems, &bytes_after, &data);

	if (nitems <= param->prop_offset)
	{
		fprintf(stderr, "Property offset doesn't exist, this is a bug.\n");
		status = EXIT_FAILURE;
		goto out;
	}

	ldata = (long*)data;
	ldata[0] = -1;
	ldata[1] = -1;
	ldata[2] = -1;
	ldata[3] = -1;

	XChangeDeviceProperty(dpy, dev, prop, type, format,
				PropModeReplace, data, nitems);
	XFlush(dpy);
out:
	XFree(data);
	return status;
}

static int set_mode(Display *dpy, XDevice *dev, param_t* param, int argc, char **argv)
{
	int mode = Absolute;

	if (argc != param->arg_count)
	{
		fprintf(stderr, "'%s' requires exactly %d value(s).\n", param->name,
			param->arg_count);
		return EXIT_INVALID_USAGE;
	}

	TRACE("Set mode '%s' for device %lu.\n", argv[0], dev->device_id);

	if (strcasecmp(argv[0], "Relative") == 0)
		mode = Relative;
	else if (strcasecmp(argv[0], "Absolute") == 0)
		mode = Absolute;
	else
	{
		fprintf(stderr, "Invalid device mode. Use 'Relative' or 'Absolute'.\n");
		return EXIT_INVALID_USAGE;
	}


	XSetDeviceMode(dpy, dev, mode);
	XFlush(dpy);
	return EXIT_SUCCESS;
}

static int set_rotate(Display *dpy, XDevice *dev, param_t* param, int argc, char **argv)
{
	int rotation = 0;
	Atom prop, type;
	int format;
	unsigned char* data;
	unsigned long nitems, bytes_after;
	int status = EXIT_SUCCESS;

	if (argc != param->arg_count)
	{
		fprintf(stderr, "'%s' requires exactly %d value(s).\n", param->name,
			param->arg_count);
		return EXIT_INVALID_USAGE;
	}

	TRACE("Rotate '%s' for device %lu.\n", argv[0], dev->device_id);

	if (strcasecmp(argv[0], "cw") == 0 || strcasecmp(argv[0], "1") == 0)
		rotation = 1;
	else if (strcasecmp(argv[0], "ccw") == 0 || strcasecmp(argv[0], "2") == 0)
		rotation = 2;
	else if (strcasecmp(argv[0], "half") == 0 || strcasecmp(argv[0], "3") == 0)
		rotation = 3;
	else if (strcasecmp(argv[0], "none") == 0 || strcasecmp(argv[0], "0") == 0)
		rotation = 0;
	else
	{
		fprintf(stderr, "'%s' is not a valid value for the '%s' property.\n",
		        argv[0], param->name);
		return EXIT_INVALID_USAGE;
	}

	prop = XInternAtom(dpy, param->prop_name, True);
	if (!prop)
	{
		fprintf(stderr, "Property for '%s' not available.\n",
			param->name);
		return EXIT_FAILURE;
	}

	XGetDeviceProperty(dpy, dev, prop, 0, 1000, False, AnyPropertyType,
				&type, &format, &nitems, &bytes_after, &data);

	if (nitems == 0 || format != 8)
	{
		fprintf(stderr, "Property for '%s' has no or wrong value - this is a bug.\n",
			param->name);
		status = EXIT_FAILURE;
		goto out;
	}

	*data = rotation;
	XChangeDeviceProperty(dpy, dev, prop, type, format,
				PropModeReplace, data, nitems);
	XFlush(dpy);
out:
	XFree(data);
	return status;
}


/**
 * Performs intelligent string->int conversion. In addition to converting strings
 * of digits into their corresponding integer values, it converts special string
 * constants such as "off" or "false" (0) and "on" or "true" (1).
 *
 * The caller is expected to allocate and free memory for return_value.
 *
 * @param      param        the property paramaters
 * @param      value        the string to be converted
 * @param[out] return_value the integer representation of the 'value' parameter
 * @return TRUE if the conversion succeeded, FALSE otherwise
 */
static Bool convert_value_from_user(const param_t *param, const char *value, int *return_value)
{
	if (param->prop_flags & PROP_FLAG_BOOLEAN)
	{
		if (strcasecmp(value, "off") == 0 || strcasecmp(value, "false") == 0)
			*return_value = 0;
		else if (strcasecmp(value, "on") == 0 || strcasecmp(value, "true") == 0)
			*return_value = 1;
		else
			return False;

		if (param->prop_flags & PROP_FLAG_INVERTED)
			*return_value = !(*return_value);
	}
	else if (param->prop_flags & PROP_FLAG_OUTPUT)
	{
		const char *prefix = "HEAD-";
		/* We currently support HEAD-X, where X is 0-9 */
		if (strlen(value) != strlen(prefix) + 1 ||
		    strncasecmp(value, prefix, strlen(prefix)) != 0)
			return False;

		*return_value = value[strlen(prefix)] - '0';
	} else
	{
		char *end;
		long conversion = strtol(value, &end, 10);
		if (end == value || *end != '\0' || errno == ERANGE ||
		    conversion < INT_MIN || conversion > INT_MAX)
			return False;

		*return_value = (int)conversion;
	}

	return True;
}

static int set(Display *dpy, int argc, char **argv)
{
	param_t *param;
	XDevice *dev = NULL;
	Atom prop = None, type;
	int format;
	unsigned char* data = NULL;
	unsigned long nitems, bytes_after;
	long *n;
	char *b;
	int i;
	char **values = NULL;
	int nvals = 0;
	int status = EXIT_SUCCESS;

	if (argc < 2)
	{
		usage();
		return EXIT_INVALID_USAGE;
	}

	TRACE("'set' requested for '%s'.\n", argv[0]);

	dev = find_device(dpy, argv[0]);
	if (!dev)
	{
		fprintf(stderr, "Cannot find device '%s'.\n", argv[0]);
		return EXIT_FAILURE;
	}

	param = find_parameter(argv[1]);
	if (!param)
	{
		status = EXIT_FAILURE;
		if (is_deprecated_parameter(argv[1]))
			goto out;
		fprintf(stderr, "Unknown parameter name '%s'.\n", argv[1]);
		goto out;
	} else if (param->prop_flags & PROP_FLAG_READONLY)
	{
		fprintf(stderr, "'%s' is a read-only option.\n", argv[1]);
		status = EXIT_FAILURE;
		goto out;
	}

	if (param->prop_name)
	{
		prop = XInternAtom(dpy, param->prop_name, True);
		if (!prop || !test_property(dpy, dev, prop))
		{
			fprintf(stderr, "Property '%s' does not exist on device.\n",
				param->prop_name);
			status = EXIT_FAILURE;
			goto out;
		}
	}

	if (param->set_func)
	{
		param->set_func(dpy, dev, param, argc - 2, &argv[2]);
		goto out;
	}

	XGetDeviceProperty(dpy, dev, prop, 0, 1000, False, AnyPropertyType,
				&type, &format, &nitems, &bytes_after, &data);

	if (nitems <= param->prop_offset)
	{
		fprintf(stderr, "Property offset doesn't exist.\n");
		status = EXIT_FAILURE;
		goto out;
	}

	values = strjoinsplit(argc - 2, &argv[2], &nvals);

	if (nvals != param->arg_count)
	{
		fprintf(stderr, "'%s' requires exactly %d value(s).\n", param->name,
			param->arg_count);
		status = EXIT_INVALID_USAGE;
		goto out;
	}

	for (i = 0; i < nvals; i++)
	{
		Bool success;
		int val;

		success = convert_value_from_user(param, values[i], &val);
		if (!success)
		{
			fprintf(stderr, "'%s' is not a valid value for the '%s' property.\n",
				values[i], param->name);
			status = EXIT_INVALID_USAGE;
			goto out;
		}

		switch(param->prop_format)
		{
			case 8:
				if (format != param->prop_format || type != XA_INTEGER) {
					fprintf(stderr, "   %-23s = format mismatch (%d)\n",
							param->name, format);
					break;
				}
				b = (char*)data;
				b[param->prop_offset + i] = rint(val);
				break;
			case 32:
				if (format != param->prop_format || type != XA_INTEGER) {
					fprintf(stderr, "   %-23s = format mismatch (%d)\n",
							param->name, format);
					break;
				}
				n = (long*)data;
				n[param->prop_offset + i] = rint(val);
				break;
		}
	}

	XChangeDeviceProperty(dpy, dev, prop, type, format,
				PropModeReplace, data, nitems);
	XFlush(dpy);

out:
	for (i = 0; i < nvals; i++)
		free(values[i]);
	free(values);
	XCloseDevice(dpy, dev);
	XFree(data);
	return status;
}

static int get_mode(Display *dpy, XDevice *dev, param_t* param, int argc, char **argv)
{
	XDeviceInfo *info, *d = NULL;
	int ndevices, i;
	XValuatorInfoPtr v;

	info = XListInputDevices(dpy, &ndevices);

	for (i = 0; i < ndevices; i++)
	{
		d = &info[i];
		if (d->id == dev->device_id)
			break;
	}

	if (i >= ndevices)
	{
		fprintf(stderr, "Unable to locate device.\n");
		return EXIT_FAILURE;
	}

	TRACE("Getting mode for device %lu.\n", dev->device_id);

	v = (XValuatorInfoPtr)d->inputclassinfo;
	for (i = 0; i < d->num_classes; i++)
	{
		if (v->class == ValuatorClass)
		{
			print_value(param, "%s", (v->mode == Absolute) ? "Absolute" : "Relative");
			break;
		}
		v = (XValuatorInfoPtr)((char*)v + v->length);
	}

	XFreeDeviceList(info);
	return EXIT_SUCCESS;
}

static int get_rotate(Display *dpy, XDevice *dev, param_t* param, int argc, char **argv)
{
	const char *rotation = NULL;
	Atom prop, type;
	int format;
	unsigned char* data;
	unsigned long nitems, bytes_after;
	int status = EXIT_SUCCESS;

	if (argc != 0)
	{
		fprintf(stderr, "Incorrect number of arguments supplied.\n");
		return EXIT_INVALID_USAGE;
	}

	prop = XInternAtom(dpy, param->prop_name, True);
	if (!prop)
	{
		fprintf(stderr, "Property for '%s' not available.\n",
			param->name);
		return EXIT_FAILURE;
	}

	TRACE("Getting rotation for device %lu.\n", dev->device_id);

	XGetDeviceProperty(dpy, dev, prop, 0, 1000, False, AnyPropertyType,
				&type, &format, &nitems, &bytes_after, &data);

	if (nitems == 0 || format != 8)
	{
		fprintf(stderr, "Property for '%s' has no or wrong value - this is a bug.\n",
			param->name);
		status = EXIT_FAILURE;
		goto out;
	}

	switch(*data)
	{
		case 0:
			rotation = "none";
			break;
		case 1:
			rotation = "cw";
			break;
		case 2:
			rotation = "ccw";
			break;
		case 3:
			rotation = "half";
			break;
	}

	print_value(param, "%s", rotation);

out:
	XFree(data);
	return status;
}

/**
 * Try to print the value of the action mapped to the given parameter's
 * property. If the property contains data in the wrong format/type then
 * nothing will be printed.
 *
 * @param dpy    X11 display to connect to
 * @param dev    Device to query
 * @param param  Info about parameter to query
 * @param offset Offset into property specified in param
 * @return       0 on failure, 1 otherwise
 */
static int get_actions(Display *dpy, XDevice *dev,
				  param_t *param, unsigned int offset)
{
	Atom prop, type;
	int format;
	unsigned long nitems, bytes_after, *data;
	char buff[1024] = {0};
	int last_type;

	prop = XInternAtom(dpy, param->prop_name, True);

	if (!prop)
		return 0;

	XGetDeviceProperty(dpy, dev, prop, 0, 100, False,
			   AnyPropertyType, &type, &format, &nitems,
			   &bytes_after, (unsigned char**)&data);

	if (offset >= nitems)
	{
		XFree(data);
		return 0;
	}

	prop = data[offset];
	XFree(data);

	if (format != 32 || type != XA_ATOM || !prop)
	{
		return 0;
	}

	XGetDeviceProperty(dpy, dev, prop, 0, 100, False,
		   AnyPropertyType, &type, &format, &nitems,
		   &bytes_after, (unsigned char**)&data);

	last_type = 0;
	for (unsigned int i = 0; i < nitems; i++)
	{
		unsigned long action = data[i];
		int current_type;
		int detail;
		int is_press = -1;
		char str[32] = {0};
		char press_str = ' ';

		current_type = action & AC_TYPE;
		detail = action & AC_CODE;


		switch (current_type)
		{
			case AC_KEY:
				if (last_type != current_type)
					strcat(buff, "key ");
				is_press = !!(action & AC_KEYBTNPRESS);
				detail = XkbKeycodeToKeysym(dpy, detail, 0, 0);
				break;
			case AC_BUTTON:
				if (last_type != current_type)
					strcat(buff, "button ");
				is_press = !!(action & AC_KEYBTNPRESS);
				break;
			case AC_MODETOGGLE:
				strcat(buff, "modetoggle ");
				break;
			case AC_DISPLAYTOGGLE:
				strcat(buff, "displaytoggle ");
				break;
			default:
				TRACE("unknown type %d\n", current_type);
				continue;
		}

		press_str = (is_press == -1) ? ' ' : ((is_press) ?  '+' : '-');
		if (current_type == AC_KEY)
			sprintf(str, "%c%s ", press_str,
				XKeysymToString(detail));
		else if (current_type == AC_BUTTON)
			sprintf(str, "%c%d ", press_str, detail);
		strcat(buff, str);
		last_type = current_type;
	}

	TRACE("%s\n", buff);

	XFree(data);

	if (param->printformat == FORMAT_XORG_CONF)
		fprintf(stderr, "%s: Actions are not supported by xorg.conf. Try shell format (-s) instead.\n", param->name);
	else
		print_button_value(param, offset, "%s", buff);

	return 1;
}

/**
 * Try to print the value of the raw button mapped to the given parameter's
 * property. If the property contains data in the wrong format/type then
 * nothing will be printed.
 *
 * @param dpy    X11 display to connect to
 * @param dev    Device to query
 * @param param  Info about parameter to query
 * @param offset Offset into the property specified in param
 * @return       0 on failure, 1 otherwise
 */
static int get_button(Display *dpy, XDevice *dev, param_t *param, unsigned int offset)
{
	Atom prop, type;
	int format;
	unsigned long nitems, bytes_after;
	unsigned char *data;

	prop = XInternAtom(dpy, param->prop_name, True);

	if (!prop)
		return 0;

	XGetDeviceProperty(dpy, dev, prop, 0, 100, False,
			   AnyPropertyType, &type, &format, &nitems,
			   &bytes_after, (unsigned char**)&data);

	if (offset >= nitems)
	{
		XFree(data);
		return 0;
	}

	prop = data[offset];
	XFree(data);

	if (format != 8 || type != XA_INTEGER || !prop)
	{
		return 0;
	}

	print_button_value(param, offset, "%lu", prop);

	return 1;
}

/**
 * Print the current button/wheel/strip mapping, be it a raw button or
 * an action. Button map requests require the button number as the first
 * argument in argv.
 *
 * @param dpy   X11 display to connect to
 * @param dev   Device to query
 * @param param Info about parameter to query
 * @param argc  Length of argv
 * @param argv  Command-line arguments
 */
static int get_map(Display *dpy, XDevice *dev, param_t *param, int argc, char** argv)
{
	int offset = param->prop_offset;

	TRACE("Getting button map for device %lu.\n", dev->device_id);

	if (argc != param->arg_count)
	{
		fprintf(stderr, "'%s' requires exactly %d value(s).\n", param->name,
		        param->arg_count);
		return EXIT_INVALID_USAGE;
	}

	if (strcmp(param->prop_name, WACOM_PROP_BUTTON_ACTIONS) == 0)
	{
		if (sscanf(argv[0], "%d", &offset) != 1 || offset <= 0)
		{
			fprintf(stderr, "'%s' is not a valid button number.\n", argv[0]);
			return EXIT_INVALID_USAGE;
		}

		offset--;        /* Property is 0-indexed, X buttons are 1-indexed */
		argc--;          /* Trim off the target button argument */
		argv = &argv[1]; /*... ditto ...                        */
	}


	if (get_actions(dpy, dev, param, offset))
		return EXIT_SUCCESS;
	else if (get_button(dpy, dev, param, offset))
		return EXIT_SUCCESS;
	else
	{
		int nmap = 256;
		unsigned char map[nmap];

		nmap = XGetDeviceButtonMapping(dpy, dev, map, nmap);

		if (offset >= nmap)
		{
			fprintf(stderr, "Button number does not exist on device.\n");
			return EXIT_FAILURE;
		}

		print_value(param, "%d", map[offset]);

		XSetDeviceButtonMapping(dpy, dev, map, nmap);
		XFlush(dpy);
		return EXIT_SUCCESS;
	}
}

/**
 * Determine if we need to use fall back to Xinerama, or if the RandR
 * extension will work OK. We depend on RandR 1.3 or better in order
 * to work.
 *
 * A server bug causes the NVIDIA driver to report RandR 1.3 support
 * despite not exposing RandR CRTCs. We need to fall back to Xinerama
 * for this case as well.
 *
 * @param Display  X11 display to connect to
 * @return         True if the Xinerama should be used instead of RandR
 */
static Bool need_xinerama(Display *dpy)
{
	int opcode, event, error;
	int maj, min;

	if (!XQueryExtension(dpy, "RANDR", &opcode, &event, &error) ||
	    !XRRQueryVersion(dpy, &maj, &min) || (maj * 1000 + min) < 1002 ||
	    XQueryExtension(dpy, "NV-CONTROL", &opcode, &event, &error))
	{
		TRACE("RandR extension not found, too old, or NV-CONTROL "
			"extension is also present.\n");
		return True;
	}

	return False;
}

/**
 * Uses the area of the desktop and the server's transformation
 * matrix to calculate the dimensions and location of the area
 * the given device is mapped to. If the matrix describes a
 * non-rectangular transform (e.g. rotation or shear), this
 * function returns False.
 *
 * @param dpy          X11 display to connect to
 * @param dev          Device to query
 * @param width[out]   Width of the mapped area
 * @param height[out]  Height of the mapped area
 * @param x_org[out]   Offset from the desktop origin to the mapped area's left edge
 * @param y_org[out]   Offset from the desktop origin to the mapped area's top edge
 * @return             True if the function could determine the mapped area
 */
static Bool get_mapped_area(Display *dpy, XDevice *dev, int *width, int *height, int *x_org, int *y_org)
{
	Atom matrix_prop = XInternAtom(dpy, "Coordinate Transformation Matrix", True);
	Atom type;
	int format;
	unsigned long nitems, bytes_after;
	unsigned long *data;
	float matrix[9];
	Bool matrix_is_valid = True;

	int display_width = DisplayWidth(dpy, DefaultScreen(dpy));
	int display_height = DisplayHeight(dpy, DefaultScreen(dpy));
	TRACE("Desktop width: %d, height: %d\n", display_width, display_height);

	if (!matrix_prop)
	{
		fprintf(stderr, "Server does not support transformation\n");
		return False;
	}

	XGetDeviceProperty(dpy, dev, matrix_prop, 0, 9, False,
	                   AnyPropertyType, &type, &format, &nitems,
	                   &bytes_after, (unsigned char**)&data);

	if (format != 32 || type != XInternAtom(dpy, "FLOAT", True) || nitems != 9)
	{
		fprintf(stderr,"Property for '%s' has unexpected type - this is a bug.\n",
			"Coordinate Transformation Matrix");
		XFree(data);
		return False;
	}

	/* XI1 stores 32 bit properties (including float) as long,
	 * regardless of architecture */
	for (size_t i = 0; i < ARRAY_SIZE(matrix); i++) {
		matrix[i] = *(float*)(&data[i]);
	}

	TRACE("Current transformation matrix:\n");
	TRACE("	[ %f %f %f ]\n", matrix[0], matrix[1], matrix[2]);
	TRACE("	[ %f %f %f ]\n", matrix[3], matrix[4], matrix[5]);
	TRACE("	[ %f %f %f ]\n", matrix[6], matrix[7], matrix[8]);

	*width  = rint(display_width  * matrix[0]);
	*x_org  = rint(display_width  * matrix[2]);
	*height = rint(display_height * matrix[4]);
	*y_org  = rint(display_height * matrix[5]);
	if ((matrix[1] != 0 || matrix[3] != 0 || matrix[6] != 0 || matrix[7] != 0) ||
	    (matrix[8] != 1)) {
		fprintf(stderr, "Non-rectangular transformation matrix detected.\n");
		matrix_is_valid = False;
	}

	XFree(data);

	return matrix_is_valid;
}

/**
 * Modifies the server's transformation matrix property for the given
 * device. It takes as input a 9-element array of floats interpreted
 * as the row-major 3x3 matrix to be set.
 *
 * @param dpy      X11 display to connect to
 * @param dev      Device to query
 * @param fmatrix  A row-major 3x3 transformation matrix
 * @return         True if the transformation matrix was successfully modified
 */
static Bool _set_matrix_prop(Display *dpy, XDevice *dev, const float fmatrix[9])
{
	Atom matrix_prop = XInternAtom(dpy, "Coordinate Transformation Matrix", True);
	Atom type;
	int format;
	unsigned long nitems, bytes_after;
	float *data;
	long matrix[9] = {0};

	if (!matrix_prop)
	{
		fprintf(stderr, "Server does not support transformation\n");
		return False;
	}

	/* XI1 expects 32 bit properties (including float) as long,
	 * regardless of architecture */

	for (size_t i = 0; i < ARRAY_SIZE(matrix); i++) {
		*(float*)(matrix + i) = fmatrix[i];
	}

	XGetDeviceProperty(dpy, dev, matrix_prop, 0, 9, False,
				AnyPropertyType, &type, &format, &nitems,
				&bytes_after, (unsigned char**)&data);

	if (format != 32 || type != XInternAtom(dpy, "FLOAT", True))
	{
		fprintf(stderr, "Property for '%s' has unexpected type - this is a bug.\n",
			"Coordinate Transformation Matrix");
		return False;
	}

	XChangeDeviceProperty(dpy, dev, matrix_prop, type, format,
			      PropModeReplace, (unsigned char*)matrix, 9);
	XFree(data);
	XFlush(dpy);

	return True;
}

/**
 * Adjust the transformation matrix based on a user-defined area.
 * This function will attempt to map the given pointer to an arbitrary
 * rectangular portion of the desktop.
 *
 * @param dpy            X11 display to connect to
 * @param dev            Device to query
 * @param offset_x       Offset of output area's left edge from desktop origin
 * @param offset_y       Offset of output area's top edge from desktop origin
 * @param output_width   Width of output area
 * @param output_height  Height of output area
 * @return               True if the transformation matrix was successfully modified
 */
static Bool set_output_area(Display *dpy, XDevice *dev,
			int offset_x, int offset_y,
			int output_width, int output_height)
{
	int width = DisplayWidth(dpy, DefaultScreen(dpy));
	int height = DisplayHeight(dpy, DefaultScreen(dpy));

	/* offset */
	float x = 1.0 * offset_x/width;
	float y = 1.0 * offset_y/height;

	/* mapping */
	float w = 1.0 * output_width/width;
	float h = 1.0 * output_height/height;

	float matrix[9] = { 1, 0, 0,
			    0, 1, 0,
			    0, 0, 1};
	matrix[2] = x;
	matrix[5] = y;
	matrix[0] = w;
	matrix[4] = h;

	TRACE("Remapping to output area %dx%d @ %d,%d.\n", output_width,
		      output_height, offset_x, offset_y);

	TRACE("Transformation matrix:\n");
	TRACE("	[ %f %f %f ]\n", matrix[0], matrix[1], matrix[2]);
	TRACE("	[ %f %f %f ]\n", matrix[3], matrix[4], matrix[5]);
	TRACE("	[ %f %f %f ]\n", matrix[6], matrix[7], matrix[8]);

	return _set_matrix_prop(dpy, dev, matrix);
}


/**
 * Adjust the transformation matrix based on RandR settings. This function
 * will attempt to map the given device to the output with the given RandR
 * output name.
 *
 * @param dpy          X11 display to connect to
 * @param dev          Device to query
 * @param output_name  Name of the RandR output to map to
 * @return             True if the transformation matrix was successfully modified
 */
static Bool set_output_xrandr(Display *dpy, XDevice *dev, char *output_name)
{
	int i, success = 0;
	int x, y, width, height;
	XRRScreenResources *res;
	XRROutputInfo *output_info;
	XRRCrtcInfo *crtc_info;

	res = XRRGetScreenResources(dpy, DefaultRootWindow(dpy));
	for (i = 0; i < res->noutput; i++)
	{
		output_info = XRRGetOutputInfo(dpy, res, res->outputs[i]);

		TRACE("Found output '%s' (%s)\n", output_info->name,
		      output_info->connection == RR_Connected ? "connected" : "disconnnected");

		if (!output_info->crtc || output_info->connection != RR_Connected)
			continue;

		crtc_info = XRRGetCrtcInfo (dpy, res, output_info->crtc);
		x = crtc_info->x;
		y = crtc_info->y;
		width = crtc_info->width;
		height = crtc_info->height;
		XRRFreeCrtcInfo(crtc_info);

		TRACE("CRTC (%dx%d) %dx%d\n", x, y, width, height);

		if (strcmp(output_info->name, output_name) == 0)
		{
			TRACE("Setting CRTC %s\n", output_name);
			success = set_output_area(dpy, dev, x, y, width, height);
			break;
		}
	}
	XRRFreeScreenResources(res);

	if (!success)
		fprintf(stderr, "Unable to find output '%s'. "
			"Output may not be connected.\n", output_name);

	return success;
}

/**
 * Adjust the transformation matrix based on the Xinerama settings. This
 * function will attempt to map the given device to the specified Xinerama
 * head number.
 *
 * For TwinView This would better be done with libXNVCtrl but until they
 * learn to package it properly, we need to rely on Xinerama. Besides,
 * libXNVCtrl isn't available on RHEL, so we'd have to do it through
 * Xinerama there anyway.
 *
 * @param dpy   X11 display to connect to
 * @param dev   Device to query
 * @param head  Index of Xinerama head to map to
 * @return      True if the transformation matrix was successfully modified
 */
static Bool set_output_xinerama(Display *dpy, XDevice *dev, int head)
{
	int event, error;
	XineramaScreenInfo *screens;
	int nscreens;
	Bool success = False;

	if (!XineramaQueryExtension(dpy, &event, &error))
	{
		fprintf(stderr, "Unable to set screen mapping. Xinerama extension not found\n");
		return success;
	}

	screens = XineramaQueryScreens(dpy, &nscreens);

	if (nscreens == 0)
	{
		fprintf(stderr, "Xinerama failed to query screens.\n");
		goto out;
	} else if (nscreens <= head)
	{
		fprintf(stderr, "Found %d screens, but you requested number %d.\n",
				nscreens, head);
		goto out;
	}

	TRACE("Setting xinerama head %d\n", head);

	success = set_output_area(dpy, dev,
		    screens[head].x_org, screens[head].y_org,
		    screens[head].width, screens[head].height);

out:
	XFree(screens);
	return success;
}

/**
 * Adjust the transformation matrix based on the desktop size.
 * This function will attempt to map the given device to the entire
 * desktop.
 *
 * @param dpy  X11 display to connect to
 * @param dev  Device to query
 * @return     True if the transformation matrix was successfully modified
 */
static Bool set_output_desktop(Display *dpy, XDevice *dev)
{
	int display_width = DisplayWidth(dpy, DefaultScreen(dpy));
	int display_height = DisplayHeight(dpy, DefaultScreen(dpy));

	return set_output_area(dpy, dev, 0, 0, display_width, display_height);
}

/**
 * Adjust the transformation matrix based on its current value. This
 * function will attempt to map the given device to the next output
 * exposed in the list of Xinerama heads. If not mapped to a Xinerama
 * head, it maps to the first head. If mapped to the last Xinerama
 * head, it maps to the entire desktop.
 *
 * @param dpy  X11 display to connect to
 * @param dev  Device to query
 * @return     True if the transformation matrix was successfully modified
 */
static Bool set_output_next(Display *dpy, XDevice *dev)
{
	XineramaScreenInfo *screens;
	int event, error, nscreens, head;
	int width, height, x_org, y_org;
	Bool success = False;

	if (!get_mapped_area(dpy, dev, &width, &height, &x_org, &y_org))
		return success;

	if (!XineramaQueryExtension(dpy, &event, &error))
	{
		fprintf(stderr, "Unable to get screen mapping. Xinerama extension not found\n");
		return success;
	}

	screens = XineramaQueryScreens(dpy, &nscreens);

	if (nscreens == 0)
	{
		fprintf(stderr, "Xinerama failed to query screens.\n");
		goto out;
	}

	TRACE("Remapping to next available output.\n");
	for (head = 0; head < nscreens && !success; head++)
	{
		if (screens[head].width == width && screens[head].height == height &&
		    screens[head].x_org == x_org && screens[head].y_org  == y_org)
		{
			if (head + 1 < nscreens)
				success = set_output_xinerama(dpy, dev, head+1);
			else
				success = set_output_desktop(dpy, dev);

			if (!success)
				goto out;
		}
	}

	if (!success)
		success = set_output_xinerama(dpy, dev, 0);

out:
	XFree(screens);
	return success;
}

static int set_output(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv)
{
	int head_no;
	int x, y, flags;
	unsigned int width, height;
	Bool success = false;
	if (argc != param->arg_count)
	{
		fprintf(stderr, "'%s' requires exactly %d value(s).\n", param->name,
			param->arg_count);
		return EXIT_INVALID_USAGE;
	}

	flags = XParseGeometry(argv[0], &x, &y, &width, &height);

	if (MaskIsSet(flags, XValue|YValue|WidthValue|HeightValue))
		success = set_output_area(dpy, dev, x, y, width, height);
	else if (strcasecmp(argv[0], "next") == 0)
		success = set_output_next(dpy, dev);
	else if (strcasecmp(argv[0], "desktop") == 0)
		success = set_output_desktop(dpy, dev);
	else if (!need_xinerama(dpy))
		success = set_output_xrandr(dpy, dev, argv[0]);
	else if  (convert_value_from_user(param, argv[0], &head_no))
		success = set_output_xinerama(dpy, dev, head_no);
	else
	{
		fprintf(stderr, "Unable to find an output '%s'.\n", argv[0]);
		return EXIT_FAILURE;
	}

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}


static int get_all(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv)
{
	param_t *p = parameters;
	int status = EXIT_FAILURE, success = 1;

	if (param->printformat == FORMAT_DEFAULT)
		param->printformat = FORMAT_XORG_CONF;

	while(p->name)
	{
		if (p != param && !(p->prop_flags & PROP_FLAG_WRITEONLY))
		{
			p->device_name = param->device_name;
			p->printformat = param->printformat;

			if (p->prop_name && strcmp(p->prop_name, WACOM_PROP_BUTTON_ACTIONS) == 0)
			{
				int i;
				int nmap = 256;
				unsigned char map[nmap];

				nmap = XGetDeviceButtonMapping(dpy, dev, map, nmap);
				for (i = 1; i <= nmap; i++)
				{
					char tmparg[16];
					char *tmpargv = &tmparg[0];

					if (i > 3 && i < 8)
						continue;
					sprintf(tmparg, "%d", i);
					success = get_param(dpy, dev, p, 1, &tmpargv);
				}
			}
			else
				success = get_param(dpy, dev, p, argc, argv);
		}
		p++;
		if(success == 0)
			status = EXIT_SUCCESS;
	}
	return status;
}

static int get(Display *dpy, enum printformat printformat, int argc, char **argv)
{
	param_t *param;
	XDevice *dev = NULL;
	int status = EXIT_SUCCESS;

	if (argc < 2)
	{
		usage();
		return EXIT_INVALID_USAGE;
	}

	TRACE("'get' requested for '%s'.\n", argv[0]);

	dev = find_device(dpy, argv[0]);
	if (!dev)
	{
		fprintf(stderr, "Cannot find device '%s'.\n", argv[0]);
		return EXIT_FAILURE;
	}

	param = find_parameter(argv[1]);
	if (!param)
	{
		if (is_deprecated_parameter(argv[1]))
			return EXIT_FAILURE;
		fprintf(stderr, "Unknown parameter name '%s'.\n", argv[1]);
		status = EXIT_FAILURE;
		goto out;
	} else if (param->prop_flags & PROP_FLAG_WRITEONLY)
	{
		fprintf(stderr, "'%s' is a write-only option.\n", argv[1]);
		status = EXIT_FAILURE;
		goto out;
	} else
	{
		param->printformat = printformat;
		param->device_name = argv[0];
	}

	status = get_param(dpy, dev, param, argc - 2, &argv[2]);

out:
	XCloseDevice(dpy, dev);
	return status;
}

static int get_param(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv)
{
	Atom prop = None, type;
	int format;
	unsigned char* data;
	unsigned long nitems, bytes_after;
	int i;
	char str[100] = {0};
	int status = EXIT_SUCCESS;

	if (param->prop_name)
	{
		prop = XInternAtom(dpy, param->prop_name, True);
		if (!prop || !test_property(dpy, dev, prop))
		{
			fprintf(stderr, "Property '%s' does not exist on device.\n",
				param->prop_name);
			return EXIT_FAILURE;
		}
	}

	if (param->get_func)
	{
		TRACE("custom get func for param\n");
		return param->get_func(dpy, dev, param, argc, argv);
	}


	TRACE("Getting property %lu, offset %u\n", prop, param->prop_offset);
	XGetDeviceProperty(dpy, dev, prop, 0, 1000, False, AnyPropertyType,
				&type, &format, &nitems, &bytes_after, &data);

	if (nitems <= param->prop_offset)
	{
		fprintf(stderr, "Property offset doesn't exist.\n");
		status = EXIT_FAILURE;
		goto out;
	}


	switch(param->prop_format)
	{
		case 8:
			for (i = 0; i < param->arg_count; i++)
			{
				int val = data[param->prop_offset + i];

				if (param->prop_flags & PROP_FLAG_BOOLEAN)
					if (param->prop_flags & PROP_FLAG_INVERTED)
						sprintf(&str[strlen(str)], "%s", val ?  "off" : "on");
					else
						sprintf(&str[strlen(str)], "%s", val ?  "on" : "off");
				else
					sprintf(&str[strlen(str)], "%d", val);

				if (i < param->arg_count - 1)
					strcat(str, " ");
			}
			print_value(param, "%s", str);
			break;
		case 32:
			for (i = 0; i < param->arg_count; i++)
			{
				long *ldata = (long*)data;
				sprintf(&str[strlen(str)], "%ld", ldata[param->prop_offset + i]);

				if (i < param->arg_count - 1)
					strcat(str, " ");
			}
			print_value(param, "%s", str);
			break;
	}

out:
	XFree(data);
	return status;
}


#ifndef ENABLE_TESTS

#ifdef BUILD_FUZZINTERFACE
void argsfromstdin(int *argc, char ***argv)
{
	const int READSIZE = 256;
	char *buf = strdup((*argv)[0]);
	size_t len = strlen(buf)+1;

	while (1) {
		char *p = realloc(buf, len + READSIZE);
		size_t n;

		if (!p)
			exit(1);
		buf = p;

		n = fread(buf+len, 1, READSIZE, stdin);
		if (n > 0) {
			len += n;
		}
		else {
			buf[len] = '\0';
			len++;
			break;
		}
	}

	*argc = 0;
	*argv = NULL;
	while (len) {
		char **p = realloc(*argv, (*argc + 1) * sizeof(char**));
		if (!p)
			exit(1);
		*argv = p;
		(*argv)[*argc] = buf;
		(*argc)++;

		len -= strlen(buf) + 1;
		buf += strlen(buf) + 1;
	}
}
#endif /* BUILD_FUZZINTERFACE */

static bool check_for_wayland(Display *dpy)
{
	bool		has_xwayland_devices = false;
	XDeviceInfo	*info;
	int		ndevices, i;

	info = XListInputDevices(dpy, &ndevices);
	for (i = 0; i < ndevices; i++) {
		if (strncmp(info[i].name, "xwayland-", 9) == 0) {
			has_xwayland_devices = true;
			break;
		}
	}
	XFreeDeviceList(info);

	return has_xwayland_devices;
}

int main (int argc, char **argv)
{
	int c;
	int optidx;
	char *display = NULL;
	Display *dpy;
	int do_list = 0, do_set = 0, do_get = 0;
	enum printformat format = FORMAT_DEFAULT;
	int status = EXIT_SUCCESS;

	struct option options[] = {
		{"help", 0, NULL, 0},
		{"verbose", 0, NULL, 0},
		{"version", 0, NULL, 0},
		{"display", 1, NULL, 'd'},
		{"shell", 0, NULL, 0},
		{"xconf", 0, NULL, 0},
		{"list", 0, NULL, 0},
		{"set", 0, NULL, 0},
		{"get", 0, NULL, 0},
		{NULL, 0, NULL, 0}
	};

#ifdef BUILD_FUZZINTERFACE
	argsfromstdin(&argc, &argv);
#endif /* BUILD FUZZINTERFACE */

	if (argc < 2)
	{
		usage();
		return EXIT_INVALID_USAGE;
	}

	while ((c = getopt_long(argc, argv, "+hvVd:sx", options, &optidx)) != -1) {
		switch(c)
		{
			case 0:
				switch(optidx)
				{
					case 0: usage(); return EXIT_SUCCESS;
					case 1: verbose = True; break;
					case 2: version(); return EXIT_SUCCESS;
					case 3: /* display */
						break;
					case 4:
						format = FORMAT_SHELL;
						break;
					case 5:
						format = FORMAT_XORG_CONF;
						break;
					case 6: do_list = 1; break;
					case 7: do_set = 1; break;
					case 8: do_get = 1; break;
				}
				break;
			case 'd':
				display = optarg;
				break;
			case 's':
				format = FORMAT_SHELL;
				break;
			case 'x':
				format = FORMAT_XORG_CONF;
				break;
			case 'v':
				verbose = True;
				break;
			case 'V':
				version();
				return EXIT_SUCCESS;
			case 'h':
			default:
				usage();
				return EXIT_SUCCESS;
		}
	}

	TRACE("Display is '%s'.\n", display);

	dpy = XOpenDisplay(display);
	if (!dpy)
	{
		fprintf(stderr, "Failed to open Display %s.\n", display ? display : "");
		return EXIT_FAILURE;
	}

	if (check_for_wayland(dpy)) {
		fprintf(stderr,
			"Wayland devices found but this tool is incompatible with Wayland. See\n"
			"https://github.com/linuxwacom/xf86-input-wacom/wiki/Wayland\n");
		return EXIT_INVALID_USAGE;
	}

	if (!do_list && !do_get && !do_set)
	{
		if (optind < argc)
		{
			if (strcmp(argv[optind], "list") == 0)
			{
				do_list = 1;
				optind++;
			} else if (strcmp(argv[optind], "set") == 0)
			{
				do_set = 1;
				optind++;
			} else if (strcmp(argv[optind], "get") == 0)
			{
				do_get = 1;
				optind++;
			}
			else
				usage();
		} else
			usage();
	}

	if (do_list)
		status = list(dpy, argc - optind, &argv[optind]);
	else if (do_set)
		status = set(dpy, argc - optind, &argv[optind]);
	else if (do_get)
		status = get(dpy, format, argc - optind, &argv[optind]);

	XCloseDisplay(dpy);
	return status;
}
#endif /* ENABLE_TESTS */

#ifdef ENABLE_TESTS
#include "wacom-test-suite.h"
#include <assert.h>
/**
 * Below are unit-tests to ensure xsetwacom continues to work as expected.
 */

TEST_CASE(test_is_modifier)
{
	char i;
	char buff[5];


	assert(is_modifier("Control_L"));
	assert(is_modifier("Control_R"));
	assert(is_modifier("Alt_L"));
	assert(is_modifier("Alt_R"));
	assert(is_modifier("Shift_L"));
	assert(is_modifier("Shift_R"));
	assert(is_modifier("Meta_L"));
	assert(is_modifier("Meta_R"));
	assert(is_modifier("Super_L"));
	assert(is_modifier("Super_R"));
	assert(is_modifier("Hyper_L"));
	assert(is_modifier("Hyper_R"));

	assert(!is_modifier(""));

	/* make sure at least the default keys (ascii 33 - 126) aren't
	 * modifiers */
	for (i = '!'; i <= '~'; i++)
	{
		sprintf(buff, "%c", i);
		assert(!is_modifier(buff));
	}
}

TEST_CASE(test_convert_specialkey)
{
	char i;
	const char *converted;
	char buff[5];
	struct modifier *m;

	/* make sure a-zA-Z aren't specialkeys */
	for (i = 'a'; i <= 'z'; i++)
	{
		sprintf(buff, "%c", i);
		converted = convert_specialkey(buff);
		assert(strcmp(converted, buff) == 0);
	}

	for (i = 'A'; i <= 'Z'; i++)
	{
		sprintf(buff, "%c", i);
		converted = convert_specialkey(buff);
		assert(strcmp(converted, buff) == 0);
	}

	/* punctuation are specialkeys */
	for (i = '!'; i <= '/'; i++)
	{
		sprintf(buff, "%c", i);
		converted = convert_specialkey(buff);
		assert(strcmp(converted, buff) != 0);
	}
	for (i = ':'; i <= '?'; i++)
	{
		sprintf(buff, "%c", i);
		converted = convert_specialkey(buff);
		assert(strcmp(converted, buff) != 0);
	}


	for (m = specialkeys; m->name; m++)
	{
		converted = convert_specialkey(m->name);
		assert(strcmp(converted, m->converted) == 0);
	}
}

TEST_CASE(test_parameter_number)
{
	/* If either of those two fails, a parameter was added or removed.
	 * This test simply exists so that we remember to properly
	 * deprecated them.
	 * Numbers include trailing NULL entry.
	 */
	assert(ARRAY_SIZE(parameters) == 40);
	assert(ARRAY_SIZE(deprecated_parameters) == 17);
}

/**
 * For the given parameter, test all words against conversion success and
 * expected value.
 *
 * @param param The parameter of type PROP_FLAG_BOOLEAN.
 * @param words NULL-terminated word list to parse
 * @param success True if conversion success for the words is expected or
 * False overwise
 * @param expected Expected converted value. If success is False, this value
 * is omitted.
 */
static void _test_conversion(const param_t *param, const char **words,
			     Bool success, Bool expected)
{

	assert(param->prop_flags & PROP_FLAG_BOOLEAN);

	while(*words)
	{
		int val;
		int rc;
		rc = convert_value_from_user(param, *words, &val);
		assert(rc == success);
		if (success)
			assert(val == expected);
		words++;
	}
}

TEST_CASE(test_convert_value_from_user)
{
	param_t test_nonbool =
	{
		.name = "Test",
		.desc = "NOT A REAL PARAMETER",
		.prop_flags = 0,
	};

	param_t test_bool =
	{
		.name = "Test",
		.desc = "NOT A REAL PARAMETER",
		.prop_flags = PROP_FLAG_BOOLEAN,
	};

	param_t test_bool_inverted =
	{
		.name = "Test",
		.desc = "NOT A REAL PARAMETER",
		.prop_flags = PROP_FLAG_BOOLEAN | PROP_FLAG_INVERTED,
	};

	const char *bool_true[] = { "true", "TRUE", "True", "On", "on", "ON", NULL };
	const char *bool_false[] = { "false", "FALSE", "False", "Off", "off", "OFF", NULL };
	const char *bool_garbage[] = { "0", "1", " on", "on ", " off", " off", NULL};

	int val;

	assert(convert_value_from_user(&test_nonbool, "1", &val) == True);
	assert(convert_value_from_user(&test_nonbool, "-8", &val) == True);
	assert(convert_value_from_user(&test_nonbool, "+314", &val) == True);
	assert(convert_value_from_user(&test_nonbool, "36893488147419103232", &val) == False); //2^65 > MAX_INT
	assert(convert_value_from_user(&test_nonbool, "123abc", &val) == False);
	assert(convert_value_from_user(&test_nonbool, "123 abc", &val) == False);

	_test_conversion(&test_bool, bool_true, True, True);
	_test_conversion(&test_bool, bool_false, True, False);
	_test_conversion(&test_bool, bool_garbage, False, False);

	_test_conversion(&test_bool_inverted, bool_true, True, False);
	_test_conversion(&test_bool_inverted, bool_false, True, True);
	_test_conversion(&test_bool_inverted, bool_garbage, False, False);
}


extern void wcm_run_tests(void); /* see wacom-test-suite.c */

int main(int argc, char** argv)
{
	wcm_run_tests();
	return 0;
}

#endif
/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
