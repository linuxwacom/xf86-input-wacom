/*****************************************************************************
 * wcm-beta.c
 *
 * Copyright 2002 by John Joganic <john@joganic.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * REVISION HISTORY
 *
 ****************************************************************************/

#include <unistd.h>
#include "wcm-beta.h"

/*****************************************************************************
 * Notes:
 *
 * Namespaces - the public functions use "wcmBeta" as the namespace.  This is
 *              to separate the XF86 module-level routines from the internal
 *              tablet-specific routines which are static and fall under the
 *              "wacom" namespace.
 ****************************************************************************/


#include "xf86.h"
#include "keysym.h"
#include "xf86_OSproc.h"
#include "xf86_libc.h"
#include "xf86Xinput.h"
#include "exevents.h"

#ifdef LINUX_INPUT
#include <unistd.h>
#include <sys/wait.h>
#endif

/****************************************************************************
** Defines
****************************************************************************/

#ifdef DBG
#undef DBG
#endif
#define DBG(lvl, f) {if ((lvl) <= xnDebugLevel) f;}

#ifdef SYSCALL
#undef SYSCALL
#endif
#define SYSCALL(call) while(((call) == -1) && (errno == EINTR))

#define WACOM_FLAG_BETA     128 /* must be same as xf86Wacom.c BETA_FLAG */

/* Note: xf86 valuator conversion code does not handle more than 6 axes. */

#define WACOM_AXIS_X 0
#define WACOM_AXIS_Y 1
#define WACOM_AXIS_PRESS 2
#define WACOM_AXIS_TILTX 3
#define WACOM_AXIS_TILTY 4
#define WACOM_AXIS_WHEEL 5
#define WACOM_AXIS_MAX 6

#define WACOM_BUTTON_COUNT 16

/*****************************************************************************
** Structures
*****************************************************************************/

typedef struct _WacomCoord WacomCoord;
typedef struct _WacomDevice WacomDevice;
typedef struct _WacomDevice* WacomDevicePtr;

struct _WacomCoord
{
	int nX;
	int nY;
	int nPress;
	int nTiltX;
	int nTiltY;
	int nWheel;
};

struct _WacomDevice
{
	unsigned int uFlags;	/* must be first for xf86Wacom.c compatibility */
	InputDriverPtr pDriver;	/* device driver */
	char* pszDevicePath;	/* device file name */
	int nScreen;			/* screen number for tablet */

	/* active tablet window */
	int nLeft;
	int nTop;
	int nRight;
	int nBottom;

	/* ratio of screen to active tablet window dimensions */
	double dRatioX;
	double dRatioY;

	/* device capabilities */
	WacomCoord min;
	WacomCoord max;
};

/****************************************************************************
** Globals
****************************************************************************/

	static int xnDebugLevel = 0;

	static const char* xszDefaultOptions[] =
	{
		"BaudRate", "9600",
		"StopBits", "1",
		"DataBits", "8",
		"Parity", "None",
		"Vmin", "1",
		"Vtime", "10",
		"FlowControl", "Xoff",
		NULL
	};

	static KeySym xSyms[] = 
	{
		/* 0x00 */ NoSymbol, NoSymbol, NoSymbol, NoSymbol,
		/* 0x04 */ NoSymbol, NoSymbol, NoSymbol, NoSymbol,
		/* 0x08 */ XK_F1, XK_F2, XK_F3, XK_F4,
		/* 0x0C */ XK_F5, XK_F6, XK_F7, XK_F8,
		/* 0x10 */ XK_F9, XK_F10, XK_F11, XK_F12,
		/* 0x14 */ XK_F13, XK_F14, XK_F15, XK_F16,
		/* 0x18 */ XK_F17, XK_F18, XK_F19, XK_F20,
		/* 0x1C */ XK_F21, XK_F22, XK_F23, XK_F24,
		/* 0x20 */ XK_F25, XK_F26, XK_F27, XK_F28,
		/* 0x24 */ XK_F29, XK_F30, XK_F31, XK_F32
	};

	static KeySymsRec xKeySyms =
	{
		xSyms,                                 /* map */
		0x08,                                  /* minimum key code */
		(sizeof(xSyms) / sizeof(*xSyms)) - 1,  /* maximum key code */
		1,                                     /* character width in bytes */
	};

