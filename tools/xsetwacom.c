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

#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>

static int verbose = False;

typedef struct _param
{
	const char *name;	/* param name as specified by the user */
	const char *desc;	/* description */
} param_t;

static param_t parameters[] =
{
	{ "TopX",
		"Bounding rect left coordinate in tablet units. ",
	},

	{ "TopY",
		"Bounding rect top coordinate in tablet units . ",
	},

	{ "BottomX",
		"Bounding rect right coordinate in tablet units. ",
	},

	{ "BottomY",
		"Bounding rect bottom coordinate in tablet units. ",
	},

	{ "Button1",
		"X11 event to which button 1 should be mapped. ",
	},

	{ "Button2",
		"X11 event to which button 2 should be mapped. ",
	},

	{ "Button3",
		"X11 event to which button 3 should be mapped. ",
	},

	{ "Button4",
		"X11 event to which button 4 should be mapped. ",
	},

	{ "Button5",
		"X11 event to which button 5 should be mapped. ",
	},

	{ "Button6",
		"X11 event to which button 6 should be mapped. ",
	},

	{ "Button7",
		"X11 event to which button 7 should be mapped. ",
	},

	{ "Button8",
		"X11 event to which button 8 should be mapped. ",
	},

	{ "Button9",
		"X11 event to which button 9 should be mapped. ",
	},

	{ "Button10",
		"X11 event to which button 10 should be mapped. ",
	},

	{ "Button11",
		"X11 event to which button 11 should be mapped. ",
	},

	{ "Button12",
		"X11 event to which button 12 should be mapped. ",
	},

	{ "Button13",
		"X11 event to which button 13 should be mapped. ",
	},

	{ "Button14",
		"X11 event to which button 14 should be mapped. ",
	},

	{ "Button15",
		"X11 event to which button 15 should be mapped. ",
	},

	{ "Button16",
		"X11 event to which button 16 should be mapped. ",
	},

	{ "Button17",
		"X11 event to which button 17 should be mapped. ",
	},

	{ "Button18",
		"X11 event to which button 18 should be mapped. ",
	},

	{ "Button19",
		"X11 event to which button 19 should be mapped. ",
	},

	{ "Button20",
		"X11 event to which button 20 should be mapped. ",
	},

	{ "Button21",
		"X11 event to which button 21 should be mapped. ",
	},

	{ "Button22",
		"X11 event to which button 22 should be mapped. ",
	},

	{ "Button23",
		"X11 event to which button 23 should be mapped. ",
	},
	{ "Button24",
		"X11 event to which button 24 should be mapped. ",
	},

	{ "Button25",
		"X11 event to which button 25 should be mapped. ",
	},

	{ "Button26",
		"X11 event to which button 26 should be mapped. ",
	},

	{ "Button27",
		"X11 event to which button 27 should be mapped. ",
	},

	{ "Button28",
		"X11 event to which button 28 should be mapped. ",
	},

	{ "Button29",
		"X11 event to which button 29 should be mapped. ",
	},

	{ "Button30",
		"X11 event to which button 30 should be mapped. ",
	},

	{ "Button31",
		"X11 event to which button 31 should be mapped. ",
	},

	{ "Button32",
		"X11 event to which button 32 should be mapped. ",
	},

	{ "DebugLevel",
		"Level of debugging trace for individual devices, "
		"default is 0 (off). ",
	},

	{ "CommonDBG",
		"Level of debugging statements applied to all devices "
		"associated with the same tablet. default is 0 (off). ",
	},

	{ "Suppress",
		"Number of points trimmed, default is 2. ",
	},

	{ "RawSample",
		"Number of raw data used to filter the points, "
		"default is 4. ",
	},

	{ "Screen_No",
		"Sets/gets screen number the tablet is mapped to, "
		"default is -1. ",
	},

	{ "PressCurve",
		"Bezier curve for pressure (default is 0 0 100 100). ",
	},

	{ "TwinView",
		"Sets the mapping to TwinView horizontal/vertical/none. "
		"Values = none, vertical, horizontal (default is none).",
	},

	{ "Mode",
		"Switches cursor movement mode (default is absolute/on). ",
	},

	{ "TPCButton",
		"Turns on/off Tablet PC buttons. "
		"default is off for regular tablets, "
		"on for Tablet PC. ",
	},

	{ "Touch",
		"Turns on/off Touch events (default is enable/on). ",
	},

	{ "Capacity",
		"Touch sensitivity level (default is 3, "
		"-1 for none capacitive tools).",
	},

	{ "CursorProx",
		"Sets cursor distance for proximity-out "
		"in distance from the tablet.  "
		"(default is 10 for Intuos series, "
		"42 for Graphire series).",
	},

	{ "Rotate",
		"Sets the rotation of the tablet. "
		"Values = NONE, CW, CCW, HALF (default is NONE).",
	},

	{ "RelWUp",
		"X11 event to which relative wheel up should be mapped. ",
	},

	{ "RelWDn",
		"X11 event to which relative wheel down should be mapped. ",
	},

	{ "AbsWUp",
		"X11 event to which absolute wheel up should be mapped. ",
	},

	{ "AbsWDn",
		"X11 event to which absolute wheel down should be mapped. ",
	},

	{ "StripLUp",
		"X11 event to which left strip up should be mapped. ",
	},

	{ "StripLDn",
		"X11 event to which left strip down should be mapped. ",
	},

	{ "StripRUp",
		"X11 event to which right strip up should be mapped. ",
	},

	{ "StripRDn",
		"X11 event to which right strip down should be mapped. ",
	},

	{ "TVResolution0",
		"Sets MetaModes option for TwinView Screen 0. ",
	},

	{ "TVResolution1",
		"Sets MetaModes option for TwinView Screen 1. ",
	},

	{ "RawFilter",
		"Enables and disables filtering of raw data, "
		"default is true/on.",
	},

	{ "SpeedLevel",
		"Sets relative cursor movement speed (default is 6). ",
	},

	{ "ClickForce",
		"Sets tip/eraser pressure threshold = ClickForce*MaxZ/100 "
		"(default is 6)",
	},

	{ "Accel",
		"Sets relative cursor movement acceleration "
		"(default is 1)",
	},

	{ "xyDefault",
		"Resets the bounding coordinates to default in tablet units. ",
	},

	{ "mmonitor",
		"Turns on/off across monitor movement in "
		"multi-monitor desktop, default is on ",
	},

	{ "CoreEvent",
		"Turns on/off device to send core event. "
		"default is decided by X driver and xorg.conf ",
	},

	{ "STopX0",
		"Screen 0 left coordinate in pixels. ",
	},

	{ "STopY0",
		"Screen 0 top coordinate in pixels. ",
	},

	{ "SBottomX0",
		"Screen 0 right coordinate in pixels. ",
	},

	{ "SBottomY0",
		"Screen 0 bottom coordinate in pixels. ",
	},

	{ "STopX1",
		"Screen 1 left coordinate in pixels. ",
	},

	{ "STopY1",
		"Screen 1 top coordinate in pixels. ",
	},

	{ "SBottomX1",
		"Screen 1 right coordinate in pixels. ",
	},

	{ "SBottomY1",
		"Screen 1 bottom coordinate in pixels. ",
	},

	{ "STopX2",
		"Screen 2 left coordinate in pixels. ",
	},

	{ "STopY2",
		"Screen 2 top coordinate in pixels. ",
	},

	{ "SBottomX2",
		"Screen 2 right coordinate in pixels. ",
	},

	{ "SBottomY2",
		"Screen 2 bottom coordinate in pixels. ",
	},

	{ "STopX3",
		"Screen 3 left coordinate in pixels. ",
	},

	{ "STopY3",
		"Screen 3 top coordinate in pixels. ",
	},

	{ "SBottomX3",
		"Screen 3 right coordinate in pixels. ",
	},

	{ "SBottomY3",
		"Screen 3 bottom coordinate in pixels. ",
	},

	{ "STopX4",
		"Screen 4 left coordinate in pixels. ",
	},

	{ "STopY4",
		"Screen 4 top coordinate in pixels. ",
	},

	{ "SBottomX4",
		"Screen 4 right coordinate in pixels. ",
	},

	{ "SBottomY4",
		"Screen 4 bottom coordinate in pixels. ",
	},

	{ "STopX5",
		"Screen 5 left coordinate in pixels. ",
	},

	{ "STopY5",
		"Screen 5 top coordinate in pixels. ",
	},

	{ "SBottomX5",
		"Screen 5 right coordinate in pixels. ",
	},

	{ "SBottomY5",
		"Screen 5 bottom coordinate in pixels. ",
	},

	{ "STopX6",
		"Screen 6 left coordinate in pixels. ",
	},

	{ "STopY6",
		"Screen 6 top coordinate in pixels. ",
	},

	{ "SBottomX6",
		"Screen 6 right coordinate in pixels. ",
	},

	{ "SBottomY6",
		"Screen 6 bottom coordinate in pixels. ",
	},

	{ "STopX7",
		"Screen 7 left coordinate in pixels. ",
	},

	{ "STopY7",
		"Screen 7 top coordinate in pixels. ",
	},

	{ "SBottomX7",
		"Screen 7 right coordinate in pixels. ",
	},

	{ "SBottomY7",
		"Screen 7 bottom coordinate in pixels. ",
	},

	{ "ToolID",
		"Returns the ID of the associated device. ",
	},

	{ "ToolSerial",
		"Returns the serial number of the associated device. ",
	},

	{ "TabletID",
		"Returns the tablet ID of the associated device. ",
	},

	{ "GetTabletID",
		"Returns the tablet ID of the associated device. ",
	},

	{ "NumScreen",
		"Returns number of screens configured for the desktop. ",
	},

	{ "XScaling",
		"Returns the status of XSCALING is set or not. ",
	},

	{ NULL }
};


