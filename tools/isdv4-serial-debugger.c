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
#define WACOM_TOOLS
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

#include "isdv4.h"

#define TRACE(...) \
	do { if (verbose) printf("... " __VA_ARGS__); } while(0)

static int verbose = 0;
static int packetlength = ISDV4_PKGLEN_TPCPEN;
static ISDV4QueryReply reply;
static ISDV4TouchQueryReply touch;


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

static void version(void)
{
	printf("%d.%d.%d\n", PACKAGE_VERSION_MAJOR, PACKAGE_VERSION_MINOR,
			     PACKAGE_VERSION_PATCHLEVEL);
}

int open_device(const char *path)
{
	int fd;
	struct serial_struct ser;

	TRACE("Opening device '%s'.\n", path);
	fd = open(path, O_RDWR);

	if (fd < 1)
		perror("Failed to open device file");

	if (ioctl(fd, TIOCGSERIAL, &ser) == -1)
	{
		fprintf(stderr, "Not a serial device?");
		close(fd);
		fd = -1;
		goto out;
	}

out:
        return fd;
}

int set_serial_attr(int fd, unsigned int baud)
{
	struct termios t;

	if (tcgetattr(fd, &t) == -1)
                memset(&t, 0, sizeof(t));

	/* defaults from xf86OpenSerial */
	t.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
	t.c_oflag &= ~OPOST;
	t.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
	t.c_cflag &= ~(CSIZE|PARENB);
	t.c_cflag |= CS8|CLOCAL;
	tcsetattr(fd, TCSANOW, &t);

	/* wacom-specific */
	t.c_cflag &= ~(CSTOPB); /* stopbits 1 */
	t.c_cflag &= ~(CSIZE); /* databits 8 */
	t.c_cflag |= (CS8); /* databits 8 */
	t.c_cflag &= ~(PARENB); /* parity none */
	t.c_cc[VMIN] = 1;	/* vmin 1 */
	t.c_cc[VTIME] = 10;	/* vtime 10 */
	t.c_iflag |= IXOFF;	/* flow controll xoff */

	TRACE("Baud rate is %d\n", baud);

	switch(baud)
	{
		case 19200: baud = B19200; break;
		case 38400: baud = B38400; break;
		default:
			fprintf(stderr, "Unsupported baud rate.\n");
			return -1;
	}

	cfsetispeed(&t, baud);
	cfsetospeed(&t, baud);

	return tcsetattr(fd, TCSANOW, &t);

}

static int write_to_tablet(int fd, char *command)
{
	int len = 0;

	do {
		int l;
		l = write(fd, &command[len], strlen(command) - len);

		TRACE("Written '%s'.\n", command);

		if (l == -1 && errno != EAGAIN)
		{
			perror("not written.\n");
			break;
		}
		len += l;
	} while (errno == EAGAIN && len < strlen(command));

	return !(len == strlen(command));
}

static int stop_tablet(int fd)
{
	int rc;
	char buffer[10];
	int fd_flags;

	TRACE("Writing STOP command.\n");
	rc = write_to_tablet(fd, ISDV4_STOP);

	usleep(250000);

	/* flush the line */
	fd_flags = fcntl(fd, F_GETFL);
	if (fcntl(fd, F_SETFL, fd_flags | O_NONBLOCK) == 0)
	{
		while (read(fd, buffer, sizeof(buffer)) > 0)
			TRACE("garbage flushed\n");
		fcntl(fd, F_SETFL, fd_flags);
	}

	return rc;
}

static int start_tablet(int fd)
{
	TRACE("Writing SAMPLING command.\n");
	return write_to_tablet(fd, ISDV4_SAMPLING);
}

static int wait_for_tablet(int fd)
{
#if 0
	struct pollfd pfd = { fd, POLLIN, 0 };
	int rc;

	TRACE("Waiting for tablet...");
	rc = poll(&pfd, 1, 1000);
	if (rc < 0) {
		perror("poll failed.");
		return -1;
	} else if (rc == 0) {
		fprintf(stderr, "timeout.\n");
		return -1;
	} else if (pfd.revents & POLLIN)
		TRACE("data available.\n");

#endif
	return 0;
}

static void memdump(const unsigned char *buffer, int len)
{
	int n = 0;
	if (!len)
		return;

	while(len-- && ++n) {
		TRACE("%#hhx ", *buffer++);
		if (n % 8 == 0)
			TRACE("\n");
	}

	TRACE("\n");
}