/****************************************************************************
** Static Prototypes
****************************************************************************/

static void wacomFreeDevice(LocalDevicePtr pDevice);
static int wacomConfigDevice(InputDriverPtr pDriver, LocalDevicePtr pDevice,
		IDevPtr pConfig);
static int wacomDeviceCtrlProc(DeviceIntPtr pInt, int nWhat);
static void wacomDeviceFeedbackCtrlProc(DeviceIntPtr pInt, PtrCtrl* pCtrl);
static int wacomInitDevice(LocalDevicePtr pDevice);
static int wacomEnableDevice(LocalDevicePtr pDevice);
static int wacomDisableDevice(LocalDevicePtr pDevice);
static void wacomCloseDevice(LocalDevicePtr pDevice);
static Bool wacomConvertDeviceValuators(LocalDevicePtr pDevice,
		int nFirst, int nNum, int v0, int v1, int v2, int v3,
		int v4, int v5, int* px, int* py);
static Bool wacomReverseDeviceValuators(LocalDevicePtr pDevice, int x, int y,
		int* pValuators);
static int wacomSwitchDeviceMode(ClientPtr pClient, DeviceIntPtr pInt,
		int nMode);
static int wacomControlDevice(LocalDevicePtr pDevice, xDeviceCtl* pCtrl);
static int wacomOpenDevice(DeviceIntPtr pInt);
static void wacomReadInput(LocalDevicePtr pDevice);

/*****************************************************************************
** Device initialization and termination
*****************************************************************************/

InputInfoPtr wcmBetaNewDevice(InputDriverPtr pDriver, IDevPtr pConfig,
		int nFlags)
{
	LocalDevicePtr pDevice;
	WacomDevicePtr pPriv;

	xf86Msg(X_CONFIG, "wcmBetaInitDevice: drv=%d %s %p %d\n",
			pDriver->driverVersion,
			pDriver->driverName,
			pDriver->module,
			pDriver->refCount);
	xf86Msg(X_CONFIG, "wcmBetaInitDevice: config=%s %s\n",
			pConfig->identifier,
			pConfig->driver);
	xf86Msg(X_CONFIG, "wcmBetaInitDevice: flags=%08X\n",nFlags);

	/* allocate private structure */
    pPriv = (WacomDevicePtr) xalloc(sizeof(WacomDevice));
    if (!pPriv) return NULL;
	memset(pPriv,0,sizeof(pPriv));

	/* allocate input device and add to list */
    pDevice = xf86AllocateInput(pDriver, 0);
    if (!pDevice)
	{
		xfree(pPriv);
		return NULL;
	}

	/* initialize and configure device */
    pDevice->private = pPriv;
	if (wacomConfigDevice(pDriver,pDevice,pConfig))
	{
		wacomFreeDevice(pDevice);
		return NULL;
	}

    return pDevice;
}

void wcmBetaDeleteDevice(InputDriverPtr pDriver, LocalDevicePtr pDevice,
		int nFlags)
{
	xf86Msg(X_CONFIG, "wcmBetaDeleteDevice: drv=%d %s %p %d\n",
			pDriver->driverVersion,
			pDriver->driverName,
			pDriver->module,
			pDriver->refCount);
	xf86Msg(X_CONFIG, "wcmBetaDeleteDevice: dev=%s\n",pDevice->name);
	xf86Msg(X_CONFIG, "wcmBetaDeleteDevice: flags=%08X\n",nFlags);

	/* disable and deallocate device */
	wacomDisableDevice(pDevice);
	wacomFreeDevice(pDevice);
}

/*****************************************************************************
** Implementation
*****************************************************************************/

