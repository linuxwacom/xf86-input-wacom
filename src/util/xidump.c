/*****************************************************************************
** xidump.c
**
** Copyright (C) 2003 - 2004 - John E. Joganic
** Copyright (C) 2004 - 2008 - Ping Cheng 
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
**   2003-02-23 0.0.1 - created for GTK1.2
**   2003-03-07 0.0.2 - added input device code
**   2003-03-08 0.0.3 - added curses code
**   2003-03-21 0.0.4 - added conditional curses code
**   2003-03-21 0.5.0 - released in development branch
**   2003-04-07 0.5.1 - added pressure bar
**   2005-07-27 0.5.2 - remove unused GTK stuff [jg]
**   2005-11-11 0.7.1 - report tool ID and serial number
**   2006-05-05 0.7.4 - Removed older 2.6 kernels
**   2006-07-19 0.7.5 - Support buttons and keys combined
**   2007-01-10 0.7.7 - Don't list uninitialized tools
**   2008-05-06 0.8.0 - Support Xorg 7.3 or later
**
****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>

#define XIDUMP_VERSION "0.8.0"

#include "../include/util-config.h"

/*****************************************************************************
** XInput
*****************************************************************************/

#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XIproto.h>
#include <X11/keysym.h>

	enum
	{
		INPUTEVENT_KEY_PRESS,
		INPUTEVENT_KEY_RELEASE,
		INPUTEVENT_FOCUS_IN,
		INPUTEVENT_FOCUS_OUT,
		INPUTEVENT_BTN_PRESS,
		INPUTEVENT_BTN_RELEASE,
		INPUTEVENT_PROXIMITY_IN,
		INPUTEVENT_PROXIMITY_OUT,
		INPUTEVENT_MOTION_NOTIFY,
		INPUTEVENT_DEVICE_STATE_NOTIFY,
		INPUTEVENT_DEVICE_MAPPING_NOTIFY,
		INPUTEVENT_CHANGE_DEVICE_NOTIFY,
		INPUTEVENT_DEVICE_POINTER_MOTION_HINT,
		INPUTEVENT_DEVICE_BUTTON_MOTION,
		INPUTEVENT_DEVICE_BUTTON1_MOTION,
		INPUTEVENT_DEVICE_BUTTON2_MOTION,
		INPUTEVENT_DEVICE_BUTTON3_MOTION,
		INPUTEVENT_DEVICE_BUTTON4_MOTION,
		INPUTEVENT_DEVICE_BUTTON5_MOTION,

		INPUTEVENT_MAX
	};

	int gnDevListCnt = 0;
	XDeviceInfoPtr gpDevList = NULL;
	int gnLastXError = 0;
	int gnVerbose = 0;
	int gnSuppress = 4;
	int gnInputEvent[INPUTEVENT_MAX] = { 0 };

int ErrorHandler(Display* pDisp, XErrorEvent* pEvent)
{
	char chBuf[64];
	XGetErrorText(pDisp,pEvent->error_code,chBuf,sizeof(chBuf));
	fprintf(stderr,"X Error: %d %s\n", pEvent->error_code, chBuf);
	gnLastXError  = pEvent->error_code;
	return 0;
}

int GetLastXError(void)
{
	return gnLastXError;
}

Display* InitXInput(void)
{
	Display* pDisp;
	int nMajor, nFEV, nFER;

	pDisp = XOpenDisplay(NULL);
	if (!pDisp)
	{
		fprintf(stderr,"Failed to connect to X server.\n");
		return NULL;
	}

	XSetErrorHandler(ErrorHandler);

	XSynchronize(pDisp,1 /*sync on*/);
	
	if (!XQueryExtension(pDisp,INAME,&nMajor,&nFEV,&nFER))
	{
		fprintf(stderr,"Server does not support XInput extension.\n");
		XCloseDisplay(pDisp);
		return NULL;
	}

	return pDisp;
}

