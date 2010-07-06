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

#define ISDV4_QUERY "*"       /* ISDV4 query command */
#define ISDV4_TOUCH_QUERY "%" /* ISDV4 touch query command */
#define ISDV4_STOP "0"        /* ISDV4 stop command */
#define ISDV4_SAMPLING "1"    /* ISDV4 sampling command */

/* packet length for individual models */
#define ISDV4_PKGLEN_TOUCH93    5
#define ISDV4_PKGLEN_TOUCH9A    7
#define ISDV4_PKGLEN_TPCPEN     9
#define ISDV4_PKGLEN_TPCCTL     11
#define ISDV4_PKGLEN_TOUCH2FG   13

#define HEADER_BIT      0x80
#define CONTROL_BIT     0x40
#define DATA_ID_MASK    0x3F
#define TOUCH_CONTROL_BIT 0x10

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

/* Touch Data format. Note that capacity and finger2 are only set for some
 * devices (0 on all others) */
typedef struct {
	uint8_t status;		/* touch down/up */
	uint16_t x;
	uint16_t y;
	uint16_t capacity;
	struct {
		uint8_t status;		/* touch down/up */
		uint16_t x;
		uint16_t y;
	} finger2;
} ISDV4TouchData;

/* Coordinate data format */
typedef struct {
	uint8_t proximity;	/* in proximity? */
	uint8_t tip;		/* tip/eraser pressed? */
	uint8_t side;		/* side switch pressed? */
	uint8_t eraser;		/* eraser pressed? */
	uint16_t x;
	uint16_t y;
	uint16_t pressure;
	uint8_t tilt_x;
	uint8_t tilt_y;
} ISDV4CoordinateData;

#endif /* WCMISDV4_H */

/* vim: set noexpandtab shiftwidth=8: */
