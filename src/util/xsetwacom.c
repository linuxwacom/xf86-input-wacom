/*****************************************************************************
** xsetwacom.c
**
** Copyright (C) 2003 - John E. Joganic
** Copyright (C) 2004-2009 - Ping Cheng
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Lesser General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
**
** REVISION HISTORY
**   2003-05-02 0.0.1 - JEJ - created
**   2005-10-24 0.0.4 - PC - added Pad
**   2005-11-17 0.0.5 - PC - update mode code
**   2006-02-27 0.0.6 - PC - fixed a typo
**   2006-07-19 0.0.7 - PC - supports button and keys combined
**   2007-01-10 0.0.8 - PC - don't display uninitialized tools
**   2007-02-07 0.0.9 - PC - support keystrokes
**   2007-02-22 0.1.0 - PC - support wheels and strips
**   2007-04-12 0.1.1 - PC - Support CoreEvent on/off (need more work)
**   2007-05-18 0.1.2 - PC - Support DebugLevel options
**   2007-06-05 0.1.3 - PC - Support Suppress and TwinView options
**   2007-07-24 0.1.4 - PC - Support Screen_No option
**   2008-01-17 0.1.5 - PC - Add DISPLAYTOGGLE command
**   2008-03-24 0.1.6 - PC - Added Touch for serial Tablet PC (ISDv4)
**   2008-08-27 0.1.7 - PC - Added get XORGVER to xsetwacom
**   2009-02-27 0.1.8 - PC - Added LeftOf and AboveOf to TwinView
**   2009-05-18 0.1.9 - PC - Support get/set serial command
**   2009-07-14 0.2.0 - PC - Support Nvidia Xinerama setting
**   2009-08-24 0.2.1 - PC - Add SCREENTOGGLE command
**   2009-09-31 0.2.2 - PC - Add Dual Touch
**   2009-10-31 0.2.3 - PC - Support spaced names from hot-plugged devices
**
****************************************************************************/

#define XSETWACOM_VERSION "0.2.3"

#include "../include/util-config.h"

#include "wacomcfg.h"
#include "../include/Xwacom.h" /* give us raw access to parameter values */
#include "wcmAction.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <errno.h>
#include <assert.h>

int gnVerbose = 0;
int gnLastXError = 0;
enum
{
        gfSIMPLE,
	gfSHELL,
	gfXCONF
} gGetFormat = gfSIMPLE;

const char* gpszDisplayName = NULL;

typedef struct _PARAMINFO PARAMINFO;

typedef enum
{
	COMMAND_NONE,
	COMMAND_LIST,
	COMMAND_SET,
	COMMAND_GET,
	COMMAND_GETDEFAULT
} COMMAND;

struct _PARAMINFO
{
	const char* pszParam;
	const char* pszDesc;
	int nParamID;
	int bEmptyOK;
	int bRange;
	int nMin, nMax;
	int nType;
	int nDefault;
};

/*****************************************************************************
** Parameters
*****************************************************************************/

#define VALUE_REQUIRED	0
#define VALUE_OPTIONAL	1
#define RANGE		1
#define SINGLE_VALUE	0
#define PACKED_CURVE	1
#define BOOLEAN_VALUE	2
#define ACTION_VALUE	3
#define TWO_VALUES	4

static const char* tv_char[] = 
{
	"none",
	"xinerama"
	"vertical",
	"horizontal",
	"aboveof"
	"leftof"
	"NULL"
};

static const char* rt_char[] = 
{
	"none",
	"cw",
	"ccw",
	"half",
	"NULL"
};

