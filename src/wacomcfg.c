/*****************************************************************************
** wacomcfg.c
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
** REVISION HISTORY
**   2003-05-02 0.0.1 - JEJ - created
**
****************************************************************************/

#include "wacomcfg.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h> /* debugging only, we've got no business output text */

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <memory.h>
#include <assert.h>

/*****************************************************************************
** XInput
*****************************************************************************/

#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XIproto.h>

#include "Xwacom.h" /* Hopefully it will be included in XFree86 someday, but
                     * in the meantime, we are expecting it in the local
                     * directory. */

/*****************************************************************************
** Internal structures
*****************************************************************************/

typedef struct _CONFIG CONFIG;
typedef struct _DEVICE DEVICE;

struct _CONFIG
{
	Display* pDisp;
	WACOMERRORFUNC pfnError;
	XDeviceInfoPtr pDevs;
	int nDevCnt;
};

struct _DEVICE
{
	CONFIG* pCfg;
	XDevice* pDev;
};

/*****************************************************************************
** Library operations
*****************************************************************************/

static int CfgError(CONFIG* pCfg, int err, const char* pszText)
{
	/* report error */
	if (pCfg->pfnError)
		(*pCfg->pfnError)(err,pszText);

	/* set and return */
	errno = err;
	return -1;
}

static int CfgGetDevs(CONFIG* pCfg)
{
	/* request device list */
	pCfg->pDevs = (XDeviceInfoPtr) XListInputDevices(pCfg->pDisp,
			&pCfg->nDevCnt);
	if (!pCfg->pDevs)
		return CfgError(pCfg,EIO,"CfgGetDevs: failed to get devices");

	return 0;
}

/*****************************************************************************
** Library initialization, termination
*****************************************************************************/

WACOMCONFIG WacomConfigInit(Display* pDisplay, WACOMERRORFUNC pfnErrorHandler)
{
	CONFIG* pCfg;
	int nMajor, nFEV, nFER;

	/* check for XInput extension */
	if (!XQueryExtension(pDisplay,INAME,&nMajor,&nFEV,&nFER))
	{
		if (pfnErrorHandler)
			(*pfnErrorHandler)(EINVAL,"XInput not supported.");
		return NULL;
	}

	/* allocate configuration structure */
	pCfg = (CONFIG*)malloc(sizeof(CONFIG));
	if (!pCfg)
	{
		if (pfnErrorHandler)
			(*pfnErrorHandler)(errno,strerror(errno));
		return NULL;
	}

	memset(pCfg,0,sizeof(*pCfg));
	pCfg->pDisp = pDisplay;
	pCfg->pfnError = pfnErrorHandler;
	return (WACOMCONFIG)pCfg;
}

void WacomConfigTerm(WACOMCONFIG hConfig)
{
	CONFIG* pCfg = (CONFIG*)hConfig;
	if (!pCfg) return;
	free(pCfg);
}

int WacomConfigListDevices(WACOMCONFIG hConfig, WACOMDEVICEINFO** ppInfo,
	unsigned int* puCount)
{
	int i, nSize, nPos, nLen, nCount;
	unsigned char* pReq;
	WACOMDEVICEINFO* pInfo;
	CONFIG* pCfg = (CONFIG*)hConfig;

	if (!pCfg || !ppInfo || !puCount)
		{ errno=EINVAL; return -1; }

	/* load devices, if not already in memory */
	if (!pCfg->pDevs && CfgGetDevs(pCfg))
		return -1;

	/* estimate size of memory needed to hold structures */
	nSize = nCount = 0;
	for (i=0; i<pCfg->nDevCnt; ++i)
	{
		if (pCfg->pDevs[i].use != IsXExtensionDevice) continue;
		nSize += sizeof(WACOMDEVICEINFO);
		nSize += strlen(pCfg->pDevs[i].name) + 1;
		++nCount;
	}

	/* allocate memory and zero */
	pReq = (unsigned char*)malloc(nSize);
	if (!pReq) return CfgError(pCfg,errno,
		"WacomConfigListDevices: failed to allocate memory");
	memset(pReq,0,nSize);

	/* populate data */
	pInfo = (WACOMDEVICEINFO*)pReq;
	nPos = nCount * sizeof(WACOMDEVICEINFO);
	for (i=0; i<pCfg->nDevCnt; ++i)
	{
		/* ignore non-extension devices */
		if (pCfg->pDevs[i].use != IsXExtensionDevice) continue;

		/* copy name */
		nLen = strlen(pCfg->pDevs[i].name);
		pInfo->pszName = (char*)(pReq + nPos);
		memcpy(pReq+nPos,pCfg->pDevs[i].name,nLen+1);
		nPos += nLen + 1;

		/* guess type for now - don't discard unknowns */
		if (strncasecmp(pInfo->pszName,"cursor",6) == 0)
			pInfo->type = WACOMDEVICETYPE_CURSOR;
		else if (strncasecmp(pInfo->pszName,"stylus",6) == 0)
			pInfo->type = WACOMDEVICETYPE_STYLUS;
		else if (strncasecmp(pInfo->pszName,"eraser",6) == 0)
			pInfo->type = WACOMDEVICETYPE_ERASER;
		else
			pInfo->type = WACOMDEVICETYPE_UNKNOWN;

		++pInfo;
	}

	/* double check our work */
	assert(nPos == nSize);
	
	*ppInfo = (WACOMDEVICEINFO*)pReq;
	*puCount = nCount;
	return 0;
}

