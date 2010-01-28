/*
 * Copyright 1999-2002 Vojtech Pavlik
 * Copyright 2010 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
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
#ifndef	_SOLARIS_USBWCM_H
#define	_SOLARIS_USBWCM_H

#ifdef	__cplusplus
extern "C" {
#endif

#define	EVIOCGVERSION	EVTIOCGVERSION
#define	EVIOCGID	EVTIOCGDEVID
#define	EVIOCGBIT	EVTIOCGBM
#define	EVIOCGABS	EVTIOCGABS

#define	input_event	event_input
#define	input_id	event_dev_id
#define	input_absinfo	event_abs_axis
	#define	maximum	max

#define	EV_KEY		EVT_BTN
#define	EV_REL		EVT_REL
#define	EV_ABS		EVT_ABS
#define	EV_SYN		EVT_SYN
#define	EV_MSC		EVT_MSC
#define	EV_MAX		EVT_MAX

#define	KEY_MAX		BTN_MAX

#define	BTN_0		BTN_MISC_0
#define	BTN_1		BTN_MISC_1
#define	BTN_2		BTN_MISC_2
#define	BTN_3		BTN_MISC_3
#define	BTN_4		BTN_MISC_4
#define	BTN_5		BTN_MISC_5
#define	BTN_6		BTN_MISC_6
#define	BTN_7		BTN_MISC_7
#define	BTN_8		BTN_MISC_8
#define	BTN_9		0x109

#define	BTN_FORWARD	0x115
#define	BTN_BACK	0x116

#define	BTN_BASE	0x126
#define	BTN_BASE2	0x127
#define	BTN_BASE3	0x128
#define	BTN_BASE4	0x129
#define	BTN_BASE5	0x12a
#define	BTN_BASE6	0x12b

#define	BTN_A		0x130
#define	BTN_B		0x131
#define	BTN_C		0x132
#define	BTN_X		0x133
#define	BTN_Y		0x134
#define	BTN_Z		0x135
#define	BTN_TL		0x136
#define	BTN_TR		0x137
#define	BTN_TL2		0x138
#define	BTN_TR2		0x139
#define	BTN_SELECT	0x13a

#define	BTN_TOOL_TRIPLETAP	0x14e

#define	BTN_STYLUS	BTN_STYLUS_1
#define	BTN_STYLUS2	BTN_STYLUS_2

#define	BTN_TOOL_RUBBER	BTN_TOOL_ERASER
#define	BTN_TOOL_LENS	BTN_TOOL_MOUSE

#define	BTN_TOOL_PENCIL		BTN_TOOL_PEN
#define	BTN_TOOL_BRUSH		BTN_TOOL_PEN
#define	BTN_TOOL_AIRBRUSH	BTN_TOOL_PEN
#define	BTN_TOOL_FINGER		BTN_TOOL_PAD
#define	BTN_TOUCH		BTN_TIP

#define	ABS_THROTTLE	0x06
#ifdef	__cplusplus
}
#endif

#endif	/* _SOLARIS_USBWCM_H */