int ListDevices(Display* pDisp, const char* pszDeviceName)
{
	int i, j, k;
	XDeviceInfoPtr pDev;
	XAnyClassPtr pClass;

	/* get list of devices */
	if (!gpDevList)
	{
		gpDevList = (XDeviceInfoPtr) XListInputDevices(pDisp, &gnDevListCnt);
		if (!gpDevList)
		{
			fprintf(stderr,"Failed to get input device list.\n");
			return 1;
		}
	}

	for (i=0; i<gnDevListCnt; ++i)
	{
		pDev = gpDevList + i;

		/* if device name is specified, skip other devices */
		if (pszDeviceName && strcasecmp(pDev->name, pszDeviceName))
			continue;

		if (pDev->num_classes)
			printf("%-30s %s\n",
				pDev->name,
				(pDev->use == 0) ? "disabled" :
				(pDev->use == IsXKeyboard) ? "keyboard" :
				(pDev->use == IsXPointer) ? "pointer" :
#ifndef WCM_ISXEXTENSIONPOINTER
				(pDev->use == IsXExtensionDevice) ? 
#else
				(pDev->use == IsXExtensionDevice || 
				 pDev->use == IsXExtensionKeyboard || 
				 pDev->use == IsXExtensionPointer) ? 
#endif
					"extension" : "unknown");

		if (gnVerbose)
		{
			pClass = pDev->inputclassinfo;
			for (j=0; j<pDev->num_classes; ++j)
			{
				switch (pClass->class)
				{
					case ButtonClass:
					{
						XButtonInfo* pBtn = (XButtonInfo*)pClass;
						printf("    btn: num=%d\n",pBtn->num_buttons);
						break;
					}
	
					case FocusClass:
					{
						printf("  focus:\n");
						break;
					}

					case KeyClass:
					{
						XKeyInfo* pKey = (XKeyInfo*)pClass;
						printf("    key: min=%d, max=%d, num=%d\n",
								pKey->min_keycode,
								pKey->max_keycode,
								pKey->num_keys);
						break;
					}

					case ValuatorClass:
					{
						XValuatorInfoPtr pVal = (XValuatorInfoPtr)pClass;
						printf("    val: axes=%d mode=%s buf=%ld\n",
								pVal->num_axes,
								pVal->mode == Absolute ? "abs" :
								pVal->mode == Relative ? "rel" : "unk",
								pVal->motion_buffer);
						for (k=0; k<pVal->num_axes; ++k)
						{
							printf("    axis[%d]: res=%d, min=%d, max=%d\n",
								k, /* index */
								pVal->axes[k].resolution,
								pVal->axes[k].min_value,
								pVal->axes[k].max_value);
						}
						break;
					}
	
					default:
						printf("  unknown class\n" );
				}
	
				/* skip to next record */
				pClass = (XAnyClassPtr)((char*)pClass + pClass->length);
			}
			printf("\n");
		}
	}

	return 0;
}

XDeviceInfoPtr GetDevice(Display* pDisp, const char* pszDeviceName)
{
	int i;

	/* get list of devices */
	if (!gpDevList)
	{
		gpDevList = (XDeviceInfoPtr) XListInputDevices(pDisp, &gnDevListCnt);
		if (!gpDevList)
		{
			fprintf(stderr,"Failed to get input device list.\n");
			return NULL;
		}
	}

	/* find device by name */
	for (i=0; i<gnDevListCnt; ++i)
	{
		if (!strcasecmp(gpDevList[i].name,pszDeviceName) &&
			gpDevList[i].num_classes)
			return gpDevList + i;
	}

	return NULL;
}

static const char* GetEventName(int nType)
{
	static char xchBuf[64];

	switch (nType)
	{
		case KeyPress: return "KeyPress";
		case KeyRelease: return "KeyRelease";
		case ButtonPress: return "ButtonPress";
		case ButtonRelease: return "ButtonRelease";
		case MotionNotify: return "MotionNotify";
		case EnterNotify: return "EnterNotify";
		case LeaveNotify: return "LeaveNotify";
		case FocusIn: return "FocusIn";
		case FocusOut: return "FocusOut";
		case KeymapNotify: return "KeymapNotify";
		case Expose: return "Expose";
		case GraphicsExpose: return "GraphicsExpose";
		case NoExpose: return "NoExpose";
		case VisibilityNotify: return "VisibilityNotify";
		case CreateNotify: return "CreateNotify";
		case DestroyNotify: return "DestroyNotify";
		case UnmapNotify: return "UnmapNotify";
		case MapNotify: return "MapNotify";
		case MapRequest: return "MapRequest";
		case ReparentNotify: return "ReparentNotify";
		case ConfigureNotify: return "ConfigureNotify";
		case ConfigureRequest: return "ConfigureRequest";
		case GravityNotify: return "GravityNotify";
		case ResizeRequest: return "ResizeRequest";
		case CirculateNotify: return "CirculateNotify";
		case CirculateRequest: return "CirculateRequest";
		case PropertyNotify: return "PropertyNotify";
		case SelectionClear: return "SelectionClear";
		case SelectionRequest: return "SelectionRequest";
		case SelectionNotify: return "SelectionNotify";
		case ColormapNotify: return "ColormapNotify";
		case ClientMessage: return "ClientMessage";
		case MappingNotify: return "MappingNotify";

		default:
		if (nType == gnInputEvent[INPUTEVENT_KEY_PRESS])
			return "XIKeyPress";
		else if (nType == gnInputEvent[INPUTEVENT_KEY_RELEASE])
			return "XIKeyRelease";
		else if (nType == gnInputEvent[INPUTEVENT_FOCUS_IN])
			return "XIFocusIn";
		else if (nType == gnInputEvent[INPUTEVENT_FOCUS_OUT])
			return "XIFocusOut";
		else if (nType == gnInputEvent[INPUTEVENT_BTN_PRESS])
			return "XIButtonPress";
		else if (nType == gnInputEvent[INPUTEVENT_BTN_RELEASE])
			return "XIButtonRelease";
		else if (nType == gnInputEvent[INPUTEVENT_PROXIMITY_IN])
			return "XIProximityIn";
		else if (nType == gnInputEvent[INPUTEVENT_PROXIMITY_OUT])
			return "XIProximityOut";
		else if (nType == gnInputEvent[INPUTEVENT_MOTION_NOTIFY])
			return "XIMotionNotify";
		else if (nType == gnInputEvent[INPUTEVENT_DEVICE_STATE_NOTIFY])
			return "XIDeviceStateNotify";
		else if (nType == gnInputEvent[INPUTEVENT_DEVICE_MAPPING_NOTIFY])
			return "XIDeviceMappingNotify";
		else if (nType == gnInputEvent[INPUTEVENT_CHANGE_DEVICE_NOTIFY])
			return "XIChangeDeviceNotify";
		else if (nType == gnInputEvent[INPUTEVENT_DEVICE_POINTER_MOTION_HINT])
			return "XIDevicePointerMotionHint";
		else if (nType == gnInputEvent[INPUTEVENT_DEVICE_BUTTON_MOTION])
			return "XIDeviceButtonMotion";
		else if (nType == gnInputEvent[INPUTEVENT_DEVICE_BUTTON1_MOTION])
			return "XIDeviceButton1Motion";
		else if (nType == gnInputEvent[INPUTEVENT_DEVICE_BUTTON2_MOTION])
			return "XIDeviceButton2Motion";
		else if (nType == gnInputEvent[INPUTEVENT_DEVICE_BUTTON3_MOTION])
			return "XIDeviceButton3Motion";
		else if (nType == gnInputEvent[INPUTEVENT_DEVICE_BUTTON4_MOTION])
			return "XIDeviceButton4Motion";
		else if (nType == gnInputEvent[INPUTEVENT_DEVICE_BUTTON5_MOTION])
			return "XIDeviceButton5Motion";
	}

	snprintf(xchBuf,sizeof(xchBuf),"Event_%d",nType);
	return xchBuf;
}


