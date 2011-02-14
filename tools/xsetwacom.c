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

#ifdef HAVE_CONFIG_H
#define WACOM_TOOLS
#include "config.h"
#endif

#include <wacom-properties.h>
#include "Xwacom.h"

#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <math.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/Xrandr.h>
#include <X11/XKBlib.h>

#define TRACE(...) \
	if (verbose) fprintf(stderr, "... " __VA_ARGS__)

#define ArrayLength(a) ((unsigned int)(sizeof(a) / (sizeof((a)[0]))))

static int verbose = False;

enum printformat {
	FORMAT_DEFAULT,
	FORMAT_XORG_CONF,
	FORMAT_SHELL
};

enum prop_flags {
	PROP_FLAG_BOOLEAN = 1,
	PROP_FLAG_READONLY = 2,
	PROP_FLAG_WRITEONLY = 4
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
	const char *desc;	/* description */
	const char *prop_name;	/* property name */
	const int prop_format;	/* property format */
	const int prop_offset;	/* offset (index) into the property values */
	const int prop_extra;   /* extra number of items after first one */
	const unsigned int prop_flags;
	void (*set_func)(Display *dpy, XDevice *dev, struct _param *param, int argc, char **argv); /* handler function, if appropriate */
	void (*get_func)(Display *dpy, XDevice *dev, struct _param *param, int argc, char **argv); /* handler function for getting, if appropriate */

	/* filled in by get() */
	char *device_name;
	enum printformat printformat;
} param_t;


