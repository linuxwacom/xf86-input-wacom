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
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _WACOM_PROPERTIES_H_
#define _WACOM_PROPERTIES_H_

/**
 * Properties exported by the wacom driver. These properties are
 * recognized by the driver and will change its behavior when modified.
 */

/* 32 bit, 4 values, top x, top y, bottom x, bottom y */
#define WACOM_PROP_TABLET_AREA "Wacom Tablet Area"

/* 8 bit, 1 value, [0 - 3] (NONE, CW, CCW, HALF) */
#define WACOM_PROP_ROTATION "Wacom Rotation"

/* 32 bit, 4 values */
#define WACOM_PROP_PRESSURECURVE "Wacom Pressurecurve"

/* 32 bit, 4 values, tablet id, old serial, old device id, serial */
#define WACOM_PROP_SERIALIDS "Wacom Serial IDs"

/* 8 bit, 4 values, left up, left down, right up, right down */
#define WACOM_PROP_STRIPBUTTONS "Wacom Strip Buttons"

/* 8 bit, 4 values, up, down, wheel up, wheel down */
#define WACOM_PROP_WHEELBUTTONS "Wacom Wheel Buttons"

/* 32 bit, 4 values */
#define WACOM_PROP_TWINVIEW_RES "Wacom TwinView Resolution"

/* 8 bit 3 values, screen number, twinview on/off, multimonitor */
#define WACOM_PROP_DISPLAY_OPTS "Wacom Display Options"

/* 32 bit, 4 values, top x, top y, bottom x, bottom y */
#define WACOM_PROP_SCREENAREA "Wacom Screen Area"

/* 32 bit, 1 value */
#define WACOM_PROP_PROXIMITY_THRESHOLD "Wacom Proximity Threshold"

/* 32 bit, 1 value */
#define WACOM_PROP_CAPACITY "Wacom Capacity"

/* 32 bit, 1 value */
#define WACOM_PROP_PRESSURE_THRESHOLD "Wacom Pressure Threshold"

/* 32 bit, 2 values, sample, suppress */
#define WACOM_PROP_SAMPLE "Wacom Sample and Suppress"

/* BOOL, 1 value */
#define WACOM_PROP_TOUCH "Wacom Enable Touch"

/* BOOL, 1 value */
#define WACOM_PROP_HOVER "Wacom Hover Click"

/* Atom, 1 value */
#define WACOM_PROP_TOOL_TYPE "Wacom Tool Type"

/* Atom, X values where X is the number of physical buttons.
   Each value points to an atom containing the sequence of actions performed
   if this button is pressed. If the value is None, no action is performed.
 */
#define WACOM_PROP_BUTTON_ACTIONS "Wacom Button Actions"

/* 8 bit, 2 values, priv->debugLevel and common->debugLevel. This property
 * is for use in the driver only and only enabled if --enable-debug is
 * given. No client may rely on this property being present or working.
 */
#define WACOM_PROP_DEBUGLEVELS "Wacom Debug Levels"

#endif