/*****************************************************************************
** FORMAT
*****************************************************************************/

typedef enum
{
	FORMATTYPE_DEFAULT,
	FORMATTYPE_ACCELERATION
} FORMATTYPE;

typedef struct _FORMAT FORMAT;
struct _FORMAT
{
	const char* pszName;
	FORMATTYPE type;
};

	FORMAT gFormats[] =
	{
		{ "default", FORMATTYPE_DEFAULT },
		{ "accel", FORMATTYPE_ACCELERATION },
		{ NULL }
	};

/*****************************************************************************
** UI
*****************************************************************************/

typedef struct _UI UI;
struct _UI
{
	const char* pszName;
	int (*Init)(void);
	void (*Term)(void);
	int (*Run)(Display* pDisp, XDeviceInfo* pDevInfo, FORMATTYPE fmt);
};

/*****************************************************************************
** Curses UI
*****************************************************************************/

#if WCM_ENABLE_NCURSES
#define USE_NCURSES 1
#include <wacscrn.h>

	int gbCursesInit = 0;

static void CursesTerm(void)
{
	if (gbCursesInit)
	{
		wacscrn_term();
		gbCursesInit = 0;
	}
}

static int CursesInit(void)
{
	if (!gbCursesInit)
	{
		gbCursesInit = 1;
		wacscrn_init();
		atexit(CursesTerm);
	}
	return 0;
}

