/*
 * Copyright 1995-2003 by Frederic Lepied, France. <Lepied@XFree86.org>
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
#include <xf86Optrec.h>  /* for XF86OptionPtr */
#include <Xwacom.h>

/* This driver can be compiled for the 4.x API (technically versions
 * beginning at 3.9) or the older API which ended around version 3.3. */

#if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(3,9,0,0,0)
#define XFREE86_V4 1
#define XFREE86_V3 0
#else
#define XFREE86_V3 1
#define XFREE86_V4 0
#endif

/*****************************************************************************
 * Linux Input Support
 ****************************************************************************/

#ifdef LINUX_INPUT
#include <asm/types.h>
#include <linux/input.h>

/* 2.4.5 module support */
#ifndef EV_MSC
#define EV_MSC 0x04
#endif

#ifndef MSC_SERIAL
#define MSC_SERIAL 0x00
#endif

/* max number of input events to read in one read call */
#define MAX_EVENTS 50

/* keithp - a hack to avoid redefinitions of these in xf86str.h */
#ifdef BUS_PCI
#undef BUS_PCI
#endif
#ifdef BUS_ISA
#undef BUS_ISA
#endif

#endif /* LINUX_INPUT */

/*****************************************************************************
 * XFree86 V4.x Headers
 ****************************************************************************/

#if XFREE86_V4
#ifndef XFree86LOADER
#include <unistd.h>
#include <errno.h>
#endif

#include <misc.h>
#include <xf86.h>
#define NEED_XF86_TYPES
#if !defined(DGUX)
#include <xf86_ansic.h>
#include <xisb.h>
#endif
#include <xf86_OSproc.h>
#include <xf86Xinput.h>
#include <exevents.h>           /* Needed for InitValuator/Proximity stuff */
#include <keysym.h>
#include <mipointer.h>

#ifdef XFree86LOADER
#include <xf86Module.h>
#endif

/*****************************************************************************
 * XFree86 V3.x Headers
 ****************************************************************************/

#elif XFREE86_V3

#include <Xos.h>
#include <signal.h>
#include <stdio.h>

#define NEED_EVENTS
#include <X.h>
#include <Xproto.h>
#include <misc.h>
#include <inputstr.h>
#include <scrnintstr.h>
#include <XI.h>
#include <XIproto.h>
#include <keysym.h>

#if defined(sun) && !defined(i386)
#define POSIX_TTY
#include <errno.h>
#include <termio.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>
#include <extio.h>

#else /* not sun or i386 */

#include <compiler.h>
#include <xf86.h>
#include <xf86Procs.h>
#include <xf86_OSlib.h>
#include <xf86_Config.h>
#include <xf86Xinput.h>
#include <atKeynames.h>
#include <xf86Version.h>
#include <osdep.h>
#include <exevents.h>
#include <extnsionst.h>
#include <extinit.h>
#endif /* not sun or i386 */

#endif /* XFREE86_V3 */

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
#define DEFAULT_SUPPRESS 2      /* default suppress */
#define MAX_SUPPRESS 6          /* max value of suppress */
#define BUFFER_SIZE 256         /* size of reception buffer */
#define XI_STYLUS "STYLUS"      /* X device name for the stylus */
#define XI_CURSOR "CURSOR"      /* X device name for the cursor */
#define XI_ERASER "ERASER"      /* X device name for the eraser */
#define MAX_VALUE 100           /* number of positions */
#define MAXTRY 3                /* max number of try to receive magic number */
#define MAX_COORD_RES 1270.0    /* Resolution of the returned MaxX and MaxY */

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

/******************************************************************************
 * WacomModule - all globals are packed in a single structure to keep the
 *               global namespaces as clean as possible.
 *****************************************************************************/

#if XFREE86_V4
struct _WacomModule4
{
	InputDriverPtr wcmDrv;
};
#endif

#if XFREE86_V3
struct _WacomModule3
{
	int __unused;
};
#endif


struct _WacomModule
{
	int debugLevel;
	KeySym* keymap;
	const char* identification;

	#if XFREE86_V4
	WacomModule4 v4;
	#elif XFREE86_V3
	WacomModule3 v3;
	#endif

