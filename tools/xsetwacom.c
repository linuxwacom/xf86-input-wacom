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
static void get_presscurve(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static void get_button(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static void not_implemented(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv)
{
	printf("Not implemented.\n");
}

static param_t parameters[] =
{
	{ "TopX",
		"Bounding rect left coordinate in tablet units. ",
		WACOM_PROP_TABLET_AREA, 32, 0, NULL, NULL,
	},

	{ "TopY",
		"Bounding rect top coordinate in tablet units . ",
		WACOM_PROP_TABLET_AREA, 32, 1, NULL,
	},

	{ "BottomX",
		"Bounding rect right coordinate in tablet units. ",
		WACOM_PROP_TABLET_AREA, 32, 2, NULL,
	},

	{ "BottomY",
		"Bounding rect bottom coordinate in tablet units. ",
		WACOM_PROP_TABLET_AREA, 32, 3, NULL,
	},

	{ "Button1",
		"X11 event to which button 1 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button2",
		"X11 event to which button 2 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button3",
		"X11 event to which button 3 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button4",
		"X11 event to which button 4 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button5",
		"X11 event to which button 5 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button6",
		"X11 event to which button 6 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button7",
		"X11 event to which button 7 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button8",
		"X11 event to which button 8 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button9",
		"X11 event to which button 9 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button10",
		"X11 event to which button 10 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button11",
		"X11 event to which button 11 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button12",
		"X11 event to which button 12 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button13",
		"X11 event to which button 13 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button14",
		"X11 event to which button 14 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button15",
		"X11 event to which button 15 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button16",
		"X11 event to which button 16 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button17",
		"X11 event to which button 17 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button18",
		"X11 event to which button 18 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button19",
		"X11 event to which button 19 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button20",
		"X11 event to which button 20 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button21",
		"X11 event to which button 21 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button22",
		"X11 event to which button 22 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button23",
		"X11 event to which button 23 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},
	{ "Button24",
		"X11 event to which button 24 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button25",
		"X11 event to which button 25 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button26",
		"X11 event to which button 26 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button27",
		"X11 event to which button 27 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button28",
		"X11 event to which button 28 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button29",
		"X11 event to which button 29 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button30",
		"X11 event to which button 30 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button31",
		"X11 event to which button 31 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "Button32",
		"X11 event to which button 32 should be mapped. ",
		NULL, 0, 0, map_button, get_button
	},

	{ "DebugLevel",
		"Level of debugging trace for individual devices, "
		"default is 0 (off). ",
		NULL, 0, 0, not_implemented
	},

	{ "CommonDBG",
		"Level of debugging statements applied to all devices "
		"associated with the same tablet. default is 0 (off). ",
		NULL, 0, 0, not_implemented
	},

	{ "Suppress",
		"Number of points trimmed, default is 2. ",
		WACOM_PROP_SAMPLE, 32, 1,
	},

	{ "RawSample",
		"Number of raw data used to filter the points, "
		"default is 4. ",
		WACOM_PROP_SAMPLE, 32, 0,
	},

	{ "Screen_No",
		"Sets/gets screen number the tablet is mapped to, "
		"default is -1. ",
		WACOM_PROP_DISPLAY_OPTS, 8, 0,
	},

	{ "PressCurve",
		"Bezier curve for pressure (default is 0 0 100 100). ",
		WACOM_PROP_PRESSURECURVE, 32, 0, NULL, get_presscurve
	},

	{ "TwinView",
		"Sets the mapping to TwinView horizontal/vertical/none. "
		"Values = none, vertical, horizontal (default is none).",
		WACOM_PROP_TWINVIEW_RES, 8, 1, NULL, NULL
	},

	{ "Mode",
		"Switches cursor movement mode (default is absolute/on). ",
		NULL, 0, 0, set_mode
	},

	{ "TPCButton",
		"Turns on/off Tablet PC buttons. "
		"default is off for regular tablets, "
		"on for Tablet PC. ",
		WACOM_PROP_HOVER, 8, 0, NULL, NULL
	},

	{ "Touch",
		"Turns on/off Touch events (default is enable/on). ",
		WACOM_PROP_TOUCH, 8, 0
	},

	{ "Capacity",
		"Touch sensitivity level (default is 3, "
		"-1 for none capacitive tools).",
		WACOM_PROP_CAPACITY, 8, 0,
	},

	{ "CursorProx",
		"Sets cursor distance for proximity-out "
		"in distance from the tablet.  "
		"(default is 10 for Intuos series, "
		"42 for Graphire series).",
		WACOM_PROP_PROXIMITY_THRESHOLD, 32, 0,
	},

	{ "Rotate",
		"Sets the rotation of the tablet. "
		"Values = NONE, CW, CCW, HALF (default is NONE).",
		WACOM_PROP_ROTATION, 8, 0,
	},

	{ "RelWUp",
		"X11 event to which relative wheel up should be mapped. ",
		WACOM_PROP_WHEELBUTTONS, 8, 0,
	},

	{ "RelWDn",
		"X11 event to which relative wheel down should be mapped. ",
		WACOM_PROP_WHEELBUTTONS, 8, 1,
	},

	{ "AbsWUp",
		"X11 event to which absolute wheel up should be mapped. ",
		WACOM_PROP_WHEELBUTTONS, 8, 2,
	},

	{ "AbsWDn",
		"X11 event to which absolute wheel down should be mapped. ",
		WACOM_PROP_WHEELBUTTONS, 8, 3,
	},

	{ "StripLUp",
		"X11 event to which left strip up should be mapped. ",
		WACOM_PROP_STRIPBUTTONS, 8, 0,
	},

	{ "StripLDn",
		"X11 event to which left strip down should be mapped. ",
		WACOM_PROP_STRIPBUTTONS, 8, 1,
	},

	{ "StripRUp",
		"X11 event to which right strip up should be mapped. ",
		WACOM_PROP_STRIPBUTTONS, 8, 2,
	},

	{ "StripRDn",
		"X11 event to which right strip down should be mapped. ",
		WACOM_PROP_STRIPBUTTONS, 8, 3,
	},

	{ "TVResolution0",
		"Sets MetaModes option for TwinView Screen 0. ",
		WACOM_PROP_TWINVIEW_RES, 32, 0,
	},

	{ "TVResolution1",
		"Sets MetaModes option for TwinView Screen 1. ",
		WACOM_PROP_TWINVIEW_RES, 32, 1,
	},

	{ "RawFilter",
		"Enables and disables filtering of raw data, "
		"default is true/on.",
		WACOM_PROP_SAMPLE, 8, 0, NULL, NULL
	},

	{ "ClickForce",
		"Sets tip/eraser pressure threshold = ClickForce*MaxZ/100 "
		"(default is 6)",
		WACOM_PROP_PRESSURE_THRESHOLD, 32, 0, NULL, NULL
	},

	{ "xyDefault",
		"Resets the bounding coordinates to default in tablet units. ",
		NULL, 0, 0, not_implemented
	},

	{ "mmonitor",
		"Turns on/off across monitor movement in "
		"multi-monitor desktop, default is on ",
		WACOM_PROP_DISPLAY_OPTS, 8, 2,
	},

	{ "STopX0",
		"Screen 0 left coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 0, NULL, NULL
	},

	{ "STopY0",
		"Screen 0 top coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 1, NULL, NULL
	},

	{ "SBottomX0",
		"Screen 0 right coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 2, NULL, NULL
	},

	{ "SBottomY0",
		"Screen 0 bottom coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 3, NULL, NULL
	},

	{ "STopX1",
		"Screen 1 left coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 4, NULL, NULL
	},

	{ "STopY1",
		"Screen 1 top coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 5, NULL, NULL
	},

	{ "SBottomX1",
		"Screen 1 right coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 6, NULL, NULL
	},

	{ "SBottomY1",
		"Screen 1 bottom coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 7, NULL, NULL
	},

	{ "STopX2",
		"Screen 2 left coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 8, NULL, NULL
	},

	{ "STopY2",
		"Screen 2 top coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 9, NULL, NULL
	},

	{ "SBottomX2",
		"Screen 2 right coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 10, NULL, NULL
	},

	{ "SBottomY2",
		"Screen 2 bottom coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 11, NULL, NULL
	},

	{ "STopX3",
		"Screen 3 left coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 12, NULL, NULL
	},

	{ "STopY3",
		"Screen 3 top coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 13, NULL, NULL
	},

	{ "SBottomX3",
		"Screen 3 right coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 14, NULL, NULL
	},

	{ "SBottomY3",
		"Screen 3 bottom coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 15, NULL, NULL
	},

	{ "STopX4",
		"Screen 4 left coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 16, NULL, NULL
	},

	{ "STopY4",
		"Screen 4 top coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 17, NULL, NULL
	},

	{ "SBottomX4",
		"Screen 4 right coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 18, NULL, NULL
	},

	{ "SBottomY4",
		"Screen 4 bottom coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 19, NULL, NULL
	},

	{ "STopX5",
		"Screen 5 left coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 20, NULL, NULL
	},

	{ "STopY5",
		"Screen 5 top coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 21, NULL, NULL
	},

	{ "SBottomX5",
		"Screen 5 right coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 22, NULL, NULL
	},

	{ "SBottomY5",
		"Screen 5 bottom coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 23, NULL, NULL
	},

	{ "STopX6",
		"Screen 6 left coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 24, NULL, NULL
	},

	{ "STopY6",
		"Screen 6 top coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 25, NULL, NULL
	},

	{ "SBottomX6",
		"Screen 6 right coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 26, NULL, NULL
	},

	{ "SBottomY6",
		"Screen 6 bottom coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 27, NULL, NULL
	},

	{ "STopX7",
		"Screen 7 left coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 28, NULL, NULL
	},

	{ "STopY7",
		"Screen 7 top coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 29, NULL, NULL
	},

	{ "SBottomX7",
		"Screen 7 right coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 30, NULL, NULL
	},

	{ "SBottomY7",
		"Screen 7 bottom coordinate in pixels. ",
		WACOM_PROP_SCREENAREA, 32, 31, NULL, NULL
	},

	{ "ToolID",
		"Returns the ID of the associated device. ",
		WACOM_PROP_TOOL_TYPE, 32, 0, NULL, NULL
	},

	{ "ToolSerial",
		"Returns the serial number of the associated device. ",
		WACOM_PROP_SERIALIDS, 32, 3, NULL, NULL
	},

	{ "TabletID",
		"Returns the tablet ID of the associated device. ",
		WACOM_PROP_SERIALIDS, 32, 0, NULL, NULL
	},

	{ "GetTabletID",
		"Returns the tablet ID of the associated device. ",
		WACOM_PROP_SERIALIDS, 32, 0, NULL, NULL
	},

	{ "NumScreen",
		"Returns number of screens configured for the desktop. ",
		NULL, 0, 0, not_implemented
	},

	{ "XScaling",
		"Returns the status of XSCALING is set or not. ",
		NULL, 0, 0, not_implemented
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
	" list [dev|param]           - display known devices, parameters \n"
	" list mod                   - display supported modifier and specific keys for keystokes [not implemented}\n"
	" set dev_name param [values...] - set device parameter by name\n"
	" get dev_name param [param...] - get current device parameter(s) value by name\n");
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
		dev = XOpenDevice(display, found->id);

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

static void set(Display *dpy, int argc, char **argv)
{
	param_t *param;
	XDevice *dev = NULL;
	Atom prop, type;
	int format;
	unsigned char* data;
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

	if (argc < 2)
	{
		usage();
		return;
	}

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

	struct option options[] = {
		{"help", 0, NULL, 0},
		{"verbose", 0, NULL, 0},
		{"version", 0, NULL, 0},
		{"display", 1, (int*)display, 0},
		{"shell", 0, NULL, 0},
		{"xconf", 0, NULL, 0},
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
						printf("Not implemented\n");
						break;
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

	dpy = XOpenDisplay(display);
	if (!dpy)
	{
		printf("Failed to open Display %s.\n", display ? display : "");
		return -1;
	}

	if (optind < argc)
	{
		if (strcmp(argv[optind], "list") == 0)
			list(dpy, argc - (optind + 1), &argv[optind + 1]);
		else if (strcmp(argv[optind], "set") == 0)
			set(dpy, argc - (optind + 1), &argv[optind + 1]);
		else if (strcmp(argv[optind], "get") == 0)
			get(dpy, argc - (optind + 1), &argv[optind + 1]);
		else
			usage();
	}

	XCloseDisplay(dpy);
	return 0;
}


/* vim: set noexpandtab shiftwidth=8: */
