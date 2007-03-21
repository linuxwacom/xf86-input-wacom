/*
 * Copyright 1995-2002 by Frederic Lepied, France. <Lepied@XFree86.org>
 * Copyright 2002-2007 by Ping Cheng, Wacom Technology. <pingc@wacom.com>
 * 
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is  hereby granted without fee, provided that
 * the  above copyright   notice appear  in   all  copies and  that both  that
 * copyright  notice   and   this  permission   notice  appear  in  supporting
 * documentation, and that   the  name of  Frederic   Lepied not  be  used  in
 * advertising or publicity pertaining to distribution of the software without
 * specific,  written      prior  permission.     Frederic  Lepied   makes  no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 * 
 * FREDERIC  LEPIED DISCLAIMS ALL   WARRANTIES WITH REGARD  TO  THIS SOFTWARE,
 * INCLUDING ALL IMPLIED   WARRANTIES OF MERCHANTABILITY  AND   FITNESS, IN NO
 * EVENT  SHALL FREDERIC  LEPIED BE   LIABLE   FOR ANY  SPECIAL, INDIRECT   OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA  OR PROFITS, WHETHER  IN  AN ACTION OF  CONTRACT,  NEGLIGENCE OR OTHER
 * TORTIOUS  ACTION, ARISING    OUT OF OR   IN  CONNECTION  WITH THE USE    OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

#ifndef __XF86_XF86WACOM_H
#define __XF86_XF86WACOM_H

/****************************************************************************/

#include <xf86Version.h>
#include "../include/Xwacom.h"

/*****************************************************************************
 * Linux Input Support
 ****************************************************************************/

#ifdef LINUX_INPUT
#include <asm/types.h>
#include <linux/input.h>

/* keithp - a hack to avoid redefinitions of these in xf86str.h */
#ifdef BUS_PCI
#undef BUS_PCI
#endif
#ifdef BUS_ISA
#undef BUS_ISA
#endif

#define MAX_USB_EVENTS 32

#endif /* LINUX_INPUT */

/* max number of input events to read in one read call */
#define MAX_EVENTS 50

/*****************************************************************************
 * XFree86 V4.x Headers
 ****************************************************************************/

#ifndef XFree86LOADER
#include <unistd.h>
#include <errno.h>
#endif

#include <misc.h>
#include <xf86.h>
#define NEED_XF86_TYPES
#if !defined(DGUX)
#include <xisb.h>
#include <xf86_ansic.h>
#endif
#include <xf86_OSproc.h>
#include <xf86Xinput.h>
#include <exevents.h>           /* Needed for InitValuator/Proximity stuff */
#include <X11/keysym.h>
#include <mipointer.h>

#ifdef XFree86LOADER
#include <xf86Module.h>
#endif

/*****************************************************************************
 * QNX support
 ****************************************************************************/

#if defined(__QNX__) || defined(__QNXNTO__)
#define POSIX_TTY
#endif

/******************************************************************************
 * Debugging support
 *****************************************************************************/

#ifdef DBG
#undef DBG
#endif
#ifdef DEBUG
#undef DEBUG
#endif

#define DEBUG 1
#if DEBUG
#define DBG(lvl, f) do { if ((lvl) <= gWacomModule.debugLevel) f; } while (0)
#else
#define DBG(lvl, f)
#endif

/*****************************************************************************
 * General Macros
 ****************************************************************************/

#define ABS(x) ((x) > 0 ? (x) : -(x))
#define mils(res) (res * 100 / 2.54) /* resolution */

/*****************************************************************************
 * General Defines
 ****************************************************************************/
#define DEFAULT_SPEED 1.0       /* default relative cursor speed */
#define MAX_ACCEL 7             /* number of acceleration levels */
#define DEFAULT_SUPPRESS 2      /* default suppress */
#define MAX_SUPPRESS 20         /* max value of suppress */
#define BUFFER_SIZE 256         /* size of reception buffer */
#define XI_STYLUS "STYLUS"      /* X device name for the stylus */
#define XI_CURSOR "CURSOR"      /* X device name for the cursor */
#define XI_ERASER "ERASER"      /* X device name for the eraser */
#define XI_PAD    "PAD"         /* X device name for the I3 Pad */
#define MAX_VALUE 100           /* number of positions */
#define MAXTRY 3                /* max number of try to receive magic number */
#define MAX_COORD_RES 1270.0    /* Resolution of the returned MaxX and MaxY */

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

