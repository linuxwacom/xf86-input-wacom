/* $XConsortium: xf86Wacom.c /main/20 1996/10/27 11:05:20 kaleb $ */
/*
 * Copyright 1995-2002 by Frederic Lepied, France. <Lepied@XFree86.org>
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

/* $XFree86: xc/programs/Xserver/hw/xfree86/input/wacom/xf86Wacom.c,v 1.26 2001/04/01 14:00:13 tsi Exp $ */

/*
 * This driver is only able to handle the Wacom IV and Wacom V protocols.
 *
 * Wacom V protocol work done by Raph Levien <raph@gtk.org> and
 * Frédéric Lepied <lepied@xfree86.org>.
 *
 * Many thanks to Dave Fleck from Wacom for the help provided to
 * build this driver.
 *
 * Modified for Linux USB by MATSUMURA Namihiko,
 * Daniel Egger, Germany. <egger@suse.de>,
 * Frederic Lepied <lepied@xfree86.org>,
 * Brion Vibber <brion@pobox.com>,
 * Aaron Optimizer Digulla <digulla@hepe.com>,
 * Jonathan Layes <jonathan@layes.com>.
 *
 */

/*
 * TO XFREE86 THE MAINTAINERS: don't remove the 3.3 code as I continue to maintain it.
 * Discuss with me before changing things in this driver!
 *
 *  Fred
 */

/*
 * REVISION HISTORY
 *
 * 2002-12-17 26-j0.3.3 - added Intuos2
 * 2002-12-17 26-j0.3.5 - added module loading for usb wacom and evdev
 */

static const char identification[] = "$Identification: 26-j0.3.5 $";

#include "xf86Version.h"

#if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(3,9,0,0,0)
#define XFREE86_V4 1
#endif

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
#endif

#ifdef XFREE86_V4
/* post 3.9 headers */

#ifndef XFree86LOADER
#include <unistd.h>
#include <errno.h>
#endif

#include "misc.h"
#include "xf86.h"
#define NEED_XF86_TYPES
#if !defined(DGUX)
#include "xf86_ansic.h"
#include "xisb.h"
#endif
#include "xf86_OSproc.h"
#include "xf86Xinput.h"
#include "exevents.h"		/* Needed for InitValuator/Proximity stuff */
#include "keysym.h"
#include "mipointer.h"

#ifdef XFree86LOADER
#include "xf86Module.h"
#endif

#undef sleep
#define sleep(t) xf86WaitForInput(-1, 1000 * (t))
#define wait_for_fd(fd) xf86WaitForInput((fd), 1000000)
#define tcflush(fd, n) xf86FlushInput((fd))
#undef read
#define read(a,b,c) xf86ReadSerial((a),(b),(c))
#undef write
#define write(a,b,c) xf86WriteSerial((a),(char*)(b),(c))
#undef close
#define close(a) xf86CloseSerial((a))
#define XCONFIG_PROBED "(==)"
#define XCONFIG_GIVEN "(**)"
#define xf86Verbose 1
#undef PRIVATE
#define PRIVATE(x) XI_PRIVATE(x)

/* 
 * Be sure to set vmin appropriately for your device's protocol. You want to
 * read a full packet before returning
 */
static const char *default_options[] =
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

static InputDriverPtr wcmDrv;

#else /* pre 3.9 headers */

#include "Xos.h"
#include <signal.h>
#include <stdio.h>

#define NEED_EVENTS
#include "X.h"
#include "Xproto.h"
#include "misc.h"
#include "inputstr.h"
#include "scrnintstr.h"
#include "XI.h"
#include "XIproto.h"
#include "keysym.h"

#if defined(sun) && !defined(i386)
#define POSIX_TTY
#include <errno.h>
#include <termio.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>

#include "extio.h"
#else
#include "compiler.h"

#include "xf86.h"
#include "xf86Procs.h"
#include "xf86_OSlib.h"
#include "xf86_Config.h"
#include "xf86Xinput.h"
#include "atKeynames.h"
#include "xf86Version.h"
#endif

#if !defined(sun) || defined(i386)
#include "osdep.h"
#include "exevents.h"

#include "extnsionst.h"
#include "extinit.h"
#endif

#endif /* Pre 3.9 headers */

#if defined(__QNX__) || defined(__QNXNTO__)
#define POSIX_TTY
#endif

/******************************************************************************
 * debugging macro
 *****************************************************************************/
#ifdef DBG
#undef DBG
#endif
#ifdef DEBUG
#undef DEBUG
#endif

static int      debug_level = 0;
#define DEBUG 1
#if DEBUG
#define DBG(lvl, f) {if ((lvl) <= debug_level) f;}
#else
#define DBG(lvl, f)
#endif

#define ABS(x) ((x) > 0 ? (x) : -(x))

/******************************************************************************
 * WacomDeviceRec flags
 *****************************************************************************/
#define DEVICE_ID(flags) ((flags) & 0x07)

#define STYLUS_ID		1
#define CURSOR_ID		2
#define ERASER_ID		4
#define ABSOLUTE_FLAG		8
#define FIRST_TOUCH_FLAG	16
#define	KEEP_SHAPE_FLAG		32
#define BAUD_19200_FLAG		64
#define BETA_FLAG		    128

/******************************************************************************
 * WacomCommonRec flags
 *****************************************************************************/
#define TILT_FLAG	1
#define GRAPHIRE_FLAG	2

typedef struct
{
    int		state;
    int		coord[3];
    int		tilt[3];
} WacomFilterState;

typedef struct
{
    int			device_id;
    int			device_type;
    unsigned int	serial_num;
    int			x;
    int			y;
    int			buttons;
    int			pressure;
    int			tiltx;
    int			tilty;
    int			rotation;
    int			wheel;
    int			discard_first;
    int			proximity;
    WacomFilterState	x_filter;
    WacomFilterState	y_filter;
} WacomDeviceState;

#define PEN(ds)         (((ds->device_id) & 0x07ff) == 0x0022 || ((ds->device_id) & 0x07ff) == 0x0052)
#define STROKING_PEN(ds) (((ds->device_id) & 0x07ff) == 0x0032)
#define AIRBRUSH(ds)    (((ds->device_id) & 0x07ff) == 0x0112)
#define MOUSE_4D(ds)    (((ds->device_id) & 0x07ff) == 0x0094)
#define LENS_CURSOR(ds) (((ds->device_id) & 0x07ff) == 0x0096)
#define INKING_PEN(ds)  (((ds->device_id) & 0x07ff) == 0x0012)

typedef struct
{
    /* configuration fields */
    unsigned int	flags;		/* various flags (device type, absolute, first touch...) */
    int			topX;		/* X top */
    int			topY;		/* Y top */
    int			bottomX;	/* X bottom */
    int			bottomY;	/* Y bottom */
    double		factorX;	/* X factor */
    double		factorY;	/* Y factor */
    unsigned int	serial;	        /* device serial number */
    int			initNumber;     /* magic number for the init phasis */
    int			screen_no;	/* associated screen */
    
    struct _WacomCommonRec *common;	/* common info pointer */
    
    /* state fields */
    int			oldX;		/* previous X position */
    int			oldY;		/* previous Y position */
    int			oldZ;		/* previous pressure */
    int			oldTiltX;	/* previous tilt in x direction */
    int			oldTiltY;	/* previous tilt in y direction */    
    int			oldWheel;	/* previous wheel value */    
    int			oldButtons;	/* previous buttons state */
    int			oldProximity;	/* previous proximity */

	/* JEJ - throttle */
	int			throttleStart;	/* time in ticks for last wheel movement */
	int			throttleLimit;	/* time in ticks for next wheel movement */
	int			throttleValue;	/* current throttle value */

} WacomDeviceRec, *WacomDevicePtr;

#define MAX_USB_EVENTS 6

typedef struct _WacomCommonRec 
{
    char		*wcmDevice;	/* device file name */
    int			wcmSuppress;	/* transmit position if increment is superior */
    unsigned char	wcmFlags;	/* various flags (handle tilt) */
    int			wcmMaxX;	/* max X value */
    int			wcmMaxY;	/* max Y value */
    int			wcmMaxZ;	/* max Z value */
    int			wcmResolX;	/* X resolution in points/inch */
    int			wcmResolY;	/* Y resolution in points/inch */
    int			wcmResolZ;	/* Z resolution in points/inch */
    LocalDevicePtr	*wcmDevices;	/* array of all devices sharing the same port */
    int			wcmNumDevices;	/* number of devices */
    int			wcmIndex;	/* number of bytes read */
    int			wcmPktLength;	/* length of a packet */
    unsigned char	wcmData[9];	/* data read on the device */
    Bool		wcmHasEraser;	/* True if an eraser has been configured */
    Bool		wcmStylusSide;	/* eraser or stylus ? */
    Bool		wcmStylusProximity; /* the stylus is in proximity ? */
    int			wcmProtocolLevel; /* 4 for Wacom IV, 5 for Wacom V */
    int			wcmThreshold;	/* Threshold for counting pressure as a button */
    WacomDeviceState	wcmDevStat[2];	/* device state for each tool */
    int			wcmInitNumber;  /* magic number for the init phasis */
    unsigned int	wcmLinkSpeed;   /* serial link speed */
    Bool		(*wcmOpen)(LocalDevicePtr /*local*/); /* function used to open the line (serial or USB) */
    unsigned int	wcmLastSerial;	/* last device (used by the USB part) */
    int			wcmLastTool;	/* last tool (used by USB part) */
#ifdef LINUX_INPUT
    struct input_event  wcmEvent[MAX_USB_EVENTS]; /* data used by USB driver */
#endif
} WacomCommonRec, *WacomCommonPtr;

/******************************************************************************
 * configuration stuff
 *****************************************************************************/
#define CURSOR_SECTION_NAME "wacomcursor"
#define STYLUS_SECTION_NAME "wacomstylus"
#define ERASER_SECTION_NAME "wacomeraser"

#ifndef XFREE86_V4

#define PORT		1
#define DEVICENAME	2
#define THE_MODE	3
#define SUPPRESS	4
#define DEBUG_LEVEL     5
#define TILT_MODE	6
#define HISTORY_SIZE	7
#define ALWAYS_CORE	8
#define	KEEP_SHAPE	9
#define	TOP_X		10
#define	TOP_Y		11
#define	BOTTOM_X	12
#define	BOTTOM_Y	13
#define	SERIAL		14
#define	BAUD_RATE	15
#define	THRESHOLD	16
#define MAX_X		17
#define MAX_Y		18
#define MAX_Z		19
#define RESOLUTION_X	20
#define RESOLUTION_Y	21
#define RESOLUTION_Z	22
#define USB		23
#define SCREEN_NO	24

#if !defined(sun) || defined(i386)
static SymTabRec WcmTab[] = {
  { ENDSUBSECTION,	"endsubsection" },
  { PORT,		"port" },
  { DEVICENAME,		"devicename" },
  { THE_MODE,		"mode" },
  { SUPPRESS,		"suppress" },
  { DEBUG_LEVEL,	"debuglevel" },
  { TILT_MODE,		"tiltmode" },
  { HISTORY_SIZE,	"historysize" },
  { ALWAYS_CORE,	"alwayscore" },
  { KEEP_SHAPE,		"keepshape" },
  { TOP_X,		"topx" },
  { TOP_Y,		"topy" },
  { BOTTOM_X,		"bottomx" },
  { BOTTOM_Y,		"bottomy" },
  { SERIAL,		"serial" },
  { BAUD_RATE,		"baudrate" },
  { THRESHOLD,		"threshold" },
  { MAX_X,		"maxx" },
  { MAX_Y,		"maxy" },
  { MAX_Z,		"maxz" },
  { RESOLUTION_X,	"resolutionx" },
  { RESOLUTION_Y,	"resolutiony" },
  { RESOLUTION_Z,	"resolutionz" },
  { USB,		"usb" },
  { SCREEN_NO,		"screenno" },
  { -1,			"" }
};

#define RELATIVE	1
#define ABSOLUTE	2

static SymTabRec ModeTabRec[] = {
  { RELATIVE,	"relative" },
  { ABSOLUTE,	"absolute" },
  { -1,		"" }
};

#endif

#endif /* Pre 3.9 headers */

/******************************************************************************
 * constant and macros declarations
 *****************************************************************************/
#define DEFAULT_MAXZ 240	/* default value of MaxZ when nothing is configured */
#define BUFFER_SIZE 256		/* size of reception buffer */
#define XI_STYLUS "STYLUS"	/* X device name for the stylus */
#define XI_CURSOR "CURSOR"	/* X device name for the cursor */
#define XI_ERASER "ERASER"	/* X device name for the eraser */
#define MAX_VALUE 100           /* number of positions */
#define MAXTRY 3                /* max number of try to receive magic number */
#define MAX_COORD_RES 1270.0	/* Resolution of the returned MaxX and MaxY */
#define INVALID_THRESHOLD 30000 /* Invalid threshold value used to test if the user
				 * configured it or not */

#define SYSCALL(call) while(((call) == -1) && (errno == EINTR))

#define WC_RESET	"\r#" /* reset to wacom IV command set or wacom V reset */
#define WC_RESET_BAUD	"\r$" /* reset baud rate to default (wacom V) or switch to wacom IIs (wacom IV) */
#define WC_CONFIG	"~R\r"	/* request a configuration string */
#define WC_COORD	"~C\r"	/* request max coordinates */
#define WC_MODEL	"~#\r"	/* request model and ROM version */

#define WC_MULTI	"MU1\r"	/* multi mode input */
#define WC_UPPER_ORIGIN	"OC1\r"	/* origin in upper left */
#define WC_SUPPRESS	"SU"	/* suppress mode */
#define WC_ALL_MACRO	"~M0\r"	/* enable all macro buttons */
#define WC_NO_MACRO1	"~M1\r"	/* disable macro buttons of group 1 */
#define WC_RATE 	"IT0\r"	/* max transmit rate (unit of 5 ms) */
#define WC_TILT_MODE	"FM1\r"	/* enable extra protocol for tilt management */
#define WC_NO_INCREMENT	"IN0\r"	/* do not enable increment mode */
#define WC_STREAM_MODE	"SR\r"	/* enable continuous mode */
#define WC_PRESSURE_MODE "PH1\r" /* enable pressure mode */
#define WC_ZFILTER	"ZF1\r" /* stop sending coordinates */
#define WC_STOP		"\nSP\r" /* stop sending coordinates */
#define WC_START	"ST\r"	/* start sending coordinates */
#define WC_NEW_RESOLUTION "NR"	/* change the resolution */

static const char * setup_string = WC_MULTI WC_UPPER_ORIGIN
 WC_ALL_MACRO WC_NO_MACRO1 WC_RATE WC_NO_INCREMENT WC_STREAM_MODE WC_ZFILTER;

static const char * penpartner_setup_string = WC_PRESSURE_MODE WC_START;

#define WC_V_SINGLE	"MT0\r"
#define WC_V_MULTI	"MT1\r"
#define WC_V_ID		"ID1\r"
#define WC_V_19200	"BA19\r"
#define WC_V_38400	"BA38\r"
/*  #define WC_V_9600	"BA96\r" */
#define WC_V_9600	"$\r"

#define WC_RESET_19200	"\r$"	/* reset to 9600 baud */
#define WC_RESET_19200_IV "\r#"

static const char * intuos_setup_string = WC_V_MULTI WC_V_ID WC_RATE WC_START;

#define COMMAND_SET_MASK	0xc0
#define BAUD_RATE_MASK		0x0a
#define PARITY_MASK		0x30
#define DATA_LENGTH_MASK	0x40
#define STOP_BIT_MASK		0x80

#define HEADER_BIT	0x80
#define ZAXIS_SIGN_BIT	0x40
#define ZAXIS_BIT    	0x04
#define ZAXIS_BITS    	0x3f
#define POINTER_BIT     0x20
#define PROXIMITY_BIT   0x40
#define BUTTON_FLAG	0x08
#define BUTTONS_BITS	0x78
#define TILT_SIGN_BIT	0x40
#define TILT_BITS	0x3f

/* defines to discriminate second side button and the eraser */
#define ERASER_PROX	4
#define OTHER_PROX	1

#define HANDLE_TILT(comm) ((comm)->wcmPktLength == 9)

#define mils(res) (res * 100 / 2.54) /* resolution */

/******************************************************************************
 * Function/Macro keys variables
 *****************************************************************************/
