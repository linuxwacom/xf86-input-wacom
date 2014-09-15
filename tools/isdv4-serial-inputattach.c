/*
 * Copyright 2014 by Red Hat, Inc.
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

/* inputattach clone for ISDV4 serial devices */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <linux/serio.h>
#include <libudev.h>

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <signal.h>

#include "tools-shared.h"

extern int verbose;  /* quiet clang's -Wmissing-variable-declarations */
int verbose = 0;

static void usage(void)
{
	printf(
		"Usage: %s [options] device\n"
		"Options: \n"
		"-h, --help            - usage\n"
		"--verbose             - verbose output\n"
		"--version             - version info\n"
		"--baudrate <19200|38400>  - set baudrate\n",
		program_invocation_short_name
	      );
}

static int set_line_discipline(int fd, int ldisc)
{
	int rc;

	rc = ioctl(fd, TIOCSETD, &ldisc);
	if (rc < 0)
		perror("can't set line discipline");

	return rc;
}

static int bind_kernel_driver(int fd)
{
	unsigned long devt;
	unsigned int id = 0, extra = 0;

	devt = SERIO_W8001 | (id << 8) | (extra << 16);
	if (ioctl(fd, SPIOCSTYPE, &devt)) {
		perror("Failed to set device type");
		return -1;
	}

	return 0;
}

static int get_baud_rate(int fd)
{
	struct stat st;
	int baudrate = 19200;
	int id;
	struct udev *udev;
	struct udev_device *device, *parent;
	const char *attr_id = NULL;

	fstat(fd, &st);

	udev = udev_new();
	device = udev_device_new_from_devnum(udev, 'c', st.st_rdev);
	parent = device;

	while (parent) {
		attr_id = udev_device_get_sysattr_value(parent, "id");
		if (attr_id &&
				(strncmp(attr_id, "WACf", 4) == 0 || strncmp(attr_id, "FUJ", 3) == 0))
			break;

		parent = udev_device_get_parent(parent);
	}

	/* Devices up to WACf007 are 19200, newer devices are 38400. FUJ
	   devices are all 19200 */
	if (attr_id && sscanf(attr_id, "WACf%x", &id) == 1 && id >= 0x8)
		baudrate = 38400;

	if (device)
		udev_device_unref(device);
	udev_unref(udev);

	return baudrate;
}

static void sighandler(int signum)
{
	/* We don't need to do anything here, triggering the signal is
	 * enough to trigger EINTR in read() and then reset the line
	 * discipline in main */
}

int main(int argc, char **argv)
{
        int sensor_id;
	char *filename;
	int fd, rc = 1;
	int baudrate = 0;
	int have_baudrate = 0;

	int c, optidx = 0;
	struct option options[] = {
		{"help", 0, NULL, 'h'},
		{"verbose", 0, NULL, 'v'},
		{"version", 0, NULL, 'V'},
		{"baudrate", 1, NULL, 'b'},
		{NULL, 0, NULL, 0}
	};

	while ((c = getopt_long(argc, argv, "h", options, &optidx)) != -1) {
		switch(c) {
			case 'v':
				verbose = 1;
				break;
			case 'V':
				version();
				return 0;
			case 'b':
				have_baudrate = 1;
				baudrate = atoi(optarg);
				if (baudrate <= 0) {
					usage();
					return 1;
				}
				break;
			case 'h':
			default:
				usage();
				return 0;
		}
	}

	if (optind == argc) {
		usage();
		return 1;
	}

	filename = argv[optind];

	fd = open_device(filename);
	if (fd < 0)
		goto out;

	/* only guess if we didn't get a baud rate */
	if (!have_baudrate && (baudrate = get_baud_rate(fd)) < 0)
		goto out;

	set_serial_attr(fd, baudrate);

	sensor_id = query_tablet(fd);
	if (sensor_id < 0 && !have_baudrate) {
		/* query failed, maybe the wrong baud rate? */
		baudrate = (baudrate == 19200) ? 38400 : 19200;

		printf("Initial tablet query failed. Trying with baud rate %d.\n", baudrate);

		set_serial_attr(fd, baudrate);
		sensor_id = query_tablet(fd);
	}

	if (sensor_id < 0) {
		fprintf(stderr, "Tablet query failed, cannot initialize.\n");
		return 1;
	}

	/* some of the 19200 tablets can't set the line discipline */
	set_line_discipline(fd, N_MOUSE);

	if (bind_kernel_driver(fd) < 0) {
		fprintf(stderr, "Failed to bind the kernel driver.\n");
		goto out;
	}

	signal(SIGINT, sighandler);
	signal(SIGHUP, sighandler);
	read(fd, NULL, 0);

	set_line_discipline(fd, 0);

	rc = 0;
out:
	if (fd >= 0)
		close(fd);
	return rc;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