	int (*DevOpen)(DeviceIntPtr pWcm);
	void (*DevReadInput)(LocalDevicePtr local);
	void (*DevControlProc)(DeviceIntPtr device, PtrCtrl* ctrl);
	void (*DevClose)(LocalDevicePtr local);
	int (*DevProc)(DeviceIntPtr pWcm, int what);
	int (*DevChangeControl)(LocalDevicePtr local, xDeviceCtl* control);
	int (*DevSwitchMode)(ClientPtr client, DeviceIntPtr dev, int mode);
	Bool (*DevConvert)(LocalDevicePtr local, int first, int num,
		int v0, int v1, int v2, int v3, int v4, int v5,
		int* x, int* y);
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

	void (*Initialize)(WacomCommonPtr common, int fd, const char* id, float version);
	void (*GetResolution)(WacomCommonPtr common, int fd);
	int (*GetRanges)(WacomCommonPtr common, int fd);
	int (*Reset)(WacomCommonPtr common, int fd);
	int (*EnableTilt)(WacomCommonPtr common, int fd);
	int (*EnableSuppress)(WacomCommonPtr common, int fd);
	int (*SetLinkSpeed)(WacomCommonPtr common, int fd);
	int (*Start)(WacomCommonPtr common, int fd);
	int (*Parse)(WacomCommonPtr common, const unsigned char* data);
	int (*FilterRaw)(WacomCommonPtr common, WacomChannelPtr pChannel,
		WacomDeviceStatePtr ds);
};

/******************************************************************************
 * WacomDeviceRec
 *****************************************************************************/

#define DEVICE_ID(flags) ((flags) & 0x07)

#define STYLUS_ID               1
#define CURSOR_ID               2
#define ERASER_ID               4
#define ABSOLUTE_FLAG           8
#define KEEP_SHAPE_FLAG         16
#define BAUD_19200_FLAG         32
#define BETA_FLAG               64
#define BUTTONS_ONLY_FLAG       128

#define IsCursor(priv) (DEVICE_ID((priv)->flags) == CURSOR_ID)
#define IsStylus(priv) (DEVICE_ID((priv)->flags) == STYLUS_ID)
#define IsEraser(priv) (DEVICE_ID((priv)->flags) == ERASER_ID)

typedef int (*FILTERFUNC)(WacomDevicePtr pDev, WacomDeviceStatePtr pState);

/* FILTERFUNC return values:
 *   -1 - data should be discarded
 *    0 - data is valid */

#define FILTER_PRESSURE_RES 2048        /* maximum points in pressure curve */

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
	int initNumber;         /* magic number for the init phasis */
	int screen_no;          /* associated screen */
 	int button[16];         /* buttons */
   
	WacomCommonPtr common;  /* common info pointer */
    
	/* state fields */
	int oldX;               /* previous X position */
	int oldY;               /* previous Y position */
	int oldZ;               /* previous pressure */
	int oldTiltX;           /* previous tilt in x direction */
	int oldTiltY;           /* previous tilt in y direction */    
	int oldWheel;           /* previous wheel value */    
	int oldRot;             /* previous rotation value */
	int oldThrottle;        /* previous throttle value */
	int oldButtons;         /* previous buttons state */
	int oldProximity;       /* previous proximity */
	double speed;           /* relative mode acceleration */
	int numScreen;          /* number of configured screens */
	int currentScreen;      /* current screen in display */
	int XineramaEnabled;

	/* JEJ - throttle */
	int throttleStart;      /* time in ticks for last wheel movement */
	int throttleLimit;      /* time in ticks for next wheel movement */
	int throttleValue;      /* current throttle value */

	/* JEJ - filters */
	int* pPressCurve;               /* pressure curve */
	int nPressCtrl[4];              /* control points for curve */
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
	int device_id;
	int device_type;
	unsigned int serial_num;
	int x;
	int y;
	int buttons;
	int pressure;
	int tiltx;
	int tilty;
	int rotation;
	int abswheel;
	int relwheel;
	int throttle;
	int discard_first;
	int proximity;
	int sample;	/* wraps every 24 days */
};