static int wacomConfigDevice(InputDriverPtr pDriver, LocalDevicePtr pDevice,
		IDevPtr pConfig)
{
    WacomDevicePtr pPriv = (WacomDevicePtr) pDevice->private;

    xf86Msg(X_INFO, "wacomConfigDevice\n");

	/* Initialize XF86 local device structure */
    pDevice->name = "TABLET";
	pDevice->type_name = "Wacom Tablet";
    pDevice->flags = 0;
    pDevice->device_control = wacomDeviceCtrlProc;
    pDevice->read_input = wacomReadInput;
    pDevice->control_proc = wacomControlDevice;
    pDevice->close_proc = wacomCloseDevice;
    pDevice->switch_mode = wacomSwitchDeviceMode;
    pDevice->conversion_proc = wacomConvertDeviceValuators;
    pDevice->reverse_conversion_proc = wacomReverseDeviceValuators;
    pDevice->fd = -1;
    pDevice->atom = 0;
    pDevice->dev = NULL;
    pDevice->private_flags = 0;
    pDevice->history_size  = 0;
    pDevice->old_x = -1;
    pDevice->old_y = -1;
    
	/* Initialize private structure */
	memset(pPriv,0,sizeof(*pPriv));
    pPriv->uFlags = WACOM_FLAG_BETA;
	pPriv->pDriver = pDriver;
    pPriv->pszDevicePath = "";
    pPriv->nScreen = -1;
	pPriv->nLeft = pPriv->nTop = pPriv->nRight = pPriv->nBottom = -1;
    
	/* retrieve values from configuration */
    pDevice->name = pConfig->identifier;
	pDevice->conf_idev = pConfig;
	xf86CollectInputOptions(pDevice, xszDefaultOptions, NULL);

	/* Handle common options like AlwaysCore, CorePointer, HistorySize, ... */
    xf86ProcessCommonOptions(pDevice, pDevice->options);

	/* Get the debugging log level */
    xnDebugLevel = xf86SetIntOption(pDevice->options,
			"DebugLevel", xnDebugLevel);

    if (xnDebugLevel > 0)
		xf86Msg(X_CONFIG, "wacomConfigDevice: debug level set to %d\n",
				xnDebugLevel);

    /* Get tablet device (eg. /dev/ttyS0 or /dev/input/event0) */
	pPriv->pszDevicePath = xf86FindOptionValue(pDevice->options, "Device");
    if (!pPriv->pszDevicePath)
	{
		xf86Msg (X_ERROR, "wacomConfigDevice: No device specified for %s.\n",
				pDevice->name);
		return 1;
    }
	else
    	xf86Msg(X_CONFIG, "wacomConfigDevice: %s device is %s\n",
				pDevice->name, pPriv->pszDevicePath);

	/* Get screen number, if provided */
    pPriv->nScreen = xf86SetIntOption(pDevice->options, "ScreenNo", -1);
    if (pPriv->nScreen != -1)
	{
		xf86Msg(X_CONFIG, "wacomConfigDevice: %s device on screen %d\n",
				pDevice->name, pPriv->nScreen);
    }

	/* Mark device as fully configured and ready */
    pDevice->flags = XI86_POINTER_CAPABLE | XI86_CONFIGURED;

	return 0;
}

static void wacomFreeDevice(LocalDevicePtr pDevice)
{
	WacomDevicePtr pPriv = (WacomDevicePtr) pDevice->private;

	DBG(1,xf86Msg(X_INFO, "wacomFreeDevice: %s\n",pDevice->name));
	
	xfree(pPriv);
	pDevice->private = NULL;
	xf86DeleteInput(pDevice,0);
}

static int wacomDeviceCtrlProc(DeviceIntPtr pInt, int nWhat)
{
	LocalDevicePtr pDevice = (LocalDevicePtr) pInt->public.devicePrivate;
  
    switch (nWhat)
	{
		case DEVICE_INIT: return wacomInitDevice(pDevice);
		case DEVICE_ON: return wacomEnableDevice(pDevice);
		case DEVICE_OFF: return wacomDisableDevice(pDevice);
		case DEVICE_CLOSE: wacomCloseDevice(pDevice); return Success;
	}

	xf86Msg(X_ERROR,"wacomDeviceCtrlProc: %s received bad request %d\n",
			pDevice->name,nWhat);

	return !Success;
}

