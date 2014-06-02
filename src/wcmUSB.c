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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "xf86Wacom.h"

#include <asm/types.h>
#include <linux/input.h>
#include <sys/utsname.h>
#include <linux/version.h>

#define MAX_USB_EVENTS 32

typedef struct {
	int wcmLastToolSerial;
	int wcmDeviceType;
	Bool wcmPenTouch;
	Bool wcmUseMT;
	int wcmMTChannel;
	int wcmEventCnt;
	struct input_event wcmEvents[MAX_USB_EVENTS];
	int nbuttons;                /* total number of buttons */
	int npadkeys;                /* number of pad keys in the above array */
	int padkey_code[WCM_MAX_BUTTONS];/* hardware codes for buttons */
	int lastChannel;
} wcmUSBData;

static Bool usbDetect(InputInfoPtr);
static Bool usbWcmInit(InputInfoPtr pDev, char* id, float *version);
static int usbProbeKeys(InputInfoPtr pInfo);
static int usbStart(InputInfoPtr pInfo);
static void usbInitProtocol5(WacomCommonPtr common, const char* id,
	float version);
static void usbInitProtocol4(WacomCommonPtr common, const char* id,
	float version);
int usbWcmGetRanges(InputInfoPtr pInfo);
static int usbParse(InputInfoPtr pInfo, const unsigned char* data, int len);
static int usbDetectConfig(InputInfoPtr pInfo);
static void usbParseEvent(InputInfoPtr pInfo,
	const struct input_event* event);
static void usbParseSynEvent(InputInfoPtr pInfo,
			     const struct input_event *event);
static void usbDispatchEvents(InputInfoPtr pInfo);
static int usbChooseChannel(WacomCommonPtr common, int device_type, unsigned int serial);

	WacomDeviceClass gWacomUSBDevice =
	{
		usbDetect,
		NULL, /* no USB-specific options */
		usbWcmInit,
		usbProbeKeys
	};

#define DEFINE_MODEL(mname, identifier, protocol) \
static struct _WacomModel mname =		\
{						\
	.name = identifier,			\
	.Initialize = usbInitProtocol##protocol,\
	.GetResolution = NULL,			\
	.GetRanges = usbWcmGetRanges,		\
	.Start = usbStart,			\
	.Parse = usbParse,			\
	.DetectConfig = usbDetectConfig,	\
};

DEFINE_MODEL(usbUnknown,	"Unknown USB",		5)
DEFINE_MODEL(usbPenPartner,	"USB PenPartner",	4);
DEFINE_MODEL(usbGraphire,	"USB Graphire",		4);
DEFINE_MODEL(usbGraphire2,	"USB Graphire2",	4);
DEFINE_MODEL(usbGraphire3,	"USB Graphire3",	4);
DEFINE_MODEL(usbGraphire4,	"USB Graphire4",	4);
DEFINE_MODEL(usbBamboo,		"USB Bamboo",		4);
DEFINE_MODEL(usbBamboo1,	"USB Bamboo1",		4);
DEFINE_MODEL(usbBambooFun,	"USB BambooFun",	4);
DEFINE_MODEL(usbCintiq,		"USB PL/Cintiq",	4);
DEFINE_MODEL(usbCintiqPartner,	"USB CintiqPartner",	4);
DEFINE_MODEL(usbIntuos,		"USB Intuos1",		5);
DEFINE_MODEL(usbIntuos2,	"USB Intuos2",		5);
DEFINE_MODEL(usbIntuos3,	"USB Intuos3",		5);
DEFINE_MODEL(usbIntuos4,	"USB Intuos4",		5);
DEFINE_MODEL(usbIntuos5,	"USB Intuos5",		5);
DEFINE_MODEL(usbIntuosPro,	"USB Intuos Pro",	5);
DEFINE_MODEL(usbVolito,		"USB Volito",		4);
DEFINE_MODEL(usbVolito2,	"USB Volito2",		4);
DEFINE_MODEL(usbCintiqV5,	"USB CintiqV5",		5);
DEFINE_MODEL(usbTabletPC,	"USB TabletPC",		4);

/*****************************************************************************
 * usbDetect --
 *   Test if the attached device is USB.
 ****************************************************************************/

static Bool usbDetect(InputInfoPtr pInfo)
{
	int version;
	int err;
#ifdef DEBUG
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;

	DBG(1, priv, "\n");
#endif

	SYSCALL(err = ioctl(pInfo->fd, EVIOCGVERSION, &version));

	if (err < 0)
	{
		xf86Msg(X_ERROR, "%s: usbDetect: can not ioctl version\n", pInfo->name);
		return 0;
	}

	return 1;
}

/*****************************************************************************
 * usbStart --
 ****************************************************************************/
static int
usbStart(InputInfoPtr pInfo)
{
	int err;

	if (xf86CheckBoolOption(pInfo->options, "GrabDevice", 0))
	{
		/* Try to grab the event device so that data don't leak to /dev/input/mice */
		SYSCALL(err = ioctl(pInfo->fd, EVIOCGRAB, (pointer)1));

		/* this is called for all tools, so all but the first one fails with
		 * EBUSY */
		if (err < 0 && errno != EBUSY)
			xf86Msg(X_ERROR, "%s: Wacom X driver can't grab event device (%s)\n",
				pInfo->name, strerror(errno));
	}
	return Success;
}

/* Key codes used to mark tablet buttons -- must be in sync
 * with the keycode array in wacom kernel drivers.
 */
static unsigned short padkey_codes [] = {
	BTN_0, BTN_1, BTN_2, BTN_3, BTN_4,
	BTN_5, BTN_6, BTN_7, BTN_8, BTN_9,
	BTN_A, BTN_B, BTN_C, BTN_X, BTN_Y, BTN_Z,
	BTN_BASE, BTN_BASE2, BTN_BASE3,
	BTN_BASE4, BTN_BASE5, BTN_BASE6,
	BTN_TL, BTN_TR, BTN_TL2, BTN_TR2, BTN_SELECT
};

/* Fixed mapped stylus and mouse buttons */

#define WCM_USB_MAX_MOUSE_BUTTONS 5
#define WCM_USB_MAX_STYLUS_BUTTONS 3

static unsigned short mouse_codes [] = {
	BTN_LEFT, BTN_MIDDLE, BTN_RIGHT, BTN_BACK, BTN_FORWARD,
	BTN_SIDE, BTN_EXTRA
};

