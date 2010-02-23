/*
 * Copyright 1995-2002 by Frederic Lepied, France. <Lepied@XFree86.org>
 * Copyright 2002-2009 by Ping Cheng, Wacom Technology. <pingc@wacom.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __XF86_XF86WACOMDEFS_H
#define __XF86_XF86WACOMDEFS_H

/*****************************************************************************
 * General Defines
 ****************************************************************************/
#include <asm/types.h>
#include <linux/input.h>
#define MAX_USB_EVENTS 32
#define WACOM_VENDOR_ID 0x056a  /* vendor ID on the kernel device */

#define DEFAULT_SUPPRESS 2      /* default suppress */
#define MAX_SUPPRESS 100        /* max value of suppress */
#define BUFFER_SIZE 256         /* size of reception buffer */
#define MAXTRY 3                /* max number of try to receive magic number */

/* Default max distance to the tablet at which a proximity-out event is generated for
 * cursor device (e.g. mouse). 
 */
#define PROXOUT_INTUOS_DISTANCE		10
#define PROXOUT_GRAPHIRE_DISTANCE	42

#define HEADER_BIT      0x80
#define ZAXIS_SIGN_BIT  0x40
#define ZAXIS_BIT       0x04
#define ZAXIS_BITS      0x3F
#define POINTER_BIT     0x20
#define PROXIMITY_BIT   0x40
#define BUTTON_FLAG     0x08
#define BUTTONS_BITS    0x78
#define TILT_SIGN_BIT   0x40
#define TILT_BITS       0x3F

#ifndef BTN_TOOL_DOUBLETAP
#define BTN_TOOL_DOUBLETAP 0x14d
#endif

/* defines to discriminate second side button and the eraser */
#define ERASER_PROX     4
#define OTHER_PROX      1

/* to access kernel defined bits */
#define BIT(x)		(1<<((x) & (BITS_PER_LONG - 1)))
#define BITS_PER_LONG	(sizeof(long) * 8)
#define NBITS(x)	((((x)-1)/BITS_PER_LONG)+1)
#define ISBITSET(x,y)	((x)[LONG(y)] & BIT(y))
#define OFF(x)		((x)%BITS_PER_LONG)
#define LONG(x)		((x)/BITS_PER_LONG)

/******************************************************************************
 * Forward Declarations
 *****************************************************************************/

typedef struct _WacomModel WacomModel, *WacomModelPtr;
typedef struct _WacomDeviceRec WacomDeviceRec, *WacomDevicePtr;
typedef struct _WacomDeviceState WacomDeviceState, *WacomDeviceStatePtr;
typedef struct _WacomChannel  WacomChannel, *WacomChannelPtr;
typedef struct _WacomCommonRec WacomCommonRec, *WacomCommonPtr;
typedef struct _WacomFilterState WacomFilterState, *WacomFilterStatePtr;
typedef struct _WacomDeviceClass WacomDeviceClass, *WacomDeviceClassPtr;
typedef struct _WacomTool WacomTool, *WacomToolPtr;
typedef struct _WacomToolArea WacomToolArea, *WacomToolAreaPtr;

/******************************************************************************
 * WacomModel - model-specific device capabilities
 *****************************************************************************/

struct _WacomModel
{
	const char* name;

	void (*Initialize)(WacomCommonPtr common, const char* id, float version);
	void (*GetResolution)(LocalDevicePtr local);
	int (*GetRanges)(LocalDevicePtr local);
	int (*Start)(LocalDevicePtr local);
	int (*Parse)(LocalDevicePtr local, const unsigned char* data);
	int (*FilterRaw)(WacomCommonPtr common, WacomChannelPtr pChannel,
		WacomDeviceStatePtr ds);
	int (*DetectConfig)(LocalDevicePtr local);
};

/******************************************************************************
 * WacomDeviceRec
 *****************************************************************************/

#define DEVICE_ID(flags) ((flags) & 0xff)
#define STYLUS_DEVICE_ID	0x02
#define TOUCH_DEVICE_ID		0x03
#define CURSOR_DEVICE_ID	0x06
#define ERASER_DEVICE_ID	0x0A
#define PAD_DEVICE_ID		0x0F

#define STYLUS_ID		0x00000001
#define TOUCH_ID		0x00000002
#define CURSOR_ID		0x00000004
#define ERASER_ID		0x00000008
#define PAD_ID			0x00000010

#define ABSOLUTE_FLAG		0x00000100
#define KEEP_SHAPE_FLAG		0x00000200
#define BAUD_19200_FLAG		0x00000400
#define BUTTONS_ONLY_FLAG	0x00000800
#define TPCBUTTONS_FLAG		0x00001000
#define TPCBUTTONONE_FLAG	0x00002000
#define COREEVENT_FLAG		0x00004000

