/*
 * Copyright 1995-2002 by Frederic Lepied, France. <Lepied@XFree86.org>
 * Copyright 2002-2013 by Ping Cheng, Wacom. <pingc@wacom.com>
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
#include <wacom-util.h>
#include <asm/types.h>
#include <linux/input.h>
#define MAX_USB_EVENTS 32

/* vendor IDs on the kernel device */
#define WACOM_VENDOR_ID 0x056a
#define WALTOP_VENDOR_ID 0x172F
#define NTRIG_VENDOR_ID 0x1b96
#define HANWANG_VENDOR_ID 0x0b57
#define LENOVO_VENDOR_ID 0x17ef

#define DEFAULT_SUPPRESS 2      /* default suppress */
#define MAX_SUPPRESS 100        /* max value of suppress */
#define BUFFER_SIZE 256         /* size of reception buffer */
#define MAXTRY 3                /* max number of try to receive magic number */
#define MIN_ROTATION  -900      /* the minimum value of the marker pen rotation */
#define MAX_ROTATION_RANGE 1800 /* the maximum range of the marker pen rotation */
#define MAX_ABS_WHEEL 1023      /* the maximum value of absolute wheel */

#define TILT_RES (180/M_PI)	/* Reported tilt resolution in points/radian
				   (1/degree) */
#define TILT_MIN -64		/* Minimum reported tilt value */
#define TILT_MAX 63		/* Maximum reported tilt value */

#define MIN_PAD_RING 0		/* I4 absolute scroll ring min value */
#define MAX_PAD_RING 71		/* I4 absolute scroll ring max value */

/* I4 cursor tool has a rotation offset of 175 degrees */
#define INTUOS4_CURSOR_ROTATION_OFFSET 175

/* Default max distance to the tablet at which a proximity-out event is generated for
 * cursor device (e.g. mouse). 
 */
#define PROXOUT_INTUOS_DISTANCE		10
#define PROXOUT_GRAPHIRE_DISTANCE	42

/* 2.6.28 */

#ifndef BTN_TOOL_DOUBLETAP
#define BTN_TOOL_DOUBLETAP 0x14d
#endif

#ifndef BTN_TOOL_TRIPLETAP
#define BTN_TOOL_TRIPLETAP 0x14e
#endif

/* 2.6.30 */

#ifndef ABS_MT_POSITION_X
#define ABS_MT_POSITION_X 0x35
#endif

#ifndef ABS_MT_POSITION_Y
#define ABS_MT_POSITION_Y 0x36
#endif

#ifndef ABS_MT_TRACKING_ID
#define ABS_MT_TRACKING_ID 0x39
#endif

/* 2.6.33 */

#ifndef ABS_MT_PRESSURE
#define ABS_MT_PRESSURE 0x3a
#endif

/* 2.6.36 */

#ifndef ABS_MT_SLOT
#define ABS_MT_SLOT 0x2f
#endif

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

/******************************************************************************
 * WacomModel - model-specific device capabilities
 *****************************************************************************/

struct _WacomModel
{
	const char* name;

	void (*Initialize)(WacomCommonPtr common, const char* id, float version);
	void (*GetResolution)(InputInfoPtr pInfo);
	int (*GetRanges)(InputInfoPtr pInfo);
	int (*Start)(InputInfoPtr pInfo);
	int (*Parse)(InputInfoPtr pInfo, const unsigned char* data, int len);
	int (*DetectConfig)(InputInfoPtr pInfo);
};

/******************************************************************************
 * WacomDeviceRec
 *****************************************************************************/

/* these device IDs are reported through ABS_MISC for Protocol 4 devices */
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

/* Each tablet may have one or more of the following
 * features */
#define WCM_PEN			0x00000001 /* Tablet supports pens */
#define WCM_PAD			0x00000002 /* Has a pad tool */

#define WCM_1FGT		0x00000004 /* One finger touch */
#define WCM_2FGT		0x00000008 /* Two finger touch */
#define WCM_STRIP		0x00000010 /* Tablet has menu strip (e.g.
					      Intuos3) */
#define WCM_RING		0x00000020 /* Tablet has touch ring (e.g.
					      Intuos4) */
#define WCM_DUALINPUT		0x00000040 /* Supports two tools on the
					      tablet simultaneously (Intuos
					      1 and 2) */
#define WCM_ROTATION		0x00000080 /* Needs to convert mouse tool
					      tilt to rotation */
#define WCM_LCD			0x00000100 /* Cintiqs and other display
					      tablets */
#define WCM_TPC			(0x00000200 | WCM_LCD) /* TabletPC (special
							  button handling,
							  always an LCD) */