static struct WacomModelDesc
{
	const unsigned int vendor_id;
	const unsigned int model_id;
	int yRes; /* tablet Y resolution in units/meter */
	int xRes; /* tablet X resolution in units/meter */
	WacomModelPtr model;
	const char *name;
} WacomModelDesc [] =
{
	{ WACOM_VENDOR_ID, 0x00,  39370,  39370, &usbPenPartner, "PenPartner"		},
	{ WACOM_VENDOR_ID, 0x10,  80000,  80000, &usbGraphire,   "Graphire"		},
	{ WACOM_VENDOR_ID, 0x11,  80000,  80000, &usbGraphire2,  "Graphire2 4x5"	},
	{ WACOM_VENDOR_ID, 0x12,  80000,  80000, &usbGraphire2,  "Graphire2 5x7"	},
	{ WACOM_VENDOR_ID, 0x13,  80000,  80000, &usbGraphire3,  "Graphire3 4x5"	},
	{ WACOM_VENDOR_ID, 0x14,  80000,  80000, &usbGraphire3,  "Graphire3 6x8"	},
	{ WACOM_VENDOR_ID, 0x15,  80000,  80000, &usbGraphire4,  "Graphire4 4x5"	},
	{ WACOM_VENDOR_ID, 0x16,  80000,  80000, &usbGraphire4,  "Graphire4 6x8"	},
	{ WACOM_VENDOR_ID, 0x17, 100000, 100000, &usbBambooFun,  "BambooFun 4x5"	},
	{ WACOM_VENDOR_ID, 0x18, 100000, 100000, &usbBambooFun,  "BambooFun 6x8"	},
	{ WACOM_VENDOR_ID, 0x19,  80000,  80000, &usbBamboo1,    "Bamboo1 Medium"	},
	{ WACOM_VENDOR_ID, 0x81,  80000,  80000, &usbGraphire4,  "Graphire4 6x8 BlueTooth" },

	{ WACOM_VENDOR_ID, 0xD1, 100000, 100000, &usbBamboo,     "CTL-460"		},
	{ WACOM_VENDOR_ID, 0xD4, 100000, 100000, &usbBamboo,     "CTH-461"		},
	{ WACOM_VENDOR_ID, 0xD3, 100000, 100000, &usbBamboo,     "CTL-660"		},
	{ WACOM_VENDOR_ID, 0xD2, 100000, 100000, &usbBamboo,     "CTL-461/S"		},
	{ WACOM_VENDOR_ID, 0xD0, 100000, 100000, &usbBamboo,     "Bamboo Touch"		},
	{ WACOM_VENDOR_ID, 0xD6, 100000, 100000, &usbBamboo,     "CTH-460/K"		},
	{ WACOM_VENDOR_ID, 0xD7, 100000, 100000, &usbBamboo,     "CTH-461/S"		},
	{ WACOM_VENDOR_ID, 0xD8, 100000, 100000, &usbBamboo,     "CTH-661/S1"		},
	{ WACOM_VENDOR_ID, 0xDA, 100000, 100000, &usbBamboo,     "CTH-461/L"		},
	{ WACOM_VENDOR_ID, 0xDB, 100000, 100000, &usbBamboo,     "CTH-661/L"		},

	{ WACOM_VENDOR_ID, 0x20, 100000, 100000, &usbIntuos,     "Intuos 4x5"		},
	{ WACOM_VENDOR_ID, 0x21, 100000, 100000, &usbIntuos,     "Intuos 6x8"		},
	{ WACOM_VENDOR_ID, 0x22, 100000, 100000, &usbIntuos,     "Intuos 9x12"		},
	{ WACOM_VENDOR_ID, 0x23, 100000, 100000, &usbIntuos,     "Intuos 12x12"		},
	{ WACOM_VENDOR_ID, 0x24, 100000, 100000, &usbIntuos,     "Intuos 12x18"		},

	{ WACOM_VENDOR_ID, 0x03,  20000,  20000, &usbCintiqPartner, "PTU600"		},

	{ WACOM_VENDOR_ID, 0x30,  20000,  20000, &usbCintiq,     "PL400"		},
	{ WACOM_VENDOR_ID, 0x31,  20000,  20000, &usbCintiq,     "PL500"		},
	{ WACOM_VENDOR_ID, 0x32,  20000,  20000, &usbCintiq,     "PL600"		},
	{ WACOM_VENDOR_ID, 0x33,  20000,  20000, &usbCintiq,     "PL600SX"		},
	{ WACOM_VENDOR_ID, 0x34,  20000,  20000, &usbCintiq,     "PL550"		},
	{ WACOM_VENDOR_ID, 0x35,  20000,  20000, &usbCintiq,     "PL800"		},
	{ WACOM_VENDOR_ID, 0x37,  20000,  20000, &usbCintiq,     "PL700"		},
	{ WACOM_VENDOR_ID, 0x38,  20000,  20000, &usbCintiq,     "PL510"		},
	{ WACOM_VENDOR_ID, 0x39,  20000,  20000, &usbCintiq,     "PL710"		},
	{ WACOM_VENDOR_ID, 0x3A,  20000,  20000, &usbCintiq,     "DTI520"		},
	{ WACOM_VENDOR_ID, 0xC0,  20000,  20000, &usbCintiq,     "DTF720"		},
	{ WACOM_VENDOR_ID, 0xC2,  20000,  20000, &usbCintiq,     "DTF720a"		},
	{ WACOM_VENDOR_ID, 0xC4,  20000,  20000, &usbCintiq,     "DTF521"		},
	{ WACOM_VENDOR_ID, 0xC7, 100000, 100000, &usbCintiq,     "DTU1931"		},
	{ WACOM_VENDOR_ID, 0xCE, 100000, 100000, &usbCintiq,     "DTU2231"		},
	{ WACOM_VENDOR_ID, 0xF0, 100000, 100000, &usbCintiq,     "DTU1631"		},

	{ WACOM_VENDOR_ID, 0x41, 100000, 100000, &usbIntuos2,    "Intuos2 4x5"		},
	{ WACOM_VENDOR_ID, 0x42, 100000, 100000, &usbIntuos2,    "Intuos2 6x8"		},
	{ WACOM_VENDOR_ID, 0x43, 100000, 100000, &usbIntuos2,    "Intuos2 9x12"		},
	{ WACOM_VENDOR_ID, 0x44, 100000, 100000, &usbIntuos2,    "Intuos2 12x12"	},
	{ WACOM_VENDOR_ID, 0x45, 100000, 100000, &usbIntuos2,    "Intuos2 12x18"	},
	{ WACOM_VENDOR_ID, 0x47, 100000, 100000, &usbIntuos2,    "Intuos2 6x8 "		},

	{ WACOM_VENDOR_ID, 0x60,  50000,  50000, &usbVolito,     "Volito"		},

	{ WACOM_VENDOR_ID, 0x61,  50000,  50000, &usbVolito2,    "PenStation"		},
	{ WACOM_VENDOR_ID, 0x62,  50000,  50000, &usbVolito2,    "Volito2 4x5"		},
	{ WACOM_VENDOR_ID, 0x63,  50000,  50000, &usbVolito2,    "Volito2 2x3"		},
	{ WACOM_VENDOR_ID, 0x64,  50000,  50000, &usbVolito2,    "PenPartner2"		},

	{ WACOM_VENDOR_ID, 0x65, 100000, 100000, &usbBamboo,     "Bamboo"		},
	{ WACOM_VENDOR_ID, 0x69,  39842,  39842, &usbBamboo1,    "Bamboo1"		},
	{ WACOM_VENDOR_ID, 0x6A, 100000, 100000, &usbBamboo1,    "Bamboo1 4x6"		},
	{ WACOM_VENDOR_ID, 0x6B, 100000, 100000, &usbBamboo1,    "Bamboo1 5x8"		},

	{ WACOM_VENDOR_ID, 0xB0, 200000, 200000, &usbIntuos3,    "Intuos3 4x5"		},
	{ WACOM_VENDOR_ID, 0xB1, 200000, 200000, &usbIntuos3,    "Intuos3 6x8"		},
	{ WACOM_VENDOR_ID, 0xB2, 200000, 200000, &usbIntuos3,    "Intuos3 9x12"		},
	{ WACOM_VENDOR_ID, 0xB3, 200000, 200000, &usbIntuos3,    "Intuos3 12x12"	},
	{ WACOM_VENDOR_ID, 0xB4, 200000, 200000, &usbIntuos3,    "Intuos3 12x19"	},
	{ WACOM_VENDOR_ID, 0xB5, 200000, 200000, &usbIntuos3,    "Intuos3 6x11"		},
	{ WACOM_VENDOR_ID, 0xB7, 200000, 200000, &usbIntuos3,    "Intuos3 4x6"		},

	{ WACOM_VENDOR_ID, 0xB8, 200000, 200000, &usbIntuos4,    "Intuos4 4x6"		},
	{ WACOM_VENDOR_ID, 0xB9, 200000, 200000, &usbIntuos4,    "Intuos4 6x9"		},
	{ WACOM_VENDOR_ID, 0xBA, 200000, 200000, &usbIntuos4,    "Intuos4 8x13"		},
	{ WACOM_VENDOR_ID, 0xBB, 200000, 200000, &usbIntuos4,    "Intuos4 12x19"	},
	{ WACOM_VENDOR_ID, 0xBC, 200000, 200000, &usbIntuos4,    "Intuos4 WL USB Endpoint"	},
	{ WACOM_VENDOR_ID, 0xBD, 200000, 200000, &usbIntuos4,    "Intuos4 WL Bluetooth Endpoint"},

	{ WACOM_VENDOR_ID, 0x26, 200000, 200000, &usbIntuos5,    "Intuos5 touch S"	},
	{ WACOM_VENDOR_ID, 0x27, 200000, 200000, &usbIntuos5,    "Intuos5 touch M"	},
	{ WACOM_VENDOR_ID, 0x28, 200000, 200000, &usbIntuos5,    "Intuos5 touch L"	},
	{ WACOM_VENDOR_ID, 0x29, 200000, 200000, &usbIntuos5,    "Intuos5 S"		},
	{ WACOM_VENDOR_ID, 0x2A, 200000, 200000, &usbIntuos5,    "Intuos5 M"		},
	{ WACOM_VENDOR_ID, 0x314,200000, 200000, &usbIntuosPro,  "Intuos Pro S"		},
	{ WACOM_VENDOR_ID, 0x315,200000, 200000, &usbIntuosPro,  "Intuos Pro M"		},
	{ WACOM_VENDOR_ID, 0x317,200000, 200000, &usbIntuosPro,  "Intuos Pro L"		},

	{ WACOM_VENDOR_ID, 0x3F, 200000, 200000, &usbCintiqV5,   "Cintiq 21UX"		},
	{ WACOM_VENDOR_ID, 0xC5, 200000, 200000, &usbCintiqV5,   "Cintiq 20WSX"		},
	{ WACOM_VENDOR_ID, 0xC6, 200000, 200000, &usbCintiqV5,   "Cintiq 12WX"		},
	{ WACOM_VENDOR_ID, 0xCC, 200000, 200000, &usbCintiqV5,   "Cintiq 21UX2"		},
	{ WACOM_VENDOR_ID, 0xF4, 200000, 200000, &usbCintiqV5,   "Cintiq 24HD"		},
	{ WACOM_VENDOR_ID, 0xFA, 200000, 200000, &usbCintiqV5,   "Cintiq 22HD"		},
	{ WACOM_VENDOR_ID, 0xF8, 200000, 200000, &usbCintiqV5,   "Cintiq 24HD touch (EMR digitizer)" },
	{ WACOM_VENDOR_ID, 0x304,200000, 200000, &usbCintiqV5,   "Cintiq 13HD"		},
	{ WACOM_VENDOR_ID, 0x57, 200000, 200000, &usbCintiqV5,   "DTK2241"		},
	{ WACOM_VENDOR_ID, 0x59, 200000, 200000, &usbCintiqV5,   "DTH2242"		},
	{ WACOM_VENDOR_ID, 0x5B, 200000, 200000, &usbCintiqV5,   "Cintiq 22HDT"		},

	{ WACOM_VENDOR_ID, 0x90, 100000, 100000, &usbTabletPC,   "TabletPC 0x90"	},
	{ WACOM_VENDOR_ID, 0x93, 100000, 100000, &usbTabletPC,   "TabletPC 0x93"	},
	{ WACOM_VENDOR_ID, 0x97, 100000, 100000, &usbTabletPC,   "TabletPC 0x97"	},
	{ WACOM_VENDOR_ID, 0x9A, 100000, 100000, &usbTabletPC,   "TabletPC 0x9A"	},
	{ WACOM_VENDOR_ID, 0x9F, 100000, 100000, &usbTabletPC,   "CapPlus  0x9F"	},
	{ WACOM_VENDOR_ID, 0xE2, 100000, 100000, &usbTabletPC,   "TabletPC 0xE2"	},
	{ WACOM_VENDOR_ID, 0xE3, 100000, 100000, &usbTabletPC,   "TabletPC 0xE3"	},
	{ WACOM_VENDOR_ID, 0xE5, 100000, 100000, &usbTabletPC,   "TabletPC 0xE5"	},
	{ WACOM_VENDOR_ID, 0xE6, 100000, 100000, &usbTabletPC,   "TabletPC 0xE6"	},
	{ WACOM_VENDOR_ID, 0xEC, 100000, 100000, &usbTabletPC,   "TabletPC 0xEC"	},
	{ WACOM_VENDOR_ID, 0xED, 100000, 100000, &usbTabletPC,   "TabletPC 0xED"	},
	{ WACOM_VENDOR_ID, 0xEF, 100000, 100000, &usbTabletPC,   "TabletPC 0xEF"	},
	{ WACOM_VENDOR_ID, 0x100,100000, 100000, &usbTabletPC,   "TabletPC 0x100"	},
	{ WACOM_VENDOR_ID, 0x101,100000, 100000, &usbTabletPC,   "TabletPC 0x101"	},
	{ WACOM_VENDOR_ID, 0x10D,100000, 100000, &usbTabletPC,   "TabletPC 0x10D"	},
	{ WACOM_VENDOR_ID, 0x116,100000, 100000, &usbTabletPC,   "TabletPC 0x116"	},
	{ WACOM_VENDOR_ID, 0x12C,100000, 100000, &usbTabletPC,   "TabletPC 0x12C"	},
	{ WACOM_VENDOR_ID, 0x4001,100000, 100000, &usbTabletPC,  "TabletPC 0x4001"	},
	{ WACOM_VENDOR_ID, 0x4004,100000, 100000, &usbTabletPC,  "TabletPC 0x4004"	},
	{ WACOM_VENDOR_ID, 0x5000,100000, 100000, &usbTabletPC,  "TabletPC 0x5000"	},
	{ WACOM_VENDOR_ID, 0x5002,100000, 100000, &usbTabletPC,  "TabletPC 0x5002"	},

	/* IDs from Waltop's driver, available http://www.waltop.com.tw/download.asp?lv=0&id=2.
	   Accessed 8 Apr 2010, driver release date 2009/08/11, fork of linuxwacom 0.8.4.
	   Some more info would be nice for the ID's below... */
	{ WALTOP_VENDOR_ID, 0x24,  80000,  80000, &usbGraphire,   NULL			},
	{ WALTOP_VENDOR_ID, 0x25,  80000,  80000, &usbGraphire2,  NULL			},
	{ WALTOP_VENDOR_ID, 0x26,  80000,  80000, &usbGraphire2,  NULL			},
	{ WALTOP_VENDOR_ID, 0x27,  80000,  80000, &usbGraphire3,  NULL			},
	{ WALTOP_VENDOR_ID, 0x28,  80000,  80000, &usbGraphire3,  NULL			},
	{ WALTOP_VENDOR_ID, 0x30,  80000,  80000, &usbGraphire4,  NULL			},
	{ WALTOP_VENDOR_ID, 0x31,  80000,  80000, &usbGraphire4,  NULL			},
	{ WALTOP_VENDOR_ID, 0x32, 100000, 100000, &usbBambooFun,  NULL			},
	{ WALTOP_VENDOR_ID, 0x33, 100000, 100000, &usbBambooFun,  NULL			},
	{ WALTOP_VENDOR_ID, 0x34,  80000,  80000, &usbBamboo1,    NULL			},
	{ WALTOP_VENDOR_ID, 0x35,  80000,  80000, &usbGraphire4,  NULL			},
	{ WALTOP_VENDOR_ID, 0x36,  80000,  80000, &usbGraphire4,  NULL			},
	{ WALTOP_VENDOR_ID, 0x37,  80000,  80000, &usbGraphire4,  NULL			},
	{ WALTOP_VENDOR_ID, 0x38, 100000, 100000, &usbBambooFun,  NULL			},
	{ WALTOP_VENDOR_ID, 0x39, 100000, 100000, &usbBambooFun,  NULL			},
	{ WALTOP_VENDOR_ID, 0x51, 100000, 100000, &usbBamboo,     NULL			},
	{ WALTOP_VENDOR_ID, 0x52, 100000, 100000, &usbBamboo,     NULL			},

	{ WALTOP_VENDOR_ID, 0x53,  100000, 100000, &usbBamboo,    NULL			},
	{ WALTOP_VENDOR_ID, 0x54,  100000, 100000, &usbBamboo,    NULL			},
	{ WALTOP_VENDOR_ID, 0x55,  100000, 100000, &usbBamboo,    NULL			},
	{ WALTOP_VENDOR_ID, 0x56,  100000, 100000, &usbBamboo,    NULL			},
	{ WALTOP_VENDOR_ID, 0x57,  100000, 100000, &usbBamboo,    NULL			},
	{ WALTOP_VENDOR_ID, 0x58,  100000, 100000, &usbBamboo,    NULL			},
	{ WALTOP_VENDOR_ID, 0x500, 100000, 100000, &usbBamboo,    NULL			},
	{ WALTOP_VENDOR_ID, 0x501, 100000, 100000, &usbBamboo,    NULL			},
	{ WALTOP_VENDOR_ID, 0x502, 200000, 200000, &usbIntuos4,   NULL			},
	{ WALTOP_VENDOR_ID, 0x503, 200000, 200000, &usbIntuos4,   NULL			},

	/* N-Trig devices */
	{ NTRIG_VENDOR_ID,  0x01, 44173, 36772, &usbTabletPC,     NULL			},

	/* Add in Lenovo W700 Palmrest digitizer */
	{ LENOVO_VENDOR_ID, 0x6004, 100000, 100000, &usbTabletPC, NULL			} /* Pen-only */
};

