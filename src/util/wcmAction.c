/*****************************************************************************
** wcmAction.c
**
** Copyright (C) 2007 - 2008 - Ping Cheng
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Lesser General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
**
** REVISION HISTORY
**   2007-02-06 0.0.1-pc - Support keystrokes
**   2008-01-17 0.0.2-pc - Add Display Toggle
**   2008-08-01 0.0.3-pc - Merge patch 1998051 (Yuri Shchedov)
**   2008-12-10 0.0.4-pc - Updated patch 1998051 for none KP buttons
*/

/* This pseudo-header file is included both from the X11 driver, and from
 * tools (notably xsetwacom). The reason is to have the function defined
 * in one place, to avoide desyncronization issues between two pieces of
 * almost identical code.
 */

#include "../include/Xwacom.h" 
#include "wcmAction.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <X11/Xlib.h>

static ACTIONCODE action_code [] =
{
	{ "CORE",          AC_CORE },
	{ "KEY",           AC_KEY },
	{ "BUTTON",        AC_BUTTON },
	{ "MODETOGGLE",    AC_MODETOGGLE },
	{ "DBLCLICK",      AC_DBLCLICK },
	{ "DISPLAYTOGGLE", AC_DISPLAYTOGGLE }
};

static ACTIONCODE modifier_code [] =
{
	{ "SHIFT",      XK_Shift_L },
	{ "CTRL",       XK_Control_L },
	{ "CONTROL",    XK_Control_L },
	{ "META",       XK_Meta_L },
	{ "ALT",        XK_Alt_L },
	{ "SUPER",      XK_Super_L },
	{ "HYPER",      XK_Hyper_L }
};

static ACTIONCODE specific_code [] =
{
	{ "F1",		    XK_F1 },
	{ "F2",		    XK_F2 },
	{ "F3",		    XK_F3 },
	{ "F4",		    XK_F4 },
	{ "F5",		    XK_F5 },
	{ "F6",		    XK_F6 },
	{ "F7",		    XK_F7 },
	{ "F8",		    XK_F8 },
	{ "F9",		    XK_F9 },
	{ "F10",	    XK_F10 },
	{ "F11",	    XK_F11 },
	{ "F12",	    XK_F12 },
	{ "Esc",	    XK_Escape },
	{ "Tab",	    XK_Tab },
	{ "CapsLock",	    XK_Caps_Lock },
	{ "Pause",	    XK_Pause },
	{ "ScrollLock",	    XK_Scroll_Lock },
	{ "SysReq",	    XK_Sys_Req },
	{ "Home",	    XK_Home },
	{ "PgUp",	    XK_Page_Up },
	{ "PageUp",	    XK_Page_Up },
	{ "PgDn",	    XK_Page_Down },
	{ "PageDown",	    XK_Page_Down },
	{ "End",	    XK_End },
	{ "Insert",	    XK_Insert },
	{ "Ins",	    XK_Insert },
	{ "Delete",	    XK_Delete },
	{ "Del",	    XK_Delete },
	{ "Left",	    XK_Left },
	{ "Up",		    XK_Up },
	{ "Down",	    XK_Down },
	{ "Right",	    XK_Right },
	{ "BackSpace",	    XK_BackSpace },
	{ "Enter",	    XK_Return },
	{ "NumLock",	    XK_Num_Lock },
	{ "space",	    XK_space },
	{ "quotedbl",       XK_quotedbl },
	{ "backslash",      XK_backslash },
	{ "KPHome",	    XK_KP_Home },
	{ "KPPgUp",	    XK_KP_Page_Up },
	{ "KPPageUp",	    XK_KP_Page_Up },
	{ "KPPgDn",	    XK_KP_Page_Down },
	{ "KPPageDown",	    XK_KP_Page_Down },
	{ "KPEnd",	    XK_KP_End },
	{ "KPInsert",	    XK_KP_Insert },
	{ "KPIns",	    XK_KP_Insert },
	{ "KPDelete",	    XK_KP_Delete },
	{ "KPDel",	    XK_KP_Delete },
	{ "KPKPLeft",	    XK_KP_Left },
	{ "KPUp",	    XK_KP_Up },
	{ "KPDown",	    XK_KP_Down },
	{ "KPRight",	    XK_KP_Right },
	{ "KPEnter",	    XK_KP_Enter },
	{ "KPPlus",	    XK_KP_Add },
	{ "KPMinus",	    XK_KP_Subtract},
	{ "KPDivide",	    XK_KP_Divide },
	{ "KPMultiply",	    XK_KP_Multiply },
	{ "NumpadPlus",     XK_KP_Add },
	{ "NumpadMinus",    XK_KP_Subtract},
	{ "NumpadDivide",   XK_KP_Divide },
        { "NumpadMultiply", XK_KP_Multiply },
	{ "break",	    XK_Break },
	{ "print",	    XK_Print }
};