static PARAMINFO gParamInfo[] =
{
	{ "TopX",
		"Bounding rect left coordinate in tablet units. ",
		XWACOM_PARAM_TOPX, VALUE_REQUIRED },

	{ "TopY",
		"Bounding rect top coordinate in tablet units . ",
		XWACOM_PARAM_TOPY, VALUE_REQUIRED },

	{ "BottomX",
		"Bounding rect right coordinate in tablet units. ",
		XWACOM_PARAM_BOTTOMX, VALUE_REQUIRED },

	{ "BottomY",
		"Bounding rect bottom coordinate in tablet units. ",
		XWACOM_PARAM_BOTTOMY, VALUE_REQUIRED },

	{ "Button1",
		"X11 event to which button 1 should be mapped. ",
		XWACOM_PARAM_BUTTON1, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 1 },

	{ "Button2",
		"X11 event to which button 2 should be mapped. ",
		XWACOM_PARAM_BUTTON2, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 2 },

	{ "Button3",
		"X11 event to which button 3 should be mapped. ",
		XWACOM_PARAM_BUTTON3, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 3 },

	{ "Button4",
		"X11 event to which button 4 should be mapped. ",
		XWACOM_PARAM_BUTTON4, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 4 },

	{ "Button5",
		"X11 event to which button 5 should be mapped. ",
		XWACOM_PARAM_BUTTON5, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 5 },

	{ "Button6",
		"X11 event to which button 6 should be mapped. ",
		XWACOM_PARAM_BUTTON6, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 6 },

	{ "Button7",
		"X11 event to which button 7 should be mapped. ",
		XWACOM_PARAM_BUTTON7, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 7 },

	{ "Button8",
		"X11 event to which button 8 should be mapped. ",
		XWACOM_PARAM_BUTTON8, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 8 },

	{ "Button9",
		"X11 event to which button 9 should be mapped. ",
		XWACOM_PARAM_BUTTON9, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 9 },

	{ "Button10",
		"X11 event to which button 10 should be mapped. ",
		XWACOM_PARAM_BUTTON10, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 10 },

	{ "Button11",
		"X11 event to which button 11 should be mapped. ",
		XWACOM_PARAM_BUTTON11, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 11 },

	{ "Button12",
		"X11 event to which button 12 should be mapped. ",
		XWACOM_PARAM_BUTTON12, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 12 },

	{ "Button13",
		"X11 event to which button 13 should be mapped. ",
		XWACOM_PARAM_BUTTON13, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 13 },

	{ "Button14", 
		"X11 event to which button 14 should be mapped. ",
		XWACOM_PARAM_BUTTON14, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 14 },

	{ "Button15", 
		"X11 event to which button 15 should be mapped. ",
		XWACOM_PARAM_BUTTON15, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 15 },

	{ "Button16",
		"X11 event to which button 16 should be mapped. ",
		XWACOM_PARAM_BUTTON16, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 16 },

	{ "Button17", 
		"X11 event to which button 17 should be mapped. ",
		XWACOM_PARAM_BUTTON17, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 17 },

	{ "Button18", 
		"X11 event to which button 18 should be mapped. ",
		XWACOM_PARAM_BUTTON18, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 18 },

	{ "Button19", 
		"X11 event to which button 19 should be mapped. ",
		XWACOM_PARAM_BUTTON19, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 19 },

	{ "Button20", 
		"X11 event to which button 20 should be mapped. ",
		XWACOM_PARAM_BUTTON20, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 20 },

	{ "Button21", 
		"X11 event to which button 21 should be mapped. ",
		XWACOM_PARAM_BUTTON21, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 21 },

	{ "Button22", 
		"X11 event to which button 22 should be mapped. ",
		XWACOM_PARAM_BUTTON22, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 22 },

	{ "Button23", 
		"X11 event to which button 23 should be mapped. ",
		XWACOM_PARAM_BUTTON23, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 23 },
	{ "Button24", 
		"X11 event to which button 24 should be mapped. ",
		XWACOM_PARAM_BUTTON24, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 24 },

	{ "Button25", 
		"X11 event to which button 25 should be mapped. ",
		XWACOM_PARAM_BUTTON25, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 25 },

	{ "Button26", 
		"X11 event to which button 26 should be mapped. ",
		XWACOM_PARAM_BUTTON26, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 26 },

	{ "Button27", 
		"X11 event to which button 27 should be mapped. ",
		XWACOM_PARAM_BUTTON27, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 27 },

	{ "Button28", 
		"X11 event to which button 28 should be mapped. ",
		XWACOM_PARAM_BUTTON28, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 28 },

	{ "Button29", 
		"X11 event to which button 29 should be mapped. ",
		XWACOM_PARAM_BUTTON29, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 29 },

	{ "Button30", 
		"X11 event to which button 30 should be mapped. ",
		XWACOM_PARAM_BUTTON30, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 30 },

	{ "Button31", 
		"X11 event to which button 31 should be mapped. ",
		XWACOM_PARAM_BUTTON31, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 31 },

	{ "Button32", 
		"X11 event to which button 32 should be mapped. ",
		XWACOM_PARAM_BUTTON32, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 32 },

	{ "DebugLevel",
		"Level of debugging trace for individual devices, "
		"default is 0 (off). ",
		XWACOM_PARAM_DEBUGLEVEL, VALUE_OPTIONAL, RANGE, 
		0, 100, SINGLE_VALUE, 0 },

	{ "CommonDBG",
		"Level of debugging statements applied to all devices "
		"associated with the same tablet. \n\t\t   default is 0 (off). ",
		XWACOM_PARAM_COMMONDBG, VALUE_OPTIONAL, RANGE, 
		0, 100, SINGLE_VALUE, 0 },

	{ "Suppress",
		"Number of points trimmed, default is 2. ",
		XWACOM_PARAM_SUPPRESS, VALUE_OPTIONAL, RANGE, 
		0, 100, SINGLE_VALUE, 2 },

	{ "RawSample",
		"Number of raw data used to filter the points, "
		"default is 4. ",
		XWACOM_PARAM_RAWSAMPLE, VALUE_OPTIONAL, RANGE, 
		1, XWACOM_MAX_SAMPLES, SINGLE_VALUE, 4 },

	{ "Screen_No",
		"Sets/gets screen number the tablet is mapped to, "
		"default is -1. ",
		XWACOM_PARAM_SCREEN_NO, VALUE_OPTIONAL, RANGE, 
		-1, 10, SINGLE_VALUE, -1 },

	{ "PressCurve",
		"Bezier curve for pressure (default is 0 0 100 100). ",
		XWACOM_PARAM_PRESSCURVE, VALUE_OPTIONAL, RANGE,
		0, 100, PACKED_CURVE, 0x00006464},

	{ "TwinView",
		"Sets the mapping to TwinView xinerama/horizontal/vertical/leftof/aboveof/none. \n"
		"\t\t   Values = none, vertical, horizontal, leftof, aboveof, xinerama "
		"(default is none).",
		XWACOM_PARAM_TWINVIEW, VALUE_OPTIONAL, RANGE, 
		TV_NONE, TV_MAX, SINGLE_VALUE, TV_NONE },

	{ "Mode",
		"Switches cursor movement mode (default is absolute/on). ",
		XWACOM_PARAM_MODE, VALUE_OPTIONAL, RANGE, 
		0, 1, BOOLEAN_VALUE, 1 },	

	{ "TPCButton",
		"Turns on/off Tablet PC buttons. "
		"default is off for regular tablets, "
		"on for Tablet PC. ",
		XWACOM_PARAM_TPCBUTTON, VALUE_OPTIONAL, 
		RANGE, 0, 1, BOOLEAN_VALUE, 1 },

	{ "Touch",
		"Turns on/off Touch events (default is enable/on). ",
		XWACOM_PARAM_TOUCH, VALUE_OPTIONAL, 
		RANGE, 0, 1, BOOLEAN_VALUE, 1 },

	{ "Gesture",
		"Turns on/off Touch Gesture (default is enable/on). ",
		XWACOM_PARAM_GESTURE, VALUE_OPTIONAL, 
		RANGE, 0, 1, BOOLEAN_VALUE, 1 },

	{ "Capacity",
		"Touch sensitivity level (default is 3, "
		"-1 for none capacitive tools). ",
		XWACOM_PARAM_CAPACITY, VALUE_OPTIONAL, 
		RANGE, -1, 5, SINGLE_VALUE, 3 },

	{ "CursorProx", 
		"Sets cursor distance for proximity-out "
		"in distance from the tablet.  \n"
		"\t\t   (default is 10 for Intuos series, "
		"42 for Graphire series). ",
		XWACOM_PARAM_CURSORPROX, VALUE_OPTIONAL, RANGE, 
		0, 255, SINGLE_VALUE, 47 },
		
	{ "Rotate",
		"Sets the rotation of the tablet. "
		"Values = NONE, CW, CCW, HALF (default is NONE). \n"
		"\t\t   Tablet mappings applied after this command will "
		"be based on the new tablet orientation. ",
		XWACOM_PARAM_ROTATE, VALUE_OPTIONAL, RANGE, 
		ROTATE_NONE, ROTATE_HALF, SINGLE_VALUE, ROTATE_NONE },

	{ "RelWUp", 
		"X11 event to which relative wheel up should be mapped. ",
		XWACOM_PARAM_RELWUP, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 4 },

	{ "RelWDn", 
		"X11 event to which relative wheel down should be mapped. ",
		XWACOM_PARAM_RELWDN, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 5 },

	{ "AbsWUp", 
		"X11 event to which absolute wheel up should be mapped. ",
		XWACOM_PARAM_ABSWUP, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 4 },

	{ "AbsWDn", 
		"X11 event to which absolute wheel down should be mapped. ",
		XWACOM_PARAM_ABSWDN, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 5 },

	{ "StripLUp", 
		"X11 event to which left strip up should be mapped. ",
		XWACOM_PARAM_STRIPLUP, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 4 },

	{ "StripLDn", 
		"X11 event to which left strip down should be mapped. ",
		XWACOM_PARAM_STRIPLDN, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 5 },

	{ "StripRUp", 
		"X11 event to which right strip up should be mapped. ",
		XWACOM_PARAM_STRIPRUP, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 4 },

	{ "StripRDn", 
		"X11 event to which right strip down should be mapped. ",
		XWACOM_PARAM_STRIPRDN, VALUE_OPTIONAL, 0, 0, 0, 
		ACTION_VALUE, 5 },

	{ "TVResolution0", 
		"Sets MetaModes option for TwinView Screen 0. ",
		XWACOM_PARAM_TVRESOLUTION0, VALUE_OPTIONAL, RANGE,
		0, 6000, TWO_VALUES, 0 },

	{ "TVResolution1", 
		"Sets MetaModes option for TwinView Screen 1. ",
		XWACOM_PARAM_TVRESOLUTION1, VALUE_OPTIONAL, RANGE,
		0, 6000, TWO_VALUES, 0 },

	{ "RawFilter",
		"Enables and disables filtering of raw data, "
		"default is true/on.",
		XWACOM_PARAM_RAWFILTER, VALUE_OPTIONAL, RANGE, 
		0, 1, BOOLEAN_VALUE, 1 },	

	{ "SpeedLevel",
		"Sets relative cursor movement speed (default is 6). ",
		XWACOM_PARAM_SPEEDLEVEL, VALUE_OPTIONAL, RANGE, 
		1, 11, SINGLE_VALUE, 6 },	

	{ "ClickForce",
		"Sets tip/eraser pressure threshold = ClickForce*MaxZ/100 "
		"(default is 6)",
		XWACOM_PARAM_CLICKFORCE, VALUE_OPTIONAL, RANGE, 
		1, 21, SINGLE_VALUE, 6 },	

	{ "Threshold",
		"Sets tip/eraser pressure threshold directly to the pressure "
		"(default is 6*MaxZ/100)",
		XWACOM_PARAM_THRESHOLD, VALUE_REQUIRED },	

	{ "Accel",
		"Sets relative cursor movement acceleration "
		"(default is 1)",
		XWACOM_PARAM_ACCEL, VALUE_OPTIONAL, RANGE, 
		1, 7, SINGLE_VALUE, 1 },
	
	{ "xyDefault",
		"Resets the bounding coordinates to default in tablet units. ",
		XWACOM_PARAM_XYDEFAULT, VALUE_OPTIONAL },

	{ "mmonitor",
		"Turns on/off across monitor movement in "
		"multi-monitor desktop, default is on ",
		XWACOM_PARAM_MMT, VALUE_OPTIONAL, 
		RANGE, 0, 1, BOOLEAN_VALUE, 1 },

	{ "CoreEvent",
		"Turns on/off device to send core event. "
		"default is decided by X driver and xorg.conf ",
		XWACOM_PARAM_COREEVENT, VALUE_OPTIONAL, 
		RANGE, 0, 1, BOOLEAN_VALUE },

	{ "STopX0",
		"Screen 0 left coordinate in pixels. ",
		XWACOM_PARAM_STOPX0, VALUE_REQUIRED },

	{ "STopY0",
		"Screen 0 top coordinate in pixels. ",
		XWACOM_PARAM_STOPY0, VALUE_REQUIRED },

	{ "SBottomX0",
		"Screen 0 right coordinate in pixels. ",
		XWACOM_PARAM_SBOTTOMX0, VALUE_REQUIRED },

	{ "SBottomY0",
		"Screen 0 bottom coordinate in pixels. ",
		XWACOM_PARAM_SBOTTOMY0, VALUE_REQUIRED },

	{ "STopX1",
		"Screen 1 left coordinate in pixels. ",
		XWACOM_PARAM_STOPX1, VALUE_REQUIRED },

	{ "STopY1",
		"Screen 1 top coordinate in pixels. ",
		XWACOM_PARAM_STOPY1, VALUE_REQUIRED },

	{ "SBottomX1",
		"Screen 1 right coordinate in pixels. ",
		XWACOM_PARAM_SBOTTOMX1, VALUE_REQUIRED },

	{ "SBottomY1",
		"Screen 1 bottom coordinate in pixels. ",
		XWACOM_PARAM_SBOTTOMY1, VALUE_REQUIRED },

	{ "STopX2",
		"Screen 2 left coordinate in pixels. ",
		XWACOM_PARAM_STOPX2, VALUE_REQUIRED },

	{ "STopY2",
		"Screen 2 top coordinate in pixels. ",
		XWACOM_PARAM_STOPY2, VALUE_REQUIRED },

	{ "SBottomX2",
		"Screen 2 right coordinate in pixels. ",
		XWACOM_PARAM_SBOTTOMX2, VALUE_REQUIRED },

	{ "SBottomY2",
		"Screen 2 bottom coordinate in pixels. ",
		XWACOM_PARAM_SBOTTOMY2, VALUE_REQUIRED },

	{ "STopX3",
		"Screen 3 left coordinate in pixels. ",
		XWACOM_PARAM_STOPX3, VALUE_REQUIRED },

	{ "STopY3",
		"Screen 3 top coordinate in pixels. ",
		XWACOM_PARAM_STOPY3, VALUE_REQUIRED },

	{ "SBottomX3",
		"Screen 3 right coordinate in pixels. ",
		XWACOM_PARAM_SBOTTOMX3, VALUE_REQUIRED },

	{ "SBottomY3",
		"Screen 3 bottom coordinate in pixels. ",
		XWACOM_PARAM_SBOTTOMY3, VALUE_REQUIRED },

	{ "STopX4",
		"Screen 4 left coordinate in pixels. ",
		XWACOM_PARAM_STOPX4, VALUE_REQUIRED },

	{ "STopY4",
		"Screen 4 top coordinate in pixels. ",
		XWACOM_PARAM_STOPY4, VALUE_REQUIRED },

	{ "SBottomX4",
		"Screen 4 right coordinate in pixels. ",
		XWACOM_PARAM_SBOTTOMX4, VALUE_REQUIRED },

	{ "SBottomY4",
		"Screen 4 bottom coordinate in pixels. ",
		XWACOM_PARAM_SBOTTOMY4, VALUE_REQUIRED },

	{ "STopX5",
		"Screen 5 left coordinate in pixels. ",
		XWACOM_PARAM_STOPX5, VALUE_REQUIRED },

	{ "STopY5",
		"Screen 5 top coordinate in pixels. ",
		XWACOM_PARAM_STOPY5, VALUE_REQUIRED },

	{ "SBottomX5",
		"Screen 5 right coordinate in pixels. ",
		XWACOM_PARAM_SBOTTOMX5, VALUE_REQUIRED },

	{ "SBottomY5",
		"Screen 5 bottom coordinate in pixels. ",
		XWACOM_PARAM_SBOTTOMY5, VALUE_REQUIRED },

	{ "STopX6",
		"Screen 6 left coordinate in pixels. ",
		XWACOM_PARAM_STOPX6, VALUE_REQUIRED },

	{ "STopY6",
		"Screen 6 top coordinate in pixels. ",
		XWACOM_PARAM_STOPY6, VALUE_REQUIRED },

	{ "SBottomX6",
		"Screen 6 right coordinate in pixels. ",
		XWACOM_PARAM_SBOTTOMX6, VALUE_REQUIRED },

	{ "SBottomY6",
		"Screen 6 bottom coordinate in pixels. ",
		XWACOM_PARAM_SBOTTOMY6, VALUE_REQUIRED },

	{ "STopX7",
		"Screen 7 left coordinate in pixels. ",
		XWACOM_PARAM_STOPX7, VALUE_REQUIRED },

	{ "STopY7",
		"Screen 7 top coordinate in pixels. ",
		XWACOM_PARAM_STOPY7, VALUE_REQUIRED },

	{ "SBottomX7",
		"Screen 7 right coordinate in pixels. ",
		XWACOM_PARAM_SBOTTOMX7, VALUE_REQUIRED },

	{ "SBottomY7",
		"Screen 7 bottom coordinate in pixels. ",
		XWACOM_PARAM_SBOTTOMY7, VALUE_REQUIRED },

	{ "ToolID", 
		"Returns the ID of the associated device. ",
		XWACOM_PARAM_TOOLID, VALUE_REQUIRED },

	{ "ToolSerial", 
		"Returns the serial number of the associated device. ",
		XWACOM_PARAM_TOOLSERIAL, VALUE_REQUIRED },

	{ "Serial", 
		"Returns/sets the serial number of the chosen device. ",
		XWACOM_PARAM_SERIAL, VALUE_REQUIRED },

	{ "TabletID", 
		"Returns the tablet ID of the associated device. ",
		XWACOM_PARAM_TID, VALUE_REQUIRED },

	{ "GetTabletID", 
		"Returns the tablet ID of the associated device. ",
		XWACOM_PARAM_TID, VALUE_REQUIRED },

	{ "NumScreen", 
		"Returns number of screens configured for the desktop. ",
		XWACOM_PARAM_NUMSCREEN, VALUE_REQUIRED },

	{ "XScaling", 
		"Returns the status of XSCALING is set or not. ",
		XWACOM_PARAM_XSCALING, VALUE_REQUIRED },

	{ NULL }
};