static int CursesRun(Display* pDisp, XDeviceInfo* pDevInfo, FORMATTYPE fmt)
{
	int i, j, k, bDown, nBtn;
	int nRow=0, nTitleRow, nPressRow, nProxRow, nFocusRow, nButtonRow,
       		nKeyRow, nValRow;
	int nMaxPress=100, nMinPress=0;
	int bStylus = 0;
	char chBuf[1024];
	XEvent event;
	XAnyEvent* pAny;
	XValuatorInfoPtr pValInfo = NULL;
	XAnyClassPtr pClass;

	/* Identify program and version */
	wacscrn_standout();
	for (i=0; i<80; ++i) wacscrn_output(nRow,i," ");
	wacscrn_output(nRow,0,"xidump v" XIDUMP_VERSION);
	wacscrn_normal();
	nRow += 2;

	/* get class info */
	pClass = pDevInfo->inputclassinfo;
	for (j=0; j<pDevInfo->num_classes; ++j)
	{
		switch (pClass->class)
		{
			case ValuatorClass:
				pValInfo = (XValuatorInfoPtr)pClass;
				break;
		}
	
		/* skip to next record */
		pClass = (XAnyClassPtr)((char*)pClass + pClass->length);
	}

	snprintf(chBuf,sizeof(chBuf),"InputDevice: %s",pDevInfo->name);
	wacscrn_output(nRow,0,chBuf);
	nRow += 1;

	/* display valuator related info */
	nTitleRow = nRow;
	if (pValInfo)
	{
		snprintf(chBuf,sizeof(chBuf),"Valuators: %s   ID: Unreported  Serial Number: Unreported",
				pValInfo->mode == Absolute ? "Absolute" :
				pValInfo->mode == Relative ? "Relative" : "Unknown");
		wacscrn_output(nRow,0,chBuf);
		nRow += 2;
		nValRow = nRow;
		nRow += 6;

		wacscrn_output(nValRow+1 ,0,"     data:");
		wacscrn_output(nValRow+2 ,0,"      min:");
		wacscrn_output(nValRow+3 ,0,"      max:");
		wacscrn_output(nValRow+4 ,0,"      res:");

		/* retain pressure range for pressure bar */
		nMaxPress = pValInfo->axes[2].max_value;
		nMinPress = pValInfo->axes[2].min_value;

		/* should be a better way to identify the stylus */
		if ((pValInfo->axes[3].min_value == -64) &&
			(pValInfo->axes[4].min_value == -64))
			bStylus = 1;

		for (k=0; k<pValInfo->num_axes && k<6; ++k)
		{
			wacscrn_output(nValRow,12 + k * 10,
				k == 0 ? " x-axis " :
				k == 1 ? " y-axis " :
				k == 2 ? "pressure" :
				k == 3 ? (bStylus ? " x-tilt " : "rotation" ) :
				k == 4 ? (bStylus ? " y-tilt " : "throttle" ) :
				k == 5 ? "  wheel " : "  error ");

			snprintf(chBuf,sizeof(chBuf),"%+06d",
				pValInfo->axes[k].min_value);
			wacscrn_output(nValRow+2,12 + k * 10, chBuf);
			snprintf(chBuf,sizeof(chBuf),"%+06d",
				pValInfo->axes[k].max_value);
			wacscrn_output(nValRow+3,12 + k * 10, chBuf);
			snprintf(chBuf,sizeof(chBuf),"%+06d",
				pValInfo->axes[k].resolution);
			wacscrn_output(nValRow+4,12 + k * 10, chBuf);
		}
	}

	nPressRow = nRow++;
	nProxRow = nRow++;
	nFocusRow = nRow++;
	nButtonRow = nRow++;
	nKeyRow = nRow++;

	wacscrn_output(nProxRow,  0,"Proximity:");
	wacscrn_output(nFocusRow, 0,"    Focus:");
	wacscrn_output(nButtonRow,0,"  Buttons:");
	wacscrn_output(nKeyRow   ,0,"     Keys:");

	/* handle events */

	while (1)
	{
		wacscrn_refresh();
		XNextEvent(pDisp,&event);

		pAny = (XAnyEvent*)&event;
		/* printf("event: type=%s\n",GetEventName(pAny->type)); */

		if (pAny->type == gnInputEvent[INPUTEVENT_PROXIMITY_IN])
		{
			if (!pValInfo)
			{
				wacscrn_output(23,0,"Unexpected valuator data received.");
			}
			wacscrn_standout();
			wacscrn_output(nProxRow,12,"IN ");
			wacscrn_normal();
		}
		else if (pAny->type == gnInputEvent[INPUTEVENT_PROXIMITY_OUT])
			wacscrn_output(nProxRow,12,"OUT ");
		else if (pAny->type == gnInputEvent[INPUTEVENT_FOCUS_IN])
		{
			wacscrn_standout();
			wacscrn_output(nFocusRow,12,"IN ");
			wacscrn_normal();
		}
		else if (pAny->type == gnInputEvent[INPUTEVENT_FOCUS_OUT])
			wacscrn_output(nFocusRow,12,"OUT ");
		else if (pAny->type == gnInputEvent[INPUTEVENT_MOTION_NOTIFY])
		{
			XDeviceMotionEvent* pMove = (XDeviceMotionEvent*)pAny;
			if (!pValInfo)
			{
				wacscrn_output(23,0,"Unexpected valuator data received.");
			}
			else
			{
				/* title value */
				snprintf(chBuf,sizeof(chBuf),"%s",
					pValInfo->mode == Absolute ? "Absolute" :
					pValInfo->mode == Relative ? "Relative" : "Unknown");
				wacscrn_output(nTitleRow,11,chBuf);

				/* Device/tool ID can only be retrieved through the ToolID option 
				 * of xsetwacom due to valuator backward compatibility concern
				 *
				v = (pMove->axis_data[3]&0xffff0000) >> 16;
				snprintf(chBuf, sizeof(chBuf), "%10d", v);
				wacscrn_output(nTitleRow, 25, chBuf);

				 * serial number can only be retrieved through the ToolSerial option
				 * of xsetwacom due to valuator backward compatibility concern
				 *
				v = (pMove->axis_data[4]&0xffff0000) | 
							((pMove->axis_data[5]&0xffff0000)>>16);
				if ( v )
				{
					snprintf(chBuf,sizeof(chBuf), "%12d", v);
					wacscrn_output(nTitleRow,52,chBuf);
				}
				*/

				for (k=0; k<pValInfo->num_axes && k<3; ++k)
				{
					snprintf(chBuf, sizeof(chBuf), "%+06d", pMove->axis_data[k]);
					wacscrn_output(nValRow+1, 12 + k * 10, chBuf);

				}

				for (k=3; k<pValInfo->num_axes && k<6; ++k)
				{
					snprintf(chBuf, sizeof(chBuf), "%+06d", 
							(short)(pMove->axis_data[k]&0xffff));
					wacscrn_output(nValRow+1, 12 + k * 10, chBuf);

				}

				/* pressure bar */
				{
					char* c;
					int s, nPos;
					wacscrn_standout();
					nPos = pMove->axis_data[2];

					if (nPos < nMinPress)
					{
						c = "<";
						nPos = 78; /* want full bar */
					}
					else if (nPos > nMaxPress)
					{
						c = ">";
						nPos = 78; /* want full bar */
					}
					else
					{
						c = "*";
						nPos = (nPos - nMinPress) * 78;
						nPos /= (nMaxPress - nMinPress);
					}

					for (s=0; s<nPos; ++s)
						wacscrn_output(nPressRow,s,c);
					wacscrn_normal();
					for (; s<78; ++s)
						wacscrn_output(nPressRow,s," ");
				}
			}
		}
		else if ((pAny->type == gnInputEvent[INPUTEVENT_BTN_PRESS]) ||
				(pAny->type == gnInputEvent[INPUTEVENT_BTN_RELEASE]))
		{
			XDeviceButtonEvent* pBtn = (XDeviceButtonEvent*)pAny;
			bDown = (pAny->type == gnInputEvent[INPUTEVENT_BTN_PRESS]);
			nBtn = pBtn->button;
			if ((nBtn < 1) || (nBtn > 5)) nBtn=6;
			snprintf(chBuf,sizeof(chBuf),"%d-%s",pBtn->button,
					bDown ? "DOWN" : "UP  ");
			if (bDown) wacscrn_standout();
			wacscrn_output(nButtonRow,12 + (nBtn-1) * 10,chBuf);
			if (bDown) wacscrn_normal();
		}
		else if ((pAny->type == gnInputEvent[INPUTEVENT_KEY_PRESS]) ||
				(pAny->type == gnInputEvent[INPUTEVENT_KEY_RELEASE]))
		{
			XDeviceKeyEvent* pKey = (XDeviceKeyEvent*)pAny;
			bDown = (pAny->type == gnInputEvent[INPUTEVENT_KEY_PRESS]);
			nBtn = pKey->keycode - 7; /* first key is always 8 */
			if ((nBtn < 1) || (nBtn > 5)) nBtn=6;
			snprintf(chBuf,sizeof(chBuf),"%d-%s",pKey->keycode - 7,
					bDown ? "DOWN" : "UP  ");
			if (bDown) wacscrn_standout();
			wacscrn_output(nKeyRow,12 + (nBtn-1) * 10,chBuf);
			if (bDown) wacscrn_normal();
		}
		else
		{
			snprintf(chBuf,sizeof(chBuf),"%ld - %-60s",
					time(NULL),
					GetEventName(pAny->type));
			wacscrn_output(22,0,chBuf);
		}
	}

	return 0;
}

	UI gCursesUI = { "curses", CursesInit, CursesTerm, CursesRun };