/* defines to discriminate second side button and the eraser */
#define ERASER_PROX     4
#define OTHER_PROX      1

/******************************************************************************
 * Forward Declarations
 *****************************************************************************/

typedef struct _WacomModule WacomModule;
typedef struct _WacomModule4 WacomModule4;
typedef struct _WacomModule3 WacomModule3;
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
 * WacomModule - all globals are packed in a single structure to keep the
 *               global namespaces as clean as possible.
 *****************************************************************************/

struct _WacomModule4
{
	InputDriverPtr wcmDrv;
};

struct _WacomModule
{
	int debugLevel;
	const char* identification;

	WacomModule4 v4;

	int (*DevOpen)(DeviceIntPtr pWcm);
	void (*DevReadInput)(LocalDevicePtr local);
	void (*DevControlProc)(DeviceIntPtr device, PtrCtrl* ctrl);
	void (*DevClose)(LocalDevicePtr local);
	int (*DevProc)(DeviceIntPtr pWcm, int what);
	int (*DevChangeControl)(LocalDevicePtr local, xDeviceCtl* control);
	int (*DevSwitchMode)(ClientPtr client, DeviceIntPtr dev, int mode);
	Bool (*DevConvert)(LocalDevicePtr local, int first, int num,
		int v0, int v1, int v2, int v3, int v4, int v5, int* x, int* y);
	Bool (*DevReverseConvert)(LocalDevicePtr local, int x, int y,
		int* valuators);
};

	extern WacomModule gWacomModule;

/******************************************************************************
 * WacomModel - model-specific device capabilities
 *****************************************************************************/

struct _WacomModel
{
	const char* name;

	void (*Initialize)(WacomCommonPtr common, const char* id, float version);
	void (*GetResolution)(LocalDevicePtr local);
	int (*GetRanges)(LocalDevicePtr local);
	int (*Reset)(LocalDevicePtr local);
	int (*EnableTilt)(LocalDevicePtr local);
	int (*EnableSuppress)(LocalDevicePtr local);
	int (*SetLinkSpeed)(LocalDevicePtr local);
	int (*Start)(LocalDevicePtr local);
	int (*Parse)(WacomCommonPtr common, const unsigned char* data);
	int (*FilterRaw)(WacomCommonPtr common, WacomChannelPtr pChannel,
		WacomDeviceStatePtr ds);
	int (*DetectConfig)(LocalDevicePtr local);
};

/******************************************************************************
 * WacomDeviceRec
 *****************************************************************************/

#define DEVICE_ID(flags) ((flags) & 0x0f)
#define STYLUS_DEVICE_ID	0x02
#define CURSOR_DEVICE_ID	0x06
#define ERASER_DEVICE_ID	0x0A
#define PAD_DEVICE_ID		0x0F

#define STYLUS_ID		0x00000001
#define CURSOR_ID		0x00000002
#define ERASER_ID		0x00000004
#define PAD_ID			0x00000008
#define ABSOLUTE_FLAG		0x00000010
#define KEEP_SHAPE_FLAG		0x00000020
#define BAUD_19200_FLAG		0x00000040
#define BUTTONS_ONLY_FLAG	0x00000080
#define TPCBUTTONS_FLAG		0x00000100
#define TPCBUTTONONE_FLAG	0x00000200

#define IsCursor(priv) (DEVICE_ID((priv)->flags) == CURSOR_ID)
#define IsStylus(priv) (DEVICE_ID((priv)->flags) == STYLUS_ID)
#define IsEraser(priv) (DEVICE_ID((priv)->flags) == ERASER_ID)
#define IsPad(priv)    (DEVICE_ID((priv)->flags) == PAD_ID)

typedef int (*FILTERFUNC)(WacomDevicePtr pDev, WacomDeviceStatePtr pState);

/* FILTERFUNC return values:
 *   -1 - data should be discarded
 *    0 - data is valid */

#define FILTER_PRESSURE_RES	2048	/* maximum points in pressure curve */
#define MAX_BUTTONS		32	/* maximum number of tablet buttons */
#define MAX_MOUSE_BUTTONS	16	/* maximum number of buttons-on-pointer
                                         * (which are treated as mouse buttons,
                                         * not as keys like tablet menu buttons). 
					 * For backword compability support, 
					 * tablet buttons besides the strips are
					 * treated as buttons */