/*****************************************************************************
** Implementation
*****************************************************************************/

void Usage(FILE* f)
{
	fprintf(f,
	"Usage: xsetwacom [options] [command [arguments...]]\n"
	"Options:\n"
	" -h, --help                 - usage\n"
	" -v, --verbose              - verbose output\n"
	" -V, --version              - version info\n"
	" -d, --display disp_name    - override default display\n"
	" -s, --shell                - generate shell commands for 'get'\n"
	" -x, --xconf                - generate X.conf lines for 'get'\n");

	fprintf(f,
	"\nCommands:\n"
	" list [dev|param]           - display known devices, parameters \n"
	" list mod                   - display supported modifier and specific keys for keystokes\n"
	" set dev_name param [values...] - set device parameter by name\n"
	" get dev_name param [param...] - get current device parameter(s) value by name\n"
	" getdefault dev_name param [param...] - get device parameter(s) default value by name\n");
}

static int XError(Display* pDisp, XErrorEvent* pEvent)
{
	char chBuf[64];
	XGetErrorText(pDisp,pEvent->error_code,chBuf,sizeof(chBuf));
	fprintf(stderr,"X Error: %d %s\n", pEvent->error_code, chBuf);
	gnLastXError  = pEvent->error_code;
	return 0;
}

/*static int GetLastXError(void)
{
	return gnLastXError;
}*/