#else /* WCM_ENABLE_NCURSES */
#define USE_NCURSES 0
#endif /* !WCM_ENABLE_NCURSES */

/*****************************************************************************
** Raw UI
*****************************************************************************/

static int RawInit(void)
{
	return 0;
}

static void RawTerm(void)
{
}

static int RawRunDefault(Display* pDisp, XDeviceInfo* pDevInfo)
{
	XEvent event;
	XAnyEvent* pAny;
	struct timeval tv;
	double dStart, dNow;

	gettimeofday(&tv,NULL);
	dStart = tv.tv_sec + (double)tv.tv_usec / 1E6;

	while (1)
	{
		XNextEvent(pDisp,&event);

		pAny = (XAnyEvent*)&event;
		/* printf("event: type=%s\n",GetEventName(pAny->type)); */

		/* display time */
		gettimeofday(&tv,NULL);
		dNow = tv.tv_sec + (double)tv.tv_usec / 1E6;
		printf("%.8f: ",(dNow - dStart));

		if (pAny->type == gnInputEvent[INPUTEVENT_PROXIMITY_IN])
			printf("Proximity In\n");
		else if (pAny->type == gnInputEvent[INPUTEVENT_PROXIMITY_OUT])
			printf("Proximity Out\n");
		else if (pAny->type == gnInputEvent[INPUTEVENT_FOCUS_IN])
			printf("Focus In\n");
		else if (pAny->type == gnInputEvent[INPUTEVENT_FOCUS_OUT])
			printf("Focus Out\n");
		else if (pAny->type == gnInputEvent[INPUTEVENT_MOTION_NOTIFY])
		{
			XDeviceMotionEvent* pMove = (XDeviceMotionEvent*)pAny;
			int v = (pMove->axis_data[4]&0xffff0000) | 
					((pMove->axis_data[5]&0xffff0000)>>16);

			printf("Motion: x=%+6d y=%+6d p=%4d tx=%+4d ty=%+4d "
				"w=%+5d ID: %4d Serial: %11d \n",
					pMove->axis_data[0],
					pMove->axis_data[1],
					pMove->axis_data[2],
					(short)(pMove->axis_data[3]&0xffff),
					(short)(pMove->axis_data[4]&0xffff),
					(short)(pMove->axis_data[5]&0xffff),
					(pMove->axis_data[3]&0xffff0000)>>16,
					v);

		}
		else if ((pAny->type == gnInputEvent[INPUTEVENT_BTN_PRESS]) ||
				(pAny->type == gnInputEvent[INPUTEVENT_BTN_RELEASE]))
		{
			XDeviceButtonEvent* pBtn = (XDeviceButtonEvent*)pAny;
			printf("Button: %d %s\n",pBtn->button,
					pAny->type == gnInputEvent[INPUTEVENT_BTN_PRESS] ?
						"DOWN" : "UP");
		}
		else if ((pAny->type == gnInputEvent[INPUTEVENT_KEY_PRESS]) ||
				(pAny->type == gnInputEvent[INPUTEVENT_KEY_RELEASE]))
		{
			XDeviceKeyEvent* pKey = (XDeviceKeyEvent*)pAny;
			printf("Key: %d %s\n", pKey->keycode - 7,
			       (pAny->type == gnInputEvent[INPUTEVENT_KEY_PRESS]) ?
			       "DOWN" : "UP");
		}
		else
		{
			printf("Event: %s\n",GetEventName(pAny->type));
		}

		/* flush data to terminal */
		fflush(stdout);
	}

	return 0;
}