WACOMDEVICE WacomConfigOpenDevice(WACOMCONFIG hConfig,
	const char* pszDeviceName)
{
	int i;
	DEVICE* pInt;
	XDevice* pDev;
	XDeviceInfoPtr pDevInfo = NULL;
	CONFIG* pCfg = (CONFIG*)hConfig;

	/* sanity check input */
	if (!pCfg || !pszDeviceName) { errno=EINVAL; return NULL; }

	/* load devices, if not already in memory */
	if (!pCfg->pDevs && CfgGetDevs(pCfg))
		return NULL;

	/* find device in question */
	for (i=0; i<pCfg->nDevCnt; ++i)
		if (strcmp(pCfg->pDevs[i].name, pszDeviceName) == 0)
			pDevInfo = pCfg->pDevs + i;

	/* no device, bail. */
	if (!pDevInfo)
	{
		CfgError(pCfg,ENOENT,"WacomConfigOpenDevice: No such device");
		return NULL;
	}

	/* Open the device. */
	pDev = XOpenDevice(pCfg->pDisp,pDevInfo->id);
	if (!pDev)
	{
		CfgError(pCfg,EIO,"WacomConfigOpenDevice: "
			"Failed to open device");
		return NULL;
	}

	/* allocate device structure and return */
	pInt = (DEVICE*)malloc(sizeof(DEVICE));
	memset(pInt,0,sizeof(*pInt));
	pInt->pCfg = pCfg;
	pInt->pDev = pDev;

	return (WACOMDEVICE)pInt;
}

int WacomConfigCloseDevice(WACOMDEVICE hDevice)
{
	DEVICE* pInt = (DEVICE*)hDevice;
	if (!pInt) { errno=EINVAL; return -1; }

	(void)XCloseDevice(pInt->pCfg->pDisp,pInt->pDev);
	free(pInt);
	return 0;
}

int WacomConfigSetRawParam(WACOMDEVICE hDevice, int nParam, int nValue)
{
	int nReturn;
	DEVICE* pInt = (DEVICE*)hDevice;
	int nValues[2] = { nParam, nValue };
	XDeviceResolutionControl c;

	if (!pInt || !nParam) { errno=EINVAL; return -1; }

	c.control = DEVICE_RESOLUTION;
	c.length = sizeof(c);
	if ( nParam == XWACOM_PARAM_FILEOPTION ) c.first_valuator = 0;
	else if ( nParam == XWACOM_PARAM_FILEMODEL ) c.first_valuator = 1;
	else c.first_valuator = 4;
	c.num_valuators = 2;
	c.resolutions = nValues;

	/* Dispatch request */
	nReturn = XChangeDeviceControl(
		pInt->pCfg->pDisp,
		pInt->pDev,
		DEVICE_RESOLUTION,
		(XDeviceControl*)&c);

	/* Convert error codes */
	if (nReturn == BadValue)
		return CfgError(pInt->pCfg,EINVAL,
			"WacomConfigSetRawParam: Bad value");
	else if (nReturn != Success)
		return CfgError(pInt->pCfg,EIO,
			"WacomConfigSetRawParam: Unknown X error");
	return 0;
}

void WacomConfigFree(void* pvData)
{
	/* JEJ - if in the future, more context is needed, make an alloc
	 *       function that places the context before the data.
	 *       In the meantime, free is good enough. */

	free(pvData);
}