#define IsCursor(priv) (DEVICE_ID((priv)->flags) == CURSOR_ID)
#define IsStylus(priv) (DEVICE_ID((priv)->flags) == STYLUS_ID)
#define IsTouch(priv)  (DEVICE_ID((priv)->flags) == TOUCH_ID)
#define IsEraser(priv) (DEVICE_ID((priv)->flags) == ERASER_ID)
#define IsPad(priv)    (DEVICE_ID((priv)->flags) == PAD_ID)

#define FILTER_PRESSURE_RES	2048	/* maximum points in pressure curve */
#define WCM_MAX_BUTTONS		32	/* maximum number of tablet buttons */
#define WCM_MAX_MOUSE_BUTTONS	16	/* maximum number of buttons-on-pointer
                                         * (which are treated as mouse buttons,
                                         * not as keys like tablet menu buttons). 
					 * For backword compability support, 
					 * tablet buttons besides the strips are
					 * treated as buttons */
/* get/set/property */
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 3

typedef struct _PROPINFO PROPINFO;

struct _PROPINFO
{
	Atom wcmProp;
	char paramName[32];
	int nParamID;
	int nFormat;
	int nSize;
	int nDefault;
};
#endif /* GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 3 */

struct _WacomDeviceRec
{
	char *name;		/* Do not move, same offset as common->wcmDevice */
	/* configuration fields */
	struct _WacomDeviceRec *next;
	LocalDevicePtr local;
	int debugLevel;

	unsigned int flags;	/* various flags (type, abs, touch...) */
	int topX;		/* X top */
	int topY;		/* Y top */
	int bottomX;		/* X bottom */
	int bottomY;		/* Y bottom */
	int resolX;             /* X resolution */
	int resolY;             /* Y resolution */
	int maxX;	        /* tool logical maxX */
	int maxY;	        /* tool logical maxY */
	int sizeX;		/* active X size */
	int sizeY;		/* active Y size */
	double factorX;		/* X factor */
	double factorY;		/* Y factor */
	unsigned int serial;	/* device serial number */
	int screen_no;		/* associated screen */
	int screenTopX[32];	/* left cordinate of the associated screen */
	int screenTopY[32];	/* top cordinate of the associated screen */
	int screenBottomX[32];	/* right cordinate of the associated screen */
	int screenBottomY[32];	/* bottom cordinate of the associated screen */
	int maxWidth;		/* max active screen width */
	int maxHeight;		/* max active screen height */
	int leftPadding;	/* left padding for virtual tablet */
	int topPadding;		/* top padding for virtual tablet */
	int button[WCM_MAX_BUTTONS];/* buttons assignments */
	unsigned keys[WCM_MAX_BUTTONS][256]; /* keystrokes assigned to buttons */
	int relup;
	unsigned rupk[256];     /* keystrokes assigned to relative wheel up event (default is button 4) */
	int reldn;
	unsigned rdnk[256];     /* keystrokes assigned to relative wheel down event (default is button 5) */
	int wheelup;
	unsigned wupk[256];     /* keystrokes assigned to absolute wheel/throttle up event (default is button 4) */
	int wheeldn;
	unsigned wdnk[256];     /* keystrokes assigned to absolute wheel/throttle down event (default is button 5) */
	int striplup;
	unsigned slupk[256];    /* keystrokes assigned to left strip up event (default is button 4) */
	int stripldn;
	unsigned sldnk[256];    /* keystrokes assigned to left strip up event (default is button 5) */
	int striprup;
	unsigned srupk[256];    /* keystrokes assigned to right strip up event (default is button 4) */
	int striprdn;
 	unsigned srdnk[256];    /* keystrokes assigned to right strip up event (default is button 4) */
	int nbuttons;           /* number of buttons for this subdevice */
	int naxes;              /* number of axes */

	WacomCommonPtr common;  /* common info pointer */

	/* state fields */
	int currentX;           /* current X position */
	int currentY;           /* current Y position */
	int currentSX;          /* current screen X position */
	int currentSY;          /* current screen Y position */
	int oldX;               /* previous X position */
	int oldY;               /* previous Y position */
	int oldZ;               /* previous pressure */
	int oldCapacity;        /* previous capacity */
	int oldTiltX;           /* previous tilt in x direction */
	int oldTiltY;           /* previous tilt in y direction */    
	int oldWheel;           /* previous wheel value */    
	int oldRot;             /* previous rotation value */
	int oldStripX;          /* previous left strip value */
	int oldStripY;          /* previous right strip value */
	int oldThrottle;        /* previous throttle value */
	int oldButtons;         /* previous buttons state */
	int oldProximity;       /* previous proximity */
	int hardProx;       	/* previous hardware proximity */
	int old_device_id;	/* last in prox device id */
	int old_serial;		/* last in prox tool serial number */
	int devReverseCount;	/* Relative ReverseConvert called twice each movement*/
	int numScreen;          /* number of configured screens */
	int currentScreen;      /* current screen in display */
	int twinview;	        /* using twinview mode of gfx card */
	int tvoffsetX;		/* X edge offset for TwinView setup */
	int tvoffsetY;		/* Y edge offset for TwinView setup */
	int tvResolution[4];	/* twinview screens' resultion */
	int wcmMMonitor;        /* disable/enable moving across screens in multi-monitor desktop */
	int wcmDevOpenCount;    /* Device open count */
	int wcmInitKeyClassCount;    /* Device InitKeyClassDeviceStruct count */