static int RawRunAccel(Display* pDisp, XDeviceInfo* pDevInfo)
{
	XEvent event;
	XAnyEvent* pAny;
	int prox=0, head=0, tail=0, points=0, prev=-1;
	int x[16], y[16];
	double d[16], dd[16], vx[16], vy[16], ax[16], ay[16],
		dx, dy, dvx, dvy, m, a;

	while (1)
	{
		XNextEvent(pDisp,&event);
		pAny = (XAnyEvent*)&event;

		if (pAny->type == gnInputEvent[INPUTEVENT_PROXIMITY_IN])
			prox=1;
		else if (pAny->type == gnInputEvent[INPUTEVENT_PROXIMITY_OUT])
			{ prox=head=tail=points=0; prev=-1; }
		else if (pAny->type == gnInputEvent[INPUTEVENT_MOTION_NOTIFY])
		{
			XDeviceMotionEvent* pMove = (XDeviceMotionEvent*)pAny;
			x[head] = pMove->axis_data[0];
			y[head] = pMove->axis_data[1];
			d[head] = (double)pMove->time;

			if (prev >= 0)
			{
				/* parametric deltas, velocity, and accel */
				dx = x[head] - x[prev];
				dy = y[head] - y[prev];

				if ((abs(dx) < gnSuppress) &&
					(abs(dy) < gnSuppress)) continue;

				dd[head] = d[head] - d[prev];
				vx[head] = dd[head] ? (dx/dd[head]) : 0;
				vy[head] = dd[head] ? (dy/dd[head]) : 0;
				dvx = vx[head] - vx[prev];
				dvy = vy[head] - vy[prev];
				ax[head] = dd[head] ? (dvx/dd[head]) : 0;
				ay[head] = dd[head] ? (dvy/dd[head]) : 0;
			}
			else
			{
				dx = dy = 0;
				vx[head] = vy[head] = 0;
				ax[head] = ay[head] = 0;
			}

			++points;
			prev = head;
			head = (head + 1) % 16;
			if (head == tail) tail = (head + 1) % 16;

			/* compute magnitude and angle of velocity */
			m = sqrt(vx[prev]*vx[prev] + vy[prev]*vy[prev]),
			a = atan2(vx[prev], -vy[prev]);

			printf("%8.0f: %4.0f "
				"[%+4d %+4d] [%6.2f %6.2f] "
				"[%+4.2f %+4.2f] (%6.2f %+3.0f)\n",
				d[prev], dd[prev],
				x[prev], y[prev],
				vx[prev], vy[prev],
				ax[prev], ay[prev],
				m, a * 180 / M_PI);
		}

		/* flush data to terminal */
		fflush(stdout);
	}

	return 0;
}

static int RawRun(Display* pDisp, XDeviceInfo* pDevInfo, FORMATTYPE fmt)
{
	switch (fmt)
	{
		case FORMATTYPE_ACCELERATION:
			return RawRunAccel(pDisp,pDevInfo);

		default:
			return RawRunDefault(pDisp,pDevInfo);
	}
}

	UI gRawUI = { "raw", RawInit, RawTerm, RawRun };

/****************************************************************************/

	UI* gpUIs[] =
	{
		/* Curses UI */
		#if USE_NCURSES
		&gCursesUI,
		#endif

		/* Raw UI is always available */
		&gRawUI,
		NULL
	};

/****************************************************************************/

void Usage(int rtn)
{
	int nCnt;
	UI** ppUI;
	FORMAT* pFmt;
	FILE* f = rtn ? stderr : stdout;

	fprintf(f, "Usage: xidump [options] input_device\n"
			"  -h, --help                - usage\n"
			"  -v, --verbose             - verbose\n"
			"  -V, --version             - version\n"
			"  -l, --list                - list available input devices\n"
			"  -s, --suppress value      - suppress changes less than value\n"
			"  -f, --format format_type  - use specified format, see below\n"
			"  -u, --ui ui_type          - use specified ui, see below\n"
			"\n"
			"Use --list option for input_device choices\n"
			"UI types: ");

	/* output UI types */
	for (ppUI=gpUIs, nCnt=0; *ppUI!=NULL; ++ppUI, ++nCnt)
		fprintf(f, "%s%s", nCnt ? ", " : "", (*ppUI)->pszName);

	fprintf(f,"\nFormat types: ");

	/* output format types */
	for (pFmt=gFormats, nCnt=0; pFmt->pszName!=NULL; ++pFmt, ++nCnt)
		fprintf(f, "%s%s", nCnt ? ", " : "", pFmt->pszName);

	fprintf(f,"\n");
	exit(rtn);
}

