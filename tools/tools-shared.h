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

#ifndef TOOLS_SHARED_H_
#define TOOLS_SHARED_H_

void version(void);
int open_device(const char *path);
int set_serial_attr(int fd, unsigned int baud);
int write_to_tablet(int fd, const char *command);
int stop_tablet(int fd);
int start_tablet(int fd);
int wait_for_tablet(int fd);
void memdump(const unsigned char *buffer, int len);
int skip_garbage(unsigned char *buffer, size_t len);
int read_data(int fd, unsigned char* buffer, int min_len);
int query_tablet(int fd);
int reset_tablet(int fd);
int parse_pen_packet(unsigned char* buffer);
int parse_touch_packet(unsigned char* buffer, int packetlength);
int event_loop(int fd, int sensor_id);

#define TRACE(...) \
	do { if (verbose) printf("... " __VA_ARGS__); } while(0)

#endif /* TOOLS_SHARED_H_ */