static int wacomInitDevice(LocalDevicePtr pDevice)
{
    int i;
    CARD8 uchMap[WACOM_BUTTON_COUNT];

    DBG(1,xf86Msg(X_INFO, "wacomInitDevice: %s\n",pDevice->name));

	/* Initialize for everything - too much in fact.
	 * The individual pointer types will be more specialized. */

	/* we report button events */
	for (i=1; i<=WACOM_BUTTON_COUNT; ++i) uchMap[i] = i;
	if (!InitButtonClassDeviceStruct(pDevice->dev, WACOM_BUTTON_COUNT, uchMap))
	{
		xf86Msg(X_ERROR, "wacomInitDevice: "
				"failed to init as button class device\n");
		return !Success;
	}
      
	/* we report key events */
	if (!InitKeyClassDeviceStruct(pDevice->dev, &xKeySyms, NULL))
	{
		xf86Msg(X_ERROR, "wacomInitDevice: "
				"failed to init as key class device\n");
		return !Success;
	}

	/* we can be focused */
	if (!InitFocusClassDeviceStruct(pDevice->dev))
	{
		xf86Msg(X_ERROR, "wacomInitDevice: "
				"failed to init as focus class device\n");
		return !Success;
	}
          
	/* we support feedbacks - sort of */
	if (!InitPtrFeedbackClassDeviceStruct(pDevice->dev,
			wacomDeviceFeedbackCtrlProc))
	{
		xf86Msg(X_ERROR, "wacomInitDevice: "
				"failed to init as feedback class device\n");
		return !Success;
	}
	    
	/* we report proximity events */
	if (!InitProximityClassDeviceStruct(pDevice->dev))
	{
		xf86Msg(X_ERROR, "wacomInitDevice: "
				"failed to init as proximity class device\n");
		return !Success;
	}

	/* we report valuator data in motion events */
	if (!InitValuatorClassDeviceStruct(pDevice->dev, WACOM_AXIS_MAX,
			xf86GetMotionEvents, pDevice->history_size,
			OutOfProximity | Absolute))
	{
		xf86Msg(X_ERROR, "wacomInitDevice: "
				"failed to init as valuator class device\n");
		return !Success;
	}

	/* allocate the motion history buffer */
	xf86MotionHistoryAllocate(pDevice);

	/* open the device to gather informations */
	return wacomOpenDevice(pDevice->dev);
}

static int wacomEnableDevice(LocalDevicePtr pDevice)
{
    DBG(1,xf86Msg(X_INFO, "wacomEnableDevice: %s\n",pDevice->name));
	if (pDevice->fd < 0)
	{
		if (wacomOpenDevice(pDevice->dev) != Success)
			return !Success;
		xf86AddEnabledDevice(pDevice);
	}

	pDevice->dev->public.on = TRUE;
	return Success;
}

static int wacomDisableDevice(LocalDevicePtr pDevice)
{
    DBG(1,xf86Msg(X_INFO, "wacomDisableDevice: %s\n",pDevice->name));
	if (pDevice->fd >= 0)
	{
		xf86RemoveEnabledDevice(pDevice);
		SYSCALL(xf86CloseSerial(pDevice->fd));
		pDevice->fd = -1;
	}

	pDevice->dev->public.on = FALSE;
	return Success;
}

static void wacomCloseDevice(LocalDevicePtr pDevice)
{
    DBG(1,xf86Msg(X_INFO, "wacomCloseDevice: %s\n",pDevice->name));
	if (pDevice->fd >= 0) wacomDisableDevice(pDevice);
}

static void wacomDeviceFeedbackCtrlProc(DeviceIntPtr pInt, PtrCtrl* pCtrl)
{
	DBG(1, xf86Msg(X_INFO, "wacomDeviceFeedbackCtrlProc\n"));
}