#define WCM_PENTOUCH		0x00000400 /* Tablet supports pen and touch */
#define WCM_DUALRING		0x00000800 /* Tablet has two touch rings */
#define TabletHasFeature(common, feature) MaskIsSet((common)->tablet_type, (feature))
#define TabletSetFeature(common, feature) MaskSet((common)->tablet_type, (feature))

#define ABSOLUTE_FLAG		0x00000100
#define BAUD_19200_FLAG		0x00000400
#define BUTTONS_ONLY_FLAG	0x00000800

#define IsCursor(priv) (DEVICE_ID((priv)->flags) == CURSOR_ID)
#define IsStylus(priv) (DEVICE_ID((priv)->flags) == STYLUS_ID)
#define IsTouch(priv)  (DEVICE_ID((priv)->flags) == TOUCH_ID)
#define IsEraser(priv) (DEVICE_ID((priv)->flags) == ERASER_ID)
#define IsPad(priv)    (DEVICE_ID((priv)->flags) == PAD_ID)
#define IsPen(priv)    (IsStylus(priv) || IsEraser(priv))
#define IsTablet(priv) (IsPen(priv) || IsCursor(priv))

#define IsUSBDevice(common) ((common)->wcmDevCls == &gWacomUSBDevice)

#define FILTER_PRESSURE_RES	2048	/* maximum points in pressure curve */
/* Tested result for setting the pressure threshold to a reasonable value */
#define THRESHOLD_TOLERANCE (FILTER_PRESSURE_RES / 125)
#define DEFAULT_THRESHOLD (FILTER_PRESSURE_RES / 75)

#define WCM_MAX_BUTTONS		32	/* maximum number of tablet buttons */
#define WCM_MAX_X11BUTTON	127	/* maximum button number X11 can handle */

#define AXIS_INVERT  0x01               /* Flag describing an axis which increases "downward" */
#define AXIS_BITWISE 0x02               /* Flag describing an axis which changes bitwise */

/* Indicies into the wheel/strip default/keys/actions arrays */
#define WHEEL_REL_UP      0
#define WHEEL_REL_DN      1
#define WHEEL_ABS_UP      2
#define WHEEL_ABS_DN      3
#define WHEEL2_ABS_UP     4
#define WHEEL2_ABS_DN     5
#define STRIP_LEFT_UP     0
#define STRIP_LEFT_DN     1
#define STRIP_RIGHT_UP    2
#define STRIP_RIGHT_DN    3

/******************************************************************************
 * WacomDeviceState
 *****************************************************************************/
struct _WacomDeviceState
{
	InputInfoPtr pInfo;
	int device_id;		/* tool id reported from the physical device */
	int device_type;
	unsigned int serial_num;
	int x;
	int y;
	int buttons;
	int pressure;
	int tiltx;
	int tilty;
	int stripx;
	int stripy;
	int rotation;
	int abswheel;
	int abswheel2;
	int relwheel;
	int distance;
	int throttle;
	int proximity;
	int sample;	/* wraps every 24 days */
	int time;
};

static const struct _WacomDeviceState OUTPROX_STATE = {
  .abswheel = MAX_PAD_RING + 1,
  .abswheel2 = MAX_PAD_RING + 1
};

struct _WacomDeviceRec
{
	char *name;		/* Do not move, same offset as common->device_path. Used by DBG macro */
	/* configuration fields */
	struct _WacomDeviceRec *next;
	InputInfoPtr pInfo;
	int debugLevel;

	unsigned int flags;	/* various flags (type, abs, touch...) */
	int topX;		/* X top in device coordinates */
	int topY;		/* Y top in device coordinates */
	int bottomX;		/* X bottom in device coordinates */
	int bottomY;		/* Y bottom in device coordinates */
	int resolX;             /* X resolution */
	int resolY;             /* Y resolution */
	int minX;	        /* tool physical minX in device coordinates */
	int minY;	        /* tool physical minY in device coordinates */
	int maxX;	        /* tool physical maxX in device coordinates */
	int maxY;	        /* tool physical maxY in device coordinates */
	unsigned int serial;	/* device serial number this device takes (if 0, any serial is ok) */
	unsigned int cur_serial; /* current serial in prox */
	int cur_device_id;	/* current device ID in prox */

	/* button mapping information
	 *
	 * 'button' variables are indexed by physical button number (0..nbuttons)
	 * 'strip' variables are indexed by STRIP_* defines
	 * 'wheel' variables are indexed by WHEEL_* defines
	 */
	int button_default[WCM_MAX_BUTTONS]; /* Default mappings set by ourselves (possibly overridden by xorg.conf) */
	int strip_default[4];
	int wheel_default[6];
	unsigned keys[WCM_MAX_BUTTONS][256]; /* Action codes to perform when the associated event occurs */
	unsigned strip_keys[4][256];
	unsigned wheel_keys[6][256];
	Atom btn_actions[WCM_MAX_BUTTONS];   /* Action references so we can update the action codes when a client makes a change */
	Atom strip_actions[4];
	Atom wheel_actions[6];