static ACTIONCODE key_code [] =
{
	{ "#",	XK_numbersign },
	{ "$",	XK_dollar },
	{ "%",	XK_percent },
	{ "&",	XK_ampersand },
	{ "'",	XK_quoteright },
	{ "(",	XK_parenleft },
	{ ")",	XK_parenright },
	{ "*",	XK_asterisk },
	{ "+",	XK_plus },
	{ ",",	XK_comma },
	{ "-",	XK_minus },
	{ ".",	XK_period },
	{ "/",	XK_slash },
	{ ":",	XK_colon },
	{ ";",	XK_semicolon },
	{ "<",	XK_less },
	{ "=",	XK_equal },
	{ ">",	XK_greater },
	{ "?",	XK_question },
	{ "@",	XK_at },
	{ "[",	XK_bracketleft },
	{ "]",	XK_bracketright },
	{ "^",	XK_asciicircum },
	{ "_",	XK_underscore },
	{ "`",	XK_grave },
	{ "{",	XK_braceleft },
	{ "}",	XK_braceright },
	{ "|",	XK_bar },
	{ "~",	XK_asciitilde },
	{ "!",	XK_exclam }
};

static ACTIONCODE number_code [] =
{
	{ "0",	XK_0 },
	{ "1",	XK_1 },
	{ "2",	XK_2 },
	{ "3",	XK_3 },
	{ "4",	XK_4 },
	{ "5",	XK_5 },
	{ "6",	XK_6 },
	{ "7",	XK_7 },
	{ "8",	XK_8 },
	{ "9",	XK_9 },

};

void xf86WcmDecodeKey(char *ev, unsigned * entev, ACTIONCODE * code, int codesize, int * butev)
{
    int i, num_keys = ((*butev) & AC_NUM_KEYS) >> 20;
    char keys[2] = " ";
    int n = 0;
    keys[0] = ev[0];
    if (code)
    {
	for (i = 0; i < codesize; i++)
	{
	    if (keys[0] == code[i].keyword[0])
		n = code[i].value;
	}
    }
    else
	n = XStringToKeysym (keys);

    if (n)
	entev[num_keys++] =  n;
    *butev = ((*butev) & AC_EVENT) | (num_keys << 20) 
		| (num_keys ? entev[0] : 0);
}


char * xf86WcmDecodeWord(char *ev, unsigned * entev, ACTIONCODE * code, int codesize, int * butev)
{
    int i, num_keys = ((*butev) & AC_NUM_KEYS) >> 20;
    for (;;)
    {
	while (ev && (*ev == ' ' || *ev == '\t'))
	    ev++;

	for (i = 0; i < codesize; i++)
	{
	    int sl = strlen (code [i].keyword);
	    if ( (strlen(ev) >= sl) &&
		(ev [sl] == 0 || ev [sl] == ' ' || ev [sl] == '\t') &&
			!strncasecmp (ev, code [i].keyword, sl))
	    {
		if (code [i].value & AC_CODE)
		    entev[num_keys++] =  code [i].value;
		else
		    (*butev) |= code [i].value;
				
		ev += sl;
		if (!strlen(ev)) 
		    i = codesize;
		break;
	    }
	}

	if (i >= codesize)
	    break;
    }
    *butev = ((*butev) & AC_EVENT) | (num_keys << 20) 
		| (num_keys ? entev[0] : 0);
    return ev;
}