struct _WacomFilterState
{
	int tilt[3];
	int coord[3];
	int state;
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
	LocalDevicePtr pDev;    /* device associated with this channel */
	WacomFilterState filter_x, filter_y;
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

#define DEVICE_ISDV4 0x000C

#define ROTATE_NONE 0
#define ROTATE_CW 1
#define ROTATE_CCW 2

#define MAX_CHANNELS 2
#define MAX_USB_EVENTS 32

struct _WacomCommonRec 
{
	char* wcmDevice;             /* device file name */
	int wcmSuppress;             /* transmit position on delta > supress */
	unsigned char wcmFlags;      /* various flags (handle tilt) */

	/* These values are in tablet coordinates */
	int wcmMaxX;                 /* tablet max X value */
	int wcmMaxY;                 /* tablet max Y value */
	int wcmMaxZ;                 /* tablet max Z value */
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

	LocalDevicePtr* wcmDevices;  /* array of devices sharing same port */
	int wcmNumDevices;           /* number of devices */
	int wcmPktLength;            /* length of a packet */
	int wcmProtocolLevel;        /* 4 for Wacom IV, 5 for Wacom V */
	float wcmVersion;            /* ROM version */
	int wcmForceDevice;          /* force device type (used by ISD V4) */
	int wcmRotate;               /* rotate screen (for TabletPC) */
	int wcmThreshold;            /* Threshold for button pressure */
	int wcmChannelCnt;           /* number of channels available */
	WacomChannel wcmChannel[MAX_CHANNELS]; /* channel device state */
	int wcmInitNumber;           /* magic number for the init phasis */
	unsigned int wcmLinkSpeed;   /* serial link speed */

	WacomDeviceClassPtr wcmDevCls; /* device class functions */
	WacomModelPtr wcmModel;        /* model-specific functions */
	int wcmGimp;                 /* support Gimp on Xinerama Enabled multi-monitor desktop */

	int bufpos;                        /* position with buffer */
	unsigned char buffer[BUFFER_SIZE]; /* data read from device */

#if LINUX_INPUT
	int wcmEventCnt;
	struct input_event wcmEvents[MAX_USB_EVENTS];  /* events for current change */
#endif
};

#define HANDLE_TILT(comm) ((comm)->wcmFlags & TILT_ENABLED_FLAG)
#define RAW_FILTERING(comm) ((comm)->wcmFlags & RAW_FILTERING_FLAG)

/*****************************************************************************
 * XFree86 V4 Inlined Functions and Prototypes
 ****************************************************************************/

#if XFREE86_V4

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
 * XFree86 V3 Inlined Functions and Prototypes
 ****************************************************************************/

#elif XFREE86_V3

extern int atoi(const char*);
int xf86WcmFlushTablet(int fd);
int xf86WcmWaitForTablet(int fd);
int xf86WcmOpenTablet(LocalDevicePtr local);
int xf86WcmSetSerialSpeed(int fd, int rate);

#define xf86WcmRead(a,b,c) read((a),(b),(c))
#define xf86WcmWrite(a,b,c) write((a),(b),(c))
#define xf86WcmClose(a) close(a)

#endif

/*****************************************************************************
 * General Inlined functions and Prototypes
 ****************************************************************************/

#define SYSCALL(call) while(((call) == -1) && (errno == EINTR))
#define RESET_RELATIVE(ds) do { (ds).relwheel = 0; } while (0)

int xf86WcmWait(int t);
int xf86WcmReady(int fd);

LocalDevicePtr xf86WcmAllocate(char* name, int flag);
LocalDevicePtr xf86WcmAllocateStylus(void);
LocalDevicePtr xf86WcmAllocateCursor(void);
LocalDevicePtr xf86WcmAllocateEraser(void);

Bool xf86WcmOpen(LocalDevicePtr local);

	int xf86WcmInitTablet(WacomCommonPtr common, WacomModelPtr model,
		int fd, const char* id, float version);
	/* common tablet initialization regime */

	void xf86WcmReadPacket(LocalDevicePtr local);
	/* standard packet handler */

	void xf86WcmEvent(WacomCommonPtr common,
		unsigned int channel, const WacomDeviceState* ds);
	/* handles suppression, filtering, and dispatch. */

	void xf86WcmSendEvents(LocalDevicePtr local,
		const WacomDeviceState* ds);
	/* dispatches data to XInput event system */

/****************************************************************************/
#endif /* __XF86_XF86WACOM_H */