static int skip_garbage(unsigned char *buffer, size_t len)
{
	int i;
	for (i = 0; i < len; i++)
		if (buffer[i] & HEADER_BIT)
			break;

	if (i != 0)
		TRACE("skipping over %d bytes.\n", (i < len) ? i : -1);

	return (i < len) ? i : -1;
}

static int read_data(int fd, unsigned char* buffer, int min_len)
{
	int len = 0;
	int attempts = 10;
	int skip;

	TRACE("Reading %d bytes from device.\n", min_len);
redo:
	do {
		int l = read(fd, &buffer[len], min_len);

		if (l == -1) {
			if (errno != EAGAIN) {
				perror("Error reading data.");
				return -1;
			}
			wait_for_tablet(fd);
			attempts--;
			continue;
		} else {
			TRACE("read %d bytes in one chunk.\n", l);
			len += l;
		}

	} while (len < min_len && attempts);

	if (!attempts) {
		fprintf(stderr, "Only able to read %d bytes.\n", len);
		memdump(buffer, len);
		return -1;
	}

	TRACE("Read %d bytes.\n", len);

	skip = skip_garbage(buffer, len);
	if (skip > 0) {
		TRACE("%d bytes garbage.\n", skip);
		len -= skip;
		memmove(buffer, &buffer[skip], len);
		goto redo;
	}

	if (len > min_len)
	{
		TRACE("%d bytes unexpected data.\n", (len - min_len));
		memdump(&buffer[min_len], len - min_len);
	}


	return len;
}

static int query_tablet(int fd)
{
	unsigned char buffer[ISDV4_PKGLEN_TPCCTL];
	int len, rc;

	TRACE("Querying tablet.\n");

	if (stop_tablet(fd)) goto out;
	if (write_to_tablet(fd, ISDV4_QUERY)) goto out;
	if (wait_for_tablet(fd)) goto out;

	len = read_data(fd, buffer, ISDV4_PKGLEN_TPCCTL);
	if (len < 1)
		goto out;

	if (!(buffer[0] & CONTROL_BIT))
	{
		TRACE("+++ out of cheese error +++ redo from start +++\n");
		/* X driver claims that the first read may fail ??? */
		len = read_data(fd, buffer, ISDV4_PKGLEN_TPCCTL);
	}

	TRACE("Parsing query reply.\n");
	rc = isdv4ParseQuery(buffer, len, &reply);
	if (rc < 0)
	{
		fprintf(stderr, "parsing error code %d\n", rc);
		goto out;
	}

	printf("TABLET: version: %d\n", reply.version);
	printf("TABLET: x max: %d y max %d\n", reply.x_max, reply.y_max);
	printf("TABLET: tilt_x max: %d tilt_y max %d\n", reply.tilt_x_max, reply.tilt_y_max);
	printf("TABLET: pressure max: %d\n", reply.pressure_max);

	/* check for touch capabilities */
	TRACE("Trying touch query\n");
	if (stop_tablet(fd)) goto out;
	if (write_to_tablet(fd, ISDV4_TOUCH_QUERY)) goto out;
	if (wait_for_tablet(fd)) goto out;

	memset(buffer, 0, sizeof(buffer));
	len = read_data(fd, buffer, ISDV4_PKGLEN_TPCCTL);
	if (len < 1 && errno != EAGAIN)
		goto out;

	TRACE("Parsing touch query reply.\n");
	rc = isdv4ParseTouchQuery(buffer, len, &touch);
	if (rc < 0)
	{
		fprintf(stderr, "touch parsing error code %d\n", rc);
		/* failure to parse touch query is not fatal */
	} else {
		printf("TOUCH version: %d\n", touch.version);
		printf("TOUCH x max: %d y max %d\n", touch.x_max, touch.y_max);
		printf("TOUCH panel resolution: %d\n", touch.panel_resolution);
		printf("TOUCH capacity resolution: %d\n", touch.capacity_resolution);
		printf("TOUCH sensor id: %d\n", touch.sensor_id);
	}

	return 0;

out:
	fprintf(stderr, "error during query.\n");
	return -1;
}