	int nbuttons;           /* number of buttons for this subdevice */
	int naxes;              /* number of axes */
				/* FIXME: always 6, and the code relies on that... */

	WacomCommonPtr common;  /* common info pointer */

	/* state fields in device coordinates */
	struct _WacomDeviceState oldState; /* previous state information */
	int oldCursorHwProx;	/* previous cursor hardware proximity */

	/* JEJ - filters */
	int pPressCurve[FILTER_PRESSURE_RES + 1]; /* pressure curve */
	int nPressCtrl[4];      /* control points for curve */
	int minPressure;	/* the minimum pressure a pen may hold */
	int oldMinPressure;     /* to record the last minPressure before going out of proximity */
	unsigned int eventCnt;  /* count number of events while in proximity */
	int maxRawPressure;     /* maximum 'raw' pressure seen until first button event */
	WacomToolPtr tool;         /* The common tool-structure for this device */

	int isParent;		/* set to 1 if the device is not auto-hotplugged */

	OsTimerPtr serial_timer; /* timer used for serial number property update */
	OsTimerPtr tap_timer;   /* timer used for tap timing */
	OsTimerPtr touch_timer; /* timer used for touch switch property update */
};

#define MAX_SAMPLES	20
#define DEFAULT_SAMPLES 4

struct _WacomFilterState
{
        int npoints;
        int x[MAX_SAMPLES];
        int y[MAX_SAMPLES];
        int tiltx[MAX_SAMPLES];
        int tilty[MAX_SAMPLES];
};

struct _WacomChannel
{
	/* data stored in this structure is raw data from the tablet, prior
	 * to transformation and user-defined filtering. Suppressed values
	 * will not be included here, and hardware filtering may occur between
	 * the work stage and the valid state. */

	WacomDeviceState work;                         /* next state */
	Bool dirty;

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
	Bool (*Detect)(InputInfoPtr pInfo); /* detect device */
	Bool (*ParseOptions)(InputInfoPtr pInfo); /* parse class-specific options */
	Bool (*Init)(InputInfoPtr pInfo, char* id, float *version);   /* initialize device */
	int  (*ProbeKeys)(InputInfoPtr pInfo); /* set the bits for the keys supported */
};

extern WacomDeviceClass gWacomUSBDevice;
extern WacomDeviceClass gWacomISDV4Device;

/******************************************************************************
 * WacomCommonRec
 *****************************************************************************/

#define TILT_ENABLED_FLAG       2

#define MAX_FINGERS 16
#define MAX_CHANNELS (MAX_FINGERS+2) /* one channel for stylus/mouse. The other one for pad */
#define PAD_CHANNEL (MAX_CHANNELS-1)

typedef struct {
	int wcmZoomDistance;	       /* minimum distance for a zoom touch gesture */
	int wcmScrollDistance;	       /* minimum motion before sending a scroll gesture */
	int wcmScrollDirection;	       /* store the vertical or horizontal bit in use */
	int wcmMaxScrollFingerSpread; /* maximum distance between fingers for scroll gesture */
	int wcmGestureUsed;	       /* retain used gesture count within one in-prox event */
	int wcmTapTime;	   	       /* minimum time between taps for a right click */
} WacomGesturesParameters;

enum WacomProtocol {
	WCM_PROTOCOL_GENERIC,
	WCM_PROTOCOL_4,
	WCM_PROTOCOL_5
};

struct _WacomDriverRec
{
	WacomDevicePtr active;     /* Arbitrate motion through this pointer */
};
extern struct _WacomDriverRec WACOM_DRIVER; // Defined in wcmCommon.c

struct _WacomCommonRec 
{
	/* Do not move device_path, same offset as priv->name. Used by DBG macro */
	char* device_path;          /* device file name */
	dev_t min_maj;               /* minor/major number */
	unsigned char wcmFlags;     /* various flags (handle tilt) */
	int debugLevel;
	int vendor_id;		     /* Vendor ID */
	int tablet_id;		     /* USB tablet ID */
	int tablet_type;	     /* bitmask of tablet features (WCM_LCD, WCM_PEN, etc) */
	int fd;                      /* file descriptor to tablet */
	int fd_refs;                 /* number of references to fd; if =0, fd is invalid */
	unsigned long wcmKeys[NBITS(KEY_MAX)]; /* supported tool types for the device */
	WacomDevicePtr wcmTouchDevice; /* The pointer for pen to access the
					  touch tool of the same device id */