typedef enum { TV_NONE = 0, TV_ABOVE_BELOW = 1, TV_LEFT_RIGHT = 2 } tvMode;

struct _WacomDeviceRec
{
	/* configuration fields */
	unsigned int flags;     /* various flags (type, abs, touch...) */
	int topX;               /* X top */
	int topY;               /* Y top */
	int bottomX;            /* X bottom */
	int bottomY;            /* Y bottom */
	double factorX;         /* X factor */
	double factorY;         /* Y factor */
	unsigned int serial;    /* device serial number */
	int screen_no;          /* associated screen */
	int screenTopX[32];	/* left cordinate of the associated screen */
	int screenTopY[32];	/* top cordinate of the associated screen */
	int screenBottomX[32];	/* right cordinate of the associated screen */
	int screenBottomY[32];	/* bottom cordinate of the associated screen */
 	int button[MAX_BUTTONS];/* buttons assignments */
	unsigned keys[MAX_BUTTONS][256]; /* keystrokes assigned to buttons */
	int relup;
	unsigned rupk[256]; /* keystrokes assigned to relative wheel up event (default is button 4) */
	int reldn;
	unsigned rdnk[256]; /* keystrokes assigned to relative wheel down event (default is button 5) */
	int wheelup;
	unsigned wupk[256]; /* keystrokes assigned to absolute wheel or throttle up event (default is button 4) */
	int wheeldn;
	unsigned wdnk[256]; /* keystrokes assigned to absolute wheel or throttle down event (default is button 5) */
	int striplup;
	unsigned slupk[256]; /* keystrokes assigned to left strip up event (default is button 4) */
	int stripldn;
	unsigned sldnk[256]; /* keystrokes assigned to left strip up event (default is button 5) */
	int striprup;
	unsigned srupk[256]; /* keystrokes assigned to right strip up event (default is button 4) */
	int striprdn;
 	unsigned srdnk[256]; /* keystrokes assigned to right strip up event (default is button 4) */
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
	double speed;           /* relative mode speed */
	int accel;              /* relative mode acceleration */
	int numScreen;          /* number of configured screens */
	int currentScreen;      /* current screen in display */
	tvMode twinview;	/* using twinview mode of gfx card */
	int tvoffsetX;		/* X edge offset for TwinView setup */
	int tvoffsetY;		/* Y edge offset for TwinView setup */
	int tvResolution[4];	/* twinview screens' resultion */
	int wcmDevOpenCount;    /* Device open count */

	/* JEJ - throttle */
	int throttleStart;      /* time in ticks for last wheel movement */
	int throttleLimit;      /* time in ticks for next wheel movement */
	int throttleValue;      /* current throttle value */

	/* JEJ - filters */
	int* pPressCurve;               /* pressure curve */
	int nPressCtrl[4];              /* control points for curve */

	WacomToolPtr tool;         /* The common tool-structure for this device */
	WacomToolAreaPtr toolarea; /* The area defined for this device */
};

/******************************************************************************
 * WacomDeviceState
 *****************************************************************************/

#define MAX_SAMPLES 4

#define PEN(ds)         ((((ds)->device_id) & 0x07ff) == 0x0022 || \
                         (((ds)->device_id) & 0x07ff) == 0x0042 || \
                         (((ds)->device_id) & 0x07ff) == 0x0052)
#define STROKING_PEN(ds) ((((ds)->device_id) & 0x07ff) == 0x0032)
#define AIRBRUSH(ds)    ((((ds)->device_id) & 0x07ff) == 0x0112)
#define MOUSE_4D(ds)    ((((ds)->device_id) & 0x07ff) == 0x0094)
#define MOUSE_2D(ds)    ((((ds)->device_id) & 0x07ff) == 0x0007)
#define LENS_CURSOR(ds) ((((ds)->device_id) & 0x07ff) == 0x0096)
#define INKING_PEN(ds)  ((((ds)->device_id) & 0x07ff) == 0x0012)
#define STYLUS_TOOL(ds) (PEN(ds) || STROKING_PEN(ds) || INKING_PEN(ds) || \
			AIRBRUSH(ds))