void Version(void)
{
	fprintf(stdout,"%s\n",XIDUMP_VERSION);
}

void Fatal(const char* pszFmt, ...)
{
	va_list args;
	va_start(args,pszFmt);
	vfprintf(stderr,pszFmt,args);
	va_end(args);
	exit(1);
}

/****************************************************************************/

int Run(Display* pDisp, UI* pUI, FORMATTYPE fmt, const char* pszDeviceName)
{
	int nRtn;
	XDevice* pDev;
	XDeviceInfoPtr pDevInfo;
	Window wnd;
	int nEventListCnt = 0;
	XEventClass eventList[32];
	XEventClass cls;

	/* create a window to receive events */
	wnd = XCreateWindow(pDisp,
			DefaultRootWindow(pDisp), /* parent */
			0,0,10,10, /* placement */
			0, /* border width */
			0, /* depth */
			InputOutput, /* class */
			CopyFromParent, /* visual */
			0, /* valuemask */
			NULL); /* attributes */

	/* mapping appears to be necessary */
	XMapWindow(pDisp,wnd);

	/* get the device by name */
	pDevInfo = GetDevice(pDisp,pszDeviceName);
	if (!pDevInfo)
	{
		fprintf(stderr,"Unable to find input device '%s'\n",pszDeviceName);
		XDestroyWindow(pDisp,wnd);
		return 1;
	}

	/* open device */
	pDev = XOpenDevice(pDisp,pDevInfo->id);
	if (!pDev)
	{
		fprintf(stderr,"Unable to open input device '%s'\n",pszDeviceName);
		XDestroyWindow(pDisp,wnd);
		return 1;
	}

	/* key events */
	DeviceKeyPress(pDev,gnInputEvent[INPUTEVENT_KEY_PRESS],cls);
	if (cls) eventList[nEventListCnt++] = cls;
	DeviceKeyRelease(pDev,gnInputEvent[INPUTEVENT_KEY_RELEASE],cls);
	if (cls) eventList[nEventListCnt++] = cls;

	/* focus events */
	DeviceFocusIn(pDev,gnInputEvent[INPUTEVENT_FOCUS_IN],cls);
	if (cls) eventList[nEventListCnt++] = cls;
	DeviceFocusOut(pDev,gnInputEvent[INPUTEVENT_FOCUS_OUT],cls);
	if (cls) eventList[nEventListCnt++] = cls;

	/* button events */
	DeviceButtonPress(pDev,gnInputEvent[INPUTEVENT_BTN_PRESS],cls);
	if (cls) eventList[nEventListCnt++] = cls;
	DeviceButtonRelease(pDev,gnInputEvent[INPUTEVENT_BTN_RELEASE],cls);
	if (cls) eventList[nEventListCnt++] = cls;

	/* proximity events */
	ProximityIn(pDev,gnInputEvent[INPUTEVENT_PROXIMITY_IN],cls);
	if (cls) eventList[nEventListCnt++] = cls;
	ProximityOut(pDev,gnInputEvent[INPUTEVENT_PROXIMITY_OUT],cls);
	if (cls) eventList[nEventListCnt++] = cls;

	/* motion events */
	DeviceMotionNotify(pDev,gnInputEvent[INPUTEVENT_MOTION_NOTIFY],cls);
	if (cls) eventList[nEventListCnt++] = cls;

	/* device state */
	DeviceStateNotify(pDev,gnInputEvent[INPUTEVENT_DEVICE_STATE_NOTIFY],cls);
	if (cls) eventList[nEventListCnt++] = cls;
	DeviceMappingNotify(pDev,
			gnInputEvent[INPUTEVENT_DEVICE_MAPPING_NOTIFY],cls);
	if (cls) eventList[nEventListCnt++] = cls;
	ChangeDeviceNotify(pDev,gnInputEvent[INPUTEVENT_CHANGE_DEVICE_NOTIFY],cls);
	if (cls) eventList[nEventListCnt++] = cls;

#if 0
	/* this cuts the motion data down - not sure if this is useful */
	DevicePointerMotionHint(pDev,
			gnInputEvent[INPUTEVENT_DEVICE_POINTER_MOTION_HINT],cls);
	if (cls) eventList[nEventListCnt++] = cls;
#endif

	/* button motion */
	DeviceButtonMotion(pDev,
			gnInputEvent[INPUTEVENT_DEVICE_BUTTON_MOTION],cls);
	if (cls) eventList[nEventListCnt++] = cls;
	DeviceButton1Motion(pDev,
			gnInputEvent[INPUTEVENT_DEVICE_BUTTON1_MOTION],cls);
	if (cls) eventList[nEventListCnt++] = cls;
	DeviceButton2Motion(pDev,
			gnInputEvent[INPUTEVENT_DEVICE_BUTTON2_MOTION],cls);
	if (cls) eventList[nEventListCnt++] = cls;
	DeviceButton3Motion(pDev,
			gnInputEvent[INPUTEVENT_DEVICE_BUTTON3_MOTION],cls);
	if (cls) eventList[nEventListCnt++] = cls;
	DeviceButton4Motion(pDev,
			gnInputEvent[INPUTEVENT_DEVICE_BUTTON4_MOTION],cls);
	if (cls) eventList[nEventListCnt++] = cls;
	DeviceButton5Motion(pDev,
			gnInputEvent[INPUTEVENT_DEVICE_BUTTON5_MOTION],cls);
	if (cls) eventList[nEventListCnt++] = cls;

	/* specify which events to report */
	/* XSelectInput(pDisp,wnd,0x00FFFFFF ^ PointerMotionHintMask); */
	/* XSelectExtensionEvent(pDisp,wnd,eventList,nEventListCnt); */

	/* grab device - work whether pointer is in active window or not */
	XGrabDevice(pDisp,pDev,wnd,
			0, /* no owner events */
			nEventListCnt, eventList, /* events */
			GrabModeAsync, /* don't queue, give me whatever you got */
			GrabModeAsync, /* same */
			CurrentTime);

	/* fire up the UI */
	if ((nRtn=pUI->Init()) != 0)
		fprintf(stderr,"failed to initialize UI\n");
	else
	{
		if ((nRtn=pUI->Run(pDisp,pDevInfo,fmt)) != 0)
			fprintf(stderr,"failed to run UI\n");
		pUI->Term();
	}

	XUngrabDevice(pDisp,pDev,CurrentTime);
	XFree(pDev);
	XCloseDisplay(pDisp);
	XDestroyWindow(pDisp,wnd);

	return nRtn;
}

