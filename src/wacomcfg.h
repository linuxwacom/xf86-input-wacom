/*****************************************************************************
** wacomcfg.h
**
** Copyright (C) 2003 - John E. Joganic
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
****************************************************************************/

#ifndef __LINUXWACOM_WACOMCFG_H
#define __LINUXWACOM_WACOMCFG_H

#include <X11/Xlib.h>

/* JEJ - NOTE WE DO NOT INCLUDE Xwacom.h HERE.  THIS ELIMINATES A CONFLICT
 *       WHEN THIS FILE IS INSTALLED SINCE Xwacom.h WILL IN MANY CASES NOT
 *       GO WITH IT.  SOMEDAY IT MAY BE PART OF XFREE86. */

typedef struct { int __unused; } _WACOMCONFIG, *WACOMCONFIG;
typedef struct { int __unused; } _WACOMDEVICE, *WACOMDEVICE;
typedef void (*WACOMERRORFUNC)(int err, const char* pszText);
typedef struct _WACOMDEVICEINFO WACOMDEVICEINFO;

typedef enum
{
	WACOMDEVICETYPE_UNKNOWN,
	WACOMDEVICETYPE_CURSOR,
	WACOMDEVICETYPE_STYLUS,
	WACOMDEVICETYPE_ERASER,
	WACOMDEVICETYPE_MAX,
} WACOMDEVICETYPE;

struct _WACOMDEVICEINFO
{
	const char* pszName;
	WACOMDEVICETYPE type;
};

/*****************************************************************************
** Functions
*****************************************************************************/

WACOMCONFIG WacomConfigInit(Display* pDisplay, WACOMERRORFUNC pfnErrorHandler);
/* Initializes configuration library.
 *   pDisplay        - display to configure
 *   pfnErrorHandler - handler to which errors are reported; may be NULL
 * Returns WACOMCONFIG handle on success, NULL on error.
 *   errno contains error code. */

void WacomConfigTerm(WACOMCONFIG hConfig);
/* Terminates configuration library, releasing display. */

int WacomConfigListDevices(WACOMCONFIG hConfig, WACOMDEVICEINFO** ppInfo,
	unsigned int* puCount);
/* Returns a list of wacom devices.
 *   ppInfo         - pointer to WACOMDEVICEINFO* to receive device data
 *   puSize         - pointer to receive device count
 * Returns 0 on success, -1 on failure.  errno contains error code.
 * Comments: You must free this structure using WacomConfigFree. */

void WacomConfigFree(void* pvData);
/* Frees memory allocated by library. */

#endif /* __LINUXWACOM_WACOMCFG_H */