void usbListModels(void)
{
	int i;
	char *usbnames[ARRAY_SIZE(WacomModelDesc)] = {0};
	SymTabRec models[ARRAY_SIZE(WacomModelDesc) + 1];

	for (i = 0; i < ARRAY_SIZE(WacomModelDesc); i++)
	{
		struct WacomModelDesc *m = &WacomModelDesc[i];

		models[i].token = i;
		if (m->name)
			models[i].name = m->name;
		else {
			/* SymTabRec has a const char *name so we can't free
			   without compiler warnings. keep allocated stuff
			   in separate array.
			 */
			usbnames[i] = malloc(64);
			if (!usbnames[i]) {
				/* if malloc fails, xf86PrintChipsets() terminates
				   here because models->name is NULL. then again,
				   malloc failed, so the rest is academic. */
				models[i].name = NULL;
				break;
			}
			sprintf(usbnames[i], "usb:%04x:%04x", m->vendor_id, m->model_id);
			models[i].name = usbnames[i];
		}
	}

	models[ARRAY_SIZE(models) - 1].name = NULL;

	xf86PrintChipsets("wacom",
			  "Driver for Wacom graphics tablets",
			  models);

	for (i = 0; i < ARRAY_SIZE(usbnames); i++)
		free(usbnames[i]);
}