static KeySym wacom_map[] = 
{
    NoSymbol,	/* 0x00 */
    NoSymbol,	/* 0x01 */
    NoSymbol,	/* 0x02 */
    NoSymbol,	/* 0x03 */
    NoSymbol,	/* 0x04 */
    NoSymbol,	/* 0x05 */
    NoSymbol,	/* 0x06 */
    NoSymbol,	/* 0x07 */
    XK_F1,	/* 0x08 */
    XK_F2,	/* 0x09 */
    XK_F3,	/* 0x0a */
    XK_F4,	/* 0x0b */
    XK_F5,	/* 0x0c */
    XK_F6,	/* 0x0d */
    XK_F7,	/* 0x0e */
    XK_F8,	/* 0x0f */
    XK_F8,	/* 0x10 */
    XK_F10,	/* 0x11 */
    XK_F11,	/* 0x12 */
    XK_F12,	/* 0x13 */
    XK_F13,	/* 0x14 */
    XK_F14,	/* 0x15 */
    XK_F15,	/* 0x16 */
    XK_F16,	/* 0x17 */
    XK_F17,	/* 0x18 */
    XK_F18,	/* 0x19 */
    XK_F19,	/* 0x1a */
    XK_F20,	/* 0x1b */
    XK_F21,	/* 0x1c */
    XK_F22,	/* 0x1d */
    XK_F23,	/* 0x1e */
    XK_F24,	/* 0x1f */
    XK_F25,	/* 0x20 */
    XK_F26,	/* 0x21 */
    XK_F27,	/* 0x22 */
    XK_F28,	/* 0x23 */
    XK_F29,	/* 0x24 */
    XK_F30,	/* 0x25 */
    XK_F31,	/* 0x26 */
    XK_F32	/* 0x27 */
};

/* minKeyCode = 8 because this is the min legal key code */
static KeySymsRec wacom_keysyms = {
  /* map	minKeyCode	maxKC	width */
  wacom_map,	8,		0x27,	1
};

/******************************************************************************
 * external declarations
 *****************************************************************************/

#ifdef LINUX_INPUT
static void xf86WcmReadUSBInput(LocalDevicePtr);
static Bool xf86WcmUSBOpen(LocalDevicePtr);
#endif


#ifndef XFREE86_V4

#if defined(sun) && !defined(i386)
#define ENQUEUE suneqEnqueue
#else
#define ENQUEUE xf86eqEnqueue

extern void xf86eqEnqueue(
#if NeedFunctionPrototypes
    xEventPtr /*e*/
#endif
);
#endif

extern void miPointerDeltaCursor(
#if NeedFunctionPrototypes
    int /*dx*/,
    int /*dy*/,
    unsigned long /*time*/
#endif
);

#endif /* pre 3.9 declarations */

#if NeedFunctionPrototypes
static LocalDevicePtr xf86WcmAllocateStylus(void);
static LocalDevicePtr xf86WcmAllocateCursor(void);
static LocalDevicePtr xf86WcmAllocateEraser(void);
#endif

#ifndef XFREE86_V4
/*
 ***************************************************************************
 *
 * xf86WcmConfig --
 *	Configure the device.
 *
 ***************************************************************************
 */
static Bool
xf86WcmConfig(LocalDevicePtr    *array,
              int               inx,
              int               max,
	      LexPtr            val)
{
    LocalDevicePtr      dev = array[inx];
    WacomDevicePtr	priv = (WacomDevicePtr)(dev->private);
    WacomCommonPtr	common = priv->common;
    int			token;
    int			mtoken;
    
    DBG(1, ErrorF("xf86WcmConfig\n"));

    if (xf86GetToken(WcmTab) != PORT) {
	xf86ConfigError("PORT option must be the first option of a Wacom SubSection");
    }
    
    if (xf86GetToken(NULL) != STRING)
	xf86ConfigError("Option string expected");
    else {
	int     loop;
		
	/* try to find another wacom device which share the same port */
	for(loop=0; loop<max; loop++) {
	    if (loop == inx)
		continue;
	    if ((array[loop]->device_config == xf86WcmConfig) &&
		(strcmp(((WacomDevicePtr)array[loop]->private)->common->wcmDevice, val->str) == 0)) {
		DBG(2, ErrorF("xf86WcmConfig wacom port share between"
			      " %s and %s\n",
			      dev->name, array[loop]->name));
		((WacomDevicePtr) array[loop]->private)->common->wcmHasEraser |= common->wcmHasEraser;
		xfree(common->wcmDevices);
		xfree(common);
		common = priv->common = ((WacomDevicePtr) array[loop]->private)->common;
		common->wcmNumDevices++;
		common->wcmDevices = (LocalDevicePtr *) xrealloc(common->wcmDevices,
								 sizeof(LocalDevicePtr) * common->wcmNumDevices);
		common->wcmDevices[common->wcmNumDevices - 1] = dev;
		break;
	    }
	}
	if (loop == max) {
	    common->wcmDevice = strdup(val->str);
	    if (xf86Verbose)
		ErrorF("%s Wacom port is %s\n", XCONFIG_GIVEN,
		       common->wcmDevice);
	}
    }

    while ((token = xf86GetToken(WcmTab)) != ENDSUBSECTION) {
	switch(token) {
	case DEVICENAME:
	    if (xf86GetToken(NULL) != STRING)
		xf86ConfigError("Option string expected");
	    dev->name = strdup(val->str);
	    if (xf86Verbose)
		ErrorF("%s Wacom X device name is %s\n", XCONFIG_GIVEN,
		       dev->name);
	    break;	    
	    
	case THE_MODE:
	    mtoken = xf86GetToken(ModeTabRec);
	    if ((mtoken == EOF) || (mtoken == STRING) || (mtoken == NUMBER)) 
		xf86ConfigError("Mode type token expected");
	    else {
		switch (mtoken) {
		case ABSOLUTE:
		    priv->flags = priv->flags | ABSOLUTE_FLAG;
		    break;
		case RELATIVE:
		    priv->flags = priv->flags & ~ABSOLUTE_FLAG; 
		    break;
		default:
		    xf86ConfigError("Illegal Mode type");
		    break;
		}
	    }
	    break;
	    
	case SUPPRESS:
	    if (xf86GetToken(NULL) != NUMBER)
		xf86ConfigError("Option number expected");
	    common->wcmSuppress = val->num;
	    if (xf86Verbose)
		ErrorF("%s Wacom suppress value is %d\n", XCONFIG_GIVEN,
		       common->wcmSuppress);      
	    break;
	    
	case DEBUG_LEVEL:
	    if (xf86GetToken(NULL) != NUMBER)
		xf86ConfigError("Option number expected");
	    debug_level = val->num;
	    if (xf86Verbose) {
#if DEBUG
		ErrorF("%s Wacom debug level sets to %d\n", XCONFIG_GIVEN,
		       debug_level);      
#else
		ErrorF("%s Wacom debug level not sets to %d because"
		       " debugging is not compiled\n", XCONFIG_GIVEN,
		       debug_level);      
#endif
	    }
	    break;

	case TILT_MODE:
	    common->wcmFlags |= TILT_FLAG;
	    break;
	    
	case HISTORY_SIZE:
	    if (xf86GetToken(NULL) != NUMBER)
		xf86ConfigError("Option number expected");
	    dev->history_size = val->num;
	    if (xf86Verbose)
		ErrorF("%s Wacom Motion history size is %d\n", XCONFIG_GIVEN,
		       dev->history_size);      
	    break;

	case ALWAYS_CORE:
	    xf86AlwaysCore(dev, TRUE);
	    if (xf86Verbose)
		ErrorF("%s Wacom device always stays core pointer\n",
		       XCONFIG_GIVEN);
	    break;

	case KEEP_SHAPE:
	    priv->flags |= KEEP_SHAPE_FLAG;
	    if (xf86Verbose)
		ErrorF("%s Wacom keeps shape\n",
		       XCONFIG_GIVEN);
	    break;

	case TOP_X:
	    if (xf86GetToken(NULL) != NUMBER)
		xf86ConfigError("Option number expected");
	    priv->topX = val->num;
	    if (xf86Verbose)
		ErrorF("%s Wacom top x = %d\n", XCONFIG_GIVEN, priv->topX);
	    break;

	case TOP_Y:
	    if (xf86GetToken(NULL) != NUMBER)
		xf86ConfigError("Option number expected");
	    priv->topY = val->num;
	    if (xf86Verbose)
		ErrorF("%s Wacom top y = %d\n", XCONFIG_GIVEN, priv->topY);
	    break;

	case BOTTOM_X:
	    if (xf86GetToken(NULL) != NUMBER)
		xf86ConfigError("Option number expected");
	    priv->bottomX = val->num;
	    if (xf86Verbose)
		ErrorF("%s Wacom bottom x = %d\n", XCONFIG_GIVEN, priv->bottomX);
	    break;

	case BOTTOM_Y:
	    if (xf86GetToken(NULL) != NUMBER)
		xf86ConfigError("Option number expected");
	    priv->bottomY = val->num;
	    if (xf86Verbose)
		ErrorF("%s Wacom bottom y = %d\n", XCONFIG_GIVEN, priv->bottomY);
	    break;

	case SERIAL:
	    if (xf86GetToken(NULL) != NUMBER)
		xf86ConfigError("Option number expected");
	    priv->serial = val->num;
	    if (xf86Verbose)
		ErrorF("%s Wacom serial number = %u\n", XCONFIG_GIVEN,
		       priv->serial);
	    break;
	    
	case BAUD_RATE:
	    if (xf86GetToken(NULL) != NUMBER)
		xf86ConfigError("Option number expected");
	    switch(val->num) {
		case 38400:
		    common->wcmLinkSpeed = 38400;
		    break;
		case 19200:
		    common->wcmLinkSpeed = 19200;
		    break;
		case 9600:
		    common->wcmLinkSpeed = 9600;
		    break;
		default:
		    xf86ConfigError("Illegal speed value");
		    break;
	    }
	    if (xf86Verbose)
		ErrorF("%s Wacom baud rate of %u\n", XCONFIG_GIVEN,
		       val->num);
	    break;
	    
	case THRESHOLD:
	    if (xf86GetToken(NULL) != STRING)
		xf86ConfigError("Option string expected");

	    common->wcmThreshold = atoi(val->str);
	    
	    if (xf86Verbose)
		ErrorF("%s Wacom pressure threshold for button 1 = %d\n",
		       XCONFIG_GIVEN, common->wcmThreshold);
	    break;

	case MAX_X:
	    if (xf86GetToken(NULL) != NUMBER)
		xf86ConfigError("Option number expected");
	    common->wcmMaxX = val->num;
	    if (xf86Verbose)
		ErrorF("%s Wacom max x = %d\n", XCONFIG_GIVEN, common->wcmMaxX);
	    break;

	case MAX_Y:
	    if (xf86GetToken(NULL) != NUMBER)
		xf86ConfigError("Option number expected");
	    common->wcmMaxY = val->num;
	    if (xf86Verbose)
		ErrorF("%s Wacom max y = %d\n", XCONFIG_GIVEN, common->wcmMaxY);
	    break;

	case MAX_Z:
	    if (xf86GetToken(NULL) != NUMBER)
		xf86ConfigError("Option number expected");
	    common->wcmMaxZ = val->num;
	    if (xf86Verbose)
		ErrorF("%s Wacom max y = %d\n", XCONFIG_GIVEN, common->wcmMaxZ);
	    break;

	case RESOLUTION_X:
	    if (xf86GetToken(NULL) != NUMBER)
		xf86ConfigError("Option number expected");
	    common->wcmResolX = val->num;
	    if (xf86Verbose)
		ErrorF("%s Wacom resolution x = %d\n", XCONFIG_GIVEN, common->wcmResolX);
	    break;

	case RESOLUTION_Y:
	    if (xf86GetToken(NULL) != NUMBER)
		xf86ConfigError("Option number expected");
	    common->wcmResolY = val->num;
	    if (xf86Verbose)
		ErrorF("%s Wacom resolution y = %d\n", XCONFIG_GIVEN, common->wcmResolY);
	    break;

	case RESOLUTION_Z:
	    if (xf86GetToken(NULL) != NUMBER)
		xf86ConfigError("Option number expected");
	    common->wcmResolZ = val->num;
	    if (xf86Verbose)
		ErrorF("%s Wacom resolution y = %d\n", XCONFIG_GIVEN, common->wcmResolZ);
	    break;

	case USB:
#ifdef LINUX_INPUT
	    dev->read_input=xf86WcmReadUSBInput;
	    common->wcmOpen=xf86WcmUSBOpen;
	    ErrorF("%s Wacom reading USB link\n", XCONFIG_GIVEN);
#else
	    ErrorF("The USB version of the driver isn't available for your platform\n");
#endif
	    break;
	    
	case SCREEN_NO:
	    if (xf86GetToken(NULL) != NUMBER)
		xf86ConfigError("Option number expected");
	    priv->screen_no = val->num;
	    if (xf86Verbose)
		ErrorF("%s Wacom attached screen = %d\n", XCONFIG_GIVEN,
		       priv->screen_no);
	    break;

	case EOF:
	    FatalError("Unexpected EOF (missing EndSubSection)");
	    break;
	    
	default:
	    xf86ConfigError("Wacom subsection keyword expected");
	    break;
	}
    }
    
    DBG(1, ErrorF("xf86WcmConfig name=%s\n", common->wcmDevice));
    
    return Success;
}
#endif /* Pre 3.9 stuff */

#if 0
/*
 ***************************************************************************
 *
 * ascii_to_hexa --
 *
 ***************************************************************************
 */
/*
 * transform two ascii hexa representation into an unsigned char
 * most significant byte is the first one
 */
static unsigned char
ascii_to_hexa(char	buf[2])
{
  unsigned char	uc;
  
  if (buf[0] >= 'A') {
    uc = buf[0] - 'A' + 10;
  }
  else {
    uc = buf[0] - '0';
  }
  uc = uc << 4;
  if (buf[1] >= 'A') {
    uc += buf[1] - 'A' + 10;
  }
  else {
    uc += buf[1] - '0';
  }
  return uc;
}
#endif

#ifndef XFREE86_V4
/*
 ***************************************************************************
 *
 * set_serial_speed --
 *
 *	Set speed of the serial port.
 *
 ***************************************************************************
 */
static int
set_serial_speed(int	fd,
		 int	speed_code)
{
    struct termios	termios_tty;
    int			err;
    
#ifdef POSIX_TTY
    SYSCALL(err = tcgetattr(fd, &termios_tty));

    if (err == -1) {
	ErrorF("Wacom tcgetattr error : %s\n", strerror(errno));
	return !Success;
    }
    termios_tty.c_iflag = IXOFF;
    termios_tty.c_oflag = 0;
    termios_tty.c_cflag = speed_code|CS8|CREAD|CLOCAL;
    termios_tty.c_lflag = 0;

    termios_tty.c_cc[VINTR] = 0;
    termios_tty.c_cc[VQUIT] = 0;
    termios_tty.c_cc[VERASE] = 0;
    termios_tty.c_cc[VEOF] = 0;
#ifdef VWERASE
    termios_tty.c_cc[VWERASE] = 0;
#endif
#ifdef VREPRINT
    termios_tty.c_cc[VREPRINT] = 0;
#endif
    termios_tty.c_cc[VKILL] = 0;
    termios_tty.c_cc[VEOF] = 0;
    termios_tty.c_cc[VEOL] = 0;
#ifdef VEOL2
    termios_tty.c_cc[VEOL2] = 0;
#endif
    termios_tty.c_cc[VSUSP] = 0;
#ifdef VDSUSP
    termios_tty.c_cc[VDSUSP] = 0;
#endif
#ifdef VDISCARD
    termios_tty.c_cc[VDISCARD] = 0;
#endif
#ifdef VLNEXT
    termios_tty.c_cc[VLNEXT] = 0; 
#endif
	
    /* minimum 1 character in one read call and timeout to 100 ms */
    termios_tty.c_cc[VMIN] = 1;
    termios_tty.c_cc[VTIME] = 10;

    SYSCALL(err = tcsetattr(fd, TCSANOW, &termios_tty));
    if (err == -1) {
	ErrorF("Wacom tcsetattr TCSANOW error : %s\n", strerror(errno));
	return !Success;
    }

#else
    Code for OSs without POSIX tty functions
#endif

    return Success;
}

/*
 ***************************************************************************
 *
 * wait_for_fd --
 *
 *	Wait one second that the file descriptor becomes readable.
 *
 ***************************************************************************
 */
static int
wait_for_fd(int	fd)
{
    int			err;
    fd_set		readfds;
    struct timeval	timeout;
    
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    SYSCALL(err = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout));

    return err;
}

/*
 ***************************************************************************
 *
 * flush_input_fd --
 *
 *	Flush all input pending on the file descriptor.
 *
 ***************************************************************************
 */
static int
flush_input_fd(int	fd)
{
    int			err;
    fd_set		readfds;
    struct timeval	timeout;
    char		dummy[1];
    
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

    do {
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	SYSCALL(err = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout));

	if (err > 0) {
	    SYSCALL(err = read(fd, &dummy, 1));
	    DBG(10, ErrorF("flush_input_fd: read %d bytes\n", err));
	}
    } while (err > 0);
    return err;
}
#endif /* Pre 3.9 stuff */

/*
 ***************************************************************************
 *
 * send_request --
 *
 ***************************************************************************
 */
/*
 * send a request and wait for the answer.
 * the answer must begin with the first two chars of the request and must end
 * with \r. The last character in the answer string (\r) is replaced by a \0.
 */
