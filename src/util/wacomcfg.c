/*****************************************************************************
** wacomcfg.c
**
** Copyright (C) 2003-2004 - John E. Joganic
** Copyright (C) 2004-2006 - Ping Cheng
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
**   2004-05-28 0.0.2 - PC - updated WacomConfigListDevices
**   2005-06-10 0.0.3 - PC - updated for x86_64
**   2005-10-24 0.0.4 - PC - Added Pad
**   2005-11-17 0.0.5 - PC - update mode code
**   2006-07-17 0.0.6 - PC - Exchange info directly with wacom_drv.o
**
****************************************************************************/

#include "wacomcfg.h"
#include "../include/Xwacom.h" /* Hopefully it will be included in XFree86 someday, but
                     * in the meantime, we are expecting it in the local
                     * directory. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h> /* debugging only, we've got no business output text */
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <memory.h>
#include <assert.h>

/*****************************************************************************
** Internal structures
*****************************************************************************/


/*****************************************************************************
** Library operations
*****************************************************************************/

static int CfgError(WACOMCONFIG* pCfg, int err, const char* pszText)
{
	/* report error */
	if (pCfg->pfnError)
		(*pCfg->pfnError)(err,pszText);

	/* set and return */
	errno = err;
	return -1;
}

static int CfgGetDevs(WACOMCONFIG* pCfg)
{
	/* request device list */
	pCfg->pDevs = XListInputDevices(pCfg->pDisp,
			&pCfg->nDevCnt);

	if (!pCfg->pDevs)
		return CfgError(pCfg,EIO,"CfgGetDevs: failed to get devices");

	return 0;
}

/*****************************************************************************
** Library initialization, termination
*****************************************************************************/

WACOMCONFIG * WacomConfigInit(Display* pDisplay, WACOMERRORFUNC pfnErrorHandler)
{
	WACOMCONFIG* pCfg;
	int nMajor, nFEV, nFER;

	/* check for XInput extension */
	if (!XQueryExtension(pDisplay,INAME,&nMajor,&nFEV,&nFER))
	{
		if (pfnErrorHandler)
			(*pfnErrorHandler)(EINVAL,"XInput not supported.");
		return NULL;
	}

	/* allocate configuration structure */
	pCfg = (WACOMCONFIG*)malloc(sizeof(WACOMCONFIG));
	if (!pCfg)
	{
		if (pfnErrorHandler)
			(*pfnErrorHandler)(errno,strerror(errno));
		return NULL;
	}

	memset(pCfg,0,sizeof(*pCfg));
	pCfg->pDisp = pDisplay;
	pCfg->pfnError = pfnErrorHandler;
	return pCfg;
}

void WacomConfigTerm(WACOMCONFIG *hConfig)
{
	if (!hConfig) return;
	free(hConfig);
}

int WacomConfigListDevices(WACOMCONFIG *hConfig, WACOMDEVICEINFO** ppInfo,
	unsigned int* puCount)
{
	int i, j, nSize, nPos, nLen, nCount;
	unsigned char* pReq;
	WACOMDEVICEINFO* pInfo;
	XDeviceInfo* info;
	char devName[64];

	if (!hConfig || !ppInfo || !puCount)
		{ errno=EINVAL; return -1; }

	/* load devices, if not already in memory */
	if (!hConfig->pDevs && CfgGetDevs(hConfig))
		return -1;

	/* estimate size of memory needed to hold structures */
	nSize = nCount = 0;
	for (i=0; i<hConfig->nDevCnt; ++i)
	{
		info = hConfig->pDevs + i;
		if (info->use != IsXExtensionDevice) continue;
		nSize += sizeof(WACOMDEVICEINFO);
		nSize += strlen(info->name) + 1;
		++nCount;
	}

	/* allocate memory and zero */
	pReq = (unsigned char*)malloc(nSize);
	if (!pReq) return CfgError(hConfig,errno,
		"WacomConfigListDevices: failed to allocate memory");
	memset(pReq,0,nSize);

	/* populate data */
	pInfo = (WACOMDEVICEINFO*)pReq;
	nPos = nCount * sizeof(WACOMDEVICEINFO);
	nCount = 0;
	for (i=0; i<hConfig->nDevCnt; ++i)
	{
		info = hConfig->pDevs + i;
		/* ignore non-extension devices */
		if (info->use != IsXExtensionDevice) continue;

		/* copy name */
		nLen = strlen(info->name);
		pInfo->pszName = (char*)(pReq + nPos);
		memcpy(pReq+nPos,info->name,nLen+1);
		nPos += nLen + 1;
		/* guess type for now - discard unknowns */
		for (j=0; j<strlen(pInfo->pszName); j++)
			devName[j] = tolower(pInfo->pszName[j]);
		devName[j] = '\0';
		if (strstr(devName,"cursor") != NULL)
			pInfo->type = WACOMDEVICETYPE_CURSOR;
		else if (strstr(devName,"stylus") != NULL)
			pInfo->type = WACOMDEVICETYPE_STYLUS;
		else if (strstr(devName,"eraser") != NULL)
			pInfo->type = WACOMDEVICETYPE_ERASER;
		else if (strstr(devName,"pad") != NULL)
			pInfo->type = WACOMDEVICETYPE_PAD;
		else
			pInfo->type = WACOMDEVICETYPE_UNKNOWN;

		if ( pInfo->type != WACOMDEVICETYPE_UNKNOWN )
		{
			++pInfo;
			++nCount;
		}
	}

	/* double check our work */
	assert(nPos == nSize);
	
	*ppInfo = (WACOMDEVICEINFO*)pReq;
	*puCount = nCount;
	return 0;
}