static Bool usbWcmInit(InputInfoPtr pInfo, char* id, float *version)
{
	int i;
	struct input_id sID;
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;
	wcmUSBData *usbdata;

	DBG(1, priv, "initializing USB tablet\n");

	if (!common->private &&
	    !(common->private = calloc(1, sizeof(wcmUSBData))))
	{
		xf86Msg(X_ERROR, "%s: unable to alloc event queue.\n",
					pInfo->name);
		return !Success;
	}

	usbdata = common->private;
	*version = 0.0;

	/* fetch vendor, product, and model name */
	ioctl(pInfo->fd, EVIOCGID, &sID);
	ioctl(pInfo->fd, EVIOCGNAME(sizeof(id)), id);

	for (i = 0; i < ARRAY_SIZE(WacomModelDesc); i++)
	{
		if (sID.vendor == WacomModelDesc[i].vendor_id &&
		    sID.product == WacomModelDesc [i].model_id)
		{
			common->wcmModel = WacomModelDesc [i].model;
			common->wcmResolX = WacomModelDesc [i].xRes;
			common->wcmResolY = WacomModelDesc [i].yRes;
		}
	}

	if (!common->wcmModel)
	{
		common->wcmModel = &usbUnknown;
		common->wcmResolX = common->wcmResolY = 1016;
	}

	/* Find out supported button codes. */
	usbdata->npadkeys = 0;
	for (i = 0; i < ARRAY_SIZE(padkey_codes); i++)
		if (ISBITSET (common->wcmKeys, padkey_codes [i]))
			usbdata->padkey_code [usbdata->npadkeys++] = padkey_codes [i];

	if (usbdata->npadkeys == 0) {
		/* If no pad keys were detected, entertain the possibility that any
		 * mouse buttons which exist may belong to the pad (e.g. Graphire4).
		 * If we're wrong, this will over-state the capabilities of the pad
		 * but that shouldn't actually cause problems.
		 */
		for (i = ARRAY_SIZE(mouse_codes) - 1; i > 0; i--)
			if (ISBITSET(common->wcmKeys, mouse_codes[i]))
				break;

		/* Make sure room for fixed map mouse buttons.  This
		 * means mappings may overlap with padkey_codes[].
		 */
		if (i != 0)
			usbdata->npadkeys = WCM_USB_MAX_MOUSE_BUTTONS;
	}

	/* nbuttons tracks maximum buttons on all tools (stylus/mouse).
	 *
	 * Mouse support left, middle, right, side, and extra side button.
	 * Stylus support tip and 2 stlyus buttons.
	 */
	if (ISBITSET (common->wcmKeys, BTN_TOOL_MOUSE))
		usbdata->nbuttons = WCM_USB_MAX_MOUSE_BUTTONS;
	else
		usbdata->nbuttons = WCM_USB_MAX_STYLUS_BUTTONS;

	return Success;
}

static void usbInitProtocol5(WacomCommonPtr common, const char* id,
	float version)
{
	common->wcmProtocolLevel = WCM_PROTOCOL_5;
	common->wcmPktLength = sizeof(struct input_event);
	common->wcmCursorProxoutDistDefault = PROXOUT_INTUOS_DISTANCE;

	/* tilt enabled */
	common->wcmFlags |= TILT_ENABLED_FLAG;
}

static void usbInitProtocol4(WacomCommonPtr common, const char* id,
	float version)
{
	common->wcmProtocolLevel = WCM_PROTOCOL_4;
	common->wcmPktLength = sizeof(struct input_event);
	common->wcmCursorProxoutDistDefault = PROXOUT_GRAPHIRE_DISTANCE;

	/* tilt disabled */
	common->wcmFlags &= ~TILT_ENABLED_FLAG;
}

/* Initialize fixed PAD channel's state to in proximity.
 *
 * Some, but not all, Wacom protocol 4/5 devices are always in proximity.
 * Because of evdev filtering, there will never be a BTN_TOOL_FINGER
 * sent to initialize state.
 * Generic protocol devices never send anything to help initialize PAD
 * device as well.
 * This helps those 2 cases and does not hurt the cases where kernel
 * driver sends out-of-proximity event for PAD since PAD is always on
 * its own channel, PAD_CHANNEL.
 */
static void usbWcmInitPadState(InputInfoPtr pInfo)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;
	WacomDeviceState *ds;
	int channel = PAD_CHANNEL;

	DBG(6, common, "Initializing PAD channel %d\n", channel);

	ds = &common->wcmChannel[channel].work;

	ds->proximity = 1;
	ds->device_type = PAD_ID;
	ds->device_id = PAD_DEVICE_ID;
	ds->serial_num = channel;
}

int usbWcmGetRanges(InputInfoPtr pInfo)
{
	struct input_absinfo absinfo;
	unsigned long ev[NBITS(EV_MAX)] = {0};
	unsigned long abs[NBITS(ABS_MAX)] = {0};
	unsigned long sw[NBITS(SW_MAX)] = {0};
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common =	priv->common;
	wcmUSBData* private = common->private;
	int is_touch = IsTouch(priv);

	/* Devices such as Bamboo P&T may have Pad data reported in the same
	 * packet as Touch.  It's normal for Pad to be called first but logic
	 * requires it to act the same as Touch.
	 */
	if (ISBITSET(common->wcmKeys, BTN_TOOL_DOUBLETAP)
	     && ISBITSET(common->wcmKeys, BTN_FORWARD))
		is_touch = 1;

	if (ioctl(pInfo->fd, EVIOCGBIT(0 /*EV*/, sizeof(ev)), ev) < 0)
	{
		xf86Msg(X_ERROR, "%s: unable to ioctl event bits.\n", pInfo->name);
		return !Success;
	}

	if (!ISBITSET(ev,EV_ABS))
	{
		xf86Msg(X_ERROR, "%s: no abs bits.\n", pInfo->name);
		return !Success;
	}

	/* absolute values */
        if (ioctl(pInfo->fd, EVIOCGBIT(EV_ABS, sizeof(abs)), abs) < 0)
	{
		xf86Msg(X_ERROR, "%s: unable to ioctl max values.\n", pInfo->name);
		return !Success;
	}

	/* max x */
	if (ioctl(pInfo->fd, EVIOCGABS(ABS_X), &absinfo) < 0)
	{
		xf86Msg(X_ERROR, "%s: unable to ioctl xmax value.\n", pInfo->name);
		return !Success;
	}

	if (absinfo.maximum <= 0)
	{
		xf86Msg(X_ERROR, "%s: xmax value is %d, expected > 0.\n",
			pInfo->name, absinfo.maximum);
		return !Success;
	}

	if (!is_touch)
	{
		common->wcmMinX = absinfo.minimum;
		common->wcmMaxX = absinfo.maximum;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
		if (absinfo.resolution > 0)
			common->wcmResolX = absinfo.resolution * 1000;
#endif
	}
	else
	{
		common->wcmMaxTouchX = absinfo.maximum;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
		if (absinfo.resolution > 0)
			common->wcmTouchResolX = absinfo.resolution * 1000;
#endif
	}

	/* max y */
	if (ioctl(pInfo->fd, EVIOCGABS(ABS_Y), &absinfo) < 0)
	{
		xf86Msg(X_ERROR, "%s: unable to ioctl ymax value.\n", pInfo->name);
		return !Success;
	}

	if (absinfo.maximum <= 0)
	{
		xf86Msg(X_ERROR, "%s: ymax value is %d, expected > 0.\n",
			pInfo->name, absinfo.maximum);
		return !Success;
	}

	if (!is_touch)
	{
		common->wcmMinY = absinfo.minimum;
		common->wcmMaxY = absinfo.maximum;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
		if (absinfo.resolution > 0)
			common->wcmResolY = absinfo.resolution * 1000;
#endif
	}
	else
	{
		common->wcmMaxTouchY = absinfo.maximum;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
		if (absinfo.resolution > 0)
			common->wcmTouchResolY = absinfo.resolution * 1000;
#endif
	}

	/* max finger strip X for tablets with Expresskeys
	 * or physical X for touch devices in hundredths of a mm */
	if (ISBITSET(abs, ABS_RX) &&
			!ioctl(pInfo->fd, EVIOCGABS(ABS_RX), &absinfo))
	{
		if (is_touch)
			common->wcmTouchResolX =
				(int)(((double)common->wcmMaxTouchX * 100000.0
				 / (double)absinfo.maximum) + 0.5);
		else
			common->wcmMaxStripX = absinfo.maximum;
	}

	/* X tilt range */
	if (ISBITSET(abs, ABS_TILT_X) &&
			!ioctl(pInfo->fd, EVIOCGABS(ABS_TILT_X), &absinfo))
	{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
		/* If resolution is specified */
		if (absinfo.resolution > 0)
		{
			/* Assume the range is centered on zero */
			common->wcmTiltOffX = 0;
			/* Convert to resolution expected by applications */
			common->wcmTiltFactX = TILT_RES /
					       (double)absinfo.resolution;
		}
		else
#endif
		{
			/*
			 * Center the reported range on zero to support
			 * kernel drivers still reporting non-zero-centered
			 * values.
			 */
			common->wcmTiltOffX = - (absinfo.minimum +
						 absinfo.maximum) / 2;
			/*
			 * Assume reported resolution is the one expected by
			 * applications
			 */
			common->wcmTiltFactX = 1.0;
		}
		common->wcmTiltMinX = round((absinfo.minimum +
					     common->wcmTiltOffX) *
					    common->wcmTiltFactX);
		common->wcmTiltMaxX = round((absinfo.maximum +
					     common->wcmTiltOffX) *
					    common->wcmTiltFactX);
	}

	/* Y tilt range */
	if (ISBITSET(abs, ABS_TILT_Y) &&
			!ioctl(pInfo->fd, EVIOCGABS(ABS_TILT_Y), &absinfo))
	{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
		/* If resolution is specified */
		if (absinfo.resolution > 0)
		{
			/* Assume the range is centered on zero */
			common->wcmTiltOffY = 0;
			/* Convert to resolution expected by applications */
			common->wcmTiltFactY = TILT_RES /
					       (double)absinfo.resolution;
		}
		else
#endif
		{
			/*
			 * Center the reported range on zero to support
			 * kernel drivers still reporting non-zero-centered
			 * values.
			 */
			common->wcmTiltOffY = - (absinfo.minimum +
						 absinfo.maximum) / 2;
			/*
			 * Assume reported resolution is the one expected by
			 * applications
			 */
			common->wcmTiltFactY = 1.0;
		}
		common->wcmTiltMinY = round((absinfo.minimum +
					     common->wcmTiltOffY) *
					    common->wcmTiltFactY);
		common->wcmTiltMaxY = round((absinfo.maximum +
					     common->wcmTiltOffY) *
					    common->wcmTiltFactY);
	}

	/* max finger strip Y for tablets with Expresskeys
	 * or physical Y for touch devices in hundredths of a mm */
	if (ISBITSET(abs, ABS_RY) &&
			!ioctl(pInfo->fd, EVIOCGABS(ABS_RY), &absinfo))
	{
		if (is_touch)
			common->wcmTouchResolY =
				 (int)(((double)common->wcmMaxTouchY * 100000.0
				 / (double)absinfo.maximum) + 0.5);
		else
			common->wcmMaxStripY = absinfo.maximum;
	}

	/* max z cannot be configured */
	if (ISBITSET(abs, ABS_PRESSURE) &&
			!ioctl(pInfo->fd, EVIOCGABS(ABS_PRESSURE), &absinfo))
		common->wcmMaxZ = absinfo.maximum;

	/* max distance */
	if (ISBITSET(abs, ABS_DISTANCE) &&
			!ioctl(pInfo->fd, EVIOCGABS(ABS_DISTANCE), &absinfo))
		common->wcmMaxDist = absinfo.maximum;

	if (ISBITSET(abs, ABS_MT_SLOT))
	{
		private->wcmUseMT = 1;

		if (!ioctl(pInfo->fd, EVIOCGABS(ABS_MT_SLOT), &absinfo))
			common->wcmMaxContacts = absinfo.maximum + 1;

		/* pen and MT on the same logical port */
		if (ISBITSET(common->wcmKeys, BTN_TOOL_PEN))
			private->wcmPenTouch = TRUE;
	}

	/* A generic protocol device does not report ABS_MISC event */
	if (!ISBITSET(abs, ABS_MISC))
		common->wcmProtocolLevel = WCM_PROTOCOL_GENERIC;

	if (ioctl(pInfo->fd, EVIOCGBIT(EV_SW, sizeof(sw)), sw) < 0)
	{
		xf86Msg(X_ERROR, "%s: usbProbeKeys unable to ioctl "
			"sw bits.\n", pInfo->name);
		return 0;
	}
	else if (ISBITSET(sw, SW_MUTE_DEVICE))
	{
		common->wcmHasHWTouchSwitch = TRUE;

		memset(sw, 0, sizeof(sw));

		ioctl(pInfo->fd, EVIOCGSW(sizeof(sw)), sw);

		if (ISBITSET(sw, SW_MUTE_DEVICE))
			common->wcmHWTouchSwitchState = 0;
		else
			common->wcmHWTouchSwitchState = 1;
	}

	usbWcmInitPadState(pInfo);

	return Success;
}