static char *
send_request(int	fd,
	     char	*request,
	     char	*answer)
{
    int	len, nr;
    int	maxtry = MAXTRY;
  
    /* send request string */
    do {
	SYSCALL(len = write(fd, request, strlen(request)));
	if ((len == -1) && (errno != EAGAIN)) {
	    ErrorF("Wacom write error : %s", strerror(errno));
	    return NULL;
	}
	maxtry--;
    } while ((len == -1) && maxtry);

    if (maxtry == 0) {
	ErrorF("Wacom unable to write request string '%s' after %d tries\n", request, MAXTRY);
	return NULL;
    }
  
    do {
	maxtry = MAXTRY;
    
	/* Read the first byte of the answer which must be equal to the first
	 * byte of the request.
	 */
	do {    
	    if ((nr = wait_for_fd(fd)) > 0) {
		SYSCALL(nr = read(fd, answer, 1));
		if ((nr == -1) && (errno != EAGAIN)) {
		    ErrorF("Wacom read error : %s\n", strerror(errno));
		    return NULL;
		}
		DBG(10, ErrorF("%c err=%d [0]\n", answer[0], nr));
	    }
	    maxtry--;  
	} while ((answer[0] != request[0]) && maxtry);

	if (maxtry == 0) {
	    ErrorF("Wacom unable to read first byte of request '%c%c' answer after %d tries\n",
		   request[0], request[1], MAXTRY);
	    return NULL;
	}

	/* Read the second byte of the answer which must be equal to the second
	 * byte of the request.
	 */
	do {    
	    maxtry = MAXTRY;
	    do {    
		if ((nr = wait_for_fd(fd)) > 0) {
		    SYSCALL(nr = read(fd, answer+1, 1));
		    if ((nr == -1) && (errno != EAGAIN)) {
			ErrorF("Wacom read error : %s\n", strerror(errno));
			return NULL;
		    }
		    DBG(10, ErrorF("%c err=%d [1]\n", answer[1], nr));
		}
		maxtry--;  
	    } while ((nr <= 0) && maxtry);
      
	    if (maxtry == 0) {
		ErrorF("Wacom unable to read second byte of request '%c%c' answer after %d tries\n",
		       request[0], request[1], MAXTRY);
		return NULL;
	    }

	    if (answer[1] != request[1])
		answer[0] = answer[1];
      
	} while ((answer[0] == request[0]) &&
		 (answer[1] != request[1]));

    } while ((answer[0] != request[0]) &&
	     (answer[1] != request[1]));

    /* Read until carriage return or timeout (to handle broken protocol
     * implementations which don't end with a <cr>).
     */
    len = 2;
    maxtry = MAXTRY;
    do {    
	do {    
	    if ((nr = wait_for_fd(fd)) > 0) {
		SYSCALL(nr = read(fd, answer+len, 1));
		if ((nr == -1) && (errno != EAGAIN)) {
		    ErrorF("Wacom read error : %s\n", strerror(errno));
		    return NULL;
		}
		DBG(10, ErrorF("%c err=%d [%d]\n", answer[len], nr, len));
	    }
	    else {
		DBG(10, ErrorF("timeout remains %d tries\n", maxtry));
		maxtry--;
	    }
	} while ((nr <= 0) && maxtry);

	if (nr > 0) {
	    len += nr;
	}
	
	if (maxtry == 0) {
	    ErrorF("Wacom unable to read last byte of request '%c%c' answer after %d tries\n",
		   request[0], request[1], MAXTRY);
	    break;
	}
    } while (answer[len-1] != '\r');

    if (len <= 3)
	return NULL;
    
    answer[len-1] = '\0';
  
    return answer;
}

/*
 ***************************************************************************
 *
 * xf86WcmConvert --
 *	Convert valuators to X and Y.
 *
 ***************************************************************************
 */
static Bool
xf86WcmConvert(LocalDevicePtr	local,
	       int		first,
	       int		num,
	       int		v0,
	       int		v1,
	       int		v2,
	       int		v3,
	       int		v4,
	       int		v5,
	       int*		x,
	       int*		y)
{
    WacomDevicePtr	priv = (WacomDevicePtr) local->private;
    
    DBG(6, ErrorF("xf86WcmConvert\n"));

    if (first != 0 || num == 1)
      return FALSE;

#ifdef XFREE86_V4
    {
	ScreenPtr	pscr;
	
	if (priv->screen_no != -1) {
	    pscr = screenInfo.screens[priv->screen_no];
	} else {
	    pscr = miPointerCurrentScreen();
	}
    
	if (pscr == NULL)
	    return FALSE;
	
	priv->factorX = ((double) pscr->width)
	    / (priv->bottomX - priv->topX);
	priv->factorY = ((double) pscr->height)
	    / (priv->bottomY - priv->topY);
    }
#endif
    
    *x = v0 * priv->factorX + 0.5;
    *y = v1 * priv->factorY + 0.5;

    DBG(6, ErrorF("Wacom converted v0=%d v1=%d to x=%d y=%d\n",
		  v0, v1, *x, *y));
#ifdef XFREE86_V4
    if (priv->screen_no != -1) {
	xf86XInputSetScreen(local, priv->screen_no, *x, *y);
    }
#endif
    return TRUE;
}

/*
 ***************************************************************************
 *
 * xf86WcmReverseConvert --
 *	Convert X and Y to valuators.
 *
 ***************************************************************************
 */
static Bool
xf86WcmReverseConvert(LocalDevicePtr	local,
		      int		x,
		      int		y,
		      int		*valuators)
{
    WacomDevicePtr	priv = (WacomDevicePtr) local->private;

#ifdef XFREE86_V4
    priv->factorX = ((double) miPointerCurrentScreen()->width)
	/ (priv->bottomX - priv->topX);
    priv->factorY = ((double) miPointerCurrentScreen()->height)
	/ (priv->bottomY - priv->topY);
#endif
    
    valuators[0] = x / priv->factorX + 0.5;
    valuators[1] = y / priv->factorY + 0.5;

    DBG(6, ErrorF("Wacom converted x=%d y=%d to v0=%d v1=%d\n", x, y,
		  valuators[0], valuators[1]));

    return TRUE;
}
 
/*
 ***************************************************************************
 *
 * xf86WcmSendButtons --
 *	Send button events by comparing the current button mask with the
 *      previous one.
 *
 ***************************************************************************
 */
static void
xf86WcmSendButtons(LocalDevicePtr	local,
		   int                  buttons,
		   int                  rx,
		   int                  ry,
		   int                  rz,
		   int                  rtx,
		   int                  rty,
		   int			rwheel)
		   
{
    int             button;
    WacomDevicePtr  priv = (WacomDevicePtr) local->private;

    for (button=1; button<=16; button++) {
	int mask = 1 << (button-1);
	
	if ((mask & priv->oldButtons) != (mask & buttons)) {
	    DBG(4, ErrorF("xf86WcmSendButtons button=%d state=%d\n", 
			  button, (buttons & mask) != 0));
	    xf86PostButtonEvent(local->dev, 
				(priv->flags & ABSOLUTE_FLAG),
				button, (buttons & mask) != 0,
				0, 6, rx, ry, rz, rtx, rty, rwheel);
	}
    }
}

/*
 ***************************************************************************
 *
 * xf86WcmSendEvents --
 *	Send events according to the device state.
 *
 ***************************************************************************
 */
static void
xf86WcmSendEvents(LocalDevicePtr	local,
		  int			type,
		  unsigned int		serial,
		  int			is_stylus,
		  int			is_button,
		  int			is_proximity,
		  int			x,
		  int			y,
		  int			z,
		  int			buttons,
		  int			tx,
		  int			ty,
		  int			wheel)
{
    WacomDevicePtr	priv = (WacomDevicePtr) local->private;
    WacomCommonPtr	common = priv->common;
    int			rx, ry, rz, rtx, rty, rwheel;
    int			is_core_pointer, is_absolute;

    if ((DEVICE_ID(priv->flags) != type) ||
	((common->wcmProtocolLevel == 5) &&
	 priv->serial && (serial != priv->serial))) {
	DBG(7,
	    {if (common->wcmProtocolLevel == 5) {
	    ErrorF("xf86WcmSendEvents not the same device id (%u,%u)\n",
		   serial, priv->serial);
	    } else {
            ErrorF("xf86WcmSendEvents not the same device type (%u,%u)\n",
		   DEVICE_ID(priv->flags), type);}});
	return;
    }
    
    DBG(7, ErrorF("[%s] prox=%s\tx=%d\ty=%d\tz=%d\tbutton=%s\tbuttons=%d\ttx=%d ty=%d\twl=%d\n",
		  (type == STYLUS_ID) ? "stylus" : (type == CURSOR_ID) ? "cursor" : "eraser",
		  is_proximity ? "true" : "false",
		  x, y, z,
		  is_button ? "true" : "false", buttons,
		  tx, ty, wheel));

    /* Translate coordinates according to Top and Bottom points
     * if we are outside the zone do as a ProximityOut event.
     */

    if (x > priv->bottomX) {
	is_proximity = FALSE;
	buttons = 0;
	x = priv->bottomX;
    }
	    
    if (y > priv->bottomY) {
	is_proximity = FALSE;
	buttons = 0;
	y = priv->bottomY;
    }

    DBG(10, ErrorF("topX=%d topY=%d\n", priv->topX, priv->topY));

    x = x - priv->topX;
    y = y - priv->topY;

    if (x < 0) {
	is_proximity = FALSE;
	buttons = 0;
	x = 0;
    }
    
    if (y < 0) {
	is_proximity = FALSE;
	buttons = 0;
	y = 0;
    }
    
    is_absolute = (priv->flags & ABSOLUTE_FLAG);
    is_core_pointer = xf86IsCorePointer(local->dev);

    DBG(6, ErrorF("[%s] %s prox=%s\tx=%d\ty=%d\tz=%d\tbutton=%s\tbuttons=%d\n",
		  is_stylus ? "stylus" : "cursor",
		  is_absolute ? "abs" : "rel",
		  is_proximity ? "true" : "false",
		  x, y, z,
		  is_button ? "true" : "false", buttons));

    /* Hardware filtering isn't working on Graphire so we do it here.
     */
    if ((common->wcmFlags & GRAPHIRE_FLAG) &&
	((is_proximity && priv->oldProximity) ||
	 ((is_proximity == 0) && (priv->oldProximity == 0))) &&
	(buttons == priv->oldButtons) &&
	(ABS(x - priv->oldX) <= common->wcmSuppress) &&
	(ABS(y - priv->oldY) <= common->wcmSuppress) &&
	 (ABS(z - priv->oldZ) < 3) &&
	(ABS(tx - priv->oldTiltX) < 3) &&
	(ABS(ty - priv->oldTiltY) < 3)) {
	
	DBG(10, ErrorF("Graphire filtered\n"));

	return;
    }
		
    /* sets rx and ry according to the mode */
    if (is_absolute) {
	rx = x;
	ry = y;
	rz = z;
	rtx = tx;
	rty = ty;
	rwheel = wheel;
    } else {
	rx = x - priv->oldX;
	ry = y - priv->oldY;
	rz = z - priv->oldZ;  
	rtx = tx - priv->oldTiltX;
	rty = ty - priv->oldTiltY;
	rwheel = wheel - priv->oldWheel;
    }

    /* coordinates are ready we can send events */
    if (is_proximity) {

	if (!priv->oldProximity) {
	    xf86PostProximityEvent(local->dev, 1, 0, 6, rx, ry, z, tx, ty, rwheel);

	    priv->flags |= FIRST_TOUCH_FLAG;
	    DBG(4, ErrorF("xf86WcmSendEvents FIRST_TOUCH_FLAG set\n"));
		    
	    if (common->wcmProtocolLevel == 4) {
		/* handle the two sides switches in the stylus */
		if (is_stylus && (buttons == 4)) {
		    priv->oldProximity = ERASER_PROX;
		}
		else {
		    priv->oldProximity = OTHER_PROX;
		}
	    }
	    else {
		priv->oldProximity = OTHER_PROX;
	    }
	}

	if (common->wcmProtocolLevel == 4 &&
	    !(common->wcmFlags & GRAPHIRE_FLAG)) {
	    /* The stylus reports button 4 for the second side
	     * switch and button 4/5 for the eraser tip. We know
	     * how to choose when we come in proximity for the
	     * first time. If we are in proximity and button 4 then
	     * we have the eraser else we have the second side
	     * switch.
	     */
	    if (is_stylus) {
		if (buttons == 4) {
		    buttons = (priv->oldProximity == ERASER_PROX) ? 0 : 3;
		}
		else {
		    if (priv->oldProximity == ERASER_PROX && buttons == 5) {
			buttons = ((DEVICE_ID(priv->flags) == ERASER_ID) ? 1 : 4);
		    }
		}
	    }
	    else {
		/* If the button flag is pressed, but the switch state
		 * is zero, this means that cursor button 16 was pressed
		 */
		if (is_button && buttons == 0) {
		    buttons = 16;
		}
	    }
	}
	DBG(4, ErrorF("xf86WcmSendEvents %s rx=%d ry=%d rz=%d buttons=%d\n",
		      is_stylus ? "stylus" : "cursor", rx, ry, rz, buttons));
	
	/* Turn button index reported into a bit mask for WACOM IV.
	 * The WACOM V and Graphire models already report buttons
	 * as a bit mask.
	 */
	if (common->wcmProtocolLevel == 4 &&
	    !(common->wcmFlags & GRAPHIRE_FLAG)) {
	    buttons = 1 << (buttons - 1);
	}
	
	if ((priv->oldX != x) ||
	    (priv->oldY != y) ||
	    (priv->oldZ != z) ||
	    (is_stylus && HANDLE_TILT(common) &&
	     (tx != priv->oldTiltX || ty != priv->oldTiltY))) {
	    if (!is_absolute && (priv->flags & FIRST_TOUCH_FLAG)) {
		priv->flags -= FIRST_TOUCH_FLAG;
		DBG(4, ErrorF("xf86WcmSendEvents FIRST_TOUCH_FLAG unset\n"));
	    } else {
		xf86PostMotionEvent(local->dev, is_absolute, 0, 6, rx, ry, rz,
				    rtx, rty, rwheel); 
	    }
	}

	if (priv->oldButtons != buttons) {
	    xf86WcmSendButtons (local, buttons, rx, ry, rz, rtx, rty, rwheel);
	}

	/* Simulate buttons 4 and 5 for Graphire wheel */
	if ((((common->wcmProtocolLevel == 4) && (common->wcmFlags & GRAPHIRE_FLAG) && (wheel != 0)) ||
	     ((common->wcmOpen == xf86WcmUSBOpen) && (wheel != priv->oldWheel))) &&
	    !is_stylus) {
	    int fake_button;

	    if (common->wcmOpen == xf86WcmUSBOpen) {
		fake_button = (wheel > priv->oldWheel) ? 5 : 4;
	    } else{
		fake_button = (wheel > 0) ? 5 : 4;
	    }
	    
	    xf86PostButtonEvent(local->dev,
				(priv->flags & ABSOLUTE_FLAG),
				fake_button, 1,
				0, 6, rx, ry, rz, rtx, rty, rwheel);
	    
	    xf86PostButtonEvent(local->dev,
				(priv->flags & ABSOLUTE_FLAG),
				fake_button, 0,
				0, 6, rx, ry, rz, rtx, rty, rwheel);
	}
	
	priv->oldButtons = buttons;
	priv->oldX = x;
	priv->oldY = y;
	priv->oldZ = z;
	priv->oldTiltX = tx;
	priv->oldTiltY = ty;
	priv->oldWheel = wheel;
    }
    else { /* !PROXIMITY */
	/* reports button up when the device has been down and becomes out of proximity */
	if (priv->oldButtons) {
	    xf86WcmSendButtons (local, 0, rx, ry, rz, rtx, rty, rwheel);
	    priv->oldButtons = 0;
	}
	if (!is_core_pointer) {
	    /* macro button management */
	    if (common->wcmProtocolLevel == 4 && buttons) {
		int	macro = z / 2;

		DBG(6, ErrorF("macro=%d buttons=%d wacom_map[%d]=%x\n",
			      macro, buttons, macro, wacom_map[macro]));

		/* First available Keycode begins at 8 => macro+7 */
		xf86PostKeyEvent(local->dev, macro+7, 1,
				 is_absolute, 0, 6,
				 0, 0, buttons, rtx, rty, rwheel);
		xf86PostKeyEvent(local->dev, macro+7, 0,
				 is_absolute, 0, 6,
				 0, 0, buttons, rtx, rty, rwheel);
	    }
	    if (priv->oldProximity) {
		xf86PostProximityEvent(local->dev, 0, 0, 6, rx, ry, rz,
				       rtx, rty, rwheel);
	    }
	}
	priv->oldProximity = 0;
    }
}

/*
 ***************************************************************************
 *
 * xf86WcmSuppress --
 *	Determine whether device state has changed enough - return 1
 *	if not.
 *
 ***************************************************************************
 */