static void Error(int err, const char* pszText)
{
	fprintf(stderr,"Error (%d): %s\n",err,pszText);
}

typedef struct { WACOMDEVICETYPE type; const char* pszText; } TYPEXLAT;

static int ListDev(WACOMCONFIG *hConfig, char** argv)
{
	const char* pszType;
	WACOMDEVICEINFO* pInfo;
	unsigned int i, j, uCount;
	char nameOut[60];

	static TYPEXLAT xTypes[] =
	{
		{ WACOMDEVICETYPE_CURSOR, "cursor" },
		{ WACOMDEVICETYPE_STYLUS, "stylus" },
		{ WACOMDEVICETYPE_ERASER, "eraser" },
		{ WACOMDEVICETYPE_TOUCH, "touch" },
		{ WACOMDEVICETYPE_PAD, "pad" }
	};

	if (*argv != NULL)
		fprintf(stderr,"ListDev: Ignoring extraneous arguments.\n");

	if (WacomConfigListDevices(hConfig,&pInfo,&uCount))
		return 1;

	for (i=0; i<uCount; ++i)
	{
		pszType = "unknown";
		for (j=0; j<sizeof(xTypes)/sizeof(*xTypes); ++j)
			if (xTypes[j].type == pInfo[i].type)
				pszType = xTypes[j].pszText;

		/* tcl/tk (wacomcpl) has problem to process spaced names 
		 * so we make the name into one string */
		for(j=0; j<strlen(pInfo[i].pszName); j++)
		{
			if(pInfo[i].pszName[j] == ' ') 
				nameOut[j] = '_';
			else
				nameOut[j] = pInfo[i].pszName[j];
		}
		nameOut[strlen(pInfo[i].pszName)] = '\0';

		fprintf(stdout,"%s     %s\n", nameOut, pszType);
	}

	WacomConfigFree(pInfo);
	return 0;
}