static int usbDetectConfig(InputInfoPtr pInfo)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;
	wcmUSBData *usbdata = common->private;

	DBG(10, common, "\n");
	if (IsPad (priv))
		priv->nbuttons = usbdata->npadkeys;
	else
		priv->nbuttons = usbdata->nbuttons;

	if (!common->wcmCursorProxoutDist)
		common->wcmCursorProxoutDist
			= common->wcmCursorProxoutDistDefault;
	return TRUE;
}

static int usbParse(InputInfoPtr pInfo, const unsigned char* data, int len)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;

	if (len < sizeof(struct input_event))
		return 0;

	usbParseEvent(pInfo, (const struct input_event*)data);
	return common->wcmPktLength;
}

/**
 * Returns a serial number for the provided device_type and serial, as
 * through it came from from a Protocol 5 device.
 *
 * Protocol 5 serial numbers will be returned unchanged. Otherwise,
 * anonymous tools (from Protocol 4 and Generic Protocol) will have
 * serial numbers of: -1 (pad), 1 (pen/1st finger), 2 (2nd finger),
 * etc.
 *
 * @param[in] device_type  Type of device (e.g. STYLUS_ID, TOUCH_ID, PAD_ID)
 * @param[in] serial       Serial number of device
 * @return                 Serial number of device as through from Protocol 5
 */
static int protocol5Serial(int device_type, unsigned int serial) {
	if (!serial) {
		/* Generic Protocol does not send serial numbers */
		return device_type == PAD_ID ? -1 : 1;
	}
	else if (serial == 0xf0) {
		/* Protocol 4 uses the expected anonymous serial
		 * numbers, but has the wrong PAD serial number.
		 * This could cause problem if 0xf0 is ever used
		 * for a Protocol 5 serial number, but isn't a
		 * problem as yet.
		 */
		return -1;
	}
	else {
		/* Protocol 5 FTW */
		return serial;
	}
}

/**
 * Find an appropriate channel to track the specified tool's state in.
 * If the tool is already in proximity, the channel currently being used
 * to store its state will be returned. Otherwise, an arbitrary available
 * channel will be cleaned and returned. Up to MAX_CHANNEL tools can be
 * tracked concurrently by driver.
 *
 * @param[in] common
 * @param[in] device_type  Type of tool (e.g. STYLUS_ID, TOUCH_ID, PAD_ID)
 * @param[in] serial       Serial number of tool
 * @return                 Channel number to track the tool's state
 */
static int usbChooseChannel(WacomCommonPtr common, int device_type, unsigned int serial)
{
	/* figure out the channel to use based on serial number */
	int i, channel = -1;

	/* force events from PAD device to PAD_CHANNEL */
	if (serial == -1)
		channel = PAD_CHANNEL;

	/* find existing channel */
	if (channel < 0)
	{
		for (i=0; i<MAX_CHANNELS; i++)
		{
			if (common->wcmChannel[i].work.proximity &&
			    common->wcmChannel[i].work.device_type == device_type &&
			    common->wcmChannel[i].work.serial_num == serial)
			{
				channel = i;
				break;
			}
		}
	}

	/* find and clean an empty channel */
	if (channel < 0)
	{
		for (i=0; i<MAX_CHANNELS; i++)
		{
			if (i == PAD_CHANNEL)
				continue;

			if (!common->wcmChannel[i].work.proximity)
			{
				channel = i;
				memset(&common->wcmChannel[channel],0, sizeof(WacomChannel));
				break;
			}
		}
	}

	/* fresh out of channels */
	if (channel < 0)
	{
		/* This should never happen in normal use.
		 * Let's start over again. Force prox-out for all channels.
		 */
		for (i=0; i<MAX_CHANNELS; i++)
		{
			if (i == PAD_CHANNEL)
				continue;

			if (common->wcmChannel[i].work.proximity &&
			    (common->wcmChannel[i].work.serial_num != -1))
			{
				common->wcmChannel[i].work.proximity = 0;
				/* dispatch event */
				wcmEvent(common, i, &common->wcmChannel[i].work);
				DBG(2, common, "free channels: dropping %u\n",
						common->wcmChannel[i].work.serial_num);
			}
		}
		DBG(1, common, "device with serial number: %u"
		    " at %d: Exceeded channel count; ignoring the events.\n",
		    serial, (int)GetTimeInMillis());
	}

	return channel;
}

static void usbParseEvent(InputInfoPtr pInfo,
	const struct input_event* event)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;
	wcmUSBData* private = common->private;

	DBG(10, common, "\n");

	/* store events until we receive the MSC_SERIAL containing
	 * the serial number or a SYN_REPORT.
	 */

	/* space left? bail if not. */
	if (private->wcmEventCnt >= ARRAY_SIZE(private->wcmEvents))
	{
		LogMessageVerbSigSafe(X_ERROR, 0, "%s: usbParse: Exceeded event queue (%d) \n",
				      pInfo->name, private->wcmEventCnt);
		private->wcmEventCnt = 0;
		return;
	}

	/* save it for later */
	private->wcmEvents[private->wcmEventCnt++] = *event;

	if (event->type == EV_MSC || event->type == EV_SYN)
		usbParseSynEvent(pInfo, event);
}

/**
 * EV_SYN marks the end of a set of events containing axes and button info.
 * Check for valid data and hand over to dispatch to extract the actual
 * values and process them. At this point, all events up to the EV_SYN are
 * queued up in wcmEvents.
 */
static void usbParseSynEvent(InputInfoPtr pInfo,
			     const struct input_event *event)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;
	wcmUSBData* private = common->private;

	if ((event->type == EV_MSC) && (event->code == MSC_SERIAL))
	{
		/* we don't report serial numbers for some tools
		 * but we never report a serial number with a value of 0 */
		if (event->value == 0)
		{
			LogMessageVerbSigSafe(X_ERROR, 0,
					      "%s: usbParse: Ignoring event from invalid serial 0\n",
					      pInfo->name);
			goto skipEvent;
		}

		/* save the serial number so we can look up the channel number later */
		private->wcmLastToolSerial = event->value;

		return;

	} else if ((event->type == EV_SYN) && (event->code == SYN_REPORT))
	{
		/* end of record. fall through to dispatch */
	}
	else
	{
		/* not an SYN_REPORT and not an SYN_REPORT, bail out */
		return;
	}

	/* ignore events without information */
	if ((private->wcmEventCnt < 2) && private->wcmLastToolSerial)
	{
		DBG(3, common, "%s: dropping empty event"
			" for serial %d\n", pInfo->name,
			private->wcmLastToolSerial);
		goto skipEvent;
	}

	/* ignore sync windows that contain no data */
	if (private->wcmEventCnt == 1 &&
	    private->wcmEvents->type == EV_SYN) {
		DBG(6, common, "no real events received\n");
		goto skipEvent;
	}

	/* dispatch all queued events */
	usbDispatchEvents(pInfo);