/*****************************************************************************
** main
*****************************************************************************/

int main(int argc, char** argv)
{
	int nRtn;
	int bList = 0;
	UI* pUI=NULL, **ppUI;
	FORMAT* pFmt;
	FORMATTYPE fmt=FORMATTYPE_DEFAULT;
	const char* pa;
	Display* pDisp = NULL;
	const char* pszDeviceName = NULL;

	++argv;
	while ((pa = *(argv++)) != NULL)
	{
		if (pa[0] == '-')
		{
			if ((strcmp(pa,"-h") == 0) || (strcmp(pa,"--help") == 0))
				Usage(0);
			else if ((strcmp(pa,"-v") == 0) || (strcmp(pa,"--verbose") == 0))
				++gnVerbose;
			else if ((strcmp(pa,"-V") == 0) || (strcmp(pa,"--version") == 0))
				{ Version(); exit(0); }
			else if ((strcmp(pa,"-l") == 0) || (strcmp(pa,"--list") == 0))
				bList = 1;
			else if ((strcmp(pa,"-f") == 0) || (strcmp(pa,"--format") == 0))
			{
				pa = *(argv++);
				if (!pa) Fatal("Missing format argument\n");

				/* find format by name */
				for (pFmt = gFormats; pFmt->pszName!=NULL; ++pFmt)
				{
					if (strcmp(pa,pFmt->pszName) == 0)
					{
						fmt = pFmt->type;
						break;
					}
				}

				/* bad format type, die */
				if (!pFmt->pszName)
					Fatal("Unknown format option %s\n",pa);
			}
			else if ((strcmp(pa,"-s") == 0) || (strcmp(pa,"--suppress") == 0))
			{
				pa = *(argv++);
				if (!pa) Fatal("Missing suppress argument\n");

				gnSuppress = atoi(pa);
				if (gnSuppress <= 0)
					Fatal("Invalid suppress value\n");
			}
			else if ((strcmp(pa,"-u") == 0) || (strcmp(pa,"--ui") == 0))
			{
				pa = *(argv++);
				if (!pa) Fatal("Missing ui argument\n");

				/* find ui by name */
				pUI = NULL;
				for (ppUI = gpUIs; *ppUI!=NULL; ++ppUI)
				{
					if (strcmp(pa,(*ppUI)->pszName) == 0)
						pUI = *ppUI;
				}

				/* bad ui type, die */
				if (!pUI)
					Fatal("Unknown ui option %s; was it configured?\n",pa);
			}
			else
				Fatal("Unknown option %s\n",pa);
		}
		else if (!pszDeviceName)
			pszDeviceName = pa;
		else
			Fatal("Unknown argument %s\n",pa);
	}

	/* device must be specified */
	if (!pszDeviceName && !bList)
	{
		fprintf(stderr,"input_device not specified\n");
		Usage(1);
	}

	/* default to first valid UI, if not specified */
	if (pUI == NULL)
		pUI = gpUIs[0];
	
	/* open connection to XServer with XInput */
	pDisp = InitXInput();
	if (!pDisp) exit(1);

	if (bList)
		nRtn = ListDevices(pDisp,pszDeviceName);
	else
		nRtn = Run(pDisp,pUI,fmt,pszDeviceName);

	/* release device list */
	if (gpDevList)
		XFreeDeviceList(gpDevList);

	XCloseDisplay(pDisp);

	return nRtn;
}