static int ListParam(WACOMCONFIG *hConfig, char** argv)
{
	PARAMINFO* p;

	if (*argv != NULL)
		fprintf(stderr,"ListParam: Ignoring extraneous arguments.\n");

	for (p=gParamInfo; p->pszParam; ++p)
		fprintf(stdout,"%-16s - %s\n",
			p->pszParam, p->pszDesc);

	printf ("\nEvent description format:\n"
		"[CORE] [EVENT TYPE] [MODIFIERS] [CODE]\n"
		"  CORE: Emit core events irrespective of the SendCoreEvents setting\n"
		"  EVENT TYPE: the type of event to emit:\n"
		"\tKEY: Emit a key event\n"
		"\tBUTTON: Emit a button event\n"
		"\tDBLCLICK: Emit a double-click button event\n"
		"\tMODETOGGLE: Toggle absolute/relative tablet mode\n");
	printf ("\tDISPLAYTOGGLE: Toggle cursor movement among all displays \n"
		"\t    which include individual screens plus the whole desktop \n"
		"\t    for the selected tool if it is not a pad. \n"
		"\t    When the tool is a pad, the function applies to all \n"
		"\t    tools that are asssociated with the tablet\n");
	printf ("\tSCREENTOGGLE: Toggle cursor movement among all screens \n"
		"\t    for the selected tool if it is not a pad. \n"
		"\t    When the tool is a pad, the function applies to all \n"
		"\t    tools that are asssociated with the tablet\n"
		"  Modifier: use \"xsetwacom list mod\"\n"
		"\t  to see a list of modifiers and specific keys\n"
		"  CODE: Button number if emitting a button event; \n"
		"\tor specific keys and any other keys not listed as mod \n");
	printf ("Examples:\n"
		"  xsetwacom set stylus Button1 \"button 5\"\n"
		"\t(emit mouse button 5 event)\n" 
		"  xsetwacom set stylus Button3 \"dblclick 1\"\n"
		"\t(emit mouse double left click event)\n" 
		"  xsetwacom set pad Button2 \"core key ctrl alt F2\"\n"
		"\t(emit ctrl + alt + F2 key event)\n" 
		"  xsetwacom set pad Button3 \"core key quotedbl a space test space string quotedbl\"\n");
	printf ("\t(emit keystroke \"a test string\" event)\n" 
		"  xsetwacom set pad Button3 \"core key quotedbl a test string quotedbl\"\n"
		"\t(emit keystroke \"ateststring\" event)\n" 
		"  xsetwacom set pad striplup \"core key up\"\n"
		"\t(emit up arrow key event)\n" 
		"  xsetwacom set pad stripldn \"core key down\"\n"
		"\t(emit down arrow key event)\n");

	return 0;
}

static int List(WACOMCONFIG *hConfig, char** argv)
{
	if (*argv == NULL)
		return ListDev(hConfig,argv);

	if (!strcmp(*argv,"dev"))
		return ListDev(hConfig,argv+1);

	if (!strcmp(*argv,"param"))
		return ListParam(hConfig,argv+1);

	if (!strcmp(*argv,"mod"))
		return xf86WcmListMod(argv+1);

	fprintf(stderr,"List: Unknown argument '%s'\n",*argv);
	return 1;
}

static int Pack(int nCount, int* pnValues, int* pnResult, PARAMINFO* p)
{
	if (nCount == 4)
	{
		int i, check = 0;
		for (i = 0; i<2; i++)
			if ((pnValues[i] < p->nMin) || (pnValues[i]> p->nMax))
				check = 1;
		if (!check)
			if (((pnValues[0] + pnValues[3]) != p->nMax) 
				|| ((pnValues[1] + pnValues[2]) != p->nMax))
				check = 1;
		if (check)
		{
			fprintf(stderr,"Pack: Value for '%s' out of range "
				"(%d - %d)\n", p->pszParam, p->nMin, p->nMax);
			return 1;
		}

		*pnResult =
			((pnValues[0] & 0xFF) << 24) |
			((pnValues[1] & 0xFF) << 16) |
			((pnValues[2] & 0xFF) << 8) |
			(pnValues[3] & 0xFF);
		return 0;
	}

	fprintf(stderr,
	"Set: Incorrect number of values for bezier curve.\n"
	"Please check your values' format. \n"
	"Bezier curve is specified by two control points x0,y0 and x1,y1\n"
	"Ranges are 0 to 100 for each value corresponding to 0.00 to 1.00\n"
	"A straight line would be: 0 0 100 100\n"
	"A slightly depressed line might be: 15 0 100 85\n"
	"A slightly amplified line might be: 0 15 85 100\n");

	return 1;
}