static int
xf86WcmSuppress(int			suppress,
		WacomDeviceState 	*ds1,
		WacomDeviceState	*ds2)
{
    if (ds1->buttons != ds2->buttons) return 0;
    if (ds1->proximity != ds2->proximity) return 0;
    if (ABS(ds1->x - ds2->x) >= suppress) return 0;
    if (ABS(ds1->y - ds2->y) >= suppress) return 0;
    if (ABS(ds1->pressure - ds2->pressure) >= suppress) return 0;
    if ((1800 + ds1->rotation - ds2->rotation) % 1800 >= suppress &&
    	(1800 + ds2->rotation - ds1->rotation) % 1800 >= suppress) return 0;
    if (ABS(ds1->wheel - ds2->wheel) >= suppress) return 0;
    return 1;
}

/*
 ***************************************************************************
 *
 * xf86WcmIntuosFilter --
 *	 Correct some hardware defects we've been seeing in Intuos pads,
 *       but also cuts down quite a bit on jitter.
 *
 ***************************************************************************
 */
static int
xf86WcmIntuosFilter(WacomFilterState	*state,
		    int			coord,
		    int			tilt)
{
    int tilt_filtered;
    int ts;
    int x0_pred;
    int x0_pred1;
    int x0, x1, x2, x3;
    int x;
    
    tilt_filtered = tilt + state->tilt[1] + state->tilt[2] + state->tilt[3];
    state->tilt[2] = state->tilt[1];
    state->tilt[1] = state->tilt[0];
    state->tilt[0] = tilt;
    
    x0 = coord;
    x1 = state->coord[0];
    x2 = state->coord[1];
    x3 = state->coord[2];
    state->coord[0] = x0;
    state->coord[1] = x1;
    state->coord[2] = x2;
    
    ts = tilt_filtered >= 0 ? 1 : -1;
    
    if (state->state == 0 || state->state == 3) {
	x0_pred = 2 * x1 - x2;
	x0_pred1 = 3 * x2 - 2 * x3;
	if (ts * (x0 - x0_pred) > 12 &&
	    ts * (x0 - x0_pred1) > 12) {
	    /* detected a jump at x0 */
	    state->state = 1;
	    x = x1;
	}
	else if (state->state == 0) {
	    x = (7 * x0 + 14 * x1 + 15 * x2 - 4 * x3 + 16) >> 5;
	}
	else { /* state->state == 3 */
	    /* a jump at x3 was detected */
	    x = (x0 + 2 * x1 + x2 + 2) >> 2;
	    state->state = 0;
	}
    }
    else if (state->state == 1) {
	/* a jump at x1 was detected */
	x = (3 * x0 + 7 * x2 - 2 * x3 + 4) >> 3;
	state->state = 2;
    }
    else { /* state->state == 2 */
	/* a jump at x2 was detected */
	x = x1;
	state->state = 3;
    }

    return x;
}

/*
 ***************************************************************************
 *
 * xf86WcmReadInput --
 *	Read the new events from the device, and enqueue them.
 *
 ***************************************************************************
 */
static void
xf86WcmReadInput(LocalDevicePtr         local)
{
    WacomDevicePtr	priv = (WacomDevicePtr) local->private;
    WacomCommonPtr	common = priv->common;
    int			len, loop, idx;
    int			is_stylus = 1, is_button, is_proximity, wheel=0;
    int			is_absolute = (priv->flags & ABSOLUTE_FLAG);
    int			x, y, z, buttons, tx = 0, ty = 0;
    unsigned char	buffer[BUFFER_SIZE];
    WacomDeviceState	*ds;
    WacomDeviceState	old_ds;
    int			have_data;
  
    DBG(7, ErrorF("xf86WcmReadInput BEGIN device=%s fd=%d\n",
		  common->wcmDevice, local->fd));

    SYSCALL(len = read(local->fd, buffer, sizeof(buffer)));

    if (len <= 0) {
	ErrorF("Error reading wacom device : %s\n", strerror(errno));
	return;
    } else {
	DBG(10, ErrorF("xf86WcmReadInput read %d bytes\n", len));
    }

    for(loop=0; loop<len; loop++) {

	/* Format of 7 bytes data packet for Wacom Tablets
	Byte 1
	bit 7  Sync bit always 1
	bit 6  Pointing device detected
	bit 5  Cursor = 0 / Stylus = 1
	bit 4  Reserved
	bit 3  1 if a button on the pointing device has been pressed
	bit 2  Reserved
	bit 1  X15
	bit 0  X14

	Byte 2
	bit 7  Always 0
	bits 6-0 = X13 - X7

	Byte 3
	bit 7  Always 0
	bits 6-0 = X6 - X0

	Byte 4
	bit 7  Always 0
	bit 6  B3
	bit 5  B2
	bit 4  B1
	bit 3  B0
	bit 2  P0
	bit 1  Y15
	bit 0  Y14

	Byte 5
	bit 7  Always 0
	bits 6-0 = Y13 - Y7

	Byte 6
	bit 7  Always 0
	bits 6-0 = Y6 - Y0

	Byte 7
	bit 7 Always 0
	bit 6  Sign of pressure data
	bit 5  P6
	bit 4  P5
	bit 3  P4
	bit 2  P3
	bit 1  P2
	bit 0  P1

	byte 8 and 9 are optional and present only
	in tilt mode.

	Byte 8
	bit 7 Always 0
	bit 6 Sign of tilt X
	bit 5  Xt6
	bit 4  Xt5
	bit 3  Xt4
	bit 2  Xt3
	bit 1  Xt2
	bit 0  Xt1
       
	Byte 9
	bit 7 Always 0
	bit 6 Sign of tilt Y
	bit 5  Yt6
	bit 4  Yt5
	bit 3  Yt4
	bit 2  Yt3
	bit 1  Yt2
	bit 0  Yt1
       
	*/

	if ((common->wcmIndex == 0) && !(buffer[loop] & HEADER_BIT)) { /* magic bit is not OK */
	    DBG(6, ErrorF("xf86WcmReadInput bad magic number 0x%x (pktlength=%d) %d\n",
			  buffer[loop], common->wcmPktLength, loop));
	    continue;
	}
	else { /* magic bit at wrong place */
	    if ((common->wcmIndex != 0) && (buffer[loop] & HEADER_BIT)) {
		DBG(6, ErrorF("xf86WcmReadInput magic number 0x%x detetected at index %d loop=%d\n",
			      (unsigned int) buffer[loop], common->wcmIndex, loop));
		common->wcmIndex = 0;
	    }
	}
	
	common->wcmData[common->wcmIndex++] = buffer[loop];

	if (common->wcmProtocolLevel == 4 &&
	    common->wcmIndex == common->wcmPktLength) {
	    int	is_graphire = common->wcmFlags & GRAPHIRE_FLAG;
	    
	    /* the packet is OK */

	    /* reset char count for next read */
	    common->wcmIndex = 0;

	    x = (((common->wcmData[0] & 0x3) << 14) +
		 (common->wcmData[1] << 7) +
		 common->wcmData[2]);
	    y = (((common->wcmData[3] & 0x3) << 14) +
		 (common->wcmData[4] << 7) +
		 common->wcmData[5]);

	    /* check which device we have */
	    is_stylus = (common->wcmData[0] & POINTER_BIT);
	      
	    z = ((common->wcmData[6] & ZAXIS_BITS) * 2) +
		((common->wcmData[3] & ZAXIS_BIT) >> 2);

	    if (common->wcmMaxZ == 512) {
		z = z*4 + ((common->wcmData[0] & ZAXIS_BIT) >> 1);

		if (!(common->wcmData[6] & ZAXIS_SIGN_BIT)) {
		    z += 256;
		}
		DBG(10, ErrorF("graphire pressure(%c)=%d\n",
			       (common->wcmData[6] & ZAXIS_SIGN_BIT) ? '-' : '+', z));
	    }
	    else {
		if (!(common->wcmData[6] & ZAXIS_SIGN_BIT)) {
		    z += (common->wcmMaxZ / 2);
		}
	    }
	    
	    is_proximity = (common->wcmData[0] & PROXIMITY_BIT);

	    if (is_graphire) {
		if (is_stylus) {
		    buttons = ((common->wcmData[3] & 0x30) >> 3) |
			(z >= common->wcmThreshold ? 1 : 0);
		}
		else {
		    buttons = (common->wcmData[3] & 0x38) >> 3;

		    wheel = (common->wcmData[6] & 0x30) >> 4;
		
		    if (common->wcmData[6] & 0x40) {
			wheel = -wheel;
		    }
		}
		is_button = (buttons != 0);

		DBG(10, ErrorF("graphire buttons=%d prox=%d wheel=%d\n", buttons, is_proximity, wheel));
	    }
	    else {
		is_button = (common->wcmData[0] & BUTTON_FLAG);
		buttons = (common->wcmData[3] & BUTTONS_BITS) >> 3;
	    }
	    
	    /* The stylus reports button 4 for the second side
	     * switch and button 4/5 for the eraser tip. We know
	     * how to choose when we come in proximity for the
	     * first time. If we are in proximity and button 4 then
	     * we have the eraser else we have the second side
	     * switch.
	     */
	    if (is_stylus) {
		if (!common->wcmStylusProximity && is_proximity) {
		    if (is_graphire) {
			    common->wcmStylusSide = !(common->wcmData[3] & 0x40);
		    }
		    else {
			common->wcmStylusSide = (buttons != 4);
		    }
		}
		DBG(8, ErrorF("xf86WcmReadInput %s side\n",
			      common->wcmStylusSide ? "stylus" : "eraser"));
		common->wcmStylusProximity = is_proximity;

		/* handle tilt values only for stylus */
		if (HANDLE_TILT(common)) {
		    tx = (common->wcmData[7] & TILT_BITS);
		    ty = (common->wcmData[8] & TILT_BITS);
		    if (common->wcmData[7] & TILT_SIGN_BIT)
			tx -= (TILT_BITS + 1);
		    if (common->wcmData[8] & TILT_SIGN_BIT)
			ty -= (TILT_BITS + 1);
		}
	    }

	    for(idx=0; idx<common->wcmNumDevices; idx++) {
	        LocalDevicePtr  local_dev = common->wcmDevices[idx];
		WacomDevicePtr	priv = (WacomDevicePtr) local_dev->private;
		int		temp_buttons = buttons;
		int		temp_is_proximity = is_proximity;
		int             curDevice;
		
		DBG(7, ErrorF("xf86WcmReadInput trying to send to %s\n",
			      local_dev->name));
		
		/* check for device type (STYLUS, ERASER or CURSOR) */
		
		if (is_stylus) {
		    /*
		     * The eraser is reported as button 4 and 5 of the stylus.
		     * if we haven't an independent device for the eraser
		     * report the button as button 3 of the stylus.
		     */
		    if (is_proximity) {
			if (is_graphire) {
			    if (common->wcmData[3] & 0x40) {
				curDevice = ERASER_ID;
			    }
			    else {
				curDevice = STYLUS_ID;
			    }
			}
			else {
			    if ((buttons & 4) && common->wcmHasEraser &&
				((!priv->oldProximity ||
				  (priv->oldProximity == ERASER_PROX)))) {
				curDevice = ERASER_ID;
			    } else {
				curDevice = STYLUS_ID;
			    }
			}
		    } else {
			/*
			 * When we are out of proximity with the eraser the
			 * button 4 isn't reported so we must check the
			 * previous proximity device.
			 */
			if (common->wcmHasEraser && (priv->oldProximity == ERASER_PROX)) {
			    curDevice = ERASER_ID;
			} else {
			    curDevice = STYLUS_ID;
			}
		    }
		    
		    /* We check here to see if we changed between eraser and stylus
		     * without leaving proximity. The most likely cause is that
		     * we were fooled by the second side switch into thinking the
		     * stylus was the eraser. If this happens, we send
		     * a proximity-out for the old device.
		     */
		    if ((DEVICE_ID(priv->flags) == STYLUS_ID ||
			 DEVICE_ID(priv->flags) == ERASER_ID) &&
			curDevice != DEVICE_ID(priv->flags)) {
			if (priv->oldProximity) {
			    curDevice = DEVICE_ID(priv->flags);
			    temp_buttons = 0;
			    temp_is_proximity = 0;
			    DBG(10, ErrorF("eraser and stylus mix\n"));
			} else 
			    continue;
		    }
		    
		    DBG(10, ErrorF((DEVICE_ID(priv->flags) == ERASER_ID) ? 
				   "Eraser\n" : 
				   "Stylus\n"));
		}
		else {
		    if (DEVICE_ID(priv->flags) != CURSOR_ID)
			continue;	
		    DBG(10, ErrorF("Cursor\n"));
		    curDevice = CURSOR_ID;
		}
		
    		xf86WcmSendEvents(common->wcmDevices[idx],
				  curDevice, 0,
				  is_stylus,
				  is_button,
				  temp_is_proximity,
				  x, y, z, temp_buttons,
				  tx, ty, wheel);
	    }
	} /* protocol 4 */
	else if (common->wcmProtocolLevel == 5 &&
		 common->wcmIndex == common->wcmPktLength) {
	    /* the packet is OK */
	    int x, y;

	    /* reset count for read of next packet */
	    common->wcmIndex = 0;

	    ds = &common->wcmDevStat[common->wcmData[0] & 0x01];
	    old_ds = *ds;
	    have_data = 0;

	    DBG(7, ErrorF("packet header = 0x%x\n",
			  (unsigned int)common->wcmData[0]));
	    
	    /* Device ID packet */
	    if ((common->wcmData[0] & 0xfc) == 0xc0) {
		memset(ds, 0, sizeof(*ds));
		ds->proximity = 1;
		ds->device_id = (((common->wcmData[1] & 0x7f) << 5) |
				 ((common->wcmData[2] & 0x7c) >> 2));
		ds->serial_num = (((common->wcmData[2] & 0x03) << 30) |
				  ((common->wcmData[3] & 0x7f) << 23) |
				  ((common->wcmData[4] & 0x7f) << 16) |
				  ((common->wcmData[5] & 0x7f) << 9) |
				  ((common->wcmData[6] & 0x7f) << 23) |
				  ((common->wcmData[7] & 0x60) >> 5));
		if ((ds->device_id & 0xf06) != 0x802)
		  ds->discard_first = 1;

		if (PEN(ds) || STROKING_PEN(ds) || INKING_PEN(ds) || AIRBRUSH(ds))
		    ds->device_type = STYLUS_ID;
		else if (MOUSE_4D(ds) || LENS_CURSOR(ds))
		    ds->device_type = CURSOR_ID;
		else
		    ds->device_type = ERASER_ID;
		
		DBG(6, ErrorF("device_id=0x%x serial_num=%u type=%s\n",
			      ds->device_id, ds->serial_num,
			      (ds->device_type == STYLUS_ID) ? "stylus"
			      : (ds->device_type == CURSOR_ID) ? "cursor"
			      : "eraser"));
	    }
	    /* Out of proximity packet */
	    else if ((common->wcmData[0] & 0xfe) == 0x80) {
		ds->proximity = 0;
		have_data = 1;
	    }
	    /* General pen packet or eraser packet or airbrush first packet */
	    else if (((common->wcmData[0] & 0xb8) == 0xa0) ||
		     /* airbrush second packet */
		     ((common->wcmData[0] & 0xbe) == 0xb4)) {
		is_stylus = 1;
		ds->x = (((common->wcmData[1] & 0x7f) << 9) |
			 ((common->wcmData[2] & 0x7f) << 2) |
			 ((common->wcmData[3] & 0x60) >> 5));
		ds->y = (((common->wcmData[3] & 0x1f) << 11) |
			 ((common->wcmData[4] & 0x7f) << 4) |
			 ((common->wcmData[5] & 0x78) >> 3));
		if ((common->wcmData[0] & 0xb8) == 0xa0) {
		    ds->pressure = (((common->wcmData[5] & 0x07) << 7) |
				    (common->wcmData[6] & 0x7f));
		    ds->buttons = (((common->wcmData[0]) & 0x06) |
				   (ds->pressure >= common->wcmThreshold));
		}
		else {
		    ds->wheel = (((common->wcmData[5] & 0x07) << 7) |
				 (common->wcmData[6] & 0x7f));
		}
		ds->tiltx = (common->wcmData[7] & TILT_BITS);
		ds->tilty = (common->wcmData[8] & TILT_BITS);
		if (common->wcmData[7] & TILT_SIGN_BIT)
		  ds->tiltx -= (TILT_BITS + 1);
		if (common->wcmData[8] & TILT_SIGN_BIT)
		  ds->tilty -= (TILT_BITS + 1);
		ds->proximity = (common->wcmData[0] & PROXIMITY_BIT);
		have_data = 1;
	    }
	    /* 4D mouse 1st packet or Lens cursor packet */
	    else if (((common->wcmData[0] & 0xbe) == 0xa8) ||
		     ((common->wcmData[0] & 0xbe) == 0xb0)) {
		is_stylus = 0;
		ds->x = (((common->wcmData[1] & 0x7f) << 9) |
		     ((common->wcmData[2] & 0x7f) << 2) |
		     ((common->wcmData[3] & 0x60) >> 5));
		ds->y = (((common->wcmData[3] & 0x1f) << 11) |
		     ((common->wcmData[4] & 0x7f) << 4) |
		     ((common->wcmData[5] & 0x78) >> 3));
		ds->tilty = 0;
		ds->wheel = (((common->wcmData[5] & 0x07) << 7) |
		     (common->wcmData[6] & 0x7f));
		if (common->wcmData[8] & 0x08) ds->wheel = -ds->wheel;
		/* 4D mouse */
		if (MOUSE_4D(ds)) {
		    ds->buttons = (((common->wcmData[8] & 0x70) >> 1) |
				   (common->wcmData[8] & 0x07));
		    have_data = !ds->discard_first;
		}
		/* Lens cursor */
		else {
		    ds->buttons = common->wcmData[8];
		    have_data = 1;
		}
		ds->proximity = (common->wcmData[0] & PROXIMITY_BIT);
	    }
	    /* 4D mouse 2nd packet */
	    else if ((common->wcmData[0] & 0xbe) == 0xaa) {
		is_stylus = 0;
		ds->x = (((common->wcmData[1] & 0x7f) << 9) |
			 ((common->wcmData[2] & 0x7f) << 2) |
			 ((common->wcmData[3] & 0x60) >> 5));
		ds->y = (((common->wcmData[3] & 0x1f) << 11) |
			 ((common->wcmData[4] & 0x7f) << 4) |
			 ((common->wcmData[5] & 0x78) >> 3));
		ds->tilty = 0;
		ds->rotation = (((common->wcmData[6] & 0x0f) << 7) |
				(common->wcmData[7] & 0x7f));
		ds->tiltx = ((900 - ((ds->rotation + 900) % 1800)) >> 1);
		ds->proximity = (common->wcmData[0] & PROXIMITY_BIT);
		have_data = 1;
		ds->discard_first = 0;
	    }
	    else {
		DBG(10, ErrorF("unknown wacom V packet 0x%x\n",
			       common->wcmData[0]));
	    }
	    
	    /* Suppress data */
	    if (have_data &&
		xf86WcmSuppress(common->wcmSuppress, &old_ds, ds)) {
	        DBG(10, ErrorF("Suppressing data according to filter\n"));
		*ds = old_ds;
		have_data = 0;
	    }

	    if (have_data) {
	        if (is_absolute) {
		    x = xf86WcmIntuosFilter (&ds->x_filter, ds->x, ds->tiltx);
		    y = xf86WcmIntuosFilter (&ds->y_filter, ds->y, ds->tilty);
		} 
		else {
		    x = ds->x;
		    y = ds->y;
		}
		for(idx=0; idx<common->wcmNumDevices; idx++) {
		    DBG(7, ErrorF("xf86WcmReadInput trying to send to %s\n",
				  common->wcmDevices[idx]->name));

		    xf86WcmSendEvents(common->wcmDevices[idx],
				      ds->device_type, ds->serial_num,
				      is_stylus,
				      ds->buttons,
				      ds->proximity,
				      x, y,
				      ds->pressure,
				      ds->buttons,
				      ds->tiltx, ds->tilty,
				      ds->wheel);
		}
	    }
	} /* protocol 5 */
    } /* next data */
    DBG(7, ErrorF("xf86WcmReadInput END   local=0x%x priv=0x%x index=%d\n",
		  local, priv, common->wcmIndex));
}