/* get_func/set_func calls for special parameters */
static void map_button(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static void map_wheels(Display *dpy, XDevice *dev, param_t* param, int argc, char **argv);
static void set_mode(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static void get_mode(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static void get_button(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static void set_rotate(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static void get_rotate(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static void set_xydefault(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static void get_all(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static void get_param(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);
static void set_output(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv);

/* NOTE: When removing or changing a parameter name, add to
 * deprecated_parameters.
 */
static param_t parameters[] =
{
	{
		.name = "Area",
		.desc = "Valid tablet area in device coordinates.",
		.prop_name = WACOM_PROP_TABLET_AREA,
		.prop_format = 32,
		.prop_offset = 0,
		.prop_extra = 3
	},
	{
		.name = "Button",
		.desc = "X11 event to which the given button should be mapped. ",
		.set_func = map_button,
		.get_func = get_button,
	},
	{
		.name = "ToolDebugLevel",
		.desc = "Level of debugging trace for individual tools, "
		"default is 0 (off). ",
		.prop_name = WACOM_PROP_DEBUGLEVELS,
		.prop_format = 8,
		.prop_offset = 0,
	},
	{
		.name = "TabletDebugLevel",
		.desc = "Level of debugging statements applied to shared "
		"code paths between all tools "
		"associated with the same tablet. default is 0 (off). ",
		.prop_name = WACOM_PROP_DEBUGLEVELS,
		.prop_format = 8,
		.prop_offset = 1,
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
		.name = "PressureCurve",
		.desc = "Bezier curve for pressure (default is 0 0 100 100). ",
		.prop_name = WACOM_PROP_PRESSURECURVE,
		.prop_format = 32,
		.prop_offset = 0,
		.prop_extra = 3,
	},
	{
		.name = "Mode",
		.desc = "Switches cursor movement mode (default is absolute/on). ",
		.set_func = set_mode,
		.get_func = get_mode,
	},
	{
		.name = "TabletPCButton",
		.desc = "Turns on/off Tablet PC buttons. "
		"default is off for regular tablets, "
		"on for Tablet PC. ",
		.prop_name = WACOM_PROP_HOVER,
		.prop_format = 8,
		.prop_offset = 0,
		.prop_flags = PROP_FLAG_BOOLEAN
	},
	{
		.name = "Touch",
		.desc = "Turns on/off Touch events (default is enable/on). ",
		.prop_name = WACOM_PROP_TOUCH,
		.prop_format = 8,
		.prop_offset = 0,
		.prop_flags = PROP_FLAG_BOOLEAN
	},
	{
		.name = "Gesture",
		.desc = "Turns on/off multi-touch gesture events "
		"(default is enable/on). ",
		.prop_name = WACOM_PROP_ENABLE_GESTURE,
		.prop_format = 8,
		.prop_offset = 0,
		.prop_flags = PROP_FLAG_BOOLEAN
	},
	{
		.name = "ZoomDistance",
		.desc = "Minimum distance for a zoom gesture "
		"(default is 50). ",
		.prop_name = WACOM_PROP_GESTURE_PARAMETERS,
		.prop_format = 32,
		.prop_offset = 0,
	},
	{
		.name = "ScrollDistance",
		.desc = "Minimum motion before sending a scroll gesture "
		"(default is 20). ",
		.prop_name = WACOM_PROP_GESTURE_PARAMETERS,
		.prop_format = 32,
		.prop_offset = 1,
	},
	{
		.name = "TapTime",
		.desc = "Minimum time between taps for a right click "
		"(default is 250). ",
		.prop_name = WACOM_PROP_GESTURE_PARAMETERS,
		.prop_format = 32,
		.prop_offset = 2,
	},
	{
		.name = "Capacity",
		.desc = "Touch sensitivity level (default is 3, "
		"-1 for non-capacitive tools).",
		.prop_name = WACOM_PROP_CAPACITY,
		.prop_format = 32,
		.prop_offset = 0,
	},
	{
		.name = "CursorProximity",
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
		"Values = none, cw, ccw, half (default is none).",
		.prop_name = WACOM_PROP_ROTATION,
		.set_func = set_rotate,
		.get_func = get_rotate,
	},
	{
		.name = "RelWheelUp",
		.desc = "X11 event to which relative wheel up should be mapped. ",
		.prop_name = WACOM_PROP_WHEELBUTTONS,
		.prop_format = 8,
		.prop_offset = 0,
		.set_func = map_wheels,
	},
	{
		.name = "RelWheelDown",
		.desc = "X11 event to which relative wheel down should be mapped. ",
		.prop_name = WACOM_PROP_WHEELBUTTONS,
		.prop_format = 8,
		.prop_offset = 1,
		.set_func = map_wheels,
	},
	{
		.name = "AbsWheelUp",
		.desc = "X11 event to which absolute wheel up should be mapped. ",
		.prop_name = WACOM_PROP_WHEELBUTTONS,
		.prop_format = 8,
		.prop_offset = 2,
		.set_func = map_wheels,
	},
	{
		.name = "AbsWheelDown",
		.desc = "X11 event to which absolute wheel down should be mapped. ",
		.prop_name = WACOM_PROP_WHEELBUTTONS,
		.prop_format = 8,
		.prop_offset = 3,
		.set_func = map_wheels,
	},
	{
		.name = "StripLeftUp",
		.desc = "X11 event to which left strip up should be mapped. ",
		.prop_name = WACOM_PROP_STRIPBUTTONS,
		.prop_format = 8,
		.prop_offset = 0,
		.set_func = map_wheels,
	},
	{
		.name = "StripLeftDown",
		.desc = "X11 event to which left strip down should be mapped. ",
		.prop_name = WACOM_PROP_STRIPBUTTONS,
		.prop_format = 8,
		.prop_offset = 1,
		.set_func = map_wheels,
	},
	{
		.name = "StripRightUp",
		.desc = "X11 event to which right strip up should be mapped. ",
		.prop_name = WACOM_PROP_STRIPBUTTONS,
		.prop_format = 8,
		.prop_offset = 2,
		.set_func = map_wheels,
	},
	{
		.name = "StripRightDown",
		.desc = "X11 event to which right strip down should be mapped. ",
		.prop_name = WACOM_PROP_STRIPBUTTONS,
		.prop_format = 8,
		.prop_offset = 3,
		.set_func = map_wheels,
	},
	{
		.name = "RawFilter",
		.desc = "Enables and disables filtering of raw data, "
		"default is true/on.",
		.prop_name = WACOM_PROP_SAMPLE,
		.prop_format = 8,
		.prop_offset = 0,
		.prop_flags = PROP_FLAG_BOOLEAN
	},
	{
		.name = "Threshold",
		.desc = "Sets tip/eraser pressure threshold "
		"(default is 27)",
		.prop_name = WACOM_PROP_PRESSURE_THRESHOLD,
		.prop_format = 32,
		.prop_offset = 0,
	},
	{
		.name = "ResetArea",
		.desc = "Resets the bounding coordinates to default in tablet units. ",
		.prop_name = WACOM_PROP_TABLET_AREA,
		.prop_format = 32,
		.prop_offset = 0,
		.prop_flags = PROP_FLAG_WRITEONLY,
		.set_func = set_xydefault,
	},
	{
		.name = "ToolID",
		.desc = "Returns the ID of the associated device. ",
		.prop_name = WACOM_PROP_TOOL_TYPE,
		.prop_format = 32,
		.prop_offset = 0,
		.prop_flags = PROP_FLAG_READONLY
	},
	{
		.name = "ToolSerial",
		.desc = "Returns the serial number of the associated device. ",
		.prop_name = WACOM_PROP_SERIALIDS,
		.prop_format = 32,
		.prop_offset = 3,
		.prop_flags = PROP_FLAG_READONLY
	},
	{
		.name = "TabletID",
		.desc = "Returns the tablet ID of the associated device. ",
		.prop_name = WACOM_PROP_SERIALIDS,
		.prop_format = 32,
		.prop_offset = 0,
		.prop_flags = PROP_FLAG_READONLY
	},
	{
		.name = "MapToOutput",
		.desc = "Map the device to the given output. ",
		.set_func = set_output,
		.prop_flags = PROP_FLAG_WRITEONLY
	},
	{
		.name = "all",
		.desc = "Get value for all parameters.",
		.get_func = get_all,
		.prop_flags = PROP_FLAG_READONLY,
	},
	{ NULL }
};

/**
 * Deprecated parameters and their respective replacements.
 */
struct deprecated
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
	{"GetTabletID",	"TabletID"},
	{"PressCurve",	"PressureCurve"},
	{"TPCButton",	"TabletPCButton"},
	{"CursorProx",	"CursorProximity"},
	{"xyDefault",	"ResetArea"},
	{"ClickForce",	"Threshold"},
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
		printf("Paramater '%s' is no longer in use. "
			"It was replaced with '%s'.\n", name, d->replacement);

	return is_deprecated;

}

struct modifier {
	char *name;
	char *converted;
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

	{"tab", "Tab"},

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

static void print_value(param_t *param, const char *msg, ...)
{
	va_list va_args;
	va_start(va_args, msg);
	switch(param->printformat)
	{
		case FORMAT_XORG_CONF:
			printf("Option \"%s\" \"", param->name);
			vprintf(msg, va_args);
			printf("\"\n");
			break;
		case FORMAT_SHELL:
			printf("xsetwacom set \"%s\" \"%s\" \"",
					param->device_name, param->name);
			vprintf(msg, va_args);
			printf("\"\n");
			break;
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
	" -d, --display \"display\"  - override default display\n"
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
				printf("%-32s	id: %ld	type: %-10s\n",
						info->name, info->id,
						type_name);
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
		printf("%-16s - %16s\n", param->name, param->desc);
		param++;
	}
}

static void list_mod(Display *dpy)
{
	struct modifier *m = modifiers;

	printf("%d modifiers are supported:\n", ArrayLength(modifiers) - 1);
	while(m->name)
		printf("	%s\n", m++->name);

	printf("\n%d specialkeys are supported:\n", ArrayLength(specialkeys) - 1);
	m = specialkeys;
	while(m->name)
		printf("	%s\n", m++->name);
}

static void list(Display *dpy, int argc, char **argv)
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
	else
		printf("unknown argument to list.\n");
}
/*
 * Convert a list of random special keys to strings that can be passed into
 * XStringToKeysym
 */
static char *convert_specialkey(const char *modifier)
{
	struct modifier *m = modifiers;

	while(m->name && strcasecmp(modifier, m->name))
		m++;

	if (!m->name)
	{
		m = specialkeys;
		while(m->name && strcasecmp(modifier, m->name))
			m++;
	}

	return m->converted ? m->converted : (char*)modifier;
}

static int is_modifier(const char* modifier)
{
	const char *modifiers[] = {
		"Control_L",
		"Control_R",
		"Alt_L",
		"Alt_R",
		"Shift_L",
		"Shift_R",
		"Meta_L",
		"Meta_R",
		"Super_L",
		"Super_R",
		"Hyper_L",
		"Hyper_R",
		NULL,
	};

	const char **m = modifiers;

	while(*m)
	{
		if (strcmp(modifier, *m) == 0)
			return 1;
		m++;
	}

	return 0;
}

static int special_map_keystrokes(Display *dpy, int argc, char **argv, unsigned long *ndata, unsigned long* data);
static int special_map_button(Display *dpy, int argc, char **argv, unsigned long *ndata, unsigned long* data);
static int special_map_core(Display *dpy, int argc, char **argv, unsigned long *ndata, unsigned long *data);
static int special_map_modetoggle(Display *dpy, int argc, char **argv, unsigned long *ndata, unsigned long *data);
static int special_map_displaytoggle(Display *dpy, int argc, char **argv, unsigned long *ndata, unsigned long *data);

/* Valid keywords for the --set ButtonX options */
struct keywords {
	const char *keyword;
	int (*func)(Display*, int, char **, unsigned long*, unsigned long *);
} keywords[] = {
	{"key", special_map_keystrokes},
	{"button", special_map_button},
	{"core", special_map_core},
	{"modetoggle", special_map_modetoggle},
	{"displaytoggle", special_map_displaytoggle},
	{ NULL, NULL }
};

/* the "core" keyword isn't supported anymore, we just have this here to
   tell people that. */
static int special_map_core(Display *dpy, int argc, char **argv, unsigned long *ndata, unsigned long *data)
{
	static int once_only = 1;
	if (once_only)
	{
		printf ("Note: The \"core\" keyword is not supported anymore and "
			"will be ignored.\n");
		once_only = 0;
	}
	return 0;
}

static int special_map_modetoggle(Display *dpy, int argc, char **argv, unsigned long *ndata, unsigned long *data)
{
	data[*ndata] = AC_MODETOGGLE;

	*ndata += 1;

	return 0;
}

static int special_map_displaytoggle(Display *dpy, int argc, char **argv, unsigned long *ndata, unsigned long *data)
{
	data[*ndata] = AC_DISPLAYTOGGLE;

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

static int special_map_button(Display *dpy, int argc, char **argv, unsigned long *ndata, unsigned long *data)
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
			return nitems;


		TRACE("Button map %d [%s,%s]\n", abs(button),
				need_press ?  "press" : "",
				need_release ?  "release" : "");

		if (need_press)
			data[*ndata + nitems++] = AC_BUTTON | AC_KEYBTNPRESS | abs(button);
		if (need_release)
			data[*ndata + nitems++] = AC_BUTTON | abs(button);
	}

	*ndata += nitems;
	return nitems;
}

/* Return the first keycode to have the required keysym in the current group.
   TODOs:
   - parse other groups as well (do we need this?)
   - for keysyms not on level 0, return the keycodes for the modifiers as
     well
*/
static int keysym_to_keycode(Display *dpy, KeySym sym)
{
	static XkbDescPtr xkb = NULL;
	XkbStateRec state;
	int group;
	int kc = 0;


	if (!xkb)
		xkb = XkbGetKeyboard(dpy, XkbAllComponentsMask, XkbUseCoreKbd);
	XkbGetState(dpy, XkbUseCoreKbd, &state);
	group = state.group;

	for (kc = xkb->min_key_code; kc <= xkb->max_key_code; kc++)
	{
		KeySym* ks;
		int i;

		ks = XkbKeySymsPtr(xkb, kc);
		for (i = 0; i < XkbKeyGroupWidth(xkb, kc, state.group); i++)
			if (ks[i] == sym)
				goto out;
	}

out:
	return kc;
}
/*
   Map gibberish like "ctrl alt f2" into the matching AC_KEY values.
   Returns 1 on success or 0 otherwise.
 */
static int special_map_keystrokes(Display *dpy, int argc, char **argv, unsigned long *ndata, unsigned long* data)
{
	int i;
	int nitems = 0;

	for (i = 0; i < argc; i++)
	{
		KeySym ks;
		KeyCode kc;
		int need_press = 0, need_release = 0;
		char *key = argv[i];

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
		kc = keysym_to_keycode(dpy, ks);

		if (need_press)
			data[*ndata + nitems++] = AC_KEY | AC_KEYBTNPRESS | kc;
		if (need_release)
			data[*ndata + nitems++] = AC_KEY | kc;

		TRACE("Key map %ld (%d, '%s') [%s,%s]\n", ks, kc,
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
	char buff[1024] = { 0 };
	char **words	= NULL;
	char *tmp, *tok;

	while(argc--)
	{
		if (strlen(buff) + strlen(*argv) + 1 >= sizeof(buff))
			break;

		strcat(buff, *argv);
		strcat(buff, " ");
		argv++;
	}

	*nwords = 0;

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

static const char *wheel_act_prop[] = {
	"Wacom Rel Wheel Up Action",
	"Wacom Rel Wheel Down Action",
	"Wacom Abs Wheel Up Action",
	"Wacom Abs Wheel Down Action",
};

/**
 * Convert the given property from an 8 bit integer property into an action
 * atom property. In the default case, this means that a property with
 * values "4 5 4 5" ends up to have the values
 * "Wacom RHU Action" "Wacom RHW Action" "Wacom AWU Action" "Wacom AWD
 * Action"
 * with each of the properties having :
 * AC_BUTTON | AC_KEYBTNPRESS | 4 (or 5)
 * AC_BUTTON | 4 (or 5)
 *
 * return 0 on success or 1 on failure.
 */
static int convert_wheel_prop(Display *dpy, XDevice *dev, Atom btnact_prop)
{
	int i;
	Atom type;
	int format;
	unsigned long btnact_nitems, bytes_after;
	unsigned char *btnact_data; /* current values (button mappings) */
	unsigned long *btnact_new_data; /* new values (action atoms) */

	XGetDeviceProperty(dpy, dev, btnact_prop, 0, 100, False,
				AnyPropertyType, &type, &format, &btnact_nitems,
				&bytes_after, (unsigned char**)&btnact_data);

	btnact_new_data = calloc(btnact_nitems, sizeof(Atom));
	if (!btnact_new_data)
		return 1;

	for (i = 0; i < btnact_nitems; i++) {
		unsigned long action_data[2];
		Atom prop = XInternAtom(dpy, wheel_act_prop[i], False);

		action_data[0] = AC_BUTTON | AC_KEYBTNPRESS | btnact_data[i];
		action_data[1] = AC_BUTTON | btnact_data[i];

		XChangeDeviceProperty(dpy, dev, prop, XA_INTEGER, 32,
				      PropModeReplace,
				      (unsigned char*)action_data, 2);

		btnact_new_data[i] = prop;
	}

	XChangeDeviceProperty(dpy, dev, btnact_prop, XA_ATOM, 32,
				PropModeReplace,
				(unsigned char*)btnact_new_data, btnact_nitems);
	return 0;
}


static void special_map_property(Display *dpy, XDevice *dev, Atom btnact_prop, int offset, int argc, char **argv)
{
	unsigned long *data, *btnact_data;
	Atom type, prop = 0;
	int format;
	unsigned long btnact_nitems, nitems, bytes_after;
	int need_update = 0;
	int i;
	int nwords = 0;
	char **words = NULL;

	XGetDeviceProperty(dpy, dev, btnact_prop, 0, 100, False,
				AnyPropertyType, &type, &format, &btnact_nitems,
				&bytes_after, (unsigned char**)&btnact_data);

	if (offset > btnact_nitems)
		return;

	/* Prop is currently 8 bit integer, i.e. plain button
	 * mappings. Convert to 32 bit Atom actions first.
	 */
	if (format == 8 && type == XA_INTEGER)
	{
		if (convert_wheel_prop(dpy, dev, btnact_prop))
			return;

		XGetDeviceProperty(dpy, dev, btnact_prop, 0, 100, False,
				   AnyPropertyType, &type, &format,
				   &btnact_nitems, &bytes_after,
				   (unsigned char**)&btnact_data);
	}

	if (argc == 0) /* unset property */
	{
		prop = btnact_data[offset];
		btnact_data[offset] = 0;
	} else if (btnact_data[offset])
		/* some atom already assigned, modify that */
		prop = btnact_data[offset];
	else
	{
		char buff[64];
		sprintf(buff, "Wacom button action %d", (offset + 1));
		prop = XInternAtom(dpy, buff, False);

		btnact_data[offset] = prop;
		need_update = 1;
	}

	data = calloc(sizeof(long), 256);
	nitems = 0;

	/* translate cmdline commands */
	words = strjoinsplit(argc, argv, &nwords);
	for (i = 0; i < nwords; i++)
	{
		int j = 0;
		while (keywords[j].keyword && i < nwords)
		{
			int parsed = 0;
			if (strcasecmp(words[i], keywords[j].keyword) == 0)
			{
				parsed = keywords[j].func(dpy, nwords - i - 1,
							  &words[i + 1],
							  &nitems, data);
				i += parsed;
			}
			if (parsed)
				j = parsed = 0; /* restart with first keyword */
			else
				j++;
		}
	}

	if (argc > 0) /* unset property */
		XChangeDeviceProperty(dpy, dev, prop, XA_INTEGER, 32,
					PropModeReplace,
					(unsigned char*)data, nitems);

	XChangeDeviceProperty(dpy, dev, btnact_prop, XA_ATOM, 32,
				PropModeReplace,
				(unsigned char*)btnact_data,
				btnact_nitems);

	if (argc == 0 && prop)
		XDeleteDeviceProperty(dpy, dev, prop);

	XFlush(dpy);
}


static void special_map_wheels(Display *dpy, XDevice *dev, param_t* param, int argc, char **argv)
{
	Atom wheel_prop;

	wheel_prop = XInternAtom(dpy, param->prop_name, True);
	if (!wheel_prop)
		return;

	TRACE("Wheel property %s (%ld)\n", param->prop_name, wheel_prop);

	special_map_property(dpy, dev, wheel_prop, param->prop_offset, argc, argv);
}

static void map_wheels(Display *dpy, XDevice *dev, param_t* param, int argc, char **argv)
{
	if (argc <= 0)
		return;

	TRACE("Mapping wheel %s for device %ld.\n", param->name, dev->device_id);

	/* FIXME:
	   if value is simple number, change back to 8 bit integer
	 */

	special_map_wheels(dpy, dev, param, argc, argv);
}

/* Handles complex button mappings through button actions. */
static void special_map_buttons(Display *dpy, XDevice *dev, param_t* param,
				int button, int argc, char **argv)
{
	Atom btnact_prop;

	TRACE("Special %s map for device %ld.\n", param->name, dev->device_id);

	btnact_prop = XInternAtom(dpy, "Wacom Button Actions", True);
	if (!btnact_prop)
		return;

	button--; /* property is zero-indexed, button numbers are 1-indexed */

	special_map_property(dpy, dev, btnact_prop, button, argc, argv);
}


static void map_button_simple(Display *dpy, XDevice *dev, param_t* param,
			      int button, int mapping)
{
	int nmap = 256;
	unsigned char map[nmap];

	nmap = XGetDeviceButtonMapping(dpy, dev, map, nmap);
	if (button > nmap)
	{
		fprintf(stderr, "Button number does not exist on device.\n");
		return;
	}

	TRACE("Mapping button %d to %d.\n", button, mapping);

	map[button - 1] = mapping;
	XSetDeviceButtonMapping(dpy, dev, map, nmap);
	XFlush(dpy);

	/* If there's a property set, unset it */
	special_map_buttons(dpy, dev, param, button, 0, NULL);
}
/*
   Supports two variations, simple mapping and special mapping:
   xsetwacom set device Button1 1
	- maps button 1 to logical button 1
   xsetwacom set device Button1 "key a b c d"
	- maps button 1 to key events a b c d
 */
static void map_button(Display *dpy, XDevice *dev, param_t* param, int argc, char **argv)
{
	int button, /* button to be mapped */
	    mapping;

	if (argc <= 1 || (sscanf(argv[0], "%d", &button) != 1))
		return;

	TRACE("Mapping %s for device %ld.\n", param->name, dev->device_id);


	/* --set "device" Button1 3 */
	if (sscanf(argv[1], "%d", &mapping) == 1)
		map_button_simple(dpy, dev, param, button, mapping);
	else
		special_map_buttons(dpy, dev, param, button, argc - 1, &argv[1]);
}

static void set_xydefault(Display *dpy, XDevice *dev, param_t* param, int argc, char **argv)
{
	Atom prop, type;
	int format;
	unsigned char* data = NULL;
	unsigned long nitems, bytes_after;
	long *ldata;

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
		fprintf(stderr, "Property offset doesn't exist, this is a bug.\n");
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
	free(data);
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

	if (strcasecmp(argv[0], "cw") == 0)
		rotation = 1;
	else if (strcasecmp(argv[0], "cww") == 0)
		rotation = 2;
	else if (strcasecmp(argv[0], "half") == 0)
		rotation = 3;
	else if (strcasecmp(argv[0], "none") == 0)
		rotation = 0;
	else if (strlen(argv[0]) == 1)
	{
		rotation = atoi(argv[0]);
		if (rotation < 0 || rotation > 3)
			goto error;
	}

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
	fprintf(stderr, "Usage: xsetwacom <device name> Rotate [none | cw | ccw | half]\n");
	return;
}

static int convert_value_from_user(param_t *param, char *value)
{
	int val;

	if ((param->prop_flags & PROP_FLAG_BOOLEAN) && strcmp(value, "off") == 0)
			val = 0;
	else if ((param->prop_flags & PROP_FLAG_BOOLEAN) && strcmp(value, "on") == 0)
			val = 1;
	else
		val = atoi(value);

	return val;
}

static void set(Display *dpy, int argc, char **argv)
{
	param_t *param;
	XDevice *dev = NULL;
	Atom prop = None, type;
	int format;
	unsigned char* data = NULL;
	unsigned long nitems, bytes_after;
	double val;
	long *n;
	char *b;
	int i;
	char **values;
	int nvals;

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
		if (is_deprecated_parameter(argv[1]))
			goto out;
		printf("Unknown parameter name '%s'.\n", argv[1]);
		goto out;
	} else if (param->prop_flags & PROP_FLAG_READONLY)
	{
		printf("'%s' is a read-only option.\n", argv[1]);
		goto out;
	}

	if (param->prop_name)
	{
		prop = XInternAtom(dpy, param->prop_name, True);
		if (!prop || !test_property(dpy, dev, prop))
		{
			printf("Property '%s' does not exist on device.\n",
				param->prop_name);
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
		goto out;
	}

	values = strjoinsplit(argc - 2, &argv[2], &nvals);

	for (i = 0; i < nvals; i++)
	{
		val = convert_value_from_user(param, values[i]);

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

	for (i = 0; i < nvals; i++)
		free(values[i]);
	free(values);
out:
	XCloseDevice(dpy, dev);
	free(data);
}

static void get_mode(Display *dpy, XDevice *dev, param_t* param, int argc, char **argv)
{
	XDeviceInfo *info, *d = NULL;
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
			print_value(param, "%s", (v->mode == Absolute) ? "Absolute" : "Relative");
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

	return;
}

static int get_special_button_map(Display *dpy, XDevice *dev,
				  param_t *param, int btn_no)
{
	Atom btnact_prop, action_prop;
	unsigned long *btnact_data;
	Atom type;
	int format;
	unsigned long btnact_nitems, bytes_after;
	int i;
	char buff[1024] = {0};

	btnact_prop = XInternAtom(dpy, "Wacom Button Actions", True);

	if (!btnact_prop)
		return 0;

	XGetDeviceProperty(dpy, dev, btnact_prop, 0, 100, False,
			   AnyPropertyType, &type, &format, &btnact_nitems,
			   &bytes_after, (unsigned char**)&btnact_data);

	/* button numbers start at 1, property is zero-indexed */
	if (btn_no >= btnact_nitems)
		return 0;

	/* FIXME: doesn't cover wheels/strips at the moment, they can be 8
	 * bits (plain buttons) or 32 bits (complex actions) */

	action_prop = btnact_data[btn_no - 1];
	if (!action_prop)
		return 0;

	XFree(btnact_data);

	XGetDeviceProperty(dpy, dev, action_prop, 0, 100, False,
			   AnyPropertyType, &type, &format, &btnact_nitems,
			   &bytes_after, (unsigned char**)&btnact_data);

	if (format != 32 && type != XA_ATOM)
		return 0;

	for (i = 0; i < btnact_nitems; i++)
	{
		static int last_type, last_press;
		unsigned long action = btnact_data[i];
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
				detail = XKeycodeToKeysym(dpy, detail, 0);
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
		else
			sprintf(str, "%c%d ", press_str, detail);
		strcat(buff, str);
		last_type = current_type;
		last_press = is_press;
	}

	TRACE("%s\n", buff);

	XFree(btnact_data);

	print_value(param, "%s", buff);

	return 1;
}

static void get_button(Display *dpy, XDevice *dev, param_t *param, int argc,
			char **argv)
{
	int nmap = 256;
	unsigned char map[nmap];
	int button = 0;

	if (argc < 1 || (sscanf(argv[0], "%d", &button) != 1))
		return;

	TRACE("Getting button map for device %ld.\n", dev->device_id);

	/* if there's a special map, print it and return */
	if (get_special_button_map(dpy, dev, param, button))
		return;

	nmap = XGetDeviceButtonMapping(dpy, dev, map, nmap);

	if (button > nmap)
	{
		fprintf(stderr, "Button number does not exist on device.\n");
		return;
	}

	print_value(param, "%d", map[button - 1]);

	XSetDeviceButtonMapping(dpy, dev, map, nmap);
	XFlush(dpy);
}

static void _set_matrix_prop(Display *dpy, XDevice *dev, const float fmatrix[9])
{
	Atom matrix_prop = XInternAtom(dpy, "Coordinate Transformation Matrix", True);
	Atom type;
	int format;
	unsigned long nitems, bytes_after;
	float *data;
	long matrix[9] = {0};
	int i;

	if (!matrix_prop)
	{
		fprintf(stderr, "Server does not support transformation");
		return;
	}

	/* XI1 expects 32 bit properties (including float) as long,
	 * regardless of architecture */
	for (i = 0; i < sizeof(matrix)/sizeof(matrix[0]); i++)
		*(float*)(matrix + i) = fmatrix[i];

	XGetDeviceProperty(dpy, dev, matrix_prop, 0, 9, False,
				AnyPropertyType, &type, &format, &nitems,
				&bytes_after, (unsigned char**)&data);

	if (format != 32 || type != XInternAtom(dpy, "FLOAT", True))
		return;

	XChangeDeviceProperty(dpy, dev, matrix_prop, type, format,
			      PropModeReplace, (unsigned char*)matrix, 9);
	XFree(data);
	XFlush(dpy);
}

static void set_output(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv)
{
	int min, maj;
	int i, found = 0;
	char *output_name;
	XRRScreenResources *res;
	XRROutputInfo *output_info;
	XRRCrtcInfo *crtc_info;

	output_name = argv[0];

	if (!XRRQueryExtension(dpy, &maj, &min)) /* using min/maj as dummy */
	{
		fprintf(stderr, "Server does not support RandR");
		return;
	}

	if (!XRRQueryVersion(dpy, &maj, &min) ||
	    (maj * 1000 + min) < 1002)
	{
		fprintf(stderr, "Server does not support RandR 1.2");
		return;
	}


	res = XRRGetScreenResources(dpy, DefaultRootWindow(dpy));

	for (i = 0; i < res->noutput && !found; i++)
	{
		output_info = XRRGetOutputInfo(dpy, res, res->outputs[i]);

		TRACE("Found output '%s' (%s)\n", output_info->name,
		      output_info->connection == RR_Connected ? "connected" : "disconnnected");

		if (!output_info->crtc || output_info->connection != RR_Connected)
			continue;

		crtc_info = XRRGetCrtcInfo (dpy, res, output_info->crtc);
		TRACE("CRTC (%dx%d) %dx%d\n", crtc_info->x, crtc_info->y,
			crtc_info->width, crtc_info->height);

		if (strcmp(output_info->name, output_name) == 0)
		{
			found = 1;
			break;
		}
	}

	/* crtc holds our screen info, need to compare to actual screen size */
	if (found)
	{
		int width = DisplayWidth(dpy, DefaultScreen(dpy));
		int height = DisplayHeight(dpy, DefaultScreen(dpy));

		/* offset */
		float x = 1.0 * crtc_info->x/width;
		float y = 1.0 * crtc_info->y/height;

		/* mapping */
		float w = 1.0 * crtc_info->width/width;
		float h = 1.0 * crtc_info->height/height;

		float matrix[9] = { 1, 0, 0,
				    0, 1, 0,
				    0, 0, 1};
		matrix[2] = x;
		matrix[5] = y;
		matrix[0] = w;
		matrix[4] = h;

		TRACE("Transformation matrix:\n");
		TRACE("	[ %f %f %f ]\n", matrix[0], matrix[1], matrix[2]);
		TRACE("	[ %f %f %f ]\n", matrix[3], matrix[4], matrix[5]);
		TRACE("	[ %f %f %f ]\n", matrix[6], matrix[7], matrix[8]);

		_set_matrix_prop(dpy, dev, matrix);
	} else
		printf("Unable to find output '%s'. "
			"Output may not be connected.\n", output_name);

	XRRFreeScreenResources(res);
}

static void get_all(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv)
{
	param_t *p = parameters;

	if (param->printformat == FORMAT_DEFAULT)
		param->printformat = FORMAT_XORG_CONF;

	while(p->name)
	{
		if (p != param && !(p->prop_flags & PROP_FLAG_WRITEONLY))
		{
			p->device_name = param->device_name;
			p->printformat = param->printformat;
			get_param(dpy, dev, p, argc, argv);
		}
		p++;
	}
}

static void get(Display *dpy, enum printformat printformat, int argc, char **argv)
{
	param_t *param;
	XDevice *dev = NULL;

	TRACE("'get' requested for '%s'.\n", argv[0]);

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
		if (is_deprecated_parameter(argv[1]))
			return;
		printf("Unknown parameter name '%s'.\n", argv[1]);
		return;
	} else if (param->prop_flags & PROP_FLAG_WRITEONLY)
	{
		printf("'%s' is a write-only option.\n", argv[1]);
		return;
	} else
	{
		param->printformat = printformat;
		param->device_name = argv[0];
	}

	get_param(dpy, dev, param, argc - 2, &argv[2]);

	XCloseDevice(dpy, dev);
}

static void get_param(Display *dpy, XDevice *dev, param_t *param, int argc, char **argv)
{
	Atom prop = None, type;
	int format;
	unsigned char* data;
	unsigned long nitems, bytes_after;
	int i;
	char str[100] = {0};

	if (param->prop_name)
	{
		prop = XInternAtom(dpy, param->prop_name, True);
		if (!prop || !test_property(dpy, dev, prop))
		{
			printf("Property '%s' does not exist on device.\n",
				param->prop_name);
			return;
		}
	}

	if (param->get_func)
	{
		param->get_func(dpy, dev, param, argc, argv);
		return;
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
			for (i = 0; i < 1 + param->prop_extra; i++)
			{
				int val = data[param->prop_offset + i];

				if (param->prop_flags & PROP_FLAG_BOOLEAN)
					sprintf(&str[strlen(str)], "%s", val ?  "on" : "off");
				else
					sprintf(&str[strlen(str)], "%d", val);

				if (i < param->prop_extra)
					strcat(str, " ");
			}
			print_value(param, "%s", str);
			break;
		case 32:
			for (i = 0; i < 1 + param->prop_extra; i++)
			{
				long *ldata = (long*)data;
				sprintf(&str[strlen(str)], "%ld", ldata[param->prop_offset + i]);

				if (i < param->prop_extra)
					strcat(str, " ");
			}
			print_value(param, "%s", str);
			break;
	}
}


#ifndef BUILD_TEST
int main (int argc, char **argv)
{
	int c;
	int optidx;
	char *display = NULL;
	Display *dpy;
	int do_list = 0, do_set = 0, do_get = 0;
	enum printformat format = FORMAT_DEFAULT;

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

	if (argc < 2)
	{
		usage();
		return 1;
	}

	while ((c = getopt_long(argc, argv, "+hvVd:sx", options, &optidx)) != -1) {
		switch(c)
		{
			case 0:
				switch(optidx)
				{
					case 0: usage(); return 0;
					case 1: verbose = True; break;
					case 2: version(); return 0;
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
		get(dpy, format, argc - optind, &argv[optind]);

	XCloseDisplay(dpy);
	return 0;
}
#endif

#ifdef BUILD_TEST
#include <glib.h>
/**
 * Below are unit-tests to ensure xsetwacom continues to work as expected.
 */

static void test_is_modifier(void)
{
	char i;
	char buff[5];


	g_assert(is_modifier("Control_L"));
	g_assert(is_modifier("Control_R"));
	g_assert(is_modifier("Alt_L"));
	g_assert(is_modifier("Alt_R"));
	g_assert(is_modifier("Shift_L"));
	g_assert(is_modifier("Shift_R"));
	g_assert(is_modifier("Meta_L"));
	g_assert(is_modifier("Meta_R"));
	g_assert(is_modifier("Super_L"));
	g_assert(is_modifier("Super_R"));
	g_assert(is_modifier("Hyper_L"));
	g_assert(is_modifier("Hyper_R"));

	g_assert(!is_modifier(""));

	/* make sure at least the default keys (ascii 33 - 126) aren't
	 * modifiers */
	for (i = '!'; i <= '~'; i++)
	{
		sprintf(buff, "%c", i);
		g_assert(!is_modifier(buff));
	}
}


int main(int argc, char** argv)
{
	g_test_init(&argc, &argv, NULL);
	g_test_add_func("/xsetwacom/is_modifier", test_is_modifier);
	return g_test_run();
}

#endif
/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
