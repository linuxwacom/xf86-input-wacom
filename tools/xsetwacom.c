/*
 * Copyright 2009 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#define WACOM_TOOLS
#include "config.h"
#endif

#include <wacom-properties.h>
#include "Xwacom.h"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <math.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/XInput.h>

#define TRACE(...) \
	if (verbose) fprintf(stderr, "... " __VA_ARGS__)

static int verbose = False;

typedef struct _param
{
	const char *name;	/* param name as specified by the user */
	const char *desc;	/* description */
	const char *prop_name;	/* property name */
	const int prop_format;	/* property format */
	const int prop_offset;	/* offset (index) into the property values */
	void (*set_func)(Display *dpy, XDevice *dev, struct _param *param, int argc, char **argv); /* handler function, if appropriate */
	void (*get_func)(Display *dpy, XDevice *dev, struct _param *param, int argc, char **argv); /* handler function for getting, if appropriate */
} param_t;

static void map_button(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static void set_mode(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static void get_mode(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static void get_presscurve(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static void get_button(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static void set_rotate(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static void get_rotate(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static void not_implemented(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv)
{
	printf("Not implemented.\n");
}

static param_t parameters[] =
{
	{
		.name = "TopX",
		.desc = "Bounding rect left coordinate in tablet units. ",
		.prop_name = WACOM_PROP_TABLET_AREA,
		.prop_format = 32,
		.prop_offset = 0,
	},
	{
		.name = "TopY",
		.desc = "Bounding rect top coordinate in tablet units . ",
		.prop_name = WACOM_PROP_TABLET_AREA,
		.prop_format = 32,
		.prop_offset = 1,
	},
	{
		.name = "BottomX",
		.desc = "Bounding rect right coordinate in tablet units. ",
		.prop_name = WACOM_PROP_TABLET_AREA,
		.prop_format = 32,
		.prop_offset = 2,
	},
	{
		.name = "BottomY",
		.desc = "Bounding rect bottom coordinate in tablet units. ",
		.prop_name = WACOM_PROP_TABLET_AREA,
		.prop_format = 32,
		.prop_offset = 3,
	},
	{
		.name = "Button1",
		.desc = "X11 event to which button 1 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button2",
		.desc = "X11 event to which button 2 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button3",
		.desc = "X11 event to which button 3 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button4",
		.desc = "X11 event to which button 4 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button5",
		.desc = "X11 event to which button 5 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button6",
		.desc = "X11 event to which button 6 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button7",
		.desc = "X11 event to which button 7 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button8",
		.desc = "X11 event to which button 8 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button9",
		.desc = "X11 event to which button 9 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button10",
		.desc = "X11 event to which button 10 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button11",
		.desc = "X11 event to which button 11 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button12",
		.desc = "X11 event to which button 12 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button13",
		.desc = "X11 event to which button 13 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button14",
		.desc = "X11 event to which button 14 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button15",
		.desc = "X11 event to which button 15 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button16",
		.desc = "X11 event to which button 16 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button17",
		.desc = "X11 event to which button 17 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button18",
		.desc = "X11 event to which button 18 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button19",
		.desc = "X11 event to which button 19 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button20",
		.desc = "X11 event to which button 20 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button21",
		.desc = "X11 event to which button 21 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button22",
		.desc = "X11 event to which button 22 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button23",
		.desc = "X11 event to which button 23 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button24",
		.desc = "X11 event to which button 24 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button25",
		.desc = "X11 event to which button 25 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button26",
		.desc = "X11 event to which button 26 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button27",
		.desc = "X11 event to which button 27 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button28",
		.desc = "X11 event to which button 28 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button29",
		.desc = "X11 event to which button 29 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button30",
		.desc = "X11 event to which button 30 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button31",
		.desc = "X11 event to which button 31 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "Button32",
		.desc = "X11 event to which button 32 should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "DebugLevel",
		.desc = "Level of debugging trace for individual devices, "
		"default is 0 (off). ",
		.set_func = not_implemented,
		.get_func = not_implemented,
	},
	{
		.name = "CommonDBG",
		.desc = "Level of debugging statements applied to all devices "
		"associated with the same tablet. default is 0 (off). ",
		.set_func = not_implemented,
		.get_func = not_implemented,
	},
	{
		.name = "Suppress",
		.desc = "Number of points trimmed, default is 2. ",
		.prop_name = WACOM_PROP_SAMPLE,
		.prop_format = 32,
		.prop_offset = 1,
	},
	{
		.name = "RawSample",
		.desc = "Number of raw data used to filter the points, "
		"default is 4. ",
		.prop_name = WACOM_PROP_SAMPLE,
		.prop_format = 32,
		.prop_offset = 0,
	},
	{
		.name = "Screen_No",
		.desc = "Sets/gets screen number the tablet is mapped to, "
		"default is -1. ",
		.prop_name = WACOM_PROP_DISPLAY_OPTS,
		.prop_format = 8,
		.prop_offset = 0,
	},
	{
		.name = "PressCurve",
		.desc = "Bezier curve for pressure (default is 0 0 100 100). ",
		.prop_name = WACOM_PROP_PRESSURECURVE,
		.prop_format = 32,
		.prop_offset = 0,
		.get_func = get_presscurve,
	},
	{
		.name = "TwinView",
		.desc = "Sets the mapping to TwinView horizontal/vertical/none. "
		"Values = none, vertical, horizontal (default is none).",
		.prop_name = WACOM_PROP_TWINVIEW_RES,
		.prop_format = 8,
		.prop_offset = 1,
	},
	{
		.name = "Mode",
		.desc = "Switches cursor movement mode (default is absolute/on). ",
		.set_func = set_mode,
		.get_func = get_mode,
	},
	{
		.name = "TPCButton",
		.desc = "Turns on/off Tablet PC buttons. "
		"default is off for regular tablets, "
		"on for Tablet PC. ",
		.prop_name = WACOM_PROP_HOVER,
		.prop_format = 8,
		.prop_offset = 0,
	},
	{
		.name = "Touch",
		.desc = "Turns on/off Touch events (default is enable/on). ",
		.prop_name = WACOM_PROP_TOUCH,
		.prop_format = 8,
		.prop_offset = 0,
	},
	{
		.name = "Capacity",
		.desc = "Touch sensitivity level (default is 3, "
		"-1 for none capacitive tools).",
		.prop_name = WACOM_PROP_CAPACITY,
		.prop_format = 8,
		.prop_offset = 0,
	},
	{
		.name = "CursorProx",
		.desc = "Sets cursor distance for proximity-out "
		"in distance from the tablet.  "
		"(default is 10 for Intuos series, "
		"42 for Graphire series).",
		.prop_name = WACOM_PROP_PROXIMITY_THRESHOLD,
		.prop_format = 32,
		.prop_offset = 0,
	},
	{
		.name = "Rotate",
		.desc = "Sets the rotation of the tablet. "
		"Values = NONE, CW, CCW, HALF (default is NONE).",
		.prop_name = WACOM_PROP_ROTATION,
		.set_func = set_rotate,
		.get_func = get_rotate,
	},
	{
		.name = "RelWUp",
		.desc = "X11 event to which relative wheel up should be mapped. ",
		.prop_name = WACOM_PROP_WHEELBUTTONS,
		.prop_format = 8,
		.prop_offset = 0,
	},
	{
		.name = "RelWDn",
		.desc = "X11 event to which relative wheel down should be mapped. ",
		.prop_name = WACOM_PROP_WHEELBUTTONS,
		.prop_format = 8,
		.prop_offset = 1,
	},
	{
		.name = "AbsWUp",
		.desc = "X11 event to which absolute wheel up should be mapped. ",
		.prop_name = WACOM_PROP_WHEELBUTTONS,
		.prop_format = 8,
		.prop_offset = 2,
	},
	{
		.name = "AbsWDn",
		.desc = "X11 event to which absolute wheel down should be mapped. ",
		.prop_name = WACOM_PROP_WHEELBUTTONS,
		.prop_format = 8,
		.prop_offset = 3,
	},
	{
		.name = "StripLUp",
		.desc = "X11 event to which left strip up should be mapped. ",
		.prop_name = WACOM_PROP_STRIPBUTTONS,
		.prop_format = 8,
		.prop_offset = 0,
	},
	{
		.name = "StripLDn",
		.desc = "X11 event to which left strip down should be mapped. ",
		.prop_name = WACOM_PROP_STRIPBUTTONS,
		.prop_format = 8,
		.prop_offset = 1,
	},
	{
		.name = "StripRUp",
		.desc = "X11 event to which right strip up should be mapped. ",
		.prop_name = WACOM_PROP_STRIPBUTTONS,
		.prop_format = 8,
		.prop_offset = 2,
	},
	{
		.name = "StripRDn",
		.desc = "X11 event to which right strip down should be mapped. ",
		.prop_name = WACOM_PROP_STRIPBUTTONS,
		.prop_format = 8,
		.prop_offset = 3,
	},
	{
		.name = "TVResolution0",
		.desc = "Sets MetaModes option for TwinView Screen 0. ",
		.prop_name = WACOM_PROP_TWINVIEW_RES,
		.prop_format = 32,
		.prop_offset = 0,
	},
	{
		.name = "TVResolution1",
		.desc = "Sets MetaModes option for TwinView Screen 1. ",
		.prop_name = WACOM_PROP_TWINVIEW_RES,
		.prop_format = 32,
		.prop_offset = 1,
	},
	{
		.name = "RawFilter",
		.desc = "Enables and disables filtering of raw data, "
		"default is true/on.",
		.prop_name = WACOM_PROP_SAMPLE,
		.prop_format = 8,
		.prop_offset = 0,
	},
	{
		.name = "ClickForce",
		.desc = "Sets tip/eraser pressure threshold = ClickForce*MaxZ/100 "
		"(default is 6)",
		.prop_name = WACOM_PROP_PRESSURE_THRESHOLD,
		.prop_format = 32,
		.prop_offset = 0,
	},
	{
		.name = "xyDefault",
		.desc = "Resets the bounding coordinates to default in tablet units. ",
		.set_func = not_implemented,
		.get_func = not_implemented,
	},
	{
		.name = "mmonitor",
		.desc = "Turns on/off across monitor movement in "
		"multi-monitor desktop, default is on ",
		.prop_name = WACOM_PROP_DISPLAY_OPTS,
		.prop_format = 8,
		.prop_offset = 2,
	},
	{
		.name = "STopX0",
		.desc = "Screen 0 left coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 0,
	},
	{
		.name = "STopY0",
		.desc = "Screen 0 top coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 1,
	},
	{
		.name = "SBottomX0",
		.desc = "Screen 0 right coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 2,
	},
	{
		.name = "SBottomY0",
		.desc = "Screen 0 bottom coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 3,
	},
	{
		.name = "STopX1",
		.desc = "Screen 1 left coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 4,
	},
	{
		.name = "STopY1",
		.desc = "Screen 1 top coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 5,
	},
	{
		.name = "SBottomX1",
		.desc = "Screen 1 right coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 6,
	},
	{
		.name = "SBottomY1",
		.desc = "Screen 1 bottom coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 7,
	},
	{
		.name = "STopX2",
		.desc = "Screen 2 left coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 8,
	},
	{
		.name = "STopY2",
		.desc = "Screen 2 top coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 9,
	},
	{
		.name = "SBottomX2",
		.desc = "Screen 2 right coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
	},
	{
		.name = "SBottomY2",
		.desc = "Screen 2 bottom coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 11,
	},
	{
		.name = "STopX3",
		.desc = "Screen 3 left coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 12,
	},
	{
		.name = "STopY3",
		.desc = "Screen 3 top coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 13,
	},
	{
		.name = "SBottomX3",
		.desc = "Screen 3 right coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 14,
	},
	{
		.name = "SBottomY3",
		.desc = "Screen 3 bottom coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 15,
	},
	{
		.name = "STopX4",
		.desc = "Screen 4 left coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 16,
	},
	{
		.name = "STopY4",
		.desc = "Screen 4 top coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 17,
	},
	{
		.name = "SBottomX4",
		.desc = "Screen 4 right coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 18,
	},
	{
		.name = "SBottomY4",
		.desc = "Screen 4 bottom coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 19,
	},
	{
		.name = "STopX5",
		.desc = "Screen 5 left coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 20,
	},
	{
		.name = "STopY5",
		.desc = "Screen 5 top coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 21,
	},
	{
		.name = "SBottomX5",
		.desc = "Screen 5 right coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 22,
	},
	{
		.name = "SBottomY5",
		.desc = "Screen 5 bottom coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 23,
	},
	{
		.name = "STopX6",
		.desc = "Screen 6 left coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 24,
	},
	{
		.name = "STopY6",
		.desc = "Screen 6 top coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 25,
	},
	{
		.name = "SBottomX6",
		.desc = "Screen 6 right coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 26,
	},
	{
		.name = "SBottomY6",
		.desc = "Screen 6 bottom coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 27,
	},
	{
		.name = "STopX7",
		.desc = "Screen 7 left coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 28,
	},
	{
		.name = "STopY7",
		"Screen 7 top coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 29,
	},
	{
		.name = "SBottomX7",
		.desc = "Screen 7 right coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 30,
	},
	{
		.name = "SBottomY7",
		.desc = "Screen 7 bottom coordinate in pixels. ",
		.prop_name = WACOM_PROP_SCREENAREA,
		.prop_format = 32,
		.prop_offset = 31,
	},
	{
		.name = "ToolID",
		.desc = "Returns the ID of the associated device. ",
		.prop_name = WACOM_PROP_TOOL_TYPE,
		.prop_format = 32,
		.prop_offset = 0,
	},
	{
		.name = "ToolSerial",
		.desc = "Returns the serial number of the associated device. ",
		.prop_name = WACOM_PROP_SERIALIDS,
		.prop_format = 32,
		.prop_offset = 3,
	},
	{
		.name = "TabletID",
		.desc = "Returns the tablet ID of the associated device. ",
		.prop_name = WACOM_PROP_SERIALIDS,
		.prop_format = 32,
		.prop_offset = 0,
	},
	{
		.name = "GetTabletID",
		.desc = "Returns the tablet ID of the associated device. ",
		.prop_name = WACOM_PROP_SERIALIDS,
		.prop_format = 32,
		.prop_offset = 0,
	},
	{
		.name = "NumScreen",
		.desc = "Returns number of screens configured for the desktop. ",
		.set_func = not_implemented,
		.get_func = not_implemented,
	},
	{
		.name = "XScaling",
		.desc = "Returns the status of XSCALING is set or not. ",
		.set_func = not_implemented,
		.get_func = not_implemented,
	},
	{ NULL }
};

static param_t* find_parameter(char *name)
{
	param_t *param = NULL;

	for (param = parameters; param->name; param++)
		if (strcmp(name, param->name) == 0)
			break;
	return param->name ? param : NULL;
}


static void usage(void)
{
	printf(
	"Usage: xsetwacom [options] [command [arguments...]]\n"
	"Options:\n"
	" -h, --help                 - usage\n"
	" -v, --verbose              - verbose output\n"
	" -V, --version              - version info\n"
	" -d, --display disp_name    - override default display\n"
	" -s, --shell                - generate shell commands for 'get' [not implemented]\n"
	" -x, --xconf                - generate X.conf lines for 'get' [not implemented]\n");

	printf(
	"\nCommands:\n"
	" --list [dev|param]           - display known devices, parameters \n"
	" --list mod                   - display supported modifier and specific keys for keystokes [not implemented}\n"
	" --set dev_name param [values...] - set device parameter by name\n"
	" --get dev_name param [param...] - get current device parameter(s) value by name\n");
}


static void version(void)
{
	printf("%d.%d.%d\n", PACKAGE_VERSION_MAJOR, PACKAGE_VERSION_MINOR,
			     PACKAGE_VERSION_PATCHLEVEL);
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
		TRACE("Checking device '%s' (%ld).\n", devices[i].name, devices[i].id);

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
		TRACE("Device '%s' (%ld) found.\n", found->name, found->id);
		dev = XOpenDevice(display, found->id);
	}

	XFreeDeviceList(devices);

	return dev;
}

static void list_one_device(Display *dpy, XDeviceInfo *info)
{
	static int	wacom_prop = 0;
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

		if (j <= natoms)
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
				printf("%-16s %-10s\n", info->name, type_name);
			}

			XFree(data);
		} else {
			TRACE("'%s' (%ld) is not a wacom device.\n", info->name, info->id);
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

		TRACE("Found device '%s' (%ld).\n", info[i].name, info[i].id);
		list_one_device(dpy, &info[i]);
	}

	XFreeDeviceList(info);
}


static void list_param(Display *dpy)
{
	param_t *param = parameters;

	while(param->name)
	{
		printf("%-16s - %16s%s\n", param->name, param->desc,
			(param->set_func == not_implemented) ? " [not implemented]" : "");
		param++;
	}
}

static void list(Display *dpy, int argc, char **argv)
{
	TRACE("'list' requested.\n");
	if (argc == 0)
		list_devices(dpy);
	else if (strcmp(argv[0], "dev") == 0)
		list_devices(dpy);
	else if (strcmp(argv[0], "param") == 0)
		list_param(dpy);
	else
		printf("unknown argument to list.\n");
}

/*
 * Convert a list of random modifiers to strings that can be passed into
 * XStringToKeysym
 */
static char *convert_modifier(const char *modifier)
{
	struct modifier {
		char *name;
		char *converted;
	} modmap[] = {
		{"ctrl", "Control_L"},
		{"ctl", "Control_L"},
		{"control", "Control_L"},
		{"lctrl", "Control_L"},
		{"rctrl", "Control_R"},

		{"alt", "Alt_L"},
		{"lalt", "Alt_L"},
		{"ralt", "Alt_R"},

		{"shift", "Shift_L"},
		{"lshift", "Shift_L"},
		{"rshift", "Shift_R"},

		{ NULL, NULL }
	};

	struct modifier *m = modmap;

	while(m->name && strcasecmp(modifier, m->name))
		m++;

	return m->converted;
}

/*
   Map gibberish like "ctrl alt f2" into the matching AC_KEY values.
   Returns 1 on success or 0 otherwise.
 */
static int special_map_keystrokes(int argc, char **argv, unsigned long *ndata, unsigned long* data)
{
	int i;
	int nitems = 0;

	for (i = 0; i < argc; i++)
	{
		KeySym ks;
		int need_press = 0, need_release = 0;
		char *key = argv[i];

		if (strlen(key) > 1)
		{
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
					break;
			}

			/* Function keys must be uppercased */
			if (strlen(key) > 1)
			{
				if (strlen(key) >= 2 && key[0] == 'f' && isdigit(key[1]))
					*key = toupper(*key);
				else
					key = convert_modifier(key);
			}

			if (!key)
			{
				fprintf(stderr, "Invalid key '%s'.\n", argv[i]);
				break;
			}

		} else
			need_press = need_release = 1;

		ks = XStringToKeysym(key);
		if (need_press)
			data[nitems++] = AC_KEY | AC_KEYBTNPRESS | ks;
		if (need_release)
			data[nitems++] = AC_KEY | ks;
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
	char buff[1024] = { 0 };
	char **words	= NULL;
	char *tmp, *tok;

	while(argc--)
	{
		if (strlen(buff) + strlen(*argv) + 1 >= sizeof(buff))
			break;

		strcat(buff, (const char*)(*argv)++);
		strcat(buff, " ");
	}

	for (tmp = buff; tmp && *tmp != '\0'; tmp = index((const char*)tmp, ' ') + 1)
		(*nwords)++;

	words = calloc(*nwords, sizeof(char*));

	*nwords = 0;
	tok = strtok(buff, " ");
	while(tok)
	{
		words[(*nwords)++] = strdup(tok);
		tok = strtok(NULL, " ");
	}

	return words;
}

static int get_button_number_from_string(const char* string)
{
	int slen = strlen("Button");
	if (slen >= strlen(string) || strncasecmp(string, "Button", slen))
		return -1;
	return atoi(&string[strlen("Button")]);
}

/* Handles complex button mappings through button actions. */
static void special_map_buttons(Display *dpy, XDevice *dev, param_t* param, int argc, char **argv)
{
	Atom btnact_prop, prop;
	unsigned long *data, *btnact_data;
	int slen = strlen("Button");
	int btn_no;
	Atom type;
	int format;
	unsigned long btnact_nitems, nitems, bytes_after;
	int need_update = 0;
	int i;
	int nwords = 0;
	char **words = NULL;

	struct keywords {
		const char *keyword;
		int (*func)(int, char **, unsigned long*, unsigned long *);
	} keywords[] = {
		{"key", special_map_keystrokes},
		{ NULL, NULL }
	};

	TRACE("Special %s map for device %ld.\n", param->name, dev->device_id);

	if (slen >= strlen(param->name) || strncmp(param->name, "Button", slen))
		return;

	btnact_prop = XInternAtom(dpy, "Wacom Button Actions", True);
	if (!btnact_prop)
		return;

	btn_no = get_button_number_from_string(param->name);

	XGetDeviceProperty(dpy, dev, btnact_prop, 0, 100, False,
				AnyPropertyType, &type, &format, &btnact_nitems,
				&bytes_after, (unsigned char**)&btnact_data);

	if (btn_no > btnact_nitems)
		return;

	/* some atom already assigned, modify that */
	if (btnact_data[btn_no])
		prop = btnact_data[btn_no];
	else
	{
		char buff[64];
		sprintf(buff, "Wacom button action %d", btn_no);
		prop = XInternAtom(dpy, buff, False);

		btnact_data[btn_no] = prop;
		need_update = 1;
	}

	data = calloc(sizeof(long), 256);
	nitems = 0;

	/* translate cmdline commands */
	words = strjoinsplit(argc, argv, &nwords);
	for (i = 0; i < nwords; i++)
	{
		int j;
		for (j = 0; keywords[j].keyword; j++)
			if (strcasecmp(words[i], keywords[j].keyword) == 0)
				i += keywords[j].func(nwords - i - 1,
						      &words[i + 1],
						      &nitems, data);
	}

	XChangeDeviceProperty(dpy, dev, prop, XA_INTEGER, 32,
				PropModeReplace,
				(unsigned char*)data, nitems);

	if (need_update)
		XChangeDeviceProperty(dpy, dev, btnact_prop, XA_ATOM, 32,
					PropModeReplace,
					(unsigned char*)btnact_data,
					btnact_nitems);
	XFlush(dpy);
}

/*
   Supports three variations.
   xsetwacom set device Button1 1
	- maps button 1 to logical button 1
   xsetwacom set device Button1 "Button 5"
	- maps button 1 to the same logical button button 5 is mapped
   xsetwacom set device Button1 "key a b c d"
	- maps button 1 to key events a b c d
 */
static void map_button(Display *dpy, XDevice *dev, param_t* param, int argc, char **argv)
{
	int nmap = 256;
	unsigned char map[nmap];
	int i, btn_no = 0;
	int ref_button = -1; /* xsetwacom set <name> Button1 "Button 5" */

	if (argc <= 0)
		return;

	TRACE("Mapping %s for device %ld.\n", param->name, dev->device_id);

	for(i = 0; i < strlen(argv[0]); i++)
	{
		if (!isdigit(argv[0][i]))
		{
			ref_button = get_button_number_from_string(argv[0]);
			if (ref_button != -1)
				break;

			special_map_buttons(dpy, dev, param, argc, argv);
			return;
		}
	}

	btn_no = get_button_number_from_string(param->name);
	if (btn_no == -1)
		return;

	nmap = XGetDeviceButtonMapping(dpy, dev, map, nmap);
	if (btn_no >= nmap)
	{
		fprintf(stderr, "Button number does not exist on device.\n");
		return;
	} else if (ref_button >= nmap)
	{
		fprintf(stderr, "Reference button number does not exist on device.\n");
		return;
	}

	if (ref_button != -1)
		map[btn_no - 1] = map[ref_button - 1];
	else
		map[btn_no - 1] = atoi(argv[0]);
	XSetDeviceButtonMapping(dpy, dev, map, nmap);
	XFlush(dpy);
}

static void set_mode(Display *dpy, XDevice *dev, param_t* param, int argc, char **argv)
{
	int mode = Absolute;
	if (argc < 1)
	{
		usage();
		return;
	}

	TRACE("Set mode '%s' for device %ld.\n", argv[0], dev->device_id);

	if (strcasecmp(argv[0], "Relative") == 0)
		mode = Relative;
	else if (strcasecmp(argv[0], "Absolute") == 0)
		mode = Absolute;
	else
	{
		printf("Invalid device mode. Use 'Relative' or 'Absolute'.\n");
		return;
	}


	XSetDeviceMode(dpy, dev, mode);
	XFlush(dpy);
}

static void set_rotate(Display *dpy, XDevice *dev, param_t* param, int argc, char **argv)
{
	int rotation = 0;
	Atom prop, type;
	int format;
	unsigned char* data;
	unsigned long nitems, bytes_after;

	if (argc != 1)
		goto error;

	TRACE("Rotate '%s' for device %ld.\n", argv[0], dev->device_id);

	if (strcasecmp(argv[0], "CW") == 0)
		rotation = 1;
	else if (strcasecmp(argv[0], "CCW") == 0)
		rotation = 2;
	else if (strcasecmp(argv[0], "HALF") == 0)
		rotation = 3;
	else if (strcasecmp(argv[0], "NONE") == 0)
		rotation = 0;
	else
		goto error;

	prop = XInternAtom(dpy, param->prop_name, True);
	if (!prop)
	{
		fprintf(stderr, "Property for '%s' not available.\n",
			param->name);
		return;
	}

	XGetDeviceProperty(dpy, dev, prop, 0, 1000, False, AnyPropertyType,
				&type, &format, &nitems, &bytes_after, &data);

	if (nitems == 0 || format != 8)
	{
		fprintf(stderr, "Property for '%s' has no or wrong value - this is a bug.\n",
			param->name);
		return;
	}

	*data = rotation;
	XChangeDeviceProperty(dpy, dev, prop, type, format,
				PropModeReplace, data, nitems);
	XFlush(dpy);

	return;

error:
	fprintf(stderr, "Usage: xsetwacom rotate <device name> [NONE | CW | CCW | HALF]\n");
	return;
}

static void set(Display *dpy, int argc, char **argv)
{
	param_t *param;
	XDevice *dev = NULL;
	Atom prop, type;
	int format;
	unsigned char* data = NULL;
	unsigned long nitems, bytes_after;
	double val;
	long *n;
	char *b;
	int i;

	if (argc < 3)
	{
		usage();
		return;
	}

	TRACE("'set' requested for '%s'.\n", argv[0]);

	dev = find_device(dpy, argv[0]);
	if (!dev)
	{
		printf("Cannot find device '%s'.\n", argv[0]);
		return;
	}

	param = find_parameter(argv[1]);
	if (!param)
	{
		printf("Unknown parameter name '%s'.\n", argv[1]);
		goto out;
	} else if (param->set_func)
	{
		param->set_func(dpy, dev, param, argc - 2, &argv[2]);
		goto out;
	}

	prop = XInternAtom(dpy, param->prop_name, True);
	if (!prop)
	{
		fprintf(stderr, "Property for '%s' not available.\n",
			param->name);
		goto out;
	}

	XGetDeviceProperty(dpy, dev, prop, 0, 1000, False, AnyPropertyType,
				&type, &format, &nitems, &bytes_after, &data);

	if (nitems <= param->prop_offset)
	{
		fprintf(stderr, "Property offset doesn't exist.\n");
		goto out;
	}

	for (i = 0; i < argc - 2; i++)
	{
		val = atof(argv[i + 2]);

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
	XCloseDevice(dpy, dev);
	free(data);
}

static void get_mode(Display *dpy, XDevice *dev, param_t* param, int argc, char **argv)
{
	XDeviceInfo *info, *d;
	int ndevices, i;
	XValuatorInfoPtr v;

	info = XListInputDevices(dpy, &ndevices);
	while(ndevices--)
	{
		d = &info[ndevices];
		if (d->id == dev->device_id)
			break;
	}

	if (!ndevices) /* device id 0 is reserved and can't be our device */
		return;

	TRACE("Getting mode for device %ld.\n", dev->device_id);

	v = (XValuatorInfoPtr)d->inputclassinfo;
	for (i = 0; i < d->num_classes; i++)
	{
		if (v->class == ValuatorClass)
		{
			printf("%s\n", (v->mode == Absolute) ? "Absolute" : "Relative");
			break;
		}
		v = (XValuatorInfoPtr)((char*)v + v->length);
	}

	XFreeDeviceList(info);
}

static void get_rotate(Display *dpy, XDevice *dev, param_t* param, int argc, char **argv)
{
	char *rotation = NULL;
	Atom prop, type;
	int format;
	unsigned char* data;
	unsigned long nitems, bytes_after;

	prop = XInternAtom(dpy, param->prop_name, True);
	if (!prop)
	{
		fprintf(stderr, "Property for '%s' not available.\n",
			param->name);
		return;
	}

	TRACE("Getting rotation for device %ld.\n", dev->device_id);

	XGetDeviceProperty(dpy, dev, prop, 0, 1000, False, AnyPropertyType,
				&type, &format, &nitems, &bytes_after, &data);

	if (nitems == 0 || format != 8)
	{
		fprintf(stderr, "Property for '%s' has no or wrong value - this is a bug.\n",
			param->name);
		return;
	}

	switch(*data)
	{
		case 0:
			rotation = "NONE";
			break;
		case 1:
			rotation = "CW";
			break;
		case 2:
			rotation = "CCW";
			break;
		case 3:
			rotation = "HALF";
			break;
	}

	printf("%s\n", rotation);

	return;
}


static void get_presscurve(Display *dpy, XDevice *dev, param_t *param, int argc,
				char **argv)
{
	Atom prop, type;
	int format, i;
	unsigned char* data;
	unsigned long nitems, bytes_after;

	prop = XInternAtom(dpy, param->prop_name, True);
	if (!prop)
	{
		fprintf(stderr, "Property for '%s' not available.\n",
			param->name);
		return;
	}

	TRACE("Getting pressure curve for device %ld.\n", dev->device_id);

	XGetDeviceProperty(dpy, dev, prop, 0, 1000, False, AnyPropertyType,
				&type, &format, &nitems, &bytes_after, &data);

	if (param->prop_format != 32)
		return;

	for (i = 0; i < nitems; i++)
	{
		long *ldata = (long*)data;
		printf(" %ld", ldata[param->prop_offset + i]);
	}

	printf("\n");

}

static void get_button(Display *dpy, XDevice *dev, param_t *param, int argc,
			char **argv)
{
	int nmap = 256;
	unsigned char map[nmap];
	int btn_no = 0;

	btn_no = get_button_number_from_string(param->name);
	if (btn_no == -1)
		return;

	TRACE("Getting button map curve for device %ld.\n", dev->device_id);

	nmap = XGetDeviceButtonMapping(dpy, dev, map, nmap);

	if (btn_no > nmap)
	{
		fprintf(stderr, "Button number does not exist on device.\n");
		return;
	}

	printf("%d\n", map[btn_no - 1]);

	XSetDeviceButtonMapping(dpy, dev, map, nmap);
	XFlush(dpy);
}

static void get(Display *dpy, int argc, char **argv)
{
	param_t *param;
	XDevice *dev = NULL;
	Atom prop, type;
	int format;
	unsigned char* data;
	unsigned long nitems, bytes_after;

	TRACE("'get' requested for '%s'.\n", argv[0]);

	dev = find_device(dpy, argv[0]);
	if (!dev)
	{
		printf("Cannot find device '%s'.\n", argv[0]);
		return;
	}

	param = find_parameter(argv[1]);
	if (!param)
	{
		printf("Unknown parameter name '%s'.\n", argv[1]);
		goto out;
	} else if (param->get_func)
	{
		param->get_func(dpy, dev, param, argc - 2, &argv[2]);
		goto out;
	}

	prop = XInternAtom(dpy, param->prop_name, True);
	if (!prop)
	{
		fprintf(stderr, "Property for '%s' not available.\n",
			param->name);
		goto out;
	}

	XGetDeviceProperty(dpy, dev, prop, 0, 1000, False, AnyPropertyType,
				&type, &format, &nitems, &bytes_after, &data);

	if (nitems <= param->prop_offset)
	{
		fprintf(stderr, "Property offset doesn't exist.\n");
		return;
	}


	switch(param->prop_format)
	{
		case 8:
			printf(" %d\n", data[param->prop_offset]);
			break;
		case 32:
			{
				long *ldata = (long*)data;
				printf(" %ld\n", ldata[param->prop_offset]);
				break;
			}
	}

out:
	XCloseDevice(dpy, dev);
}


int main (int argc, char **argv)
{
	char c;
	int optidx;
	char *display = NULL;
	Display *dpy;
	int do_list = 0, do_set = 0, do_get = 0;

	struct option options[] = {
		{"help", 0, NULL, 0},
		{"verbose", 0, NULL, 0},
		{"version", 0, NULL, 0},
		{"display", 1, (int*)display, 0},
		{"shell", 0, NULL, 0},
		{"xconf", 0, NULL, 0},
		{"list", 0, NULL, 0},
		{"set", 0, NULL, 0},
		{"get", 0, NULL, 0},
		{NULL, 0, NULL, 0}
	};

	if (argc < 2)
	{
		usage();
		return 1;
	}

	while ((c = getopt_long(argc, argv, "hvVd:sx", options, &optidx)) != -1) {
		switch(c)
		{
			case 0:
				switch(optidx)
				{
					case 0: usage(); break;
					case 1: verbose = True; break;
					case 2: version(); break;
					case 3:
					case 4:
					case 5:
						printf("Not implemented\n");
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
			case 'x':
				printf("Not implemented\n");
				break;
			case 'v':
				verbose = True;
				break;
			case 'V':
				version();
				break;
			case 'h':
			default:
				usage();
				return 0;
		}
	}

	TRACE("Display is '%s'.\n", display);

	dpy = XOpenDisplay(display);
	if (!dpy)
	{
		printf("Failed to open Display %s.\n", display ? display : "");
		return -1;
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
		list(dpy, argc - optind, &argv[optind]);
	else if (do_set)
		set(dpy, argc - optind, &argv[optind]);
	else if (do_get)
		get(dpy, argc - optind, &argv[optind]);

	XCloseDisplay(dpy);
	return 0;
}


/* vim: set noexpandtab shiftwidth=8: */