skipEvent:
	private->wcmEventCnt = 0;
}

static int usbFilterEvent(WacomCommonPtr common, struct input_event *event)
{
	wcmUSBData* private = common->private;

	/* For devices that report multitouch, the following list is a set of
	 * duplicate data from one slot and needs to be filtered out.
	 */
	if (private->wcmUseMT)
	{
		if (event->type == EV_KEY)
		{
			switch(event->code)
			{
				case BTN_TOUCH:
				case BTN_TOOL_FINGER:
				case BTN_TOOL_DOUBLETAP:
				case BTN_TOOL_TRIPLETAP:
					return 1;
			}
		}
		else if (event->type == EV_ABS)
		{
			if (private->wcmDeviceType == TOUCH_ID)
			{
				/* filter ST for MT */
				switch(event->code)
				{
					case ABS_X:
					case ABS_Y:
					case ABS_PRESSURE:
						return 1;
				}
			}
			else
			{
				/* filter MT for pen */
				switch(event->code)
				{
					case ABS_MT_SLOT:
					case ABS_MT_TRACKING_ID:
					case ABS_MT_POSITION_X:
					case ABS_MT_POSITION_Y:
					case ABS_MT_PRESSURE:
						return 1;
				}
			}
		}
	}

	/* For generic devices, filter out doubletap/tripletap that
	 * can be confused with older protocol.
	 */
	if (common->wcmProtocolLevel == WCM_PROTOCOL_GENERIC)
	{
		if (event->type == EV_KEY)
		{
			switch(event->code)
			{
				case BTN_TOOL_DOUBLETAP:
				case BTN_TOOL_TRIPLETAP:
					return 1;
			}
		}
	}

	return 0;
}

#define ERASER_BIT      0x008
#define PUCK_BITS	0xf00
#define PUCK_EXCEPTION  0x806
/**
 * Decide the tool type by its id for protocol 5 devices
 *
 * @param id The tool id received from the kernel.
 * @return The tool type associated with the tool id.
 */
static int usbIdToType(int id)
{
	int type = STYLUS_ID;

	if (!id)
		return 0;

	/* The existing tool ids have the following patten: all pucks, except
	 * one, have the third byte set to zero; all erasers have the fourth
	 * bit set. The rest are styli.
	 */
	if (id & ERASER_BIT)
		type = ERASER_ID;
	else if (!(id & PUCK_BITS) || (id == PUCK_EXCEPTION))
		type = CURSOR_ID;

	return type;
}

/**
 * Find the tool type (STYLUS_ID, etc.) based on the device_id.
 *
 * Protocol 5 devices report different IDs for different styli and pucks,
 * Protocol 4 devices simply report STYLUS_DEVICE_ID, etc.
 *
 * @device_id id of the device
 * @return device type
 */
static int usbFindDeviceTypeById(int device_id)
{
	switch (device_id)
	{
		case STYLUS_DEVICE_ID:
			return STYLUS_ID;
		case ERASER_DEVICE_ID:
			return ERASER_ID;
		case CURSOR_DEVICE_ID:
			return CURSOR_ID;
		case TOUCH_DEVICE_ID:
			return TOUCH_ID;
		case PAD_DEVICE_ID:
			return PAD_ID;
		default: /* protocol 5 */
			return usbIdToType(device_id);
	}
	return 0;
}

static void usbParseAbsEvent(WacomCommonPtr common,
			    struct input_event *event, int channel_number)
{
	WacomChannel *channel = &common->wcmChannel[channel_number];
	WacomDeviceState *ds = &channel->work;
	int change = 1;

	switch(event->code)
	{
		case ABS_X:
			ds->x = event->value;
			break;
		case ABS_Y:
			ds->y = event->value;
			break;
		case ABS_RX:
			ds->stripx = event->value;
			break;
		case ABS_RY:
			ds->stripy = event->value;
			break;
		case ABS_RZ:
			ds->rotation = event->value;
			break;
		case ABS_TILT_X:
			ds->tiltx = round((event->value + common->wcmTiltOffX) *
					  common->wcmTiltFactX);
			break;
		case ABS_TILT_Y:
			ds->tilty = round((event->value + common->wcmTiltOffY) *
					  common->wcmTiltFactY);
			break;
		case ABS_PRESSURE:
			ds->pressure = event->value;
			break;
		case ABS_DISTANCE:
			ds->distance = event->value;
			break;
		case ABS_WHEEL:
			ds->abswheel = event->value;
			break;
		case ABS_Z:
			ds->abswheel = event->value;
			break;
		case ABS_THROTTLE:
			/* 2nd touch ring comes in over ABS_THROTTLE for 24HD */
			if ((common->vendor_id == WACOM_VENDOR_ID) &&
			    (common->tablet_id == 0xF4 || common->tablet_id == 0xF8))
				ds->abswheel2 = event->value;
			else
				ds->throttle = event->value;
			break;
		case ABS_MISC:
			ds->proximity = (event->value != 0);
			if (event->value)
				ds->device_id = event->value;
			break;
		default:
			change = 0;
	}

	ds->time = (int)GetTimeInMillis();
	channel->dirty |= change;
}

/**
 * Flip the mask bit in buttons corresponding to btn to the specified state.
 *
 * @param buttons The current button mask
 * @param btn Zero-indexed button number to change
 * @param state Zero to unset, non-zero to set the mask for the button
 *
 * @return The new button mask
 */
TEST_NON_STATIC int
mod_buttons(int buttons, int btn, int state)
{
	int mask;

	if (btn >= sizeof(int) * 8)
	{
		LogMessageVerbSigSafe(X_ERROR, 0,
				      "%s: Invalid button number %d. Insufficient storage\n",
				      __func__, btn);
		return buttons;
	}

	mask = 1 << btn;

	if (state)
		buttons |= mask;
	else
		buttons &= ~mask;

	return buttons;
}

static void usbParseAbsMTEvent(WacomCommonPtr common, struct input_event *event)
{
	int change = 1;
	wcmUSBData* private = common->private;
	WacomDeviceState *ds;

	ds = &common->wcmChannel[private->wcmMTChannel].work;

	switch(event->code)
	{
		case ABS_MT_SLOT:
			if (event->value >= 0) {
				int serial = event->value + 1;
				private->wcmMTChannel = usbChooseChannel(common, TOUCH_ID, serial);
				ds = &common->wcmChannel[private->wcmMTChannel].work;
				ds->serial_num = serial;
			}
			break;

		case ABS_MT_TRACKING_ID:
			ds->proximity = (event->value != -1);
			/* set this here as type for this channel doesn't get set in usbDispatchEvent() */
			ds->device_type = TOUCH_ID;
			ds->device_id = TOUCH_DEVICE_ID;
			ds->sample = (int)GetTimeInMillis();
			break;

		case ABS_MT_POSITION_X:
			ds->x = event->value;
			break;

		case ABS_MT_POSITION_Y:
			ds->y = event->value;
			break;

		case ABS_MT_PRESSURE:
			ds->pressure = event->value;
			break;

		default:
			change = 0;
	}

	ds->time = (int)GetTimeInMillis();
	(&common->wcmChannel[private->wcmMTChannel])->dirty |= change;
}

static void usbParseKeyEvent(WacomCommonPtr common,
			    struct input_event *event, int channel_number)
{
	int change = 1;
	WacomChannel *channel = &common->wcmChannel[channel_number];
	WacomDeviceState *ds = &channel->work;
	WacomDeviceState *dslast = &channel->valid.state;