	/* JEJ - throttle */
	int throttleStart;      /* time in ticks for last wheel movement */
	int throttleLimit;      /* time in ticks for next wheel movement */
	int throttleValue;      /* current throttle value */

	/* JEJ - filters */
	int* pPressCurve;       /* pressure curve */
	int nPressCtrl[4];      /* control points for curve */

	WacomToolPtr tool;         /* The common tool-structure for this device */
	WacomToolAreaPtr toolarea; /* The area defined for this device */

	int isParent;		/* set to 1 if the device is not auto-hotplugged */
	Atom btn_actions[WCM_MAX_BUTTONS]; /* property handlers to listen to */
};

/******************************************************************************
 * WacomDeviceState
 *****************************************************************************/

#define MAX_SAMPLES	20
#define DEFAULT_SAMPLES 4

struct _WacomDeviceState
{
	LocalDevicePtr local;
	int device_id;		/* tool id reported from the physical device */
	int device_type;
	unsigned int serial_num;
	int x;
	int y;
	int buttons;
	int pressure;
	int capacity;
	int tiltx;
	int tilty;
	int stripx;
	int stripy;
	int rotation;
	int abswheel;
	int relwheel;
	int distance;
	int throttle;
	int discard_first;
	int proximity;
	int sample;	/* wraps every 24 days */
};

struct _WacomFilterState
{
        int npoints;
        int x[MAX_SAMPLES];
        int y[MAX_SAMPLES];
        int tiltx[MAX_SAMPLES];
        int tilty[MAX_SAMPLES];
        int statex;
        int statey;
};

struct _WacomChannel
{
	/* data stored in this structure is raw data from the tablet, prior
	 * to transformation and user-defined filtering. Suppressed values
	 * will not be included here, and hardware filtering may occur between
	 * the work stage and the valid state. */

	WacomDeviceState work;                         /* next state */

	/* the following union contains the current known state of the
	 * device channel, as well as the previous MAX_SAMPLES states
	 * for use in detecting hardware defects, jitter, trends, etc. */
	union
	{
		WacomDeviceState state;                /* current state */
		WacomDeviceState states[MAX_SAMPLES];  /* states 0..MAX */
	} valid;

	int nSamples;
	WacomFilterState rawFilter;
};

/******************************************************************************
 * WacomDeviceClass
 *****************************************************************************/

struct _WacomDeviceClass
{
	Bool (*Detect)(LocalDevicePtr local); /* detect device */
	Bool (*Init)(LocalDevicePtr local, char* id, float *version);   /* initialize device */
	void (*Read)(LocalDevicePtr local);   /* reads device */
};

	extern WacomDeviceClass gWacomUSBDevice;
	extern WacomDeviceClass gWacomISDV4Device;

/******************************************************************************
 * WacomCommonRec
 *****************************************************************************/

#define TILT_REQUEST_FLAG       1
#define TILT_ENABLED_FLAG       2
#define RAW_FILTERING_FLAG      4
/* set if the /dev/input driver should wait for SYN_REPORT events as the
 * end of record indicator or not 
*/
#define USE_SYN_REPORTS_FLAG	8
#define AUTODEV_FLAG		16

#define DEVICE_ISDV4 		0x000C

#define MAX_CHANNELS 2
#define MAX_FINGERS  2

struct _WacomCommonRec 
{
	/* Do not move wcmDevice, same offset as priv->name */
	char* wcmDevice;             /* device file name */
	dev_t min_maj;               /* minor/major number */
	unsigned char wcmFlags;     /* various flags (handle tilt) */
	int debugLevel;
	int tablet_id;		     /* USB tablet ID */
	int fd;                      /* file descriptor to tablet */
	int fd_refs;                 /* number of references to fd; if =0, fd is invalid */