#define CURSOR_TOOL(ds) (MOUSE_4D(ds) || LENS_CURSOR(ds) || MOUSE_2D(ds))

struct _WacomDeviceState
{
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

	/* the following struct contains the current known state of the
	 * device channel, as well as the previous MAX_SAMPLES states
	 * for use in detecting hardware defects, jitter, trends, etc. */

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
	Bool (*Init)(LocalDevicePtr local);   /* initialize device */
	void (*Read)(LocalDevicePtr local);   /* reads device */
};

#ifdef LINUX_INPUT
	extern WacomDeviceClass gWacomUSBDevice;
#endif

	extern WacomDeviceClass gWacomISDV4Device;
	extern WacomDeviceClass gWacomSerialDevice;

/******************************************************************************
 * WacomCommonRec
 *****************************************************************************/

#define TILT_REQUEST_FLAG       1
#define TILT_ENABLED_FLAG       2
#define RAW_FILTERING_FLAG      4
#ifdef LINUX_INPUT
/* set if the /dev/input driver should wait for SYN_REPORT events as the
   end of record indicator */
#define USE_SYN_REPORTS_FLAG	8
#endif

#define DEVICE_ISDV4 		0x000C

#define ROTATE_NONE XWACOM_VALUE_ROTATE_NONE
#define ROTATE_CW XWACOM_VALUE_ROTATE_CW
#define ROTATE_CCW XWACOM_VALUE_ROTATE_CCW
#define ROTATE_HALF XWACOM_VALUE_ROTATE_HALF

#define MAX_CHANNELS 2

struct _WacomCommonRec 
{
	char* wcmDevice;             /* device file name */
	int wcmSuppress;             /* transmit position on delta > supress */
	unsigned char wcmFlags;      /* various flags (handle tilt) */
	int tablet_id;		     /* USB tablet ID */
	int fd;                      /* file descriptor to tablet */
	int fd_refs;                 /* number of references to fd; if =0, fd is invalid */

	/* These values are in tablet coordinates */
	int wcmMaxX;                 /* tablet max X value */
	int wcmMaxY;                 /* tablet max Y value */
	int wcmMaxZ;                 /* tablet max Z value */
	int wcmMaxDist;              /* tablet max distance value */
	int wcmResolX;               /* tablet X resolution in points/inch */
	int wcmResolY;               /* tablet Y resolution in points/inch */
	                             /* tablet Z resolution is equivalent
	                              * to wcmMaxZ which is equal to 100%
	                              * pressure */

	/* These values are in user coordinates */
	int wcmUserResolX;           /* user-defined X resolution */
	int wcmUserResolY;           /* user-defined Y resolution */
	int wcmUserResolZ;           /* user-defined Z resolution,
	                              * value equal to 100% pressure */

	int wcmMaxStripX;            /* Maximum fingerstrip X */
	int wcmMaxStripY;            /* Maximum fingerstrip Y */

	int nbuttons;                /* total number of buttons */
	int npadkeys;                /* number of pad keys in the above array */
	int padkey_code[MAX_BUTTONS];/* hardware codes for buttons */

	LocalDevicePtr* wcmDevices;  /* array of devices sharing same port */
	int wcmNumDevices;           /* number of devices */
	int wcmPktLength;            /* length of a packet */
	int wcmProtocolLevel;        /* 4 for Wacom IV, 5 for Wacom V */
	float wcmVersion;            /* ROM version */
	int wcmForceDevice;          /* force device type (used by ISD V4) */
	int wcmRotate;               /* rotate screen (for TabletPC) */
	int wcmThreshold;            /* Threshold for button pressure */
	WacomChannel wcmChannel[MAX_CHANNELS]; /* channel device state */
	unsigned int wcmLinkSpeed;   /* serial link speed */
	unsigned int wcmISDV4Speed;  /* serial ISDV4 link speed */

	WacomDeviceClassPtr wcmDevCls; /* device class functions */
	WacomModelPtr wcmModel;        /* model-specific functions */
	char * wcmEraserID;	     /* eraser associated with the stylus */
	int wcmMMonitor;             /* disable/enable moving across screens in multi-monitor desktop */
	int wcmTPCButton;	     /* set Tablet PC button on/off */
	int wcmTPCButtonDefault;     /* Tablet PC button default */
	int wcmMaxCursorDist;	     /* Max mouse distance reported so far */
	int wcmCursorProxoutDist;    /* Max mouse distance for proxy-out max/256 units */
	int wcmCursorProxoutDistDefault; /* Default max mouse distance for proxy-out */