#ifdef LINUX_INPUT
/*
 ***************************************************************************
 *
 * xf86WcmIsUSBLine --
 *	Test if the attached device is a USB one.
 *
 ***************************************************************************
 */
static int
xf86WcmIsUSBLine(int	fd)
{
    int version;
    int err;

    SYSCALL(err = ioctl(fd, EVIOCGVERSION, &version));
    
    if (!err) {
	ErrorF("%s Wacom Kernel Input driver version is %d.%d.%d\n", XCONFIG_PROBED,
	       version >> 16, (version >> 8) & 0xff, version & 0xff);
	return 1;
    } else {
	return 0;
    }
}

static int ThrottleToRate(int x)
{
	if (x<0) x=-x;

	/* piece-wise exponential function */
	
	if (x < 128) return 0;		/* infinite */
	if (x < 256) return 1000;	/* 1 second */
	if (x < 512) return 500;	/* 0.5 seconds */
	if (x < 768) return 250;	/* 0.25 seconds */
	if (x < 896) return 100;	/* 0.1 seconds */
	if (x < 960) return 50;	/* 0.05 seconds */
	if (x < 1024) return 25;	/* 0.025 seconds */
	return 0;					/* infinite */
}

/*
 ***************************************************************************
 *
 * xf86WcmReadUSBInput --
 *	Read the new events from the device, and enqueue them.
 *
 ***************************************************************************
 */
static void
xf86WcmReadUSBInput(LocalDevicePtr         local)
{
    WacomDevicePtr	priv = (WacomDevicePtr) local->private;
    WacomCommonPtr	common = priv->common;
    int serial = common->wcmLastSerial;
    int is_proximity = priv->oldProximity;
    int x = priv->oldX;
    int y = priv->oldY;
    int pressure = priv->oldZ;
    int buttons = priv->oldButtons;
    int tilt_x = priv->oldTiltX;
    int tilt_y = priv->oldTiltY;
    int wheel = priv->oldWheel;
	int sampleTime, ticks;

    ssize_t              len;
    int                  idx, loop;
    struct input_event * event, *readevent;
    char                 eventbuf[sizeof(struct input_event) * MAX_EVENTS];
#define MOD_BUTTONS(bit, value) \
    { int _b=bit, _v=value; buttons = (((_v) != 0) ? (buttons | _b) : (buttons & ~ _b)); }

	/* get the sample time */
	sampleTime = GetTimeInMillis();

	/* account for roll overs and initialization */
	if ((priv->throttleStart > sampleTime) || (!priv->throttleStart))
	{
		priv->throttleStart = sampleTime;
		priv->throttleLimit = -1;
	}

    SYSCALL(len = read(local->fd, eventbuf, sizeof(eventbuf)));

    DBG(10, ErrorF("xf86WcmReadUSBInput read %d events\n", len/sizeof(struct input_event)));
    
    if (len <= 0) {
	ErrorF("Error reading wacom device : %s\n", strerror(errno));
	return;
    }
    
    for (readevent=(struct input_event *)eventbuf;
	 readevent<(struct input_event *)(eventbuf+len); readevent++) {
	/* sanity check */
	if (common->wcmIndex >= MAX_USB_EVENTS) {
	    DBG(11, ErrorF("xf86WcmReadUSBInput resetting buffer index\n"));
	    common->wcmIndex = 0;
	}
	    
	common->wcmEvent[common->wcmIndex++] = *readevent;
	
	/* MSC_SERIAL is the event terminator */
	if (!((readevent->type == EV_MSC && readevent->code == MSC_SERIAL) ||
	      (readevent->type == EV_ABS && readevent->code == ABS_MISC))) {
	    continue;
	}

	for(loop=0; loop<common->wcmIndex; loop++) {
	event = common->wcmEvent + loop;
	DBG(11, ErrorF("xf86WcmReadUSBInput event[%d]->type=%d code=%d value=%d\n",
		       loop, event->type, event->code, event->value));
	switch (event->type) {
	case EV_ABS:
	    switch (event->code) {
	    case ABS_X:
		x = event->value;
		break;

	    case ABS_Y:
		y = event->value;
		break;

	    case ABS_TILT_X:
	    case ABS_RZ:
		tilt_x = event->value;
		break;

	    case ABS_TILT_Y:
		tilt_y = event->value;
		break;

	    case ABS_PRESSURE:
		pressure = event->value;
 		MOD_BUTTONS (1, event->value > common->wcmThreshold ? 1 : 0);
		break;

	    case ABS_DISTANCE:
		/* This is not sent by the driver */
		/* JEJ - actually it is, but it's not very useful */
		break;

	    case ABS_MISC:
		serial = event->value;
		DBG(10, ErrorF("wacom tool serial id=%d\n", serial));
		break;

	    case ABS_WHEEL:
			wheel = event->value;
			break;

	    case ABS_THROTTLE:
			priv->throttleValue = event->value;
			ticks = ThrottleToRate(event->value);
			priv->throttleLimit = ticks ? priv->throttleStart + ticks : -1;
			break;
	    }
	    break; /* EV_ABS */

	case EV_REL:
	    switch (event->code) {
	    case REL_WHEEL:
		wheel += event->value;
		break;
	    default:
		ErrorF("wacom: relative event received (%d)!!!\n", event->code);
		break;
	    }
	    break; /* EV_REL */

	case EV_KEY:
	    switch (event->code) {
	    case BTN_TOOL_PEN:
	    case BTN_TOOL_PENCIL:
	    case BTN_TOOL_BRUSH:
	    case BTN_TOOL_AIRBRUSH:
		DBG(10, ErrorF("USB Stylus detected %x\n", event->code));
		common->wcmLastTool = STYLUS_ID;
		is_proximity = (event->value != 0);
		break;

	    case BTN_TOOL_RUBBER:
		DBG(10, ErrorF("USB eraser detected %x\n", event->code));
		common->wcmLastTool = ERASER_ID;
		is_proximity = (event->value != 0);
		break;

	    case BTN_TOOL_MOUSE:
	    case BTN_TOOL_LENS:
		DBG(10, ErrorF("USB mouse detected %x\n", event->code));
		common->wcmLastTool = CURSOR_ID;
		is_proximity = (event->value != 0);
		break;

	    case BTN_TOUCH:
		/* we use the pressure to determine the button 1 */
		break;

	    case BTN_STYLUS:
	    case BTN_MIDDLE:
		MOD_BUTTONS (2, event->value);
		break;

	    case BTN_STYLUS2:
	    case BTN_RIGHT:
		MOD_BUTTONS (4, event->value);
		break;

	    case BTN_LEFT:
		MOD_BUTTONS (1, event->value);
		break;

	    case BTN_SIDE:
		MOD_BUTTONS (8, event->value);
		break;

	    case BTN_EXTRA:
		MOD_BUTTONS (16, event->value);
		break;
	    }
	    break; /* EV_KEY */
	case EV_MSC:
	    switch (event->code) {
	    case MSC_SERIAL:
		serial = event->value;
		DBG(10, ErrorF("wacom tool serial id=%d\n", serial));
		break;
	    }
	    break; /* EV_MSC */
	} /* switch event->type */
	
	    	/* MSC_SERIAL is the event terminator */
	if (!(event->type == EV_MSC && event->code == MSC_SERIAL) &&
	    !(event->type == EV_ABS && event->code == ABS_MISC)) {
	    continue;
	}

	/* handle throttle */
	if ((priv->throttleLimit >= 0) && (priv->throttleLimit < sampleTime))
	{
		DBG(6, ErrorF("LIMIT REACHED: s=%d l=%d n=%d v=%d N=%d\n",
				priv->throttleStart,
				priv->throttleLimit,
				sampleTime,
				priv->throttleValue,
				sampleTime + ThrottleToRate(priv->throttleValue)));

		wheel += (priv->throttleValue > 0) ? 1 :
				(priv->throttleValue < 0) ? -1 : 0;

		priv->throttleStart = sampleTime;
		priv->throttleLimit = sampleTime + ThrottleToRate(priv->throttleValue);
	}

	if ((is_proximity == priv->oldProximity) &&
	    (buttons == priv->oldButtons) &&
	    (wheel == priv->oldWheel) &&
	    (ABS(x - priv->oldX) <= common->wcmSuppress) &&
	    (ABS(y - priv->oldY) <= common->wcmSuppress) &&
	    (ABS(pressure - priv->oldZ) < 3) &&
	    (ABS(tilt_x - priv->oldTiltX) < 3) &&
	    (ABS(tilt_y - priv->oldTiltY) < 3)) {
	    DBG(10, ErrorF("filtered %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
			   x, priv->oldX,
			   y, priv->oldY,
			   pressure, priv->oldZ,
			   is_proximity, priv->oldProximity,
			   buttons, priv->oldButtons,
			   wheel, priv->oldWheel,
			   tilt_x, priv->oldTiltX,
			   tilt_y, priv->oldTiltY));
	    continue;
	}

	DBG(10, ErrorF("sending event %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
		       x, priv->oldX,
		       y, priv->oldY,
		       pressure, priv->oldZ,
		       is_proximity, priv->oldProximity,
		       buttons, priv->oldButtons,
		       wheel, priv->oldWheel,
		       tilt_x, priv->oldTiltX,
		       tilt_y, priv->oldTiltY));
	
	for (idx=0; idx<common->wcmNumDevices; idx++) {
	   WacomDevicePtr	dev = common->wcmDevices[idx]->private;
	   int		id;

	   id  = DEVICE_ID (dev->flags);

	   /* Find the device the current events are meant for */
	   if (id == common->wcmLastTool) {
	       DBG(11, ErrorF("tool id=%d for %s\n", id, local->name));
	       
		xf86WcmSendEvents(common->wcmDevices[idx],
				  common->wcmLastTool,
				  serial,
				  (common->wcmLastTool == STYLUS_ID || common->wcmLastTool == ERASER_ID),
				  !!(buttons),
				  is_proximity,
				  x, y, pressure, buttons,
				  tilt_x, tilt_y, wheel);
	   }
	}
	priv->oldX = x;
	priv->oldY = y;
	priv->oldZ = pressure;
	priv->oldTiltX = tilt_x;
	priv->oldTiltY = tilt_y;
	priv->oldProximity = is_proximity;
	priv->oldButtons = buttons;
	priv->oldWheel = wheel;
	common->wcmLastSerial = serial;
	} /* next event */
	common->wcmIndex=0;
    } /* next event group */
}