	/* These values are in tablet coordinates */
	int wcmMaxX;                 /* tablet max X value */
	int wcmMaxY;                 /* tablet max Y value */
	int wcmMaxZ;                 /* tablet max Z value */
	int wcmMaxTouchX;            /* touch panel max X value */
	int wcmMaxTouchY;            /* touch panel max Y value */
	int wcmResolX;		     /* pen tool X resolution in points/inch */
	int wcmResolY;		     /* pen tool Y resolution in points/inch */
	int wcmTouchResolX;	     /* touch X resolution in points/mm */
	int wcmTouchResolY;	     /* touch Y resolution in points/mm */
	                             /* tablet Z resolution is equivalent
	                              * to wcmMaxZ which is equal to 100% pressure */
	int wcmMaxCapacity;	     /* max capacity value */
	int wcmMaxDist;              /* tablet max distance value */
	int wcmMaxtiltX;	     /* styli max tilt in X directory */ 
	int wcmMaxtiltY;	     /* styli max tilt in Y directory */ 

	int wcmMaxStripX;            /* Maximum fingerstrip X */
	int wcmMaxStripY;            /* Maximum fingerstrip Y */

	int nbuttons;                /* total number of buttons */
	int npadkeys;                /* number of pad keys in the above array */
	int padkey_code[WCM_MAX_BUTTONS];/* hardware codes for buttons */

	WacomDevicePtr wcmDevices;   /* list of devices sharing same port */
	int wcmPktLength;            /* length of a packet */
	int wcmProtocolLevel;        /* 4 for Wacom IV, 5 for Wacom V */
	float wcmVersion;            /* ROM version */
	int wcmForceDevice;          /* force device type (used by ISD V4) */
	int wcmRotate;               /* rotate screen (for TabletPC) */
	int wcmThreshold;            /* Threshold for button pressure */
	WacomChannel wcmChannel[MAX_CHANNELS]; /* channel device state */
	unsigned int wcmISDV4Speed;  /* serial ISDV4 link speed */

	WacomDeviceClassPtr wcmDevCls; /* device class functions */
	WacomModelPtr wcmModel;        /* model-specific functions */
	char * wcmEraserID;	     /* eraser associated with the stylus */
	int wcmTPCButton;	     /* set Tablet PC button on/off */
	int wcmTouch;	             /* disable/enable touch event */
	int wcmTPCButtonDefault;     /* Tablet PC button default */
	int wcmTouchDefault;	     /* default to disable when not supported */
	int wcmGesture;	     	     /* disable/enable touch gesture */
	int wcmGestureDefault;       /* default touch gesture to disable when not supported */
	int wcmGestureMode;	       /* data is in Gesture Mode? */
	WacomDeviceState wcmGestureState[MAX_FINGERS]; /* inital state when in gesture mode */
	int wcmCapacity;	     /* disable/enable capacity */
	int wcmCapacityDefault;      /* default to -1 when capacity isn't supported/disabled */
				     /* 3 when capacity is supported */
	int wcmMaxCursorDist;	     /* Max mouse distance reported so far */
	int wcmCursorProxoutDist;    /* Max mouse distance for proxy-out max/256 units */
	int wcmCursorProxoutDistDefault; /* Default max mouse distance for proxy-out */
	int wcmSuppress;        	 /* transmit position on delta > supress */
	int wcmRawSample;	     /* Number of raw data used to filter an event */
	int wcmScaling;		     /* dealing with missing calling DevConvert case. Default 0 */

	int bufpos;                        /* position with buffer */
	unsigned char buffer[BUFFER_SIZE]; /* data read from device */

	int wcmLastToolSerial;
	int wcmEventCnt;
	struct input_event wcmEvents[MAX_USB_EVENTS];  /* events for current change */

	WacomToolPtr wcmTool; /* List of unique tools */
};

#define HANDLE_TILT(comm) ((comm)->wcmFlags & TILT_ENABLED_FLAG)
#define RAW_FILTERING(comm) ((comm)->wcmFlags & RAW_FILTERING_FLAG)
#define USE_SYN_REPORTS(comm) ((comm)->wcmFlags & USE_SYN_REPORTS_FLAG)

/******************************************************************************
 * WacomTool
 *****************************************************************************/
struct _WacomTool
{
	WacomToolPtr next; /* Next tool in list */

	int typeid; /* Tool type */
	int serial; /* Serial id, 0 == no serial id */

	WacomToolAreaPtr current;  /* Current area in-prox */
	WacomToolAreaPtr arealist; /* List of defined areas */
};

/******************************************************************************
 * WacomToolArea
 *****************************************************************************/
struct _WacomToolArea
{
	WacomToolAreaPtr next;

	int topX;    /* Top X/Y */
	int topY;
	int bottomX; /* Bottom X/Y */
	int bottomY;

	LocalDevicePtr device; /* The InputDevice connected to this area */
};

#endif /*__XF86_XF86WACOMDEFS_H */