static Bool wacomConvertDeviceValuators(LocalDevicePtr pDevice,
		int nFirst, int nNum, int v0, int v1, int v2, int v3,
		int v4, int v5, int* px, int* py)
{
    WacomDevicePtr pPriv = (WacomDevicePtr)pDevice->private;

	/* what does this do? */
    if (nFirst != 0 || nNum == 1) return FALSE;
    
	/* screen coordinates are based solely on X and Y axis */

    *px = (int)(v0 * pPriv->dRatioX + 0.5);    /* WACOM_X_AXIS */
    *py = (int)(v1 * pPriv->dRatioY + 0.5);    /* WACOM_Y_AXIS */

    return TRUE;
}

static Bool wacomReverseDeviceValuators(LocalDevicePtr pDevice, int x, int y,
		int* pValuators)
{
    WacomDevicePtr pPriv = (WacomDevicePtr)pDevice->private;

	/* screen coordinates are based solely on X and Y axis */

	pValuators[WACOM_AXIS_X] = (int)(x / pPriv->dRatioX + 0.5);
	pValuators[WACOM_AXIS_Y] = (int)(y / pPriv->dRatioY + 0.5);

    return TRUE;
}

static int wacomSwitchDeviceMode(ClientPtr pClient, DeviceIntPtr pInt,
		int nMode)
{
	LocalDevicePtr pDevice = (LocalDevicePtr) pInt->public.devicePrivate;

    DBG(3, xf86Msg(X_INFO, "wacomSwitchDeviceMode %s mode=%s\n",
			pDevice->name, (nMode == Absolute) ? "ABS" :
			(nMode == Relative) ? "REL" : "UNK"));

	/* ignore */
    return Success;
}

static int wacomControlDevice(LocalDevicePtr pDevice, xDeviceCtl* pCtrl)
{
	/* no controls defined */
    return Success;
}