static int ReCount(int * nValues, const char * pszValues)
{
	int nCount = 0;
	const char* pszEnd = pszValues;
	const char* tmpValues;

	do
	{
		tmpValues = pszEnd;
		if (strncasecmp(tmpValues,"0x",2) == 0)
			nValues[nCount++] = strtol(tmpValues,(char **)&pszEnd,16);
		else
			nValues[nCount++] = strtol(tmpValues,(char **)&pszEnd,10);
	} while (pszEnd != tmpValues && (*pszEnd != '\0') && pszEnd[0] == ' ' );

	return nCount;
}

static int ParseValues(int nCount, const char* pszValues, int* nValues, 
	int* nValue, unsigned * keys, PARAMINFO* p, const char * devName)
{
	if (p->nType == BOOLEAN_VALUE ||
		 p->nType == SINGLE_VALUE ||
		 p->nType == ACTION_VALUE)
	{
		if ((p->nType == BOOLEAN_VALUE) &&
			(!strcasecmp(pszValues,"on") ||
			!strcasecmp(pszValues,"true") ||
			!strcasecmp(pszValues,"enable") ||
			!strcasecmp(pszValues,"1") ||
			!strcasecmp(pszValues,"absolute")))
				*nValue = 1;
		else if ((p->nType == BOOLEAN_VALUE) &&
			(!strcasecmp(pszValues,"off") ||
			!strcasecmp(pszValues,"false") ||
			!strcasecmp(pszValues,"disable") ||
			!strcasecmp(pszValues,"0") ||
			!strcasecmp(pszValues,"relative")))
				*nValue = 0;
		else if (p->nType == ACTION_VALUE)
		{
			*nValue = xf86WcmDecode (devName,
						p->pszParam,
						pszValues,
						keys);
		}
		else if (p->nType == SINGLE_VALUE)
		{
		    if (p->nParamID == XWACOM_PARAM_TWINVIEW ||
			p->nParamID == XWACOM_PARAM_ROTATE)
		    {
			int check = 0, j;
			const char ** option_char = rt_char;
			if (p->nParamID == XWACOM_PARAM_TWINVIEW)
				option_char = tv_char;
			for (j = p->nMin; j <= p->nMax; j++)
				if (!strcasecmp(pszValues, option_char[j]))
				{
					check = 1;
					*nValue = j;
				}
 			if (!check)
			{
				fprintf(stderr,"ParseValues: Value '%s' is "
					"invalid.\n", pszValues);
				return 1;
			}
		    }
		    else
		   	 *nValue = nValues[0];
		}
		/* Is there a range and are we in it? */
		if (p->bRange &&
			((*nValue < p->nMin) || (*nValue > p->nMax)))
		{
			fprintf(stderr,"ParseValues: Value for '%s' (%d) out of range "
			"(%d - %d)\n", p->pszParam, *nValue, p->nMin, p->nMax);
			return 1;
		}
	}
	else if (p->nType == PACKED_CURVE)
	{
		if (nCount == 1)
			nCount = ReCount(nValues, pszValues);

		if (Pack(nCount,nValues,nValue,p))
			return 1;
	}
	else if (p->nType == TWO_VALUES)
	{
		if (nCount == 1)
			nCount = ReCount(nValues, pszValues);

		if (nCount != 2)
		{
			fprintf (stderr, "ParseValues: only two values allowed for %s. \n"
				"Please check your values' format. \n", p->pszParam);
			return 1;
		}
		*nValue = nValues [0] | (nValues [1] << 16);
	}
	else
	{
		fprintf(stderr,"ParseValues: Value (%s) for '%s' is "
			"invalid.\n", pszValues, p->pszParam);
		return 1;
	}
	return 0;
}

