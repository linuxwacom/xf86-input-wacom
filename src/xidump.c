/*****************************************************************************
** xidump.c
**
** Copyright (C) 2003 - John E. Joganic
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
**
****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define XIDUMP_VERSION "0.0.2"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*****************************************************************************
** XInput
*****************************************************************************/

#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XIproto.h>

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

		printf("%-30s %s\n",
				pDev->name,
				(pDev->use == 0) ? "disabled" :
				(pDev->use == IsXKeyboard) ? "keyboard" :
				(pDev->use == IsXPointer) ? "pointer" :
				(pDev->use == IsXExtensionDevice) ? "extension" :
					"unknown");

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
							printf("    axis[%d]: res=%d, max=%d, max=%d\n",
								k, /* index */
								pVal->axes[k].resolution,
								pVal->axes[k].min_value,
								pVal->axes[k].max_value);
						}
						break;
					}
	
					default:
						printf("  unk: class=%lu\n",pClass->class);
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
		if (strcasecmp(gpDevList[i].name,pszDeviceName) == 0)
			return gpDevList + i;
	}

	return NULL;
}

/*****************************************************************************
** UI
*****************************************************************************/

typedef struct _UI UI;
struct _UI
{
	const char* pszName;
	int (*Init)(void);
	void (*Term)(void);
	int (*Run)(Display* pDisp, XDevice* pDev);
};

/*****************************************************************************
** GTK UI
*****************************************************************************/

#if WCM_ENABLE_GTK12 || WCM_ENABLE_GTK20
#define USE_GTK 1
#include <gtk/gtk.h>

static int GTKInit(void)
{
	return 1;
}

static void GTKTerm(void)
{
}

static int GTKRun(Display* pDisp, XDevice* pDev)
{
	return 1;
}

	UI gGTKUI = { "gtk", GTKInit, GTKTerm, GTKRun };
#else
#define USE_GTK 0
#endif

/*****************************************************************************
** Curses UI
*****************************************************************************/

static int CursesInit(void)
{
	return 1;
}

static void CursesTerm(void)
{
}

static int CursesRun(Display* pDisp, XDevice* pDev)
{
	return 1;
}

	UI gCursesUI = { "curses", CursesInit, CursesTerm, CursesRun };

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

static int RawRun(Display* pDisp, XDevice* pDev)
{
	XEvent event;
	XAnyEvent* pAny;

	while (1)
	{
		XNextEvent(pDisp,&event);

		pAny = (XAnyEvent*)&event;
		/* printf("event: type=%s\n",GetEventName(pAny->type)); */

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
			printf("Motion: x=%+6d y=%+6d p=%4d tx=%+4d ty=%+4d w=%+5d\n",
					pMove->axis_data[0],
					pMove->axis_data[1],
					pMove->axis_data[2],
					pMove->axis_data[3],
					pMove->axis_data[4],
					pMove->axis_data[5]);
		}
		else if ((pAny->type == gnInputEvent[INPUTEVENT_BTN_PRESS]) ||
				(pAny->type == gnInputEvent[INPUTEVENT_BTN_RELEASE]))
		{
			XDeviceButtonEvent* pBtn = (XDeviceButtonEvent*)pAny;
			printf("Button: %s %s\n",
					(pBtn->button == 1) ? "1-LEFT" :
					(pBtn->button == 2) ? "2-MIDDLE" :
					(pBtn->button == 3) ? "3-RIGHT" :
					(pBtn->button == 4) ? "4-EXTRA" :
					(pBtn->button == 5) ? "5-SIDE" : "?-ERROR",
					pAny->type == gnInputEvent[INPUTEVENT_BTN_PRESS] ?
						"DOWN" : "UP");
		}
		else
		{
			printf("Event: %s\n",GetEventName(pAny->type));
		}
	}

	return 0;
}

	UI gRawUI = { "raw", RawInit, RawTerm, RawRun };

/****************************************************************************/

void Usage(int rtn)
{
	fprintf(rtn ? stderr : stdout,
			"Usage: xidump [options] input_device\n"
			"  -h, --help          - usage\n"
			"  -v, --verbose       - verbose\n"
			"  -V, --version       - version\n"
			"  -l, --list          - list available input devices\n"
			"  -u, --ui ui_type    - use specified ui, see below\n"
			"\n"
			"Use --list option for input_device choices\n"
			"UI types: "
			#if USE_GTK
				"gtk, "
			#endif
			"curses, raw\n");
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

int Run(Display* pDisp, UI* pUI, const char* pszDeviceName)
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
			0,0,100,100, /* placement */
			0, /* border width */
			0, /* depth */
			InputOnly, /* class */
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
		if ((nRtn=pUI->Run(pDisp,pDev)) != 0)
			fprintf(stderr,"failed to run UI\n");
		pUI->Term();
	}

	XUngrabDevice(pDisp,pDev,CurrentTime);
	XCloseDevice(pDisp,pDev);
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
	UI* pUI = NULL;
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
			else if ((strcmp(pa,"-u") == 0) || (strcmp(pa,"--ui") == 0))
			{
				pa = *(argv++);
				if (!pa) Fatal("Missing ui argument\n");
				if (strcmp(pa,"gtk") == 0)
				{
					#if USE_GTK
					pUI = &gGTKUI;
					#else
					Fatal("Not configured for GTK.\n");
					#endif
				}
				else if (strcmp(pa,"curses") == 0)
					pUI = &gCursesUI;
				else if (strcmp(pa,"raw") == 0)
					pUI = &gRawUI;
				else
					Fatal("Unknown ui option %s\n",pa);
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

	/* default to a given UI */
	if (pUI == NULL)
	{
		pUI = &gRawUI;

//		#if USE_GTK
//		pUI = &gGTKUI;
//		#else
//		pUI = &gCursesUI;
//		#endif
	}
	
	/* open connection to XServer with XInput */
	pDisp = InitXInput();
	if (!pDisp) exit(1);

	if (bList)
		nRtn = ListDevices(pDisp,pszDeviceName);
	else
		nRtn = Run(pDisp,pUI,pszDeviceName);

	/* release device list */
	if (gpDevList)
		XFreeDeviceList(gpDevList);

	XCloseDisplay(pDisp);

	return nRtn;
}