	int bufpos;                        /* position with buffer */
	unsigned char buffer[BUFFER_SIZE]; /* data read from device */

#ifdef LINUX_INPUT
	int wcmLastToolSerial;
	int wcmEventCnt;
	struct input_event wcmEvents[MAX_USB_EVENTS];  /* events for current change */
#endif

	WacomToolPtr wcmTool; /* List of unique tools */
};

#define HANDLE_TILT(comm) ((comm)->wcmFlags & TILT_ENABLED_FLAG)
#define RAW_FILTERING(comm) ((comm)->wcmFlags & RAW_FILTERING_FLAG)
#ifdef LINUX_INPUT
#define USE_SYN_REPORTS(comm) ((comm)->wcmFlags & USE_SYN_REPORTS_FLAG)
#endif

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

/*****************************************************************************
 * XFree86 V4 Inlined Functions and Prototypes
 ****************************************************************************/

#define xf86WcmFlushTablet(fd) xf86FlushInput(fd)
#define xf86WcmWaitForTablet(fd) xf86WaitForInput((fd), 1000000)
#define xf86WcmOpenTablet(local) xf86OpenSerial((local)->options)
#define xf86WcmSetSerialSpeed(fd,rate) xf86SetSerialSpeed((fd),(rate))

#define xf86WcmRead(a,b,c) xf86ReadSerial((a),(b),(c))
#define xf86WcmWrite(a,b,c) xf86WriteSerial((a),(char*)(b),(c))
#define xf86WcmClose(a) xf86CloseSerial((a))

#define XCONFIG_PROBED "(==)"
#define XCONFIG_GIVEN "(**)"
#define xf86Verbose 1
#undef PRIVATE
#define PRIVATE(x) XI_PRIVATE(x)

/*****************************************************************************
 * General Inlined functions and Prototypes
 ****************************************************************************/
/* BIG HAIRY WARNING:
 * Don't overuse SYSCALL(): use it ONLY when you call low-level functions such
 * as ioctl(), read(), write() and such. Otherwise you can easily lock up X11,
 * for example: you pull out the USB tablet, the handle becomes invalid,
 * xf86WcmRead() returns -1 AND errno is left as EINTR from hell knows where.
 * Then you'll loop forever, and even Ctrl+Alt+Backspace doesn't help.
 * xf86WcmReadSerial, WriteSerial, CloseSerial & company already use SYSCALL()
 * internally; there's no need to duplicate it outside the call.
 */
#define SYSCALL(call) while(((call) == -1) && (errno == EINTR))

#define RESET_RELATIVE(ds) do { (ds).relwheel = 0; } while (0)

int xf86WcmWait(int t);
int xf86WcmReady(int fd);

LocalDevicePtr xf86WcmAllocate(char* name, int flag);
LocalDevicePtr xf86WcmAllocateStylus(void);
LocalDevicePtr xf86WcmAllocateCursor(void);
LocalDevicePtr xf86WcmAllocateEraser(void);
LocalDevicePtr xf86WcmAllocatePad(void);

Bool xf86WcmOpen(LocalDevicePtr local);

/* common tablet initialization regime */
int xf86WcmInitTablet(LocalDevicePtr local, WacomModelPtr model, const char* id, float version);

/* standard packet handler */
void xf86WcmReadPacket(LocalDevicePtr local);

/* handles suppression, filtering, and dispatch. */
void xf86WcmEvent(WacomCommonPtr common, unsigned int channel, const WacomDeviceState* ds);

/* dispatches data to XInput event system */
void xf86WcmSendEvents(LocalDevicePtr local, const WacomDeviceState* ds);

/* generic area check for wcmConfig.c, xf86Wacom.c, and wcmCommon.c */
Bool xf86WcmPointInArea(WacomToolAreaPtr area, int x, int y);
Bool xf86WcmAreaListOverlap(WacomToolAreaPtr area, WacomToolAreaPtr list);

/****************************************************************************/
#endif /* __XF86WACOM_H */
