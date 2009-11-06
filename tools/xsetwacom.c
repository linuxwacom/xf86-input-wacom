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

int main (int argc, char **argv)
{
	char c;
	int optidx;
	char *display = NULL;

	struct option options[] = {
		{"help", 0, NULL, 0},
		{"verbose", 0, NULL, 0},
		{"version", 0, NULL, 0},
		{"display", 1, (int*)display, 0},
		{"shell", 0, NULL, 0},
		{"xconf", 0, NULL, 0}
	};

	if (argc < 2)
	{
		usage();
		return 1;
	}

	while ((c = getopt_long(argc, argv, "hvVd:sxd", options, &optidx)) != -1) {
		switch(c)
		{
			case 0:
				switch(optidx)
				{
					case 0: usage(); break;
					case 2: version(); break;

				}
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
	return 0;
}


/* vim: set noexpandtab shiftwidth=8: */