int xf86WcmDecode (const char *dev, const char *but, const char *ev, unsigned * entev)
{
    int butev = 0, 
	codesize = sizeof (action_code) / sizeof (action_code [0]), 
	num_keys = (butev & AC_NUM_KEYS) >> 20;
    char new_ev[256] ="", *ev_p;

    strcat(new_ev, ev);
    ev_p = new_ev;
    if (!strlen(ev)) 
    {
	printf("xf86WcmDecodeAction No action defined\n");
	return 0;
    }

    /* Get action type first */
    ev_p = xf86WcmDecodeWord(new_ev, entev, action_code, codesize, &butev);
    switch (butev & AC_TYPE)
    {
	case AC_BUTTON:
	    if (strlen(ev_p))
	    {
		char *end;
		int n = strtol (ev_p, &end, 0);
	    	if (ev_p == end)
			printf ("xf86WcmDecode %s: invalid %s value: button (%x) \"%s\".  Ignore it (assign to 0)\n",
				dev, but, butev, ev);
		butev |= n;
	    }
	case AC_MODETOGGLE:
	case AC_DBLCLICK:
	case AC_DISPLAYTOGGLE:
	    break;
	case AC_KEY:
	    if (!strlen(ev_p))
	    {
		printf ("xf86WcmDecode %s: invalid %s value: button has no key defined.\n", dev, but);
		return 0;
	    }
	    codesize = sizeof (modifier_code) / sizeof (modifier_code [0]);
	    ev_p = xf86WcmDecodeWord(ev_p, entev, modifier_code, codesize, &butev);
	    codesize = sizeof (specific_code) / sizeof (specific_code [0]);
	    ev_p = xf86WcmDecodeWord(ev_p, entev, specific_code, codesize, &butev);

	    while(strlen(ev_p))
	    {
		if (ev_p[0] == ' ')
		{
		    ev_p++;
		    codesize = sizeof (specific_code) / sizeof (specific_code [0]);
		    ev_p = xf86WcmDecodeWord(ev_p, entev, specific_code, codesize, &butev);
		}
		codesize = sizeof (key_code) / sizeof (key_code [0]);
		num_keys = (butev & AC_NUM_KEYS) >> 20;
		xf86WcmDecodeKey(ev_p, entev, key_code, codesize, &butev);
		if (num_keys == ((butev & AC_NUM_KEYS) >> 20))
		{
		    codesize = sizeof (number_code) / sizeof (number_code [0]);
		    xf86WcmDecodeKey(ev_p, entev, number_code, codesize, &butev);
		    if (num_keys == ((butev & AC_NUM_KEYS) >> 20))
			xf86WcmDecodeKey(ev_p, entev, 0, 1, &butev);
		}
		ev_p++;
	    }
	    break;
    }
    return butev;
}

int xf86WcmListMod(char** argv)
{
    int modsize = sizeof (modifier_code) / sizeof (modifier_code [0]), i;
    if (*argv != NULL)
	fprintf(stderr,"ListMod: Ignoring extraneous arguments (%s).\n", *argv);

    fprintf(stderr,"ListMod: %d modifiers are supported:\n\n", modsize);
    for (i=0; i<modsize; i++)
	fprintf(stderr,"\t%s\n", modifier_code[i].keyword);

    modsize = sizeof (specific_code) / sizeof (specific_code [0]);
    fprintf(stderr,"\n\nListMod: %d special keys are supported. "
	"For example: to send \", you need to use quotedbl:\n\n", modsize);
    for (i=0; i<modsize; i++)
	fprintf(stderr,"\t%s\n", specific_code[i].keyword);

    return 0;
}

int xf86WcmGetString(unsigned keySym, char * kstring)
{
    int i = 0;

    kstring[0] = 0;
    for (i=0; i<sizeof (action_code) / sizeof (action_code [0]); i++)
	if (action_code[i].value == keySym)
	{
	    strcat(kstring, action_code[i].keyword);
	    return 1;
	}	
    for (i=0; i<sizeof (modifier_code) / sizeof (modifier_code [0]); i++)
	if (modifier_code[i].value == keySym)
	{
	    strcat(kstring, modifier_code[i].keyword);
	    return 1;
	}	
	
    for (i=0; i<sizeof (specific_code) / sizeof (specific_code [0]); i++)
	if (specific_code[i].value == keySym)
	{
	    strcat(kstring, specific_code[i].keyword);
	    return 1;
	}	

    for (i=0; i<sizeof (key_code) / sizeof (key_code [0]); i++)
	if (key_code[i].value == keySym)
	{
	    strcat(kstring, key_code[i].keyword);
	    return 1;
	}	

    for (i=0; i<sizeof (number_code) / sizeof (number_code [0]); i++)
	if (number_code[i].value == keySym)
	{
	    strcat(kstring, number_code[i].keyword);
	    return 1;
	}
    strcat(kstring, XKeysymToString((KeySym)keySym));
    return 1;
}