static int wacomOpenDevice(DeviceIntPtr pInt)
{
	LocalDevicePtr pDevice = (LocalDevicePtr)pInt->public.devicePrivate;
    WacomDevicePtr pPriv = (WacomDevicePtr)pDevice->private;
	ScreenPtr pScreen;

	xf86Msg(X_INFO, "wacomOpenDevice\n");

	if (0)
	{
		int i=xf86execl("/home/jej/proj/linuxwacom-dev/src/wcm-client","wcm-client","/tmp/jej1",NULL);
		xf86Msg(X_INFO, "ok %d\n",i);
	}

#if 0
	{
	pid_t pid;
	switch (pid=fork())
	{
		case 0: /* child */
			exit(0xdeadbeef);
			break;

		case -1: /* fork failed*/
			break;

		default: /* parent */
		{
			int count=0, p, status;
			do { p=waitpid(pid,&status,0); }
			while (p == -1 && count++ < 4);
			xf86Msg(X_INFO,"wacomOpenDevice: joined (%d,%08X,%X,%X)\n",
					p,status,WIFEXITED(status),WEXITSTATUS(status));
		}
	}
	}
#endif
    
	/* open device as serial line */
	if (pDevice->fd < 0)
	{
		pDevice->fd = xf86OpenSerial(pDevice->options);
		if (pDevice->fd < 0)
			return !Success;
	}

	/* Query device capabilities for hypothetical 6x4 tablet */
	pPriv->min.nX = 0;
	pPriv->max.nX = 15240;
	pPriv->min.nY = 0;
	pPriv->max.nY = 10160;
	pPriv->min.nPress = 0;
	pPriv->max.nPress = 1023;
	pPriv->min.nTiltX = -64;
	pPriv->max.nTiltX = 63;
	pPriv->min.nTiltY = -64;
	pPriv->max.nTiltY = 63;
	pPriv->min.nWheel = 0;
	pPriv->max.nWheel = 1023;

	/* set bounds to defaults, if not specified by configuration */
	if (pPriv->nLeft < 0) pPriv->nLeft = pPriv->min.nX;
	if (pPriv->nTop < 0) pPriv->nTop = pPriv->min.nY;
	if (pPriv->nRight < 0) pPriv->nRight = pPriv->max.nX;
	if (pPriv->nBottom < 0) pPriv->nBottom = pPriv->max.nY;

	/* validate left bound */
	if ((pPriv->nLeft > pPriv->max.nX || pPriv->nLeft < pPriv->min.nX))
		pPriv->nLeft = pPriv->min.nX;

	/* validate top bound */
	if ((pPriv->nTop > pPriv->max.nY || pPriv->nTop < pPriv->min.nY))
		pPriv->nTop = pPriv->min.nY;

	/* validate right bound */
	if ((pPriv->nRight > pPriv->max.nX || pPriv->nRight < pPriv->min.nX))
		pPriv->nRight = pPriv->max.nX;
			
	/* validate bottom bound */
	if ((pPriv->nBottom > pPriv->max.nY || pPriv->nBottom < pPriv->min.nY))
		pPriv->nBottom = pPriv->max.nY;

	/* validate width */
	if (pPriv->nLeft >= pPriv->nRight)
	{
		pPriv->nLeft = pPriv->min.nX;
		pPriv->nRight = pPriv->max.nX;
	}
	
	/* validate height */
	if (pPriv->nTop >= pPriv->nBottom)
	{
		pPriv->nTop = pPriv->min.nY;
		pPriv->nBottom = pPriv->max.nY;
	}
	
	xf86Msg(X_INFO, "wacomOpenDevice: %s bounds %d,%d %d,%d\n",
			pDevice->name, pPriv->nLeft, pPriv->nTop,
			pPriv->nRight, pPriv->nBottom);

	/* get screen */
	if (pPriv->nScreen < 0) pPriv->nScreen = 0;
	if (pPriv->nScreen >= screenInfo.numScreens)
	{
		xf86Msg(X_INFO,"wacomOpenDevice: %s on invalid screen %d, reset to 0\n",
				pDevice->name, pPriv->nScreen);
		pPriv->nScreen = 0;
	}
	pScreen = screenInfo.screens[pPriv->nScreen];

	xf86Msg(X_INFO, "wacomOpenDevice: %s on screen %d (%d, %d)\n",
			pDevice->name, pPriv->nScreen, pScreen->width, pScreen->height);

	/* calculate screen to tablet ratios */
	pPriv->dRatioX = (double)pScreen->width / (pPriv->nRight - pPriv->nLeft);
	pPriv->dRatioY = (double)pScreen->height / (pPriv->nBottom - pPriv->nTop);
    
	xf86Msg(X_INFO, "wacomOpenDevice: %s scaled %.4g, %.4g\n",
			pDevice->name, pPriv->dRatioX, pPriv->dRatioY);

	/* Initialize valuators */
	InitValuatorAxisStruct(pInt,WACOM_AXIS_X,
			0, pPriv->nRight - pPriv->nLeft,
			100000,0,100000);
	InitValuatorAxisStruct(pInt,WACOM_AXIS_Y,
			0, pPriv->nBottom - pPriv->nTop,
			100000,0,100000);
	InitValuatorAxisStruct(pInt,WACOM_AXIS_PRESS,
			pPriv->min.nPress, pPriv->max.nPress,
			100000,0,100000);
	InitValuatorAxisStruct(pInt,WACOM_AXIS_TILTX,
			pPriv->min.nTiltX, pPriv->max.nTiltX,
			100000,0,100000);
	InitValuatorAxisStruct(pInt,WACOM_AXIS_TILTY,
			pPriv->min.nTiltY, pPriv->max.nTiltY,
			100000,0,100000);
    InitValuatorAxisStruct(pInt,WACOM_AXIS_WHEEL,
			pPriv->min.nWheel, pPriv->max.nWheel,
			100000,0,100000);
    return (pDevice->fd >= 0) ? Success : !Success;
}

static void wacomReadInput(LocalDevicePtr pDevice)
{
	int nLen;
	unsigned char chBuf[256];

	/* read and ignore for now */
    SYSCALL(nLen = xf86ReadSerial(pDevice->fd, chBuf, sizeof(chBuf)));
	return;
}