static int reset_tablet(int fd)
{
	char buffer[10];
	TRACE("Reset requested, resetting tablet\n");

	if (stop_tablet(fd)) goto out;
	if (write_to_tablet(fd, ISDV4_RESET)) goto out;
	if (wait_for_tablet(fd)) goto out;

	memset(buffer, 0, sizeof(buffer));
	if (read(fd, buffer, sizeof(buffer)) < 1 && errno != EAGAIN)
		goto out;

	if (buffer[0] == '&')
		return 0;

out:
	fprintf(stderr, "failed to reset tablet.\n");
	return 1;
}

static int parse_pen_packet(unsigned char* buffer)
{
	int rc;
	ISDV4CoordinateData coord;

	TRACE("Parsing coordinate data.\n");
	rc = isdv4ParseCoordinateData(buffer, ISDV4_PKGLEN_TPCPEN, &coord);
	if (rc == -1) {
		fprintf(stderr, "failed to parse coordinate data.\n");
		return -1;
	}

	printf("PEN	");
	printf("%ld:", time(NULL));
	printf("%5d/%5d | pressure: %3d | ", coord.x, coord.y, coord.pressure);
	printf(" %3d/%3d |", coord.tilt_x, coord.tilt_y);
	printf("%1s %1s %1s %1s |\n", 	coord.proximity ? "p" : "",
					coord.tip ? "t" : "",
					coord.side ? "s" : "",
					coord.eraser ? "e" : "");
	return 0;
}

static int parse_touch_packet(unsigned char* buffer, int packetlength)
{
	ISDV4TouchData touchdata;
	int rc;

	rc = isdv4ParseTouchData(buffer, packetlength, packetlength, &touchdata);
	if (rc == -1) {
		fprintf(stderr, "failed to parse touch data.\n");
		return -1;
	}

	printf("TOUCH	");
	printf("%ld:", time(NULL));
	printf("%5d/%5d | capacity: %3d | ", touchdata.x, touchdata.y, touchdata.capacity);
	printf("%d | ", touchdata.status);

	if (packetlength == ISDV4_PKGLEN_TOUCH2FG)
		printf("%d | %d/%d |", touchdata.finger2.status, touchdata.finger2.x, touchdata.finger2.y);

	printf("\n");

	return 0;

}

int event_loop(int fd)
{
	unsigned char buffer[256];
	int dlen = 0;

	TRACE("Waiting for events\n");

	if (fcntl(fd, F_SETFD, O_NONBLOCK) == -1)
		perror("Nonblock failed.");

	memset(buffer, 0, sizeof(buffer));

	while (1) {
		int r, garbage = 0;
		r = read(fd, &buffer[dlen], sizeof(buffer) - dlen);

		if (r == -1) {
			if (errno == EAGAIN)
				continue;
			else {
				perror("Error during read.");
				goto out;
			}
		}

		dlen += r;

		if (buffer[0] & HEADER_BIT)
		{
			packetlength = ISDV4_PKGLEN_TPCPEN;
			if (buffer[0] & TOUCH_CONTROL_BIT)
				packetlength = ISDV4PacketLengths[touch.sensor_id];
		} else {
			int bytes = skip_garbage(buffer, dlen);
			if (bytes > 0) {
				dlen -= bytes;
				memmove(buffer, &buffer[bytes], sizeof(buffer) - bytes);
			}
			continue;
		}


		if (dlen < packetlength)
			continue;
		TRACE("Expecting packet sized %d\n", packetlength);

		if (buffer[0] & CONTROL_BIT) {
			dlen -= packetlength;
			continue;
		}

		switch(packetlength)
		{
			case ISDV4_PKGLEN_TPCPEN:
				if (parse_pen_packet(buffer))
					garbage = 1;
				break;
			default: /* all others */
				if (parse_touch_packet(buffer, packetlength))
					garbage = 1;
		}

		if (garbage) {
			int bytes;
			bytes = skip_garbage(buffer, packetlength);
			if (bytes > 0) {
				dlen -= bytes;
				memmove(buffer, &buffer[bytes], sizeof(buffer) - bytes);
			}
			garbage = 0;
		} else {
			memmove(buffer, &buffer[packetlength], sizeof(buffer) - packetlength);
			dlen -= packetlength;
		}
	}


	return 0;
out:
	return 1;
}

int main (int argc, char **argv)
{
	int fd;
	char *filename;
	int baudrate = 38400;
	int reset = 0;
	int rc;

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

	rc = query_tablet(fd);
	if (rc < 0)
		return 1;

	start_tablet(fd);

	return event_loop(fd);
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