static int Set(WACOMCONFIG * hConfig, char** argv)
{
	PARAMINFO* p;
	WACOMDEVICE * hDev;
	char* a, *pszEnd;
	const char* pszValues[32];
	int nValues[4];
	const char* pszDevName = NULL;
	const char* pszParam = NULL;
	int i, nValue=0, nReturn, nCount = 0;
	unsigned keys[256];

	while ((a=*argv++) != NULL)
	{
		if (!pszDevName) pszDevName = a;
		else if (!pszParam) pszParam = a;
		else if (nCount < 4) pszValues[nCount++] = a;
		else
		{
			fprintf(stderr,"Set: Unknown argument '%s'\n",a);
			return 1;
		}
	}
	/* No device or param? Error. */
	if (!pszDevName || !pszParam)
		{ Usage(stderr); return 1; }

	/* Find param. */
	for (p=gParamInfo; p->pszParam; ++p)
		if (strcasecmp(p->pszParam,pszParam) == 0)
			break;

	/* Unknown? Complain. */
	if (!p->pszParam)
	{
		fprintf(stderr,"Set: Unknown parameter '%s'\n",pszParam);
		return 1;
	}

	if (p->nParamID > XWACOM_PARAM_GETONLYPARAM)
	{
		fprintf(stderr,"Set: '%s' doesn't support set option.\n",
			pszParam);
		return 1;
	}

	/* Set case correctly for error messages below. */
	pszParam = p->pszParam;

	/* If value is empty, do we support this? */
	if (!nCount && !p->bEmptyOK)
	{
		fprintf(stderr,"Set: Value for '%s' must be specified.\n",
			pszParam);
		return 1;
	}
	if (!nCount)
		nValue = p->nDefault;
	else
	{
		for (i=0; i<nCount; i++)
		{
			/* Convert value to 32 bit integer; hex OK, octal is not. */
			if (strncasecmp(pszValues[i],"0x",2) == 0)
				nValues[i] = strtol(pszValues[i],&pszEnd,16);
			else
				nValues[i] = strtol(pszValues[i],&pszEnd,10);
		}
		if (p->nType == SINGLE_VALUE && nCount == 1 &&
			(*pszEnd == '\0')) /* numbers */
		{
			nValue = nValues[0];
			/* Is there a range and are we in it? */
			if (p->bRange &&
				((nValue < p->nMin) || (nValue > p->nMax)))
			{
				fprintf(stderr,"Set: Value for '%s' (%d) out of range "
				"(%d - %d)\n", p->pszParam, nValue, p->nMin, p->nMax);
				return 1;
			}
		}
		else if ((strlen (pszValues[0]) > 255) || 
			ParseValues(nCount, pszValues[0],
			nValues, &nValue, keys, p, pszDevName) ) 
			return 1;
	}

	/* Looks good, send it. */
	if (gnVerbose)
		printf("Set: sending %d %d (0x%X)\n",p->nParamID,nValue,nValue);

	/* Open device */
	hDev = WacomConfigOpenDevice(hConfig, pszDevName);
	if (!hDev)
	{
		fprintf(stderr,"Set: Failed to open device '%s'\n",
			pszDevName);
		return 1;
	}

	/* Send request */
	nReturn = WacomConfigSetRawParam(hDev, p->nParamID, nValue, keys);

	if (nReturn)
		fprintf(stderr,"Set: Failed to set %s value for '%s'\n",
			pszDevName, pszParam);

	/* Close device and return */
 	(void)WacomConfigCloseDevice(hDev);
	return nReturn ? 1 : 0;
}

static void DisplayValue (WACOMDEVICE *hDev, const char *devname, PARAMINFO *p,
			  int disperr, int valu)
{
	int value = 0, sl = 0, i = 0;
        char strval [200] = "";
	unsigned keys[256];
	if (WacomConfigGetRawParam (hDev, p->nParamID, &value, valu, keys))
	{
		fprintf (stderr, "GetRawParam: Failed to get %s value for '%s'\n",
			 devname, p->pszParam);
		return;
	}

	switch (p->nType)
	{
	case SINGLE_VALUE:
		if (p->nParamID == XWACOM_PARAM_TWINVIEW ||
			p->nParamID == XWACOM_PARAM_ROTATE)
		{
			const char ** option_char = rt_char;
			if (p->nParamID == XWACOM_PARAM_TWINVIEW)
				option_char = tv_char;
		
				snprintf (strval, sizeof (strval), 
					"%s", option_char[value] );
		}
		else if (p->nParamID == XWACOM_PARAM_TOOLSERIAL)
			snprintf (strval, sizeof (strval), "%u", value);
		else
			snprintf (strval, sizeof (strval), "%d", value);
                break;
	case PACKED_CURVE:
		snprintf (strval, sizeof (strval), gGetFormat == gfXCONF ?
			  "%d,%d,%d,%d" : "%d %d %d %d",
			  (value >> 24) & 0xff, (value >> 16) & 0xff,
			  (value >> 8) & 0xff, value & 0xff);
                break;
	case TWO_VALUES:
		snprintf (strval, sizeof (strval), gGetFormat == gfXCONF ?
			  "%d,%d" : "%d %d",
			  value & 0xffff, (value >> 16) & 0xffff);
                break;
	case BOOLEAN_VALUE:
		snprintf (strval, sizeof (strval), "%s", value ? "on" : "off");
		break;
	case ACTION_VALUE:
		sl = 0;
		if (value & AC_CORE)
			sl += snprintf (strval + sl, sizeof (strval) - sl, "CORE ");
		switch (value & AC_TYPE)
		{
		    case AC_BUTTON:
			sl += snprintf (strval + sl, sizeof (strval) - sl, "BUTTON ");
			break;
		    case AC_KEY:
			sl += snprintf (strval + sl, sizeof (strval) - sl, "KEY ");
			break;
		    case AC_MODETOGGLE:
			sl += snprintf (strval + sl, sizeof (strval) - sl, "MODETOGGLE ");
			break;
		    case AC_SCREENTOGGLE:
			sl += snprintf (strval + sl, sizeof (strval) - sl, "SCREENTOGGLE ");
			break;
		    case AC_DBLCLICK:
			sl += snprintf (strval + sl, sizeof (strval) - sl, "DBLCLICK ");
			break;
		    case AC_DISPLAYTOGGLE:
			sl += snprintf (strval + sl, sizeof (strval) - sl, "DISPLAYTOGGLE ");
			break;
		}
		if ((value & AC_TYPE) != AC_KEY)
		{
			sl += snprintf (strval + sl, sizeof (strval) - sl, "%d",
					value & AC_CODE);
		}
		else if (!(value & AC_CODE))
		{
			printf ("xsetwacom %s %s \"%s\" missing keystrokes \n", devname, p->pszParam, strval);
			return;
		}
		else
		{
			char keyString[32];
			if (gGetFormat == gfXCONF)
			{
				printf ("Button keystroke is only an xsetwacom command \n");
				return;
			}
			if (!xf86WcmGetString(keys[0], keyString))
			{
				printf ("Button keystroke key error \n");
				return;
			}
			i=0;
			while (i<((value & AC_NUM_KEYS)>>20))
			{
				if (!xf86WcmGetString(keys[i++], keyString))
					return;

				if (strlen(keyString) == 1)
					sl += snprintf (strval + sl, sizeof (strval) - sl, "%s", keyString);
				else
					sl += snprintf (strval + sl, sizeof (strval) - sl, " %s ", keyString);
			}
		}
		if (strval [sl - 1] == ' ')
			strval [sl - 1] = 0;
		break;
	}

	switch (gGetFormat)
	{
	case gfSHELL:
		if (p->nParamID < XWACOM_PARAM_GETONLYPARAM)
			printf ("xsetwacom set %s %s \"%s\"\n", devname, p->pszParam, strval);
		else
			printf ("%s option is only an xsetwacom get command \n", p->pszParam);
                break;
	case gfXCONF:
		if (p->nParamID > XWACOM_PARAM_NOXOPTION)
		{
			if (p->nParamID < XWACOM_PARAM_GETONLYPARAM)
				printf ("%s is not an X configuration option \n", p->pszParam);
			else
				printf ("%s option is only an xsetwacom get command \n", p->pszParam);
		}
		else
		{
			if ((p->nParamID >= XWACOM_PARAM_BUTTON1) &&
				(p->nParamID <= XWACOM_PARAM_STRIPRDN) &&
				((value & AC_TYPE) != AC_BUTTON))
				printf ("%s option is only an xsetwacom command \n", p->pszParam);
			else
				printf ("\tOption\t\"%s\"\t\"%s\"\n", p->pszParam, strval);
		}
		break;
	default:
		if ( (p->nType == ACTION_VALUE) && 
			(((value & AC_TYPE) == AC_KEY) ||  
			((value & AC_TYPE) == AC_MODETOGGLE) ||
			((value & AC_TYPE) == AC_DBLCLICK) ||
			((value & AC_TYPE) == AC_SCREENTOGGLE) ||
			((value & AC_TYPE) == AC_DISPLAYTOGGLE)) )
			printf ("%s\n", strval);
		else
			printf ("%d\n", value);
		break;
	}
}