WACOMDEVICE * WacomConfigOpenDevice(WACOMCONFIG * hConfig,
	const char* pszDeviceName)
{
	int i;
	WACOMDEVICE* pInt;
	XDevice* pDev;
	XDeviceInfo *pDevInfo = NULL, *info;

	/* sanity check input */
	if (!hConfig || !pszDeviceName) { errno=EINVAL; return NULL; }

	/* load devices, if not already in memory */
	if (!hConfig->pDevs && CfgGetDevs(hConfig))
		return NULL;

	/* find device in question */
	for (i=0; i<hConfig->nDevCnt; ++i)
	{
		info = hConfig->pDevs + i;
		if (strcmp(info->name, pszDeviceName) == 0)
			pDevInfo = info;
	}

	/* no device, bail. */
	if (!pDevInfo)
	{
		CfgError(hConfig,ENOENT,"WacomConfigOpenDevice: No such device");
		return NULL;
	}

	/* Open the device. */
	pDev = XOpenDevice(hConfig->pDisp,pDevInfo->id);
	if (!pDev)
	{
		CfgError(hConfig,EIO,"WacomConfigOpenDevice: "
			"Failed to open device");
		return NULL;
	}

	/* allocate device structure and return */
	pInt = (WACOMDEVICE*)malloc(sizeof(WACOMDEVICE));
	memset(pInt,0,sizeof(*pInt));
	pInt->pCfg = hConfig;
	pInt->pDev = pDev;

	return pInt;
}

int WacomConfigCloseDevice(WACOMDEVICE *hDevice)
{
	if (!hDevice) { errno=EINVAL; return -1; }

	if (hDevice->pDev)
		XFree(hDevice->pDev);
	free(hDevice);
	return 0;
}

int WacomConfigSetRawParam(WACOMDEVICE *hDevice, int nParam, int nValue)
{
	int nReturn;
	int nValues[2];
	XDeviceResolutionControl c;
	XDeviceControl *dc = (XDeviceControl *)(void *)&c;

	nValues[0] = nParam;
	nValues[1] = nValue;
	if (!hDevice || !nParam) { errno=EINVAL; return -1; }

	c.control = DEVICE_RESOLUTION;
	c.length = sizeof(c);
	c.first_valuator = 0;
	c.num_valuators = 2;
	c.resolutions = nValues;
	/* Dispatch request */
	nReturn = XChangeDeviceControl (hDevice->pCfg->pDisp, hDevice->pDev,
					DEVICE_RESOLUTION, dc);

	/* Convert error codes:
	 * hell knows what XChangeDeviceControl should return */
	if (nReturn == BadValue || nReturn == BadRequest)
		return CfgError(hDevice->pCfg,EINVAL,
				"WacomConfigSetRawParam: failed");

	if (nParam == XWACOM_PARAM_MODE)
	{
		/* tell Xinput the mode has been changed */
		XSetDeviceMode(hDevice->pCfg->pDisp, hDevice->pDev, nValue);
	}
	return 0;
}

int WacomConfigGetRawParam(WACOMDEVICE *hDevice, int nParam, int *nValue, int valu)
{
	int nReturn;
	XDeviceResolutionControl c;
	XDeviceResolutionState *ds;
	int nValues[1];

	if (!hDevice || !nParam) { errno=EINVAL; return -1; }

	nValues[0] = nParam;

	c.control = DEVICE_RESOLUTION;
	c.length = sizeof(c);
	c.first_valuator = 0;
	c.num_valuators = valu;
	c.resolutions = nValues;
	/* Dispatch request */
	nReturn = XChangeDeviceControl(hDevice->pCfg->pDisp, hDevice->pDev,
		DEVICE_RESOLUTION, (XDeviceControl *)(void *)&c);

	if (nReturn == BadValue || nReturn == BadRequest)
	{
error:		return CfgError(hDevice->pCfg, EINVAL,
			"WacomConfigGetRawParam: failed");
	}

	ds = (XDeviceResolutionState *)XGetDeviceControl (hDevice->pCfg->pDisp,
		hDevice->pDev, DEVICE_RESOLUTION);

	/* Restore resolution */
	nValues [0] = 0;
	XChangeDeviceControl(hDevice->pCfg->pDisp, hDevice->pDev,
		DEVICE_RESOLUTION, (XDeviceControl *)(void *)&c);

	if (!ds)
		goto error;

	*nValue = ds->resolutions [ds->num_valuators-1];
	XFreeDeviceControl ((XDeviceControl *)ds);

	return 0;
}

void WacomConfigFree(void* pvData)
{
	/* JEJ - if in the future, more context is needed, make an alloc
	 *       function that places the context before the data.
	 *       In the meantime, free is good enough. */

	free(pvData);
}