	/* BTN_TOOL_* are sent to indicate when a specific tool is going
	 * in our out of proximity.  When going in proximity, here we
	 * initialize tool specific values.  Making sure shared values
	 * are correct values during tool change is done elsewhere.
	 */
	switch (event->code)
	{
		case BTN_TOOL_PEN:
		case BTN_TOOL_PENCIL:
		case BTN_TOOL_BRUSH:
		case BTN_TOOL_AIRBRUSH:
			/* V5 tools use ABS_MISC to report device_id */
			if (common->wcmProtocolLevel != WCM_PROTOCOL_5)
				ds->device_id = STYLUS_DEVICE_ID;
			ds->proximity = (event->value != 0);
			DBG(6, common,
			    "USB stylus detected %x\n",
			    event->code);
			break;

		case BTN_TOOL_RUBBER:
			/* V5 tools use ABS_MISC to report device_id */
			if (common->wcmProtocolLevel != WCM_PROTOCOL_5)
				ds->device_id = ERASER_DEVICE_ID;
			ds->proximity = (event->value != 0);
			DBG(6, common,
			    "USB eraser detected %x (value=%d)\n",
			    event->code, event->value);
			break;

		case BTN_TOOL_MOUSE:
		case BTN_TOOL_LENS:
			DBG(6, common,
			    "USB mouse detected %x (value=%d)\n",
			    event->code, event->value);
			/* V5 tools use ABS_MISC to report device_id */
			if (common->wcmProtocolLevel != WCM_PROTOCOL_5)
				ds->device_id = CURSOR_DEVICE_ID;
			ds->proximity = (event->value != 0);
			break;

               case BTN_TOUCH:
			if (common->wcmProtocolLevel == WCM_PROTOCOL_GENERIC)
			{
				/* 1FG USB touchscreen */
				if (!TabletHasFeature(common, WCM_PEN) &&
					TabletHasFeature(common, WCM_1FGT) &&
					TabletHasFeature(common, WCM_LCD))
				{
					DBG(6, common,
					    "USB 1FG Touch detected %x (value=%d)\n",
					    event->code, event->value);
					ds->device_id = TOUCH_DEVICE_ID;
					ds->proximity = event->value;
				}
			}
			break;

		case BTN_TOOL_FINGER:
			/* A pad tool */
			if (common->wcmProtocolLevel != WCM_PROTOCOL_GENERIC)
			{
				DBG(6, common,
				    "USB Pad detected %x (value=%d)\n",
				event->code, event->value);
				ds->device_id = PAD_DEVICE_ID;
				ds->proximity = (event->value != 0);
				break;
			}

			/* fall through */
		case BTN_TOOL_DOUBLETAP:
			DBG(6, common,
			    "USB Touch detected %x (value=%d)\n",
			    event->code, event->value);
			ds->device_id = TOUCH_DEVICE_ID;
			ds->proximity = event->value;
			/* time stamp for 2FGT gesture events */
			if ((ds->proximity && !dslast->proximity) ||
			    (!ds->proximity && dslast->proximity))
				ds->sample = (int)GetTimeInMillis();
			break;

		case BTN_TOOL_TRIPLETAP:
			DBG(6, common,
			    "USB Touch second finger detected %x (value=%d)\n",
			    event->code, event->value);
			ds->device_id = TOUCH_DEVICE_ID;
			ds->proximity = event->value;
			/* time stamp for 2GT gesture events */
			if ((ds->proximity && !dslast->proximity) ||
			    (!ds->proximity && dslast->proximity))
				ds->sample = (int)GetTimeInMillis();
			/* Second finger events will be considered in
			 * combination with the first finger data */
			break;

		default:
			change = 0;
	}

	ds->time = (int)GetTimeInMillis();
	channel->dirty |= change;

	if (change)
		return;

	/* Rest back to non-default value for next switch statement */
	change = 1;

	/* From this point on, all BTN_* will be real button presses.
	 * Stylus buttons always go with *ds.  Handle remaining
	 * cases upon return.
	 */
	switch (event->code)
	{
		case BTN_STYLUS:
			ds->buttons = mod_buttons(ds->buttons, 1, event->value);
			break;

		case BTN_STYLUS2:
			ds->buttons = mod_buttons(ds->buttons, 2, event->value);
			break;

		default:
			change = 0;
	}

	ds->time = (int)GetTimeInMillis();
	channel->dirty |= change;
}

/* Handle all button presses except for stylus buttons */
static void usbParseBTNEvent(WacomCommonPtr common,
			    struct input_event *event, int channel_number)
{
	int nkeys;
	int change = 1;
	wcmUSBData *usbdata = common->private;
	WacomChannel *channel = &common->wcmChannel[channel_number];
	WacomDeviceState *ds = &channel->work;

	switch (event->code)
	{
		case BTN_LEFT:
			ds->buttons = mod_buttons(ds->buttons, 0, event->value);
			break;

		case BTN_MIDDLE:
			ds->buttons = mod_buttons(ds->buttons, 1, event->value);
			break;

		case BTN_RIGHT:
			ds->buttons = mod_buttons(ds->buttons, 2, event->value);
			break;

		case BTN_SIDE:
		case BTN_BACK:
			ds->buttons = mod_buttons(ds->buttons, 3, event->value);
			break;

		case BTN_EXTRA:
		case BTN_FORWARD:
			ds->buttons = mod_buttons(ds->buttons, 4, event->value);
			break;

		default:
			for (nkeys = 0; nkeys < usbdata->npadkeys; nkeys++)
			{
				if (event->code == usbdata->padkey_code[nkeys])
				{
					ds->buttons = mod_buttons(ds->buttons, nkeys, event->value);
					break;
				}
			}
			if (nkeys >= usbdata->npadkeys)
				change = 0;
	}

	ds->time = (int)GetTimeInMillis();
	channel->dirty |= change;
}

/**
 * Translates an event code from the kernel (e.g. type: EV_ABS code: ABS_MISC value: STYLUS_DEVICE_ID)
 * into the corresponding device type for the driver (e.g. STYLUS_ID).
 *
 * @param[in] common
 * @param[in] type      Linux input tool type (e.g. EV_KEY)
 * @param[in] code      Linux input tool code (e.g. BTN_STYLUS_PEN)
 * @param[in] value     Linux input tool value (e.g. STYLUS_DEVICE_ID)
 * @return              Wacom device ID (e.g. STYLUS_ID) or 0 if no match.
 */
static int deviceTypeFromEvent(WacomCommonPtr common, int type, int code, int value)
{
	wcmUSBData* private = common->private;

	if (type == EV_KEY) {
		switch(code) {
			case BTN_TOOL_PEN:
			case BTN_TOOL_PENCIL:
			case BTN_TOOL_BRUSH:
			case BTN_TOOL_AIRBRUSH:
				return STYLUS_ID;

			case BTN_TOOL_MOUSE:
			case BTN_TOOL_LENS:
				return CURSOR_ID;

			case BTN_TOOL_FINGER:
				if ((common->wcmProtocolLevel != WCM_PROTOCOL_GENERIC)
				    && !private->wcmUseMT)  /* this isn't in usbParseKeyEvent() */
					return PAD_ID;
				else
					return TOUCH_ID;

			case BTN_TOOL_RUBBER:
				return ERASER_ID;

			case BTN_TOOL_DOUBLETAP:
			case BTN_TOOL_TRIPLETAP:
				return TOUCH_ID;

			case BTN_TOUCH:
				if (common->wcmProtocolLevel == WCM_PROTOCOL_GENERIC &&
				    /* 1FG USB touchscreen */
				    !TabletHasFeature(common, WCM_PEN) &&
				    TabletHasFeature(common, WCM_1FGT) &&
				    TabletHasFeature(common, WCM_LCD))
					return TOUCH_ID;
				else
					break;
		}
	}
	else if (type == EV_ABS) {
		switch (code) {
			case ABS_MT_SLOT:
			case ABS_MT_TRACKING_ID:
				return TOUCH_ID;
			case ABS_MISC:
				return usbFindDeviceTypeById(value);
		}
	}

	return 0;
}

/**
 * Queries the kernel through EVIOCGKEY for the latest device type
 * information. The result is the first tool type (e.g. STYLUS_ID)
 * found associated with the in-prox tool.
 *
 * @param[in] common
 * @return            A tool type (e.g. STYLUS_ID) associated with the in-prox tool
 */
static int refreshDeviceType(WacomCommonPtr common, int fd)
{
	int device_type = 0;
	unsigned long keys[NBITS(KEY_MAX)] = { 0 };
	int rc = ioctl(fd, EVIOCGKEY(sizeof(keys)), keys);
	int i;

	if (rc == -1) {
		xf86Msg(X_ERROR, "%s: failed to retrieve key bits\n", common->device_path);
		return 0;
	}

	for (i = 0; i < KEY_MAX; i++)
	{
		if (ISBITSET(keys, i))
			device_type = deviceTypeFromEvent(common, EV_KEY, i, 0);
		if (device_type)
			return device_type;
	}

	return 0;
}

static int deriveDeviceTypeFromButtonEvent(WacomCommonPtr common,
					   const struct input_event *event_ptr)
{
	wcmUSBData *usbdata = common->private;
	int nkeys;

	if (event_ptr->type == EV_KEY) {

		switch (event_ptr->code) {
		case BTN_LEFT:
		case BTN_MIDDLE:
		case BTN_RIGHT:
		case BTN_SIDE:
		case BTN_BACK:
		case BTN_EXTRA:
		case BTN_FORWARD:
			return PAD_ID;
		default:
			for (nkeys = 0; nkeys < usbdata->npadkeys; nkeys++)
			{
				if (event_ptr->code == usbdata->padkey_code[nkeys]) {
					return PAD_ID;
				}
			}
		}
	}
	return 0;
}

/***
 * Retrieve the tool type from an USB data packet by looking at the event
 * codes. Refer to linux/input.h for event codes that define tool types.
 *
 * @param[in] common
 * @param[in] event_ptr A pointer to the USB data packet that contains the
 * events to be processed.
 * @param[in] nevents Number of events in the packet.
 * @param[in] last_device_type The device type for the last event
 *
 * @return The tool type. This falls back on last_device_type if no
 *         pen/touch/eraser event code in the event, and on EVIOCGKEY
 *         if last_device_type is not a tool. If all else fails, '0'
 *         is returned.
 */
