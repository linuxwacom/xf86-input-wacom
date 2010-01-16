/*
 * Copyright 2009 by Ping Cheng, Wacom. <pingc@wacom.com> 
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
#include <config.h>
#endif

#include <math.h>
#include "xf86Wacom.h"

void wcmTilt2R(WacomDeviceStatePtr ds);

/*****************************************************************************
 *  wcmTilt2R -
 *   Converts tilt X and Y to rotation, for Intuos4 mouse for now. 
 *   It can be used for other devices when necessary.
 ****************************************************************************/

void wcmTilt2R(WacomDeviceStatePtr ds)
{
	short tilt_x = ds->tiltx;
	short tilt_y = ds->tilty;
	double rotation = 0.0;

	/* other tilt-enabled devices need to apply round() after this call */
	if (tilt_x || tilt_y)
		rotation = ((180.0 * atan2(-tilt_x,tilt_y)) / M_PI) + 180.0;

	/* Intuos4 mouse has an (180-5) offset */
	ds->rotation = round((360.0 - rotation + 180.0 - 5.0) * 5.0);
		ds->rotation %= 1800;

	if (ds->rotation >= 900) 
		ds->rotation = 1800 - ds->rotation;
	else
		ds->rotation = -ds->rotation;
}

/* vim: set noexpandtab shiftwidth=8: */