/*
 ***************************************************************************
 *
 * xf86WcmUSBOpen --
 *
 ***************************************************************************
 */

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define test_bit(bit, array)	((array[LONG(bit)] >> OFF(bit)) & 1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define LONG(x) ((x)/BITS_PER_LONG)

static Bool
xf86WcmUSBOpen(LocalDevicePtr	local)
{
    int			err = 0;
    WacomDevicePtr	priv = (WacomDevicePtr)local->private;
    WacomCommonPtr	common = priv->common;
    char		name[256] = "Unknown";
    int			abs[5];
    unsigned long	bit[EV_MAX][NBITS(KEY_MAX)];
    int			i, j;

#ifdef XFREE86_V4
    local->fd = xf86OpenSerial(local->options);
#else
    SYSCALL(local->fd = open(common->wcmDevice, O_RDONLY|O_NDELAY, 0));
#endif
    if (local->fd == -1) {
	ErrorF("Error opening %s : %s\n", common->wcmDevice, strerror(errno));
	return !Success;
    }

    ioctl(local->fd, EVIOCGNAME(sizeof(name)), name);
    ErrorF("%s Wacom Kernel Input device name: \"%s\"\n", XCONFIG_PROBED, name);

    memset(bit, 0, sizeof(bit));
    ioctl(local->fd, EVIOCGBIT(0, EV_MAX), bit[0]);

    for (i = 0; i < EV_MAX; i++)
	if (test_bit(i, bit[0])) {
	    ioctl(local->fd, EVIOCGBIT(i, KEY_MAX), bit[i]);
	    for (j = 0; j < KEY_MAX; j++) 
		if (test_bit(j, bit[i])) {
		    if (i == EV_ABS) {
			ioctl(local->fd, EVIOCGABS(j), abs);
			switch (j) {
			case ABS_X:
			    if (common->wcmMaxX == 0) {
				common->wcmMaxX = abs[2];
			    }
			    break;
			    
			case ABS_Y:
			    if (common->wcmMaxY == 0) {
				common->wcmMaxY = abs[2];
			    }
			    break;
			    
			case ABS_PRESSURE:
			    if (common->wcmMaxZ == DEFAULT_MAXZ) {
				common->wcmMaxZ = abs[2];
			    }
			    break;
			}
		    }
		}
	}
    
    DBG(2, ErrorF("setup is max X=%d(%d) Y=%d(%d) Z=%d(%d)\n",
		  common->wcmMaxX, common->wcmResolX,
		  common->wcmMaxY, common->wcmResolY,
		  common->wcmMaxZ, common->wcmResolZ));
  
    /* send the tilt mode command after setup because it must be enabled */
    /* after multi-mode to take precedence */
    if (HANDLE_TILT(common)) {
      /* Unfortunately, the USB driver doesn't allow to send this
       * command to the tablet. Any other solutions ? */
	DBG(2, ErrorF("Sending tilt mode order\n"));
    }
  
    if (common->wcmSuppress < 0) {
	int	xratio = common->wcmMaxX/screenInfo.screens[0]->width;
	int	yratio = common->wcmMaxY/screenInfo.screens[0]->height;
	
	common->wcmSuppress = (xratio > yratio) ? yratio : xratio;
    }
    
    if (common->wcmSuppress > 100) {
	common->wcmSuppress = 99;
    }
    /* Cannot send WC_SUPPRESS to the table. Will have to do
     * this manually. */
    
    priv->topX = 0;
    priv->bottomX = common->wcmMaxX;
    priv->topY = 0;
    priv->bottomY = common->wcmMaxY;

    if (xf86Verbose)
	ErrorF("%s Wacom tablet maximum X=%d maximum Y=%d "
	       "X resolution=%d Y resolution=%d suppress=%d%s\n",
	       XCONFIG_PROBED, common->wcmMaxX, common->wcmMaxY,
	       common->wcmResolX, common->wcmResolY, common->wcmSuppress,
	       HANDLE_TILT(common) ? " Tilt" : "");
  
    if (err < 0) {
        ErrorF("ERROR: %d\n", err);
	SYSCALL(close(local->fd));
	return !Success;
    }

    /* to have the button field handled as a bit field */
    common->wcmProtocolLevel = 5;

    return Success;
}

#endif /* LINUX_INPUT */

/*
 ***************************************************************************
 *
 * xf86WcmControlProc --
 *
 ***************************************************************************
 */
static void
xf86WcmControlProc(DeviceIntPtr	device,
		   PtrCtrl	*ctrl)
{
  DBG(2, ErrorF("xf86WcmControlProc\n"));
}

/*
 ***************************************************************************
 *
 * xf86WcmOpen --
 *
 ***************************************************************************
 */
#ifdef XFREE86_V4
#define WAIT(t)							\
    err = xf86WaitForInput(-1, ((t) * 1000));			\
    if (err == -1) {						\
	ErrorF("Wacom select error : %s\n", strerror(errno));	\
	return !Success;					\
    }
#else
#define WAIT(t)							\
    timeout.tv_sec = 0;						\
    timeout.tv_usec = (t) * 1000;				\
    SYSCALL(err = select(0, NULL, NULL, NULL, &timeout));	\
    if (err == -1) {						\
	ErrorF("Wacom select error : %s\n", strerror(errno));	\
	return !Success;					\
    }
#endif

static Bool
xf86WcmOpen(LocalDevicePtr	local)
{
#ifndef XFREE86_V4
    struct timeval	timeout;
#endif
    char		buffer[256];
    char		header[64]; /* This is a small buffer for discarding the unwanted header */
    int			err;
    WacomDevicePtr	priv = (WacomDevicePtr)local->private;
    WacomCommonPtr	common = priv->common;
    int			a, b;
    int			loop, idx;
    float		version = 0.0;
    int			is_a_penpartner = 0;
    
    DBG(1, ErrorF("opening %s\n", common->wcmDevice));

#ifdef XFREE86_V4
    local->fd = xf86OpenSerial(local->options);
#else
    SYSCALL(local->fd = open(common->wcmDevice, O_RDWR|O_NDELAY, 0));
#endif
    if (local->fd < 0) {
	ErrorF("Error opening %s : %s\n", common->wcmDevice, strerror(errno));
	return !Success;
    }

#ifdef LINUX_INPUT
    DBG(1, ErrorF("testing USB\n"));
    
    if (xf86WcmIsUSBLine(local->fd)) {
	int	loop;
	
	SYSCALL(close(local->fd));

	for(loop=0; loop<common->wcmNumDevices; loop++) {
	    common->wcmDevices[loop]->read_input=xf86WcmReadUSBInput;
	}
	common->wcmOpen=xf86WcmUSBOpen;
	
	return xf86WcmUSBOpen(local);
    }
#endif
    
    DBG(1, ErrorF("initializing tablet\n"));    

    /* Set the speed of the serial link to 38400 */
#ifdef XFREE86_V4
   if (xf86SetSerialSpeed(local->fd, 38400) < 0) {
       return !Success;
   }
#else
    if (set_serial_speed(local->fd, B38400) == !Success)
        return !Success;
#endif
    
    /* Send reset to the tablet */
    SYSCALL(err = write(local->fd, WC_RESET_BAUD, strlen(WC_RESET_BAUD)));
    if (err == -1) {
	ErrorF("Wacom write error : %s\n", strerror(errno));
	return !Success;
    }
    
    /* Wait 250 mSecs */
    WAIT(250);

    /* Send reset to the tablet */
    SYSCALL(err = write(local->fd, WC_RESET, strlen(WC_RESET)));
    if (err == -1) {
	ErrorF("Wacom write error : %s\n", strerror(errno));
	return !Success;
    }
    
    /* Wait 75 mSecs */
    WAIT(75);

    /* Set the speed of the serial link to 19200 */
#ifdef XFREE86_V4
   if (xf86SetSerialSpeed(local->fd, 19200) < 0) {
       return !Success;
   }
#else
    if (set_serial_speed(local->fd, B19200) == !Success)
        return !Success;
#endif
    
    /* Send reset to the tablet */
    SYSCALL(err = write(local->fd, WC_RESET_BAUD, strlen(WC_RESET_BAUD)));
    if (err == -1) {
	ErrorF("Wacom write error : %s\n", strerror(errno));
	return !Success;
    }
    
    /* Wait 250 mSecs */
    WAIT(250);

    /* Send reset to the tablet */
    SYSCALL(err = write(local->fd, WC_RESET, strlen(WC_RESET)));
    if (err == -1) {
	ErrorF("Wacom write error : %s\n", strerror(errno));
	return !Success;
    }
    
    /* Wait 75 mSecs */
    WAIT(75);

    /* Set the speed of the serial link to 9600 */
#ifdef XFREE86_V4
   if (xf86SetSerialSpeed(local->fd, 9600) < 0) {
       return !Success;
   }
#else
    if (set_serial_speed(local->fd, B9600) == !Success)
        return !Success;
#endif
    
    /* Send reset to the tablet */
    SYSCALL(err = write(local->fd, WC_RESET_BAUD, strlen(WC_RESET_BAUD)));
    if (err == -1) {
	ErrorF("Wacom write error : %s\n", strerror(errno));
	return !Success;
    }
    
    /* Wait 250 mSecs */
    WAIT(250);

    SYSCALL(err = write(local->fd, WC_STOP, strlen(WC_STOP)));
    if (err == -1) {
	ErrorF("Wacom write error : %s\n", strerror(errno));
	return !Success;
    }

    /* Wait 30 mSecs */
    WAIT(30);

#ifdef XFREE86_V4
    xf86FlushInput(local->fd);
#else
    flush_input_fd(local->fd);
#endif

    DBG(2, ErrorF("reading model\n"));
    if (!send_request(local->fd, WC_MODEL, buffer)) {
	return !Success;
    }
    DBG(2, ErrorF("%s\n", buffer));
  
    if (xf86Verbose) {
	ErrorF("%s Wacom tablet model : %s\n", XCONFIG_PROBED, buffer+2);
    }
    
    /* Answer is in the form ~#Tablet-Model VRom_Version */
    /* look for the first V from the end of the string */
    /* this seems to be the better way to find the version of the ROM */
    for(loop=strlen(buffer); loop>=0 && *(buffer+loop) != 'V'; loop--);
    for(idx=loop; idx<strlen(buffer) && *(buffer+idx) != '-'; idx++);
    *(buffer+idx) = '\0';

    /* Extract version numbers */
    sscanf(buffer+loop+1, "%f", &version);

    if ((buffer[2] == 'G' && buffer[3] == 'D') ||
    	(buffer[2] == 'X' && buffer[3] == 'D')) {
	DBG(2, ErrorF("detected an Intuos model\n"));
	common->wcmProtocolLevel = 5;
	common->wcmMaxZ = 1023;		/* max Z value */
	common->wcmResolX = 2540;	/* X resolution in points/inch */
	common->wcmResolY = 2540;	/* Y resolution in points/inch */
	common->wcmResolZ = 2540;	/* Z resolution in points/inch */
	common->wcmPktLength = 9;	/* length of a packet */
	if (common->wcmThreshold == INVALID_THRESHOLD) {
	    common->wcmThreshold = -480; /* Threshold for counting pressure as a button */
	    if (xf86Verbose) {
		ErrorF("%s Wacom using pressure threshold of %d for button 1\n",
		       XCONFIG_PROBED, common->wcmThreshold);
	    }
	}
    }
	
    /* Tilt works on ROM 1.4 and above */
    DBG(2, ErrorF("wacom flags=%d ROM version=%f buffer=%s\n",
		  common->wcmFlags, version, buffer+loop+1));
    if (common->wcmProtocolLevel == 4 &&
	(common->wcmFlags & TILT_FLAG) && (version >= (float)1.4)) {
	common->wcmPktLength = 9;
    }

    /* Check for a PenPartner or Graphire model which doesn't answer WC_CONFIG
     * request. The Graphire model is handled like a PenPartner except that
     * it doesn't answer WC_COORD requests.
     */
    if ((buffer[2] == 'C' || buffer[2] == 'E') && buffer[3] == 'T') {
	if (buffer[2] == 'E') {
	    DBG(2, ErrorF("detected a Graphire model\n"));
	    common->wcmFlags |= GRAPHIRE_FLAG;
	    /* Graphire models don't answer WC_COORD requests */
	    common->wcmMaxX = 5103;
	    common->wcmMaxY = 3711;
	    common->wcmMaxZ = 512;
	}
	else {
	    DBG(2, ErrorF("detected a PenPartner model\n"));
	    common->wcmMaxZ = 256;
	}
	common->wcmResolX = 1000;
	common->wcmResolY = 1000;
	is_a_penpartner = 1;
    }
    else if (common->wcmProtocolLevel == 4 && !(common->wcmResolX && common->wcmResolY)) {
	DBG(2, ErrorF("reading config\n"));
	if (send_request(local->fd, WC_CONFIG, buffer)) {
	    DBG(2, ErrorF("%s\n", buffer));
	    /* The header string is simply a place to put the unwanted
	     * config header don't use buffer+xx because the header size
	     * varies on different tablets
	     */
	    if (sscanf(buffer, "%[^,],%d,%d,%d,%d", header, &a, &b, &common->wcmResolX, &common->wcmResolY) == 5) {
		DBG(6, ErrorF("WC_CONFIG Header = %s\n", header));
	    }
	    else {
		ErrorF("WACOM: unable to parse resolution. Using default.\n");
		common->wcmResolX = common->wcmResolY = 1270;
	    }
	}
	else {
	    ErrorF("WACOM: unable to read resolution. Using default.\n");
	    common->wcmResolX = common->wcmResolY = 1270;
	}
    }

    if (!(common->wcmFlags & GRAPHIRE_FLAG) && !(common->wcmMaxX && common->wcmMaxY)) {
	DBG(2, ErrorF("reading max coordinates\n"));
	if (!send_request(local->fd, WC_COORD, buffer)) {
	    ErrorF("WACOM: unable to read max coordinates. Use the MaxX and MaxY options.\n");
	    return !Success;
	}
	DBG(2, ErrorF("%s\n", buffer));
	if (sscanf(buffer+2, "%d,%d", &common->wcmMaxX, &common->wcmMaxY) != 2) {
	    ErrorF("WACOM: unable to parse max coordinates. Use the MaxX and MaxY options.\n");
	    return !Success;
	}
    }
    
    DBG(2, ErrorF("setup is max X=%d max Y=%d resol X=%d resol Y=%d\n",
		  common->wcmMaxX, common->wcmMaxY, common->wcmResolX,
		  common->wcmResolY));

    /* We can't change the resolution on PenPartner and Graphire models */
    if (!is_a_penpartner && common->wcmProtocolLevel == 4) {
	int resolX = common->wcmResolX, resolY = common->wcmResolY;
	
	/* Force the resolution.
	 */
        if (((float)version) >= 1.2) {
	    resolX = resolY = 2540;
	}
	sprintf(buffer, "%s%d\r", WC_NEW_RESOLUTION, resolX);
	SYSCALL(err = write(local->fd, buffer, strlen(buffer)));
	
	/* Verify the resolution change.
	 */
	DBG(2, ErrorF("rereading config\n"));
	if (send_request(local->fd, WC_CONFIG, buffer)) {
	    DBG(2, ErrorF("%s\n", buffer));
	    /* The header string is simply a place to put the unwanted
	     * config header don't use buffer+xx because the header size
	     * varies on different tablets
	     */
	    if (sscanf(buffer, "%[^,],%d,%d,%d,%d", header, &a, &b, &common->wcmResolX, &common->wcmResolY) == 5) {
		DBG(6, ErrorF("WC_CONFIG Header = %s\n", header));
	    }
	    else {
		ErrorF("WACOM: unable to reparse resolution. Using previous values.\n");
	    }
	}
	else {
	    ErrorF("WACOM: unable to reread resolution. Using previous values.\n");
	}
	
	/* The following couple of lines convert the MaxX and MaxY returned by
	 * the Wacom from 1270lpi to the Wacom's active resolution.
	 */
	common->wcmMaxX = (common->wcmMaxX / MAX_COORD_RES) * common->wcmResolX;
	common->wcmMaxY = (common->wcmMaxY / MAX_COORD_RES) * common->wcmResolY;
    }
    
    DBG(2, ErrorF("setup is max X=%d max Y=%d resol X=%d resol Y=%d\n",
		  common->wcmMaxX, common->wcmMaxY, common->wcmResolX,
		  common->wcmResolY));
  
    /* Send a setup string to the tablet */
    if (is_a_penpartner) {
	SYSCALL(err = write(local->fd, penpartner_setup_string,
			    strlen(penpartner_setup_string)));
    }
    else if (common->wcmProtocolLevel == 4) {
        SYSCALL(err = write(local->fd, WC_RESET, strlen(WC_RESET)));
	WAIT(75);
	SYSCALL(err = write(local->fd, setup_string, strlen(setup_string)));
    }
    else {
	SYSCALL(err = write(local->fd, intuos_setup_string,
			    strlen(intuos_setup_string)));
    }
    
    if (err == -1) {
	ErrorF("Wacom write error : %s\n", strerror(errno));
	return !Success;
    }

    /* Send the tilt mode command after setup because it must be enabled */
    /* after multi-mode to take precedence */
    if (common->wcmProtocolLevel == 4 && HANDLE_TILT(common)) {
	DBG(2, ErrorF("Sending tilt mode order\n"));
	
	SYSCALL(err = write(local->fd, WC_TILT_MODE, strlen(WC_TILT_MODE)));
	if (err == -1) {
	    ErrorF("Wacom write error : %s\n", strerror(errno));
	    return !Success;
	}
    }
  
    if (common->wcmSuppress < 0) {
	int	xratio = common->wcmMaxX/screenInfo.screens[0]->width;
	int	yratio = common->wcmMaxY/screenInfo.screens[0]->height;
	
	common->wcmSuppress = (xratio > yratio) ? yratio : xratio;
    }
    
    if (common->wcmSuppress > 100) {
	common->wcmSuppress = 99;
    }

    if (common->wcmProtocolLevel == 4) {
	char	buf[20];
      
	sprintf(buf, "%s%d\r", WC_SUPPRESS, common->wcmSuppress);
	SYSCALL(err = write(local->fd, buf, strlen(buf)));

	if (err == -1) {
	    ErrorF("Wacom write error : %s\n", strerror(errno));
	    return !Success;
	}
    }
    
    if (xf86Verbose)
	ErrorF("%s Wacom %s tablet maximum X=%d maximum Y=%d "
	       "X resolution=%d Y resolution=%d suppress=%d%s\n",
	       XCONFIG_PROBED, common->wcmProtocolLevel == 4 ? "IV" : "V",
	       common->wcmMaxX, common->wcmMaxY,
	       common->wcmResolX, common->wcmResolY, common->wcmSuppress,
	       HANDLE_TILT(common) ? " Tilt" : "");
  
    if (err <= 0) {
	SYSCALL(close(local->fd));
	return !Success;
    }

    /* change the serial speed if requested */
    if (common->wcmLinkSpeed > 9600) {
	if (common->wcmProtocolLevel == 5) {
	    char	*speed_init_string = WC_V_19200;
#ifndef XFREE86_V4
	    int		speed = B19200;
#endif
	    DBG(1, ErrorF("Switching serial link to %d\n", common->wcmLinkSpeed));

	    if (common->wcmLinkSpeed == 38400 && version < 2.0) {
		ErrorF("Wacom: 38400 speed not supported with this Intuos firmware (%f)\n", version);
		ErrorF("Switching to 19200\n");
		common->wcmLinkSpeed = 19200;
	    }

	    switch (common->wcmLinkSpeed) {
	    case 38400:
		speed_init_string = WC_V_38400;
#ifndef XFREE86_V4
		speed = B38400;
#endif
		break;

	    case 19200:
		speed_init_string = WC_V_19200;
#ifndef XFREE86_V4
		speed = B19200;
#endif
		break;
	    }
	    /* Switch the tablet to the requested speed */
	    SYSCALL(err = write(local->fd, speed_init_string, strlen(speed_init_string)));
	    if (err == -1) {
		ErrorF("Wacom write error : %s\n", strerror(errno));
		return !Success;
	    }
	    
	    /* Wait 75 mSecs */
	    WAIT(75);
    
	    /* Set the speed of the serial link to requested speed */
#ifdef XFREE86_V4
	    if (xf86SetSerialSpeed(local->fd, common->wcmLinkSpeed) < 0) {
		return !Success;
	    }
#else
	    if (set_serial_speed(local->fd, speed) == !Success)
		return !Success;
#endif
	}
	else {
	    ErrorF("Changing the speed of a wacom IV device is not yet implemented\n");
	}
    }

    /* Tell the tablet to start sending coordinates */
    SYSCALL(err = write(local->fd, WC_START, strlen(WC_START)));

    if (err == -1) {
	ErrorF("Wacom write error : %s\n", strerror(errno));
	return !Success;
    }

    return Success;
}

/*
 ***************************************************************************
 *
 * xf86WcmOpenDevice --
 *	Open the physical device and init information structs.
 *
 ***************************************************************************
 */
static int
xf86WcmOpenDevice(DeviceIntPtr       pWcm)
{
    LocalDevicePtr	local = (LocalDevicePtr)pWcm->public.devicePrivate;
    WacomDevicePtr	priv = (WacomDevicePtr)PRIVATE(pWcm);
    WacomCommonPtr	common = priv->common;
    double		screenRatio, tabletRatio;
    int			gap;
    int			loop;
    int			screen_idx = 0;
    
    if (local->fd < 0) {
        if (common->wcmInitNumber > 2 ||
	    priv->initNumber == common->wcmInitNumber) {
	    if (common->wcmOpen(local) != Success) {
	        if (local->fd >= 0) {
		    SYSCALL(close(local->fd));
	        }
	        local->fd = -1;
	    }
	    else {
	        /* report the file descriptor to all devices */
	        for(loop=0; loop<common->wcmNumDevices; loop++) {
		    common->wcmDevices[loop]->fd = local->fd;
	        }
	    }
	    common->wcmInitNumber++;
	    priv->initNumber = common->wcmInitNumber;
	}
	else {
	  priv->initNumber = common->wcmInitNumber;
	}
    }

    if (local->fd != -1 &&
	priv->factorX == 0.0) {
	
	if (priv->bottomX == 0) priv->bottomX = common->wcmMaxX;

	if (priv->bottomY == 0) priv->bottomY = common->wcmMaxY;

	/* Verify Box validity */

	if (priv->topX > common->wcmMaxX ||
	    priv->topX < 0) {
	    ErrorF("Wacom invalid TopX (%d) reseting to 0\n", priv->topX);
	    priv->topX = 0;
	}

	if (priv->topY > common->wcmMaxY ||
	    priv->topY < 0) {
	    ErrorF("Wacom invalid TopY (%d) reseting to 0\n", priv->topY);
	    priv->topY = 0;
	}

	if (priv->bottomX > common->wcmMaxX ||
	    priv->bottomX < priv->topX) {
	    ErrorF("Wacom invalid BottomX (%d) reseting to %d\n",
		   priv->bottomX, common->wcmMaxX);
	    priv->bottomX = common->wcmMaxX;
	}

	if (priv->bottomY > common->wcmMaxY ||
	    priv->bottomY < priv->topY) {
	    ErrorF("Wacom invalid BottomY (%d) reseting to %d\n",
		   priv->bottomY, common->wcmMaxY);
	    priv->bottomY = common->wcmMaxY;
	}

	if (priv->screen_no != -1 &&
	    (priv->screen_no >= screenInfo.numScreens ||
	     priv->screen_no < 0)) {
	    ErrorF("%s: invalid screen number %d, resetting to 0\n",
		   local->name, priv->screen_no);
	    priv->screen_no = 0;
	}

	/* Calculate the ratio according to KeepShape, TopX and TopY */

	if (priv->screen_no != -1) {
	    screen_idx = priv->screen_no;
	}
	
	if (priv->flags & KEEP_SHAPE_FLAG) {
	    screenRatio = ((double) screenInfo.screens[screen_idx]->width)
		/ screenInfo.screens[screen_idx]->height;

	    tabletRatio = ((double) (common->wcmMaxX - priv->topX))
		/ (common->wcmMaxY - priv->topY);

	    DBG(2, ErrorF("screenRatio = %.3g, tabletRatio = %.3g\n",
			  screenRatio, tabletRatio));

	    if (screenRatio > tabletRatio) {
		gap = common->wcmMaxY * (1 - tabletRatio/screenRatio);
		priv->bottomX = common->wcmMaxX;
		priv->bottomY = common->wcmMaxY - gap;
	    } else {
		gap = common->wcmMaxX * (1 - screenRatio/tabletRatio);
		priv->bottomX = common->wcmMaxX - gap;
		priv->bottomY = common->wcmMaxY;
	    }
	}
	priv->factorX = ((double) screenInfo.screens[0]->width)
	    / (priv->bottomX - priv->topX);
	priv->factorY = ((double) screenInfo.screens[0]->height)
	    / (priv->bottomY - priv->topY);
    
	if (xf86Verbose)
	    ErrorF("%s Wacom tablet top X=%d top Y=%d "
		   "bottom X=%d bottom Y=%d\n",
		   XCONFIG_PROBED, priv->topX, priv->topY,
		   priv->bottomX, priv->bottomY);
	
	DBG(2, ErrorF("X factor = %.3g, Y factor = %.3g\n",
		      priv->factorX, priv->factorY));
    }

    /* Check threshold correctness */
    DBG(2, ErrorF("Threshold=%d\n", common->wcmThreshold));
    
    if (common->wcmThreshold > common->wcmMaxZ ||
	common->wcmThreshold < 0) {
	if (((common->wcmProtocolLevel == 5) ||
	     (common->wcmFlags & GRAPHIRE_FLAG)) &&
	    xf86Verbose &&
	    common->wcmThreshold != INVALID_THRESHOLD)
	    ErrorF("%s Wacom invalid threshold %d. Reset to %d\n",
		   XCONFIG_PROBED, common->wcmThreshold, common->wcmMaxZ / 3);
	common->wcmThreshold = common->wcmMaxZ / 3;
    }
    DBG(2, ErrorF("New threshold=%d\n", common->wcmThreshold));    

    /* Set the real values */
    InitValuatorAxisStruct(pWcm,
			   0,
			   0,		/* min val */
			   priv->bottomX - priv->topX, /* max val */
			   mils(common->wcmResolX), /* resolution */
			   0,		/* min_res */
			   mils(common->wcmResolX)); /* max_res */
    InitValuatorAxisStruct(pWcm,
			   1,
			   0,		/* min val */
			   priv->bottomY - priv->topY, /* max val */
			   mils(common->wcmResolY), /* resolution */
			   0,		/* min_res */
			   mils(common->wcmResolY)); /* max_res */
    InitValuatorAxisStruct(pWcm,
			   2,
			   0,		/* min val */
			   common->wcmMaxZ, /* max val */
			   mils(common->wcmResolZ), /* resolution */
			   0,		/* min_res */
			   mils(common->wcmResolZ)); /* max_res */
    InitValuatorAxisStruct(pWcm,
			   3,
			   -64,		/* min val */
			   63,		/* max val */
			   128,		/* resolution ??? */
			   0,
			   128);
    InitValuatorAxisStruct(pWcm,
			   4,
			   -64,		/* min val */
			   63,		/* max val */
			   128,		/* resolution ??? */
			   0,
			   128);
    InitValuatorAxisStruct(pWcm,
			   5,
			   0,		/* min val */
			   1023,        /* max val */
			   128,		/* resolution ??? */
			   0,
			   128);
    
    return (local->fd != -1);
}

/*
 ***************************************************************************
 *
 * xf86WcmClose --
 *
 ***************************************************************************
 */
static void
xf86WcmClose(LocalDevicePtr	local)
{
    WacomDevicePtr	priv = (WacomDevicePtr)local->private;
    WacomCommonPtr	common = priv->common;
    int			loop;
    int			num = 0;
    
    for(loop=0; loop<common->wcmNumDevices; loop++) {
	if (common->wcmDevices[loop]->fd >= 0) {
	    num++;
	}
    }
    DBG(4, ErrorF("Wacom number of open devices = %d\n", num));
    
    if (num == 1) {		    
	SYSCALL(close(local->fd));
    }
    
    local->fd = -1;
}

/*
 ***************************************************************************
 *
 * xf86WcmProc --
 *      Handle the initialization, etc. of a wacom
 *
 ***************************************************************************
 */
static int
xf86WcmProc(DeviceIntPtr       pWcm,
	    int                what)
{
    CARD8                 map[(32 << 4) + 1];
    int                   nbaxes;
    int                   nbbuttons;
    int                   loop;
    LocalDevicePtr        local = (LocalDevicePtr)pWcm->public.devicePrivate;
    WacomDevicePtr        priv = (WacomDevicePtr)PRIVATE(pWcm);
  
    DBG(2, ErrorF("BEGIN xf86WcmProc dev=0x%x priv=0x%x type=%s flags=%d what=%d\n",
		  pWcm, priv, (DEVICE_ID(priv->flags) == STYLUS_ID) ? "stylus" :
		  (DEVICE_ID(priv->flags) == CURSOR_ID) ? "cursor" : "eraser",
		  priv->flags, what));
  
    switch (what)
	{
	case DEVICE_INIT: 
	    DBG(1, ErrorF("xf86WcmProc pWcm=0x%x what=INIT\n", pWcm));
      
	    nbaxes = 6;		/* X, Y, Pressure, Tilt-X, Tilt-Y, Wheel */
	    
	    switch(DEVICE_ID(priv->flags)) {
	    case ERASER_ID:
		nbbuttons = 1;
		break;
	    case STYLUS_ID:
		nbbuttons = 4;
		break;
	    default:
		nbbuttons = 16;
		break;
	    }
	    
	    for(loop=1; loop<=nbbuttons; loop++) map[loop] = loop;

	    if (InitButtonClassDeviceStruct(pWcm,
					    nbbuttons,
					    map) == FALSE) {
		ErrorF("unable to allocate Button class device\n");
		return !Success;
	    }
      
	    if (InitFocusClassDeviceStruct(pWcm) == FALSE) {
		ErrorF("unable to init Focus class device\n");
		return !Success;
	    }
          
	    if (InitPtrFeedbackClassDeviceStruct(pWcm,
						 xf86WcmControlProc) == FALSE) {
		ErrorF("unable to init ptr feedback\n");
		return !Success;
	    }
	    
	    if (InitProximityClassDeviceStruct(pWcm) == FALSE) {
		ErrorF("unable to init proximity class device\n");
		return !Success;
	    }

	    if (InitKeyClassDeviceStruct(pWcm, &wacom_keysyms, NULL) == FALSE) {
		ErrorF("unable to init key class device\n"); 
		return !Success;
	    }

	    if (InitValuatorClassDeviceStruct(pWcm, 
					      nbaxes,
					      xf86GetMotionEvents, 
					      local->history_size,
					      ((priv->flags & ABSOLUTE_FLAG) 
					      ? Absolute : Relative) |
					      OutOfProximity)
		== FALSE) {
		ErrorF("unable to allocate Valuator class device\n"); 
		return !Success;
	    }
	    else {
		/* allocate the motion history buffer if needed */
		xf86MotionHistoryAllocate(local);
#ifndef XFREE86_V4
		AssignTypeAndName(pWcm, local->atom, local->name);
#endif
	    }

	    /* open the device to gather informations */
	    xf86WcmOpenDevice(pWcm);

	    break; 
      
	case DEVICE_ON:
	    DBG(1, ErrorF("xf86WcmProc pWcm=0x%x what=ON\n", pWcm));

	    if ((local->fd < 0) && (!xf86WcmOpenDevice(pWcm))) {
		return !Success;
	    }
#ifdef XFREE86_V4	    
	    xf86AddEnabledDevice(local);
#else
	    AddEnabledDevice(local->fd);
#endif
	    pWcm->public.on = TRUE;
	    break;
      
	case DEVICE_OFF:
	case DEVICE_CLOSE:
	    DBG(1, ErrorF("xf86WcmProc  pWcm=0x%x what=%s\n", pWcm,
			  (what == DEVICE_CLOSE) ? "CLOSE" : "OFF"));
	    if (local->fd >= 0) {
#ifdef XFREE86_V4	    
		xf86RemoveEnabledDevice(local);
#else
		RemoveEnabledDevice(local->fd);
#endif
		xf86WcmClose(local);
	    }
	    pWcm->public.on = FALSE;
	    break;
	    
	default:
	    ErrorF("wacom unsupported mode=%d\n", what);
	    return !Success;
	    break;
	}
    DBG(2, ErrorF("END   xf86WcmProc Success what=%d dev=0x%x priv=0x%x\n",
		  what, pWcm, priv));
    return Success;
}

/*
 ***************************************************************************
 *
 * xf86WcmChangeControl --
 *
 ***************************************************************************
 */
static int
xf86WcmChangeControl(LocalDevicePtr	local,
		     xDeviceCtl		*control)
{
    xDeviceResolutionCtl	*res;
    int				*resolutions;
    char			str[10];
  
    res = (xDeviceResolutionCtl *)control;
	
    if ((control->control != DEVICE_RESOLUTION) ||
	(res->num_valuators < 1))
	return (BadMatch);
  
    resolutions = (int *)(res +1);
    
    DBG(3, ErrorF("xf86WcmChangeControl changing to %d (suppressing under)\n",
		  resolutions[0]));

    sprintf(str, "SU%d\r", resolutions[0]);
    SYSCALL(write(local->fd, str, strlen(str)));
  
    return(Success);
}

/*
 ***************************************************************************
 *
 * xf86WcmSwitchMode --
 *
 ***************************************************************************
 */
static int
xf86WcmSwitchMode(ClientPtr	client,
		  DeviceIntPtr	dev,
		  int		mode)
{
    LocalDevicePtr        local = (LocalDevicePtr)dev->public.devicePrivate;
    WacomDevicePtr        priv = (WacomDevicePtr)local->private;

    DBG(3, ErrorF("xf86WcmSwitchMode dev=0x%x mode=%d\n", dev, mode));
  
    if (mode == Absolute) {
	priv->flags = priv->flags | ABSOLUTE_FLAG;
    }
    else {
	if (mode == Relative) {
	    priv->flags = priv->flags & ~ABSOLUTE_FLAG; 
	}
	else {
	    DBG(1, ErrorF("xf86WcmSwitchMode dev=0x%x invalid mode=%d\n", dev,
			  mode));
	    return BadMatch;
	}
    }
    return Success;
}

/*
 ***************************************************************************
 *
 * xf86WcmAllocate --
 *
 ***************************************************************************
 */
static LocalDevicePtr
xf86WcmAllocate(char *  name,
                int     flag)
{
    LocalDevicePtr        local;
    WacomDevicePtr        priv;
    WacomCommonPtr        common;

    priv = (WacomDevicePtr) xalloc(sizeof(WacomDeviceRec));
    if (!priv)
	return NULL;

    common = (WacomCommonPtr) xalloc(sizeof(WacomCommonRec));
    if (!common) {
	xfree(priv);
	return NULL;
    }

#ifdef XFREE86_V4
    local = xf86AllocateInput(wcmDrv, 0);
#else
    local = (LocalDevicePtr) xalloc(sizeof(LocalDeviceRec));
#endif
    if (!local) {
	xfree(priv);
	xfree(common);
	return NULL;
    }

    local->name = name;
    local->flags = 0;
#ifndef XFREE86_V4
    local->device_config = xf86WcmConfig;
#endif 
    local->device_control = xf86WcmProc;
    local->read_input = xf86WcmReadInput;
    local->control_proc = xf86WcmChangeControl;
    local->close_proc = xf86WcmClose;
    local->switch_mode = xf86WcmSwitchMode;
    local->conversion_proc = xf86WcmConvert;
    local->reverse_conversion_proc = xf86WcmReverseConvert;
    local->fd = -1;
    local->atom = 0;
    local->dev = NULL;
    local->private = priv;
    local->private_flags = 0;
    local->history_size  = 0;
    local->old_x = -1;
    local->old_y = -1;
    
    priv->flags = flag;			/* various flags (device type, absolute, first touch...) */
    priv->oldX = -1;			/* previous X position */
    priv->oldY = -1;			/* previous Y position */
    priv->oldZ = -1;			/* previous pressure */
    priv->oldTiltX = -1;		/* previous tilt in x direction */
    priv->oldTiltY = -1;		/* previous tilt in y direction */
    priv->oldButtons = 0;		/* previous buttons state */
    priv->oldProximity = 1;		/* previous proximity */
    priv->oldWheel = 0;			/* previous wheel */
    priv->topX = 0;			/* X top */
    priv->topY = 0;			/* Y top */
    priv->bottomX = 0;			/* X bottom */
    priv->bottomY = 0;			/* Y bottom */
    priv->factorX = 0.0;		/* X factor */
    priv->factorY = 0.0;		/* Y factor */
    priv->common = common;		/* common info pointer */
    priv->oldProximity = 0;		/* previous proximity */
    priv->serial = 0;		        /* serial number */
    priv->initNumber = 0;	        /* magic number for the init phasis */
    priv->screen_no = -1;		/* associated screen */

	/* JEJ - throttle sampling code */
	priv->throttleValue = 0;
	priv->throttleStart = 0;
	priv->throttleLimit = -1;
    
    common->wcmDevice = "";		/* device file name */
    common->wcmSuppress = -1;		/* transmit position if increment is superior */
    common->wcmFlags = 0;		/* various flags */
    common->wcmDevices = (LocalDevicePtr*) xalloc(sizeof(LocalDevicePtr));
    common->wcmDevices[0] = local;
    common->wcmNumDevices = 1;		/* number of devices */
    common->wcmIndex = 0;		/* number of bytes read */
    common->wcmPktLength = 7;		/* length of a packet */
    common->wcmMaxX = 0;		/* max X value */
    common->wcmMaxY = 0;		/* max Y value */
    common->wcmMaxZ = DEFAULT_MAXZ;	/* max Z value */
    common->wcmResolX = 0;		/* X resolution in points/inch */
    common->wcmResolY = 0;		/* Y resolution in points/inch */
    common->wcmResolZ = 1270;		/* Z resolution in points/inch */
    common->wcmHasEraser = (flag & ERASER_ID) ? TRUE : FALSE;	/* True if an eraser has been configured */
    common->wcmStylusSide = TRUE;	/* eraser or stylus ? */
    common->wcmStylusProximity = FALSE;	/* a stylus is in proximity ? */
    common->wcmProtocolLevel = 4;	/* protocol level */
    common->wcmThreshold = INVALID_THRESHOLD; /* button 1 threshold for some tablet models */
    common->wcmInitNumber = 0;	        /* magic number for the init phasis */
    common->wcmLinkSpeed = 9600;        /* serial link speed */
    common->wcmOpen = xf86WcmOpen;	/* function used to open the line (serial or USB) */
    common->wcmLastSerial = 0;		/* last device (used by the USB part) */
    common->wcmLastTool = -1;		/* last tool (used by the USB part) */
    return local;
}

/*
 ***************************************************************************
 *
 * xf86WcmAllocateStylus --
 *
 ***************************************************************************
 */
static LocalDevicePtr
xf86WcmAllocateStylus()
{
    LocalDevicePtr        local = xf86WcmAllocate(XI_STYLUS, STYLUS_ID);

    if (local)
	local->type_name = "Wacom Stylus";
    return local;
}

/*
 ***************************************************************************
 *
 * xf86WcmAllocateCursor --
 *
 ***************************************************************************
 */
static LocalDevicePtr
xf86WcmAllocateCursor()
{
    LocalDevicePtr        local = xf86WcmAllocate(XI_CURSOR, CURSOR_ID);

    if (local)
	local->type_name = "Wacom Cursor";
    return local;
}

/*
 ***************************************************************************
 *
 * xf86WcmAllocateEraser --
 *
 ***************************************************************************
 */
static LocalDevicePtr
xf86WcmAllocateEraser()
{
    LocalDevicePtr        local = xf86WcmAllocate(XI_ERASER, ABSOLUTE_FLAG|ERASER_ID);

    if (local)
	local->type_name = "Wacom Eraser";
    return local;
}

/*
 ***************************************************************************
 *
 * Wacom Stylus device association --
 *
 ***************************************************************************
 */
DeviceAssocRec wacom_stylus_assoc =
{
    STYLUS_SECTION_NAME,		/* config_section_name */
    xf86WcmAllocateStylus		/* device_allocate */
};

/*
 ***************************************************************************
 *
 * Wacom Cursor device association --
 *
 ***************************************************************************
 */
DeviceAssocRec wacom_cursor_assoc =
{
    CURSOR_SECTION_NAME,		/* config_section_name */
    xf86WcmAllocateCursor		/* device_allocate */
};

/*
 ***************************************************************************
 *
 * Wacom Eraser device association --
 *
 ***************************************************************************
 */
DeviceAssocRec wacom_eraser_assoc =
{
    ERASER_SECTION_NAME,		/* config_section_name */
    xf86WcmAllocateEraser		/* device_allocate */
};

#ifndef XFREE86_V4
#ifdef DYNAMIC_MODULE
/*
 ***************************************************************************
 *
 * entry point of dynamic loading
 *
 ***************************************************************************
 */
int
#ifndef DLSYM_BUG
init_module(unsigned long	server_version)
#else
init_xf86Wacom(unsigned long    server_version)
#endif
{
    xf86AddDeviceAssoc(&wacom_stylus_assoc);
    xf86AddDeviceAssoc(&wacom_cursor_assoc);
    xf86AddDeviceAssoc(&wacom_eraser_assoc);

    if (server_version != XF86_VERSION_CURRENT) {
	ErrorF("Warning: Wacom module compiled for version%s\n", XF86_VERSION);
	return 0;
    } else {
	return 1;
    }
}
#endif /* DYNAMIC_MODULE */

#else /* XFREE86_V4 */

#include "wcm-beta.h"

/*
 * xf86WcmUninit --
 *
 * called when the device is no longer needed.
 */
static void
xf86WcmUninit(InputDriverPtr	drv,
	      LocalDevicePtr	local,
	      int flags)
{
    WacomDevicePtr	priv;

    priv = (WacomDevicePtr) local->private;
    
    DBG(1, ErrorF("xf86WcmUninit\n"));

	if (priv->flags & BETA_FLAG)
	{
		wcmBetaDeleteDevice(drv,local,flags);
		return;
	}
    
    xf86WcmProc(local->dev, DEVICE_OFF);
    
    xfree (priv);
    xf86DeleteInput(local, 0);    
}

/*
 * xf86WcmInit --
 *
 * called when the module subsection is found in XF86Config
 */
static InputInfoPtr
xf86WcmInit(InputDriverPtr	drv,
	    IDevPtr		dev,
	    int			flags)
{
    LocalDevicePtr	local = NULL;
    LocalDevicePtr	fakeLocal = NULL;
    WacomDevicePtr	priv = NULL;
    WacomCommonPtr	common = NULL;
    char		*s;
    LocalDevicePtr	localDevices;

    wcmDrv = drv;

    fakeLocal = (LocalDevicePtr) xcalloc(1, sizeof(LocalDeviceRec));
    if (!fakeLocal)
	return NULL;

    fakeLocal->conf_idev = dev;

    /* Force default serial port options to exist because the serial init
     * phasis is based on those values.
     */
    xf86CollectInputOptions(fakeLocal, default_options, NULL);

    /* Type is mandatory */
    s = xf86FindOptionValue(fakeLocal->options, "Type");

	/* Beta code jumps out here */
    if (s && (xf86NameCmp(s, "beta") == 0))
	{
		xfree(fakeLocal);
		return wcmBetaNewDevice(drv,dev,flags);
    }
	else if (s && (xf86NameCmp(s, "stylus") == 0)) {
	local = xf86WcmAllocateStylus();
    }
    else if (s && (xf86NameCmp(s, "cursor") == 0)) {
	local = xf86WcmAllocateCursor();
    }
    else if (s && (xf86NameCmp(s, "eraser") == 0)) {
	local = xf86WcmAllocateEraser();
    }
    else {
	xf86Msg(X_ERROR, "%s: No type or invalid type specified.\n"
		"Must be one of stylus, cursor or eraser\n",
		dev->identifier);
	goto SetupProc_fail;
    }
    
    if (!local) {
	xfree(fakeLocal);
	return NULL;
    }

    priv = (WacomDevicePtr) local->private;
    common = priv->common;

    local->options = fakeLocal->options;
    local->conf_idev = fakeLocal->conf_idev;
    local->name = dev->identifier;
    xfree(fakeLocal);
    
    /* Serial Device is mandatory */
    common->wcmDevice = xf86FindOptionValue(local->options, "Device");

    if (!common->wcmDevice) {
	xf86Msg (X_ERROR, "%s: No Device specified.\n", dev->identifier);
	goto SetupProc_fail;
    }

    /* Lookup to see if there is another wacom device sharing
     * the same serial line.
     */
    localDevices = xf86FirstLocalDevice();
    
    while(localDevices) {
	if ((local != localDevices) &&
	    (localDevices->device_control == xf86WcmProc) &&
	    (strcmp(((WacomDevicePtr)localDevices->private)->common->wcmDevice,
		    common->wcmDevice) == 0)) {
		DBG(2, ErrorF("xf86WcmConfig wacom port share between"
			      " %s and %s\n",
			      local->name, localDevices->name));
		((WacomDevicePtr) localDevices->private)->common->wcmHasEraser |= common->wcmHasEraser;
		xfree(common->wcmDevices);
		xfree(common);
		common = priv->common = ((WacomDevicePtr) localDevices->private)->common;
		common->wcmNumDevices++;
		common->wcmDevices = (LocalDevicePtr *) xrealloc(common->wcmDevices,
								 sizeof(LocalDevicePtr) * common->wcmNumDevices);
		common->wcmDevices[common->wcmNumDevices - 1] = local;
		break;
	}
	localDevices = localDevices->next;
    }

    /* Process the common options. */
    xf86ProcessCommonOptions(local, local->options);

    /* Optional configuration */

    xf86Msg(X_CONFIG, "%s serial device is %s\n", dev->identifier,
	    common->wcmDevice);

    debug_level = xf86SetIntOption(local->options, "DebugLevel", debug_level);
    if (debug_level > 0) {
	xf86Msg(X_CONFIG, "WACOM: debug level set to %d\n", debug_level);
    }

    s = xf86FindOptionValue(local->options, "Mode");

    if (s && (xf86NameCmp(s, "absolute") == 0)) {
	priv->flags = priv->flags | ABSOLUTE_FLAG;
    }
    else if (s && (xf86NameCmp(s, "relative") == 0)) {
	priv->flags = priv->flags & ~ABSOLUTE_FLAG;
    }
    else if (s) {
	xf86Msg(X_ERROR, "%s: invalid Mode (should be absolute or relative). Using default.\n",
		dev->identifier);
    }
    xf86Msg(X_CONFIG, "%s is in %s mode\n", local->name,
	    (priv->flags & ABSOLUTE_FLAG) ? "absolute" : "relative");	    

    common->wcmSuppress = xf86SetIntOption(local->options, "Suppress", common->wcmSuppress);
    if (common->wcmSuppress != -1) {
	xf86Msg(X_CONFIG, "WACOM: suppress value is %d\n", XCONFIG_GIVEN,
		common->wcmSuppress);      
    }
    
    if (xf86SetBoolOption(local->options, "Tilt", (common->wcmFlags & TILT_FLAG))) {
	common->wcmFlags |= TILT_FLAG;
    }

#ifdef LINUX_INPUT
    if (xf86SetBoolOption(local->options, "USB", (common->wcmOpen == xf86WcmUSBOpen))) {
	/* best effort attempt at loading the wacom and evdev kernel modules */
	(void)xf86LoadKernelModule("wacom");
	(void)xf86LoadKernelModule("evdev");
    
	local->read_input=xf86WcmReadUSBInput;
	common->wcmOpen=xf86WcmUSBOpen;
	xf86Msg(X_CONFIG, "%s: reading USB link\n", dev->identifier);
	}
#else
    if (xf86SetBoolOption(local->options, "USB", 0)) {
	ErrorF("The USB version of the driver isn't available for your platform\n");
    }
#endif

    priv->screen_no = xf86SetIntOption(local->options, "ScreenNo", -1);
    if (priv->screen_no != -1) {
	xf86Msg(X_CONFIG, "%s: attached screen number %d\n", dev->identifier,
		priv->screen_no);
    }
	
    if (xf86SetBoolOption(local->options, "KeepShape", 0)) {
	priv->flags |= KEEP_SHAPE_FLAG;
	xf86Msg(X_CONFIG, "%s: keeps shape\n", dev->identifier);
    }

    priv->topX = xf86SetIntOption(local->options, "TopX", 0);
    if (priv->topX != 0) {
	xf86Msg(X_CONFIG, "%s: top x = %d\n", dev->identifier, priv->topX);
    }
    priv->topY = xf86SetIntOption(local->options, "TopY", 0);
    if (priv->topY != 0) {
	xf86Msg(X_CONFIG, "%s: top x = %d\n", dev->identifier, priv->topY);
    }
    priv->bottomX = xf86SetIntOption(local->options, "BottomX", 0);
    if (priv->bottomX != 0) {
	xf86Msg(X_CONFIG, "%s: bottom x = %d\n", dev->identifier,
		priv->bottomX);
    }
    priv->bottomY = xf86SetIntOption(local->options, "BottomY", 0);
    if (priv->bottomY != 0) {
	xf86Msg(X_CONFIG, "%s: bottom x = %d\n", dev->identifier,
		priv->bottomY);
    }
    priv->serial = xf86SetIntOption(local->options, "Serial", 0);
    if (priv->bottomY != 0) {
	xf86Msg(X_CONFIG, "%s: serial number = %u\n", dev->identifier,
		priv->serial);
    }
    common->wcmThreshold = xf86SetIntOption(local->options, "Threshold", common->wcmThreshold);
    if (common->wcmThreshold != INVALID_THRESHOLD) {
	xf86Msg(X_CONFIG, "%s: threshold = %d\n", dev->identifier,
		common->wcmThreshold);
    }
    common->wcmMaxX = xf86SetIntOption(local->options, "MaxX", common->wcmMaxX);
    if (common->wcmMaxX != 0) {
	xf86Msg(X_CONFIG, "%s: max x = %d\n", dev->identifier,
		common->wcmMaxX);
    }
    common->wcmMaxY = xf86SetIntOption(local->options, "MaxY", common->wcmMaxY);
    if (common->wcmMaxY != 0) {
	xf86Msg(X_CONFIG, "%s: max x = %d\n", dev->identifier,
		common->wcmMaxY);
    }
    common->wcmMaxZ = xf86SetIntOption(local->options, "MaxZ", common->wcmMaxZ);
    if (common->wcmMaxZ != DEFAULT_MAXZ) {
	xf86Msg(X_CONFIG, "%s: max x = %d\n", dev->identifier,
		common->wcmMaxZ);
    }
    common->wcmResolX = xf86SetIntOption(local->options, "ResolutionX", common->wcmResolX);
    if (common->wcmResolX != 0) {
	xf86Msg(X_CONFIG, "%s: resol x = %d\n", dev->identifier,
		common->wcmResolX);
    }
    common->wcmResolY = xf86SetIntOption(local->options, "ResolutionY", common->wcmResolY);
    if (common->wcmResolY != 0) {
	xf86Msg(X_CONFIG, "%s: resol x = %d\n", dev->identifier,
		common->wcmResolY);
    }
    common->wcmResolZ = xf86SetIntOption(local->options, "ResolutionZ", common->wcmResolZ);
    if (common->wcmResolZ != 0) {
	xf86Msg(X_CONFIG, "%s: resol x = %d\n", dev->identifier,
		common->wcmResolZ);
    }

    {
	int	val;
	val = xf86SetIntOption(local->options, "BaudRate", 0);

	switch(val) {
	case 38400:
	    common->wcmLinkSpeed = 38400;
	    break;
	case 19200:
	    common->wcmLinkSpeed = 19200;
	    break;
	case 9600:
	    common->wcmLinkSpeed = 9600;
	    break;
	default:
	    xf86Msg(X_ERROR, "%s: Illegal speed value (must be 9600 or 19200 or 38400).", dev->identifier);
	    break;
	}
	if (xf86Verbose)
	    xf86Msg(X_CONFIG, "%s: serial speed %u\n", dev->identifier,
		    val);
    }
    /* mark the device configured */
    local->flags |= XI86_POINTER_CAPABLE | XI86_CONFIGURED;

    /* return the LocalDevice */
    return (local);

  SetupProc_fail:
    if (common)
	xfree(common);
    if (priv)
	xfree(priv);
    if (local)
	xfree(local);
    return NULL;
}

#ifdef XFree86LOADER
static
#endif
InputDriverRec WACOM = {
    1,				/* driver version */
    "wacom",			/* driver name */
    NULL,				/* identify */
    xf86WcmInit,		/* pre-init */
    xf86WcmUninit,		/* un-init */
    NULL,			/* module */
    0				/* ref count */
};

/*
 ***************************************************************************
 *
 * Dynamic loading functions
 *
 ***************************************************************************
 */
#ifdef XFree86LOADER
/*
 * xf86WcmUnplug --
 *
 * called when the module subsection is found in XF86Config
 */
static void
xf86WcmUnplug(pointer	p)
{
    DBG(1, ErrorF("xf86WcmUnplug\n"));
}

/*
 * xf86WcmPlug --
 *
 * called when the module subsection is found in XF86Config
 */
static pointer
xf86WcmPlug(pointer	module,
	    pointer	options,
	    int		*errmaj,
	    int		*errmin)
{
    xf86Msg(X_INFO, "Wacom driver level: %s\n", identification+strlen("$Identification: "));
	    
    xf86AddInputDriver(&WACOM, module, 0);

    return module;
}

static XF86ModuleVersionInfo xf86WcmVersionRec =
{
    "wacom",
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XF86_VERSION_CURRENT,
    1, 0, 0,
    ABI_CLASS_XINPUT,
    ABI_XINPUT_VERSION,
    MOD_CLASS_XINPUT,
    {0, 0, 0, 0}		/* signature, to be patched into the file by */
				/* a tool */
};

XF86ModuleData wacomModuleData = {&xf86WcmVersionRec,
				  xf86WcmPlug,
				  xf86WcmUnplug};

#endif /* XFree86LOADER */
#endif /* XFREE86_V4 */

/*
 * Local variables:
 * change-log-default-name: "~/xinput.log"
 * c-file-style: "bsd"
 * End:
 */
/* end of xf86Wacom.c */
