/*****************************************************************************
 *
 * Copyright 2003 by John Joganic <john@joganic.com>
 * Copyright 2003 - 2007 by Ping Cheng <pingc@wacom.com> 
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * JOHN JOGANIC BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __XF86_XWACOM_H
#define __XF86_XWACOM_H

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
#define XWACOM_PARAM_NOXOPTION		200
#define XWACOM_PARAM_SPEEDLEVEL 	201
#define XWACOM_PARAM_CLICKFORCE 	202
#define XWACOM_PARAM_ACCEL      	203
#define XWACOM_PARAM_XYDEFAULT  	204
#define XWACOM_PARAM_MMT        	205
#define XWACOM_PARAM_RAWFILTER  	206
#define XWACOM_PARAM_GETONLYPARAM	220
#define XWACOM_PARAM_TID		221
#define XWACOM_PARAM_TOOLID		222
#define XWACOM_PARAM_TOOLSERIAL		223
#define XWACOM_PARAM_GETMODEL		224
#define XWACOM_PARAM_NUMSCREEN		250
#define XWACOM_PARAM_SCREENTOPX		251
#define XWACOM_PARAM_SCREENTOPY		252
#define XWACOM_PARAM_SCREENBOTTOMX	253
#define XWACOM_PARAM_SCREENBOTTOMY	254

#define XWACOM_VALUE_ROTATE_NONE 0
#define XWACOM_VALUE_ROTATE_CW 1
#define XWACOM_VALUE_ROTATE_CCW 2
#define XWACOM_VALUE_ROTATE_HALF 3

/* The following flags are used for XWACOM_PARAM_BUTTON# values to mark
 * the type of event that should be emitted when that button is pressed;
 * combined together they form an Action Code (AC).
 */
#define AC_CODE             0x100fffff	/* Mask to isolate button number or key code */
#define AC_BUTTON           0x00000000	/* Emit a button event */
#define AC_KEY              0x00100000	/* Emit a key event */
#define AC_MODETOGGLE       0x00200000	/* Toggle absolute/relative mode */
#define AC_DBLCLICK         0x00300000	/* Emit a button1 double-click event */
#define AC_TYPE             0x00300000	/* The mask to isolate event type bits */
#define AC_SHIFT            0x00400000  /* Emulate SHIFT+event */
#define AC_CONTROL          0x00800000  /* Emulate CONTROL+event */
#define AC_META             0x01000000  /* Emulate META+event */
#define AC_ALT              0x02000000  /* Emulate ALT+event */
#define AC_SUPER            0x04000000  /* Emulate SUPER+event */
#define AC_HYPER            0x08000000  /* Emulate HYPER+event */
#define AC_ANYMOD           0x0fc00000  /* Any modifier key bit */
#define AC_CORE             0x20000000	/* Always emit a core event */
#define AC_KEY_END          0x40000000	/* End of a keystroke */

#endif /* __XF86_XWACOM_H */
