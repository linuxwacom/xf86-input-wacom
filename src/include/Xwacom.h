/*
 * Copyright 2003 by John Joganic <john@joganic.com>
 * Copyright 2003 - 2007 by Ping Cheng <pingc@wacom.com> 
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

#ifndef __XF86_XWACOM_H
#define __XF86_XWACOM_H

#include <X11/keysym.h>

#define XWACOM_PARAM_TOPX       	1
#define XWACOM_PARAM_TOPY       	2
#define XWACOM_PARAM_BOTTOMX    	3
#define XWACOM_PARAM_BOTTOMY    	4
#define XWACOM_PARAM_DEBUGLEVEL 	5
#define XWACOM_PARAM_PRESSCURVE 	6
#define XWACOM_PARAM_MODE       	7
#define XWACOM_PARAM_TPCBUTTON  	8
#define XWACOM_PARAM_CURSORPROX  	9
#define XWACOM_PARAM_ROTATE             10
#define XWACOM_PARAM_TWINVIEW		11
#define XWACOM_PARAM_SUPPRESS		12
#define XWACOM_PARAM_SCREEN_NO		13
#define XWACOM_PARAM_COMMONDBG		14
#define XWACOM_PARAM_RAWSAMPLE		15

/* The followings are defined together */
#define XWACOM_PARAM_BUTTON1    	101
#define XWACOM_PARAM_BUTTON2    	102
#define XWACOM_PARAM_BUTTON3    	103
#define XWACOM_PARAM_BUTTON4    	104
#define XWACOM_PARAM_BUTTON5    	105
#define XWACOM_PARAM_BUTTON6    	106
#define XWACOM_PARAM_BUTTON7    	107
#define XWACOM_PARAM_BUTTON8    	108
#define XWACOM_PARAM_BUTTON9    	109
#define XWACOM_PARAM_BUTTON10    	110
#define XWACOM_PARAM_BUTTON11    	111
#define XWACOM_PARAM_BUTTON12    	112
#define XWACOM_PARAM_BUTTON13    	113
#define XWACOM_PARAM_BUTTON14    	114
#define XWACOM_PARAM_BUTTON15    	115
#define XWACOM_PARAM_BUTTON16    	116
#define XWACOM_PARAM_BUTTON17    	117
#define XWACOM_PARAM_BUTTON18    	118
#define XWACOM_PARAM_BUTTON19    	119
#define XWACOM_PARAM_BUTTON20    	120
#define XWACOM_PARAM_BUTTON21    	121
#define XWACOM_PARAM_BUTTON22    	122
#define XWACOM_PARAM_BUTTON23    	123
#define XWACOM_PARAM_BUTTON24    	124
#define XWACOM_PARAM_BUTTON25    	125
#define XWACOM_PARAM_BUTTON26    	126
#define XWACOM_PARAM_BUTTON27    	127
#define XWACOM_PARAM_BUTTON28    	128
#define XWACOM_PARAM_BUTTON29    	129
#define XWACOM_PARAM_BUTTON30    	130
#define XWACOM_PARAM_BUTTON31    	131
#define XWACOM_PARAM_BUTTON32    	132

#define XWACOM_PARAM_NOXOPTION		150
#define XWACOM_PARAM_RELWUP    		151
#define XWACOM_PARAM_RELWDN    		152
#define XWACOM_PARAM_ABSWUP    		153
#define XWACOM_PARAM_ABSWDN    		154
#define XWACOM_PARAM_STRIPLUP    	155
#define XWACOM_PARAM_STRIPLDN    	156
#define XWACOM_PARAM_STRIPRUP    	157
#define XWACOM_PARAM_STRIPRDN    	158
/* End of together */

#define XWACOM_PARAM_SPEEDLEVEL 	201
#define XWACOM_PARAM_CLICKFORCE 	202
#define XWACOM_PARAM_ACCEL      	203
#define XWACOM_PARAM_XYDEFAULT  	204
#define XWACOM_PARAM_MMT        	205
#define XWACOM_PARAM_RAWFILTER  	206
/* the following 2 stays together */
#define XWACOM_PARAM_TVRESOLUTION0	207
#define XWACOM_PARAM_TVRESOLUTION1	208
#define XWACOM_PARAM_COREEVENT		209

#define XWACOM_PARAM_GETONLYPARAM	320
#define XWACOM_PARAM_TID		321
#define XWACOM_PARAM_TOOLID		322
#define XWACOM_PARAM_TOOLSERIAL		323
#define XWACOM_PARAM_NUMSCREEN		350
#define XWACOM_PARAM_STOPX0		351
#define XWACOM_PARAM_STOPY0		352
#define XWACOM_PARAM_SBOTTOMX0		353
#define XWACOM_PARAM_SBOTTOMY0		354
#define XWACOM_PARAM_STOPX1		355
#define XWACOM_PARAM_STOPY1		356
#define XWACOM_PARAM_SBOTTOMX1		357
#define XWACOM_PARAM_SBOTTOMY1		358
#define XWACOM_PARAM_STOPX2		359
#define XWACOM_PARAM_STOPY2		360
#define XWACOM_PARAM_SBOTTOMX2		361
#define XWACOM_PARAM_SBOTTOMY2		362

#define TV_NONE 		0
#define TV_ABOVE_BELOW 		1
#define TV_LEFT_RIGHT		2

#define ROTATE_NONE 		0
#define ROTATE_CW 		1
#define ROTATE_CCW 		2
#define ROTATE_HALF 		3

#define XWACOM_MAX_SAMPLES	20

/* The following flags are used for XWACOM_PARAM_BUTTON# values to mark
 * the type of event that should be emitted when that button is pressed;
 * combined together they form an Action Code (AC).
 */
#define AC_CODE             0x0000ffff	/* Mask to isolate button number or key code */
#define AC_BUTTON           0x00000000	/* Emit button events */
#define AC_KEY              0x00010000	/* Emit key events */
#define AC_MODETOGGLE       0x00020000	/* Toggle absolute/relative mode */
#define AC_DBLCLICK         0x00030000	/* Emit a button1 double-click event */
#define AC_TYPE             0x000f0000	/* The mask to isolate event type bits */
#define AC_NUM_KEYS         0x0ff00000  /* The mask to isolate number of keys to send */
#define AC_CORE             0x10000000	/* Always emit a core event */
#define AC_EVENT            0xf00f0000	/* Mask to isolate event flag */

#endif /* __XF86_XWACOM_H */