static void usage(void)
{
	printf(
	"Usage: xsetwacom [options] [command [arguments...]]\n"
	"Options:\n"
	" -h, --help                 - usage\n"
	" -v, --verbose              - verbose output\n"
	" -V, --version              - version info\n"
	" -d, --display disp_name    - override default display\n"
	" -s, --shell                - generate shell commands for 'get'\n"
	" -x, --xconf                - generate X.conf lines for 'get'\n");

	printf(
	"\nCommands:\n"
	" list [dev|param]           - display known devices, parameters \n"
	" list mod                   - display supported modifier and specific keys for keystokes\n"
	" set dev_name param [values...] - set device parameter by name\n"
	" get dev_name param [param...] - get current device parameter(s) value by name\n"
	" getdefault dev_name param [param...] - get device parameter(s) default value by name\n");
}


static void version(void)
{
	printf("%d.%d.%d\n", PACKAGE_VERSION_MAJOR, PACKAGE_VERSION_MINOR,
			     PACKAGE_VERSION_PATCHLEVEL);
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
		printf("%-16s - %16s\n", param->name, param->desc);
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

static void set(Display *dpy)
{
}

static void get(Display *dpy)
{
}

static void getdefault(Display *dpy)
{
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
			set(dpy);
		else if (strcmp(argv[optind], "get") == 0)
			get(dpy);
		else if (strcmp(argv[optind], "getdefault") == 0)
			getdefault(dpy);
		else
			usage();
	}

	XCloseDisplay(dpy);
	return 0;
}


/* vim: set noexpandtab shiftwidth=8: */
