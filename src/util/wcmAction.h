/*****************************************************************************
** wcmAction.h
**
** Copyright (C) 2006 - Andrew Zabolotny
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
**   2006-07-17 0.0.1-az - Initial release
*/

/* This pseudo-header file is included both from the X11 driver, and from
 * tools (notably xsetwacom). The reason is to have the function defined
 * in one place, to avoide desyncronization issues between two pieces of
 * almost identical code.
 */

#include "../include/Xwacom.h" 

typedef struct _ACTIONCODE ACTIONCODE;

struct _ACTIONCODE
{
	const char *keyword;
	unsigned value;
};

int xf86WcmDecode (const char *dev, const char *but, const char *ev, unsigned * entev);
int xf86WcmListMod(char** argv);
int xf86WcmGetString(unsigned keySym, char * kstring);