static int usbInitToolType(WacomCommonPtr common, int fd,
                           const struct input_event *event_ptr,
                           int nevents, int last_device_type)
{
	int i, device_type = 0;

	for (i = 0; (i < nevents) && !device_type; ++i, event_ptr++)
	{
		device_type = deviceTypeFromEvent(common, event_ptr->type, event_ptr->code, event_ptr->value);
	}

	if (!device_type)
		device_type = last_device_type;

	if (!device_type)
		device_type = refreshDeviceType(common, fd);

	if (!device_type) /* expresskey pressed at startup or missing type */
		for (i = 0; (i < nevents) && !device_type; ++i, event_ptr++)
			device_type = deriveDeviceTypeFromButtonEvent(common, event_ptr);

	return device_type;
}

/**
 * Check if the tool is a stylus/eraser/cursor and in-prox or not.
 *
 * @param device_type The tool type stored in wcmChannel
 * @param proximity The tool's proximity state

 * @return True if stylus/eraser/cursor is in-prox; False otherwise.
 */
static Bool usbIsTabletToolInProx(int device_type, int proximity)
{
	Bool is_tablet_tool = (device_type == STYLUS_ID) ||
				(device_type == CURSOR_ID) ||
				(device_type == ERASER_ID);
	return (is_tablet_tool && proximity);
}

static void usbDispatchEvents(InputInfoPtr pInfo)
{
	int i, c;
	WacomDeviceState *ds;
	struct input_event* event;
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;
	int channel;
	wcmUSBData* private = common->private;
	WacomDeviceState dslast = common->wcmChannel[private->lastChannel].valid.state;

	DBG(6, common, "%d events received\n", private->wcmEventCnt);

	private->wcmDeviceType = usbInitToolType(common, pInfo->fd,
	                                         private->wcmEvents,
	                                         private->wcmEventCnt,
	                                         dslast.device_type);

	if (private->wcmPenTouch)
	{
		/* We get both tablet tool and touch data from the kernel when
		 * both tools are in/down. So, if we were (hence the need of dslast)
		 * processing tablet tool events, we should ignore touch events.
		 *
		 * MT events will be posted to the userland when XInput 2.1
		 * is ready.
		 */
		if ((private->wcmDeviceType == TOUCH_ID) &&
				usbIsTabletToolInProx(dslast.device_type, dslast.proximity))
		{
			private->wcmEventCnt = 0;
			return;
		}
	}

	private->wcmLastToolSerial = protocol5Serial(private->wcmDeviceType, private->wcmLastToolSerial);
	channel = usbChooseChannel(common, private->wcmDeviceType, private->wcmLastToolSerial);

	/* couldn't decide channel? invalid data */
	if (channel == -1) {
		private->wcmEventCnt = 0;
		return;
	}

	ds = &common->wcmChannel[channel].work;
	dslast = common->wcmChannel[channel].valid.state;

	if (ds->device_type && ds->device_type != private->wcmDeviceType)
		LogMessageVerbSigSafe(X_ERROR, 0,
				      "usbDispatchEvents: Device Type mismatch - %d -> %d. This is a BUG.\n",
				      ds->device_type, private->wcmDeviceType);
	/* no device type? */
	if (!ds->device_type && private->wcmDeviceType) {
		ds->device_type = private->wcmDeviceType;
		ds->proximity = 1;
	}

	/* all USB data operates from previous context except relative values*/
	ds->relwheel = 0;
	ds->serial_num = private->wcmLastToolSerial;

	/* loop through all events in group */
	for (i=0; i<private->wcmEventCnt; ++i)
	{
		event = private->wcmEvents + i;
		DBG(11, common,
			"event[%d]->type=%d code=%d value=%d\n",
			i, event->type, event->code, event->value);

		/* Check for events to be ignored and skip them up front. */
		if (usbFilterEvent(common, event))
			continue;

		if (common->wcmHasHWTouchSwitch)
		{
			if ((event->type == EV_SW) &&
			    (event->code == SW_MUTE_DEVICE))
			{
				/* touch is disabled when SW_MUTE_DEVICE is set */
				int touch_enabled = (event->value == 0);

				if (touch_enabled != common->wcmHWTouchSwitchState)
				/* this property is only set for touch device */
					wcmUpdateHWTouchProperty(
						common->wcmTouchDevice,
						touch_enabled);
			}
		}

		/* absolute events */
		if (event->type == EV_ABS)
		{
			usbParseAbsEvent(common, event, channel);
			usbParseAbsMTEvent(common, event);
		}
		else if (event->type == EV_REL)
		{
			if (event->code == REL_WHEEL)
			{
				ds->relwheel = -event->value;
				ds->time = (int)GetTimeInMillis();
				common->wcmChannel[channel].dirty |= TRUE;
			}
			else
				LogMessageVerbSigSafe(X_ERROR, 0,
						      "%s: rel event recv'd (%d)!\n",
						      pInfo->name, event->code);
		}
		else if (event->type == EV_KEY)
		{
			/* Button events can be from puck or expresskeys */
			int btn_channel = (ds->device_type == CURSOR_ID) ?
					   channel : PAD_CHANNEL;

			usbParseKeyEvent(common, event, channel);
			usbParseBTNEvent(common, event, btn_channel);
		}
	} /* next event */

	/* DTF720 and DTF720a don't support eraser */
	if (((common->tablet_id == 0xC0) || (common->tablet_id == 0xC2)) && 
		(ds->device_type == ERASER_ID)) 
	{
		DBG(10, common,
			"DTF 720 doesn't support eraser ");
		return;
	}

	/*reset the serial number when the tool is going out */
	if (!ds->proximity)
		private->wcmLastToolSerial = 0;

	private->lastChannel = channel;

	for (c = 0; c < MAX_CHANNELS; c++) {
		ds = &common->wcmChannel[c].work;

		/* walk through all channels */
		if (common->wcmChannel[c].dirty) {
			DBG(10, common, "Dirty flag set on channel %d; sending event.\n", c);
			common->wcmChannel[c].dirty = FALSE;
			/* don't send touch event when touch isn't enabled */
			if (ds->device_type != TOUCH_ID || common->wcmTouch)
				wcmEvent(common, c, ds);
		}
	}
}

/* Quirks to unify the tool and tablet types for GENERIC protocol tablet PCs
 *
 * @param[in,out] keys Contains keys queried from hardware. If a
 *   touchscreen is detected, keys are modified to add BTN_TOOL_FINGER so
 *   that a TOUCH device is created later.
 * @param[in] abs Used to detect multi-touch touchscreens.  When detected,
 *   updates keys to add possibly missing BTN_TOOL_DOUBLETAP.
 * @param[in,out] common Used only for tablet features.  Adds TCM_TPC for
 *   touchscreens so correct defaults, such as absolute mode, are used.
 */
static void usbGenericTouchscreenQuirks(unsigned long *keys,
					unsigned long *abs,
					WacomCommonPtr common)
{
	/* USB Tablet PC single finger touch devices do not emit
	 * BTN_TOOL_FINGER since it is a touchscreen device.
	 */
	if (ISBITSET(keys, BTN_TOUCH) &&
			!ISBITSET(keys, BTN_TOOL_FINGER) &&
			!ISBITSET(keys, BTN_TOOL_PEN))
	{
		SETBIT(keys, BTN_TOOL_FINGER); /* 1FGT */
		TabletSetFeature(common, WCM_TPC);
	}

	/* Serial Tablet PC two finger touch devices do not emit
	 * BTN_TOOL_DOUBLETAP since they are not touchpads.
	 */
	if (ISBITSET(abs, ABS_MT_SLOT) && !ISBITSET(keys, BTN_TOOL_DOUBLETAP))
		SETBIT(keys, BTN_TOOL_DOUBLETAP); /* 2FGT */
}

/**
 * Query the device's fd for the key bits and the tablet ID. Returns the ID
 * on success or 0 on failure.
 * For USB devices, we simply copy the information the kernel gives us.
 */
static int usbProbeKeys(InputInfoPtr pInfo)
{
	struct input_id wacom_id;
	WacomDevicePtr  priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr  common = priv->common;
	unsigned long abs[NBITS(ABS_MAX)] = {0};

	if (ioctl(pInfo->fd, EVIOCGBIT(EV_KEY, (sizeof(unsigned long)
						* NBITS(KEY_MAX))), common->wcmKeys) < 0)
	{
		xf86Msg(X_ERROR, "%s: usbProbeKeys unable to "
				"ioctl USB key bits.\n", pInfo->name);
		return 0;
	}

	if (ioctl(pInfo->fd, EVIOCGID, &wacom_id) < 0)
	{
		xf86Msg(X_ERROR, "%s: usbProbeKeys unable to "
				"ioctl Device ID.\n", pInfo->name);
		return 0;
	}

        if (ioctl(pInfo->fd, EVIOCGBIT(EV_ABS, sizeof(abs)), abs) < 0)
	{
		xf86Msg(X_ERROR, "%s: usbProbeKeys unable to ioctl "
			"abs bits.\n", pInfo->name);
		return 0;
	}

	/* The wcmKeys stored above have different meaning for generic
	 * protocol.  Detect that and change default protocol 4 to
	 * generic.
	 */
	if (!ISBITSET(abs, ABS_MISC))
	{
		common->wcmProtocolLevel = WCM_PROTOCOL_GENERIC;
		usbGenericTouchscreenQuirks(common->wcmKeys, abs, common);
	}

	common->vendor_id = wacom_id.vendor;
	common->tablet_id = wacom_id.product;

	return wacom_id.product;
}


/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
