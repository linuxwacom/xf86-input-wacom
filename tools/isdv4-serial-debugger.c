/*
 * Copyright 2010 by Red Hat, Inc.
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

/* Simple protocol debugger for ISDV4 serial devices */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <linux/serial.h>
#include <getopt.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>

#include "tools-shared.h"

extern int verbose; /* quiet clang's -Wmissing-variable-declarations */
int verbose = 0;

static void usage(void)
{
	printf(
	"Usage: wacom-serial-debugger [options] device\n"
	"Options:\n"
	" -h, --help                 - usage\n"
	" -v, --verbose              - verbose output\n"
	" -V, --version              - version info\n"
	" -b, --baudrate baudrate    - set baudrate\n"
	" --reset                    - send reset command before doing anything\n");
}

int main (int argc, char **argv)
{
	int fd;
	char *filename;
	int baudrate = 38400;
	int reset = 0;
	int rc;
	int sensor_id;

	int c, optidx = 0;
	struct option options[] = {
		{"help", 0, NULL, 'h'},
		{"verbose", 0, NULL, 'v'},
		{"version", 0, NULL, 'V'},
		{"baudrate", 1, NULL, 'b'},
		{"reset", 0, NULL, 'r' },
		{NULL, 0, NULL, 0}
	};

	while ((c = getopt_long(argc, argv, "+hvVb:", options, &optidx)) != -1) {
		switch(c) {
			case 'v':
				verbose = 1;
				break;
			case 'V':
				version();
				return 0;
			case 'b':
				baudrate = atoi(optarg);
				break;
			case 'r':
				reset = 1;
				break;
			case 'h':
			default:
				usage();
				return 0;
		}
	}

	if (optind == argc) {
		usage();
		return 0;
	}

	filename = argv[optind];

	fd = open_device(filename);
	if (fd < 0)
		return 1;

	rc = set_serial_attr(fd, baudrate);
	if (rc < 0)
		return 1;

	if (reset) {
		rc = reset_tablet(fd);
		if (rc < 0)
			return 1;
	}

	sensor_id = query_tablet(fd);
	if (sensor_id < 0)
		return 1;

	start_tablet(fd);

	return event_loop(fd, sensor_id);
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