	Bool wcmHasHWTouchSwitch;    /* Tablet has a touch on/off switch */
	int wcmHWTouchSwitchState;   /* touch event disable/enabled by hardware switch */

	/* These values are in tablet coordinates */
	int wcmMinX;                 /* tablet min X value */
	int wcmMinY;                 /* tablet min Y value */
	int wcmMaxX;                 /* tablet max X value */
	int wcmMaxY;                 /* tablet max Y value */
	int wcmMaxZ;                 /* tablet max Z value */
	int wcmMaxTouchX;            /* touch panel max X value */
	int wcmMaxTouchY;            /* touch panel max Y value */
	int wcmResolX;		     /* pen tool X resolution in points/m */
	int wcmResolY;		     /* pen tool Y resolution in points/m */
	int wcmTouchResolX;	     /* touch X resolution in points/m */
	int wcmTouchResolY;	     /* touch Y resolution in points/m */
	                             /* tablet Z resolution is equivalent
	                              * to wcmMaxZ which is equal to 100% pressure */
	int wcmMaxDist;              /* tablet max distance value */
	int wcmMaxContacts;          /* MT device max number of contacts */

	/*
	 * TODO Remove wcmTiltOff*, once the kernel drivers reporting
	 * 	non-zero-centered tilt values are no longer in use.
	 */
	int wcmTiltOffX;	     /* styli tilt offset in X direction */
	int wcmTiltOffY;	     /* styli tilt offset in Y direction */
	double wcmTiltFactX;	     /* styli tilt factor in X direction */
	double wcmTiltFactY;	     /* styli tilt factor in Y direction */
	int wcmTiltMinX;	     /* styli min reported tilt in X direction */
	int wcmTiltMinY;	     /* styli min reported tilt in Y direction */
	int wcmTiltMaxX;	     /* styli max reported tilt in X direction */
	int wcmTiltMaxY;	     /* styli max reported tilt in Y direction */

	int wcmMaxStripX;            /* Maximum fingerstrip X */
	int wcmMaxStripY;            /* Maximum fingerstrip Y */

	WacomDevicePtr wcmDevices;   /* list of devices sharing same port */
	int wcmPktLength;            /* length of a packet */
	int wcmProtocolLevel;        /* Wacom Protocol used */
	float wcmVersion;            /* ROM version */
	int wcmRotate;               /* rotate screen (for TabletPC) */
	int wcmThreshold;            /* Threshold for button pressure */
	WacomChannel wcmChannel[MAX_CHANNELS]; /* channel device state */

	WacomDeviceClassPtr wcmDevCls; /* device class functions */
	WacomModelPtr wcmModel;        /* model-specific functions */
	int wcmTPCButton;	     /* set Tablet PC button on/off */
	int wcmTouch;	             /* disable/enable touch event */
	int wcmTouchDefault;	     /* default to disable when not supported */
	int wcmGesture;	     	     /* disable/enable touch gesture */
	int wcmGestureDefault;       /* default touch gesture to disable when not supported */
	int wcmGestureMode;	       /* data is in Gesture Mode? */
	WacomDeviceState wcmGestureState[MAX_FINGERS]; /* inital state when in gesture mode */
	WacomGesturesParameters wcmGestureParameters;
	int wcmMaxCursorDist;	     /* Max mouse distance reported so far */
	int wcmCursorProxoutDist;    /* Max mouse distance for proxy-out max/256 units */
	int wcmCursorProxoutDistDefault; /* Default max mouse distance for proxy-out */
	int wcmSuppress;        	 /* transmit position on delta > supress */
	int wcmRawSample;	     /* Number of raw data used to filter an event */
	int wcmPressureRecalibration; /* Determine if pressure recalibration of
					 worn pens should be performed */

	int bufpos;                        /* position with buffer */
	unsigned char buffer[BUFFER_SIZE]; /* data read from device */

	void *private;		     /* backend-specific information */

	WacomToolPtr wcmTool; /* List of unique tools */
	WacomToolPtr serials; /* Serial numbers provided at startup*/

	/* DO NOT TOUCH THIS. use wcmRefCommon() instead */
	int refcnt;			/* number of devices sharing this struct */

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 16
	ValuatorMask *touch_mask;
#endif
};

#define HANDLE_TILT(comm) ((comm)->wcmFlags & TILT_ENABLED_FLAG)

/******************************************************************************
 * WacomTool
 *****************************************************************************/
struct _WacomTool
{
	WacomToolPtr next; /* Next tool in list */

	int typeid; /* Tool type */
	unsigned int serial; /* Serial id, 0 == no serial id */
	Bool enabled;
	char *name;

	InputInfoPtr device; /* The InputDevice connected to this tool */
};

#endif /*__XF86_XF86WACOMDEFS_H */

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
