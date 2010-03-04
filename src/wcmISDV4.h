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


#ifndef WCMISDV4_H
#define WCMISDV4_H

/* ISDV4 protocol parsing structs. */

/* Query reply data */
typedef struct {
	unsigned char data_id;	 /* always 00H */
	uint16_t x_max;
	uint16_t y_max;
	uint16_t pressure_max;
	uint8_t  tilt_x_max;
	uint8_t  tilt_y_max;
	uint16_t version;
} ISDV4QueryReply;


/* Touch Query reply data */
typedef struct {
	uint8_t data_id;	/* always 01H */
	uint8_t panel_resolution;
	uint8_t sensor_id;
	uint16_t x_max;
	uint16_t y_max;
	uint8_t capacity_resolution;
	uint16_t version;
} ISDV4TouchQueryReply;

#endif /* WCMISDV4_H */

/* vim: set noexpandtab shiftwidth=8: */
