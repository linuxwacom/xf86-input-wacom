/*****************************************************************************
** wactablet.c
**
** Copyright (C) 2002, 2003 - John E. Joganic
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
****************************************************************************/

#include "wactablet.h"
#include "wacserial.h"
#include "wacusb.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <memory.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WAC_ENABLE_LINUXINPUT
#include <linux/input.h>
#endif

/*****************************************************************************
** Implementation
*****************************************************************************/

static int WacomIsSerial(int fd)
{
	return isatty(fd);
}

static int WacomIsUSB(int fd)
{
#ifdef WAC_ENABLE_LINUXINPUT
	short sID[4];
	if (ioctl(fd,EVIOCGID,sID) < 0) return 0;
	return 1;
#else
	return 0;
#endif
}

WACOMTABLET WacomOpenTablet(const char* pszDevice)
{
	int fd, e;
	WACOMTABLET hTablet = NULL;

	/* open device for read/write access */
	fd = open(pszDevice,O_RDWR);
	if (fd < 0) 
		{ perror("open"); return NULL; }

	/* configure serial */
	if (WacomIsSerial(fd))
	{
		hTablet = WacomOpenSerialTablet(fd);
		if (!hTablet) { e=errno; close(fd); errno=e; return NULL; }
	}

	/* configure usb */
	else if (WacomIsUSB(fd))
	{
		hTablet = WacomOpenUSBTablet(fd);
		if (!hTablet) { e=errno; close(fd); errno=e; return NULL; }
	}

	/* give up */
	else
	{
		close(fd);
		errno = EINVAL;
		return NULL;
	}

	return hTablet;
}

int WacomCopyState(WACOMSTATE* pDest, WACOMSTATE* pSrc)
{
	unsigned int uCnt;
	if (!pSrc || !pDest || !pSrc->uValueCnt || !pDest->uValueCnt)
		{ errno=EINVAL; return 1; }

	/* copy valid bits over */
	pDest->uValid = pSrc->uValid;

	/* determine how many values to transfer */
	uCnt = (pDest->uValueCnt < pSrc->uValueCnt) ?
			pDest->uValueCnt : pSrc->uValueCnt;

	/* copy them over */
	memcpy(pDest->values,pSrc->values,uCnt*sizeof(WACOMVALUE));

	return 0;
}

/*****************************************************************************
** Virtual Functions
*****************************************************************************/

void WacomCloseTablet(WACOMTABLET hTablet)
{
	WACOMTABLET_PRIV* pTablet = (WACOMTABLET_PRIV*)hTablet;
	if (!pTablet || !pTablet->Close) return;
	pTablet->Close(pTablet);
}

WACOMMODEL WacomGetModel(WACOMTABLET hTablet)
{
	WACOMTABLET_PRIV* pTablet = (WACOMTABLET_PRIV*)hTablet;
	if (!pTablet || !pTablet->GetModel) { errno=EBADF; return 0; }
	return pTablet->GetModel(pTablet);
}

const char* WacomGetVendorName(WACOMTABLET hTablet)
{
	WACOMTABLET_PRIV* pTablet = (WACOMTABLET_PRIV*)hTablet;
	if (!pTablet || !pTablet->GetVendorName) { errno=EBADF; return NULL; }
	return pTablet->GetVendorName(pTablet);
}

const char* WacomGetModelName(WACOMTABLET hTablet)
{
	WACOMTABLET_PRIV* pTablet = (WACOMTABLET_PRIV*)hTablet;
	if (!pTablet || !pTablet->GetModelName) { errno=EBADF; return NULL; }
	return pTablet->GetModelName(pTablet);
}

int WacomGetROMVersion(WACOMTABLET hTablet, int* pnMajor, int* pnMinor,
		int* pnRelease)
{
	WACOMTABLET_PRIV* pTablet = (WACOMTABLET_PRIV*)hTablet;
	if (!pTablet || !pTablet->GetROMVer) { errno=EBADF; return 0; }
	return pTablet->GetROMVer(pTablet,pnMajor,pnMinor,pnRelease);
}

int WacomGetCapabilities(WACOMTABLET hTablet)
{
	WACOMTABLET_PRIV* pTablet = (WACOMTABLET_PRIV*)hTablet;
	if (!pTablet || !pTablet->GetCaps) { errno=EBADF; return 0; }
	return pTablet->GetCaps(pTablet);
}

int WacomGetState(WACOMTABLET hTablet, WACOMSTATE* pState)
{
	WACOMTABLET_PRIV* pTablet = (WACOMTABLET_PRIV*)hTablet;
	if (!pTablet || !pTablet->GetState) { errno=EBADF; return 0; }
	return pTablet->GetState(pTablet,pState);
}

int WacomReadRaw(WACOMTABLET hTablet, unsigned char* puchData,
		unsigned int uSize)
{
	WACOMTABLET_PRIV* pTablet = (WACOMTABLET_PRIV*)hTablet;
	if (!pTablet || !pTablet->ReadRaw) { errno=EBADF; return 0; }
	return pTablet->ReadRaw(pTablet,puchData,uSize);
}

int WacomParseData(WACOMTABLET hTablet, const unsigned char* puchData,
		unsigned int uLength, WACOMSTATE* pState)
{
	WACOMTABLET_PRIV* pTablet = (WACOMTABLET_PRIV*)hTablet;
	if (!pTablet || !pTablet->ParseData) { errno=EBADF; return 0; }
	return pTablet->ParseData(pTablet,puchData,uLength,pState);
}