static int Get(WACOMCONFIG *hConfig, char **argv, int valu)
{
	WACOMDEVICE *hDev;
	const char *devname;

	/* First argument is device name. */

	if (!*argv)
	{
		fprintf(stderr,"Get: Expecting device name\n");
		return 1;
	}

	/* Open device */
	devname = *argv++;
	hDev = WacomConfigOpenDevice (hConfig, devname);
	if (!hDev)
	{
		fprintf(stderr,"Get: Failed to open device '%s'\n", devname);
		return 1;
	}

	/* Interpret every following argument as parameter name
	 * and display its value.
	 */
	while (*argv)
	{
		PARAMINFO* p;

		for (p = gParamInfo; p->pszParam; ++p)
			if (strcasecmp (p->pszParam, *argv) == 0)
				break;

		/* Unknown? Complain. */
		if (!p->pszParam)
		{
			if (strcasecmp (*argv, "all") == 0)
			{
				for (p = gParamInfo; p->pszParam; ++p)
					DisplayValue (hDev, devname, p, 0, valu);
				argv++;
				continue;
			}

			fprintf (stderr,"Get: Unknown parameter '%s'\n", *argv);
			return 1;
		}

		/* no X option */
		if (p->nParamID > XWACOM_PARAM_NOXOPTION && gGetFormat == gfXCONF)
			fprintf (stderr,"Get: parameter '%s' is only an xsetwacom command\n", *argv);
		else
			DisplayValue (hDev, devname, p, 1, valu);
		argv++;
	}

	/* Close device and return */
	WacomConfigCloseDevice (hDev);

	return 0;
}

int DoCommand (COMMAND cmd, char** argv)
{
	int nReturn;
	Display* pDisp;
	WACOMCONFIG * hConf;

	pDisp = XOpenDisplay(gpszDisplayName);
	if (!pDisp)
	{
		fprintf(stderr,"Failed to open display (%s)\n",
			gpszDisplayName ? gpszDisplayName : "");
		return 1;
	}

	XSetErrorHandler(XError);
	XSynchronize(pDisp,1 /*sync on*/);

	hConf = WacomConfigInit(pDisp,Error);
	if (!hConf)
	{
		fprintf(stderr,"Failed to init WacomConfig\n");
		XCloseDisplay(pDisp);
		return 1;
	}

	switch (cmd)
	{
		case COMMAND_LIST:
			nReturn = List(hConf,argv);
			break;

		case COMMAND_SET:
			nReturn = Set(hConf,argv);
			break;

		case COMMAND_GET:
			nReturn = Get(hConf,argv,1);
			break;

		case COMMAND_GETDEFAULT:
			nReturn = Get(hConf,argv,3);
			break;

		default:
			assert(0);
	}

	WacomConfigTerm(hConf);
	XCloseDisplay(pDisp);
	return nReturn;
}

int main(int argc, char** argv)
{
	char* a;

	++argv;
	while ((a=*argv++) != NULL)
	{
		if (!strcmp(a,"-h") || !strcmp(a,"--help"))
		{
			Usage(stdout);
			return 0;
		}
		else if (!strcmp(a,"-s") || !strcmp(a,"--shell"))
			gGetFormat = gfSHELL;
		else if (!strcmp(a,"-x") || !strcmp(a,"--xconf"))
			gGetFormat = gfXCONF;
		else if (!strcmp(a,"-v") || !strcmp(a,"--verbose"))
			++gnVerbose;
		else if (!strcmp(a,"-V") || !strcmp(a,"--version"))
		{
			fprintf(stdout,"%s\n",XSETWACOM_VERSION);
			return 0;
		}
		else if (!strcmp(a,"-d") || !strcmp(a,"--display"))
		{
			a = *argv++;
			if (!a)
			{
				fprintf(stderr,"Missing display name\n");
				return 1;
			}
			gpszDisplayName = a;
		}

		/* commands */
		else if (!strcmp(a,"list"))
			return DoCommand(COMMAND_LIST,argv);
		else if (!strcmp(a,"set"))
			return DoCommand(COMMAND_SET,argv);
		else if (!strcmp (a, "get"))
			return DoCommand (COMMAND_GET,argv);
		else if (!strcmp (a, "getdefault"))
			return DoCommand (COMMAND_GETDEFAULT,argv);
		else
		{
			fprintf(stderr,"Unknown command '%s'\n", a);
			Usage(stderr);
			return 1;
		}
	}

	Usage(stdout);
	return 0;
}
