/*
 * Copyright 1995-2002 by Frederic Lepied, France. <Lepied@XFree86.org>
 * Copyright 2002-2010 by Ping Cheng, Wacom. <pingc@wacom.com>
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
#include "wcmFilter.h"

#include <asm/types.h>
#include <linux/input.h>
#include <sys/utsname.h>

#define MAX_USB_EVENTS 32

typedef struct {
	int wcmLastToolSerial;
	int wcmBTNChannel;
	int wcmEventCnt;
	struct input_event wcmEvents[MAX_USB_EVENTS];
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
static int usbChooseChannel(WacomCommonPtr common);

	WacomDeviceClass gWacomUSBDevice =
	{
		usbDetect,
		NULL, /* no USB-specific options */
		usbWcmInit,
		usbProbeKeys
	};

#define DEFINE_MODEL(mname, identifier, protocol, filter) \
static struct _WacomModel mname =		\
{						\
	.name = identifier,			\
	.Initialize = usbInitProtocol##protocol,\
	.GetResolution = NULL,			\
	.GetRanges = usbWcmGetRanges,		\
	.Start = usbStart,			\
	.Parse = usbParse,			\
	.FilterRaw = filter,			\
	.DetectConfig = usbDetectConfig,	\
};

DEFINE_MODEL(usbUnknown,	"Unknown USB",		5, NULL);
DEFINE_MODEL(usbPenPartner,	"USB PenPartner",	4, wcmFilterCoord);
DEFINE_MODEL(usbGraphire,	"USB Graphire",		4, wcmFilterCoord);
DEFINE_MODEL(usbGraphire2,	"USB Graphire2",	4, wcmFilterCoord);
DEFINE_MODEL(usbGraphire3,	"USB Graphire3",	4, wcmFilterCoord);
DEFINE_MODEL(usbGraphire4,	"USB Graphire4",	4, wcmFilterCoord);
DEFINE_MODEL(usbBamboo,		"USB Bamboo",		4, wcmFilterCoord);
DEFINE_MODEL(usbBamboo1,	"USB Bamboo1",		4, wcmFilterCoord);
DEFINE_MODEL(usbBambooFun,	"USB BambooFun",	4, wcmFilterCoord);
DEFINE_MODEL(usbCintiq,		"USB PL/Cintiq",	4, NULL);
DEFINE_MODEL(usbCintiqPartner,	"USB CintiqPartner",	4, NULL);
DEFINE_MODEL(usbIntuos,		"USB Intuos1",		5, wcmFilterIntuos);
DEFINE_MODEL(usbIntuos2,	"USB Intuos2",		5, wcmFilterIntuos);
DEFINE_MODEL(usbIntuos3,	"USB Intuos3",		5, wcmFilterIntuos);
DEFINE_MODEL(usbIntuos4,	"USB Intuos4",		5, wcmFilterIntuos);
DEFINE_MODEL(usbVolito,		"USB Volito",		4, wcmFilterCoord);
DEFINE_MODEL(usbVolito2,	"USB Volito2",		4, wcmFilterCoord);
DEFINE_MODEL(usbCintiqV5,	"USB CintiqV5",		5, wcmFilterIntuos);
DEFINE_MODEL(usbTabletPC,	"USB TabletPC",		4, NULL);

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

#ifdef EVIOCGRAB
	/* Try to grab the event device so that data don't leak to /dev/input/mice */
	SYSCALL(err = ioctl(pInfo->fd, EVIOCGRAB, (pointer)1));

	/* this is called for all tools, so all but the first one fails with
	 * EBUSY */
	if (err < 0 && errno != EBUSY)
		xf86Msg(X_ERROR, "%s: Wacom X driver can't grab event device (%s)\n",
				pInfo->name, strerror(errno));
#endif
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

static struct
{
	const unsigned int vendor_id;
	const unsigned int model_id;
	int yRes; /* tablet Y resolution in points/inch */
	int xRes; /* tablet X resolution in points/inch */
	WacomModelPtr model;
} WacomModelDesc [] =
{
	{ WACOM_VENDOR_ID, 0x00, 1000, 1000, &usbPenPartner }, /* PenPartner */
	{ WACOM_VENDOR_ID, 0x10, 2032, 2032, &usbGraphire   }, /* Graphire */
	{ WACOM_VENDOR_ID, 0x11, 2032, 2032, &usbGraphire2  }, /* Graphire2 4x5 */
	{ WACOM_VENDOR_ID, 0x12, 2032, 2032, &usbGraphire2  }, /* Graphire2 5x7 */
	{ WACOM_VENDOR_ID, 0x13, 2032, 2032, &usbGraphire3  }, /* Graphire3 4x5 */
	{ WACOM_VENDOR_ID, 0x14, 2032, 2032, &usbGraphire3  }, /* Graphire3 6x8 */
	{ WACOM_VENDOR_ID, 0x15, 2032, 2032, &usbGraphire4  }, /* Graphire4 4x5 */
	{ WACOM_VENDOR_ID, 0x16, 2032, 2032, &usbGraphire4  }, /* Graphire4 6x8 */
	{ WACOM_VENDOR_ID, 0x17, 2540, 2540, &usbBambooFun  }, /* BambooFun 4x5 */
	{ WACOM_VENDOR_ID, 0x18, 2540, 2540, &usbBambooFun  }, /* BambooFun 6x8 */
	{ WACOM_VENDOR_ID, 0x19, 2032, 2032, &usbBamboo1    }, /* Bamboo1 Medium*/
	{ WACOM_VENDOR_ID, 0x81, 2032, 2032, &usbGraphire4  }, /* Graphire4 6x8 BlueTooth */

	{ WACOM_VENDOR_ID, 0xD1, 2540, 2540, &usbBamboo     }, /* CTL-460 */
	{ WACOM_VENDOR_ID, 0xD4, 2540, 2540, &usbBamboo     }, /* CTH-461 */
	{ WACOM_VENDOR_ID, 0xD3, 2540, 2540, &usbBamboo     }, /* CTL-660 */
	{ WACOM_VENDOR_ID, 0xD2, 2540, 2540, &usbBamboo     }, /* CTL-461/S */
	{ WACOM_VENDOR_ID, 0xD0, 2540, 2540, &usbBamboo     }, /* Bamboo Touch */
	{ WACOM_VENDOR_ID, 0xD8, 2540, 2540, &usbBamboo     }, /* CTH-661/S1 */
	{ WACOM_VENDOR_ID, 0xDA, 2540, 2540, &usbBamboo     }, /* CTH-461/L */
	{ WACOM_VENDOR_ID, 0xDB, 2540, 2540, &usbBamboo     }, /* CTH-661 */

	{ WACOM_VENDOR_ID, 0x20, 2540, 2540, &usbIntuos     }, /* Intuos 4x5 */
	{ WACOM_VENDOR_ID, 0x21, 2540, 2540, &usbIntuos     }, /* Intuos 6x8 */
	{ WACOM_VENDOR_ID, 0x22, 2540, 2540, &usbIntuos     }, /* Intuos 9x12 */
	{ WACOM_VENDOR_ID, 0x23, 2540, 2540, &usbIntuos     }, /* Intuos 12x12 */
	{ WACOM_VENDOR_ID, 0x24, 2540, 2540, &usbIntuos     }, /* Intuos 12x18 */

	{ WACOM_VENDOR_ID, 0x03,  508,  508, &usbCintiqPartner }, /* PTU600 */

	{ WACOM_VENDOR_ID, 0x30,  508,  508, &usbCintiq     }, /* PL400 */
	{ WACOM_VENDOR_ID, 0x31,  508,  508, &usbCintiq     }, /* PL500 */
	{ WACOM_VENDOR_ID, 0x32,  508,  508, &usbCintiq     }, /* PL600 */
	{ WACOM_VENDOR_ID, 0x33,  508,  508, &usbCintiq     }, /* PL600SX */
	{ WACOM_VENDOR_ID, 0x34,  508,  508, &usbCintiq     }, /* PL550 */
	{ WACOM_VENDOR_ID, 0x35,  508,  508, &usbCintiq     }, /* PL800 */
	{ WACOM_VENDOR_ID, 0x37,  508,  508, &usbCintiq     }, /* PL700 */
	{ WACOM_VENDOR_ID, 0x38,  508,  508, &usbCintiq     }, /* PL510 */
	{ WACOM_VENDOR_ID, 0x39,  508,  508, &usbCintiq     }, /* PL710 */
	{ WACOM_VENDOR_ID, 0xC0,  508,  508, &usbCintiq     }, /* DTF720 */
	{ WACOM_VENDOR_ID, 0xC2,  508,  508, &usbCintiq     }, /* DTF720a */
	{ WACOM_VENDOR_ID, 0xC4,  508,  508, &usbCintiq     }, /* DTF521 */
	{ WACOM_VENDOR_ID, 0xC7, 2540, 2540, &usbCintiq     }, /* DTU1931 */
	{ WACOM_VENDOR_ID, 0xCE, 2540, 2540, &usbCintiq     }, /* DTU2231 */
	{ WACOM_VENDOR_ID, 0xF0, 2540, 2540, &usbCintiq     }, /* DTU1631 */

	{ WACOM_VENDOR_ID, 0x41, 2540, 2540, &usbIntuos2    }, /* Intuos2 4x5 */
	{ WACOM_VENDOR_ID, 0x42, 2540, 2540, &usbIntuos2    }, /* Intuos2 6x8 */
	{ WACOM_VENDOR_ID, 0x43, 2540, 2540, &usbIntuos2    }, /* Intuos2 9x12 */
	{ WACOM_VENDOR_ID, 0x44, 2540, 2540, &usbIntuos2    }, /* Intuos2 12x12 */
	{ WACOM_VENDOR_ID, 0x45, 2540, 2540, &usbIntuos2    }, /* Intuos2 12x18 */
	{ WACOM_VENDOR_ID, 0x47, 2540, 2540, &usbIntuos2    }, /* Intuos2 6x8  */

	{ WACOM_VENDOR_ID, 0x60, 1016, 1016, &usbVolito     }, /* Volito */

	{ WACOM_VENDOR_ID, 0x61, 1016, 1016, &usbVolito2    }, /* PenStation */
	{ WACOM_VENDOR_ID, 0x62, 1016, 1016, &usbVolito2    }, /* Volito2 4x5 */
	{ WACOM_VENDOR_ID, 0x63, 1016, 1016, &usbVolito2    }, /* Volito2 2x3 */
	{ WACOM_VENDOR_ID, 0x64, 1016, 1016, &usbVolito2    }, /* PenPartner2 */

	{ WACOM_VENDOR_ID, 0x65, 2540, 2540, &usbBamboo     }, /* Bamboo */
	{ WACOM_VENDOR_ID, 0x69, 1012, 1012, &usbBamboo1    }, /* Bamboo1 */

	{ WACOM_VENDOR_ID, 0xB0, 5080, 5080, &usbIntuos3    }, /* Intuos3 4x5 */
	{ WACOM_VENDOR_ID, 0xB1, 5080, 5080, &usbIntuos3    }, /* Intuos3 6x8 */
	{ WACOM_VENDOR_ID, 0xB2, 5080, 5080, &usbIntuos3    }, /* Intuos3 9x12 */
	{ WACOM_VENDOR_ID, 0xB3, 5080, 5080, &usbIntuos3    }, /* Intuos3 12x12 */
	{ WACOM_VENDOR_ID, 0xB4, 5080, 5080, &usbIntuos3    }, /* Intuos3 12x19 */
	{ WACOM_VENDOR_ID, 0xB5, 5080, 5080, &usbIntuos3    }, /* Intuos3 6x11 */
	{ WACOM_VENDOR_ID, 0xB7, 5080, 5080, &usbIntuos3    }, /* Intuos3 4x6 */

	{ WACOM_VENDOR_ID, 0xB8, 5080, 5080, &usbIntuos4    }, /* Intuos4 4x6 */
	{ WACOM_VENDOR_ID, 0xB9, 5080, 5080, &usbIntuos4    }, /* Intuos4 6x9 */
	{ WACOM_VENDOR_ID, 0xBA, 5080, 5080, &usbIntuos4    }, /* Intuos4 8x13 */
	{ WACOM_VENDOR_ID, 0xBB, 5080, 5080, &usbIntuos4    }, /* Intuos4 12x19*/
	{ WACOM_VENDOR_ID, 0xBC, 5080, 5080, &usbIntuos4    }, /* Intuos4 WL USB Endpoint */
	{ WACOM_VENDOR_ID, 0xBD, 5080, 5080, &usbIntuos4    }, /* Intuos4 WL Bluetooth Endpoint */

	{ WACOM_VENDOR_ID, 0x3F, 5080, 5080, &usbCintiqV5   }, /* Cintiq 21UX */
	{ WACOM_VENDOR_ID, 0xC5, 5080, 5080, &usbCintiqV5   }, /* Cintiq 20WSX */
	{ WACOM_VENDOR_ID, 0xC6, 5080, 5080, &usbCintiqV5   }, /* Cintiq 12WX */
	{ WACOM_VENDOR_ID, 0xCC, 5080, 5080, &usbCintiqV5   }, /* Cintiq 21UX2 */

	{ WACOM_VENDOR_ID, 0x90, 2540, 2540, &usbTabletPC   }, /* TabletPC 0x90 */
	{ WACOM_VENDOR_ID, 0x93, 2540, 2540, &usbTabletPC   }, /* TabletPC 0x93 */
	{ WACOM_VENDOR_ID, 0x9A, 2540, 2540, &usbTabletPC   }, /* TabletPC 0x9A */
	{ WACOM_VENDOR_ID, 0x9F,   10,   10, &usbTabletPC   }, /* CapPlus  0x9F */
	{ WACOM_VENDOR_ID, 0xE2,   10,   10, &usbTabletPC   }, /* TabletPC 0xE2 */
	{ WACOM_VENDOR_ID, 0xE3, 2540, 2540, &usbTabletPC   }, /* TabletPC 0xE3 */

	/* IDs from Waltop's driver, available http://www.waltop.com.tw/download.asp?lv=0&id=2.
	   Accessed 8 Apr 2010, driver release date 2009/08/11, fork of linuxwacom 0.8.4.
	   Some more info would be nice for the ID's below... */
	{ WALTOP_VENDOR_ID, 0x24, 2032, 2032, &usbGraphire   },
	{ WALTOP_VENDOR_ID, 0x25, 2032, 2032, &usbGraphire2  },
	{ WALTOP_VENDOR_ID, 0x26, 2032, 2032, &usbGraphire2  },
	{ WALTOP_VENDOR_ID, 0x27, 2032, 2032, &usbGraphire3  },
	{ WALTOP_VENDOR_ID, 0x28, 2032, 2032, &usbGraphire3  },
	{ WALTOP_VENDOR_ID, 0x30, 2032, 2032, &usbGraphire4  },
	{ WALTOP_VENDOR_ID, 0x31, 2032, 2032, &usbGraphire4  },
	{ WALTOP_VENDOR_ID, 0x32, 2540, 2540, &usbBambooFun  },
	{ WALTOP_VENDOR_ID, 0x33, 2540, 2540, &usbBambooFun  },
	{ WALTOP_VENDOR_ID, 0x34, 2032, 2032, &usbBamboo1    },
	{ WALTOP_VENDOR_ID, 0x35, 2032, 2032, &usbGraphire4  },
	{ WALTOP_VENDOR_ID, 0x36, 2032, 2032, &usbGraphire4  },
	{ WALTOP_VENDOR_ID, 0x37, 2032, 2032, &usbGraphire4  },
	{ WALTOP_VENDOR_ID, 0x38, 2540, 2540, &usbBambooFun  },
	{ WALTOP_VENDOR_ID, 0x39, 2540, 2540, &usbBambooFun  },
	{ WALTOP_VENDOR_ID, 0x51, 2540, 2540, &usbBamboo     },
	{ WALTOP_VENDOR_ID, 0x52, 2540, 2540, &usbBamboo     },

	{ WALTOP_VENDOR_ID, 0x53, 2540, 2540, &usbBamboo     },
	{ WALTOP_VENDOR_ID, 0x54, 2540, 2540, &usbBamboo     },
	{ WALTOP_VENDOR_ID, 0x55, 2540, 2540, &usbBamboo     },
	{ WALTOP_VENDOR_ID, 0x56, 2540, 2540, &usbBamboo     },
	{ WALTOP_VENDOR_ID, 0x57, 2540, 2540, &usbBamboo     },
	{ WALTOP_VENDOR_ID, 0x58, 2540, 2540, &usbBamboo     },
	{ WALTOP_VENDOR_ID, 0x500, 2540, 2540, &usbBamboo    },
	{ WALTOP_VENDOR_ID, 0x501, 2540, 2540, &usbBamboo    },
	{ WALTOP_VENDOR_ID, 0x502, 5080, 5080, &usbIntuos4   },
	{ WALTOP_VENDOR_ID, 0x503, 5080, 5080, &usbIntuos4   },

	/* N-Trig devices */
	{ NTRIG_VENDOR_ID,  0x01, 1122, 934, &usbTabletPC    }
};

static Bool usbWcmInit(InputInfoPtr pInfo, char* id, float *version)
{
	int i;
	struct input_id sID;
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;

	DBG(1, priv, "initializing USB tablet\n");

	if (!common->private &&
	    !(common->private = calloc(1, sizeof(wcmUSBData))))
	{
		xf86Msg(X_ERROR, "%s: unable to alloc event queue.\n",
					pInfo->name);
		return !Success;
	}

	*version = 0.0;

	/* fetch vendor, product, and model name */
	ioctl(pInfo->fd, EVIOCGID, &sID);
	ioctl(pInfo->fd, EVIOCGNAME(sizeof(id)), id);

	for (i = 0; i < sizeof (WacomModelDesc) / sizeof (WacomModelDesc [0]); i++)
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

	/* Find out supported button codes - except mouse button codes
	 * BTN_LEFT and BTN_RIGHT, which are always fixed. */
	common->npadkeys = 0;
	for (i = 0; i < sizeof (padkey_codes) / sizeof (padkey_codes [0]); i++)
		if (ISBITSET (common->wcmKeys, padkey_codes [i]))
			common->padkey_code [common->npadkeys++] = padkey_codes [i];

	/* nbuttons tracks maximum buttons on all tools (stylus/mouse).
	 *
	 * Mouse support left, middle, right, side, and extra side button.
	 * Stylus support tip and 2 stlyus buttons.
	 */
	if (ISBITSET (common->wcmKeys, BTN_TOOL_MOUSE))
		common->nbuttons = WCM_USB_MAX_MOUSE_BUTTONS;
	else
		common->nbuttons = WCM_USB_MAX_STYLUS_BUTTONS;

	return Success;
}

static void usbInitProtocol5(WacomCommonPtr common, const char* id,
	float version)
{
	common->wcmProtocolLevel = WCM_PROTOCOL_5;
	common->wcmPktLength = sizeof(struct input_event);
	common->wcmCursorProxoutDistDefault 
			= PROXOUT_INTUOS_DISTANCE;

	/* tilt enabled */
	common->wcmFlags |= TILT_ENABLED_FLAG;

	/* reinitialize max here since 0 is for Graphire series */
	common->wcmMaxCursorDist = 256;

}

static void usbInitProtocol4(WacomCommonPtr common, const char* id,
	float version)
{
	common->wcmProtocolLevel = WCM_PROTOCOL_4;
	common->wcmPktLength = sizeof(struct input_event);
	common->wcmCursorProxoutDistDefault 
			= PROXOUT_GRAPHIRE_DISTANCE;

	/* tilt disabled */
	common->wcmFlags &= ~TILT_ENABLED_FLAG;
}

int usbWcmGetRanges(InputInfoPtr pInfo)
{
	struct input_absinfo absinfo;
	unsigned long ev[NBITS(EV_MAX)] = {0};
	unsigned long abs[NBITS(ABS_MAX)] = {0};
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common =	priv->common;
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

        if (ioctl(pInfo->fd, EVIOCGBIT(EV_ABS,sizeof(abs)),abs) < 0)
	{
		xf86Msg(X_ERROR, "%s: unable to ioctl abs bits.\n", pInfo->name);
		return !Success;
	}

	/* absolute values */
	if (!ISBITSET(ev,EV_ABS))
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
		xf86Msg(X_ERROR, "%s: xmax value is wrong.\n", pInfo->name);
		return !Success;
	}
	if (!is_touch)
		common->wcmMaxX = absinfo.maximum;
	else
		common->wcmMaxTouchX = absinfo.maximum;

	/* max y */
	if (ioctl(pInfo->fd, EVIOCGABS(ABS_Y), &absinfo) < 0)
	{
		xf86Msg(X_ERROR, "%s: unable to ioctl ymax value.\n", pInfo->name);
		return !Success;
	}

	if (absinfo.maximum <= 0)
	{
		xf86Msg(X_ERROR, "%s: ymax value is wrong.\n", pInfo->name);
		return !Success;
	}
	if (!is_touch)
		common->wcmMaxY = absinfo.maximum;
	else
		common->wcmMaxTouchY = absinfo.maximum;

	/* max finger strip X for tablets with Expresskeys
	 * or touch physical X for TabletPCs with touch */
	if (ioctl(pInfo->fd, EVIOCGABS(ABS_RX), &absinfo) == 0)
	{
		if (is_touch)
			common->wcmTouchResolX = absinfo.maximum;
		else
			common->wcmMaxStripX = absinfo.maximum;
	}

	/* max finger strip Y for tablets with Expresskeys
	 * or touch physical Y for TabletPCs with touch */
	if (ioctl(pInfo->fd, EVIOCGABS(ABS_RY), &absinfo) == 0)
	{
		if (is_touch)
			common->wcmTouchResolY = absinfo.maximum;
		else
			common->wcmMaxStripY = absinfo.maximum;
	}

	if (is_touch && common->wcmTouchResolX && common->wcmMaxTouchX)
	{
		common->wcmTouchResolX = (int)(((double)common->wcmTouchResolX)
			 / ((double)common->wcmMaxTouchX) + 0.5);
		common->wcmTouchResolY = (int)(((double)common->wcmTouchResolY)
			 / ((double)common->wcmMaxTouchY) + 0.5);

		if (!common->wcmTouchResolX || !common->wcmTouchResolY)
		{
			xf86Msg(X_ERROR, "%s: touch resolution value(s) was wrong TouchResolX"
				" = %d MaxTouchY = %d.\n", pInfo->name, common->wcmTouchResolX,
				common->wcmTouchResolY);
			return !Success;
		}

	}

	/* max z cannot be configured */
	if (ioctl(pInfo->fd, EVIOCGABS(ABS_PRESSURE), &absinfo) == 0)
		common->wcmMaxZ = absinfo.maximum;

	/* max distance */
	if (ioctl(pInfo->fd, EVIOCGABS(ABS_DISTANCE), &absinfo) == 0)
		common->wcmMaxDist = absinfo.maximum;


	if ((common->tablet_id >= 0xd0) && (common->tablet_id <= 0xd3))
	{
		/* BTN_TOOL_DOUBLETAP means this is a touchpad and
		 * !BTN_TOOL_TRIPLETAP detects this is MT-version
		 * of touchpad; which uses generic protocol.
		 */
		if (ISBITSET(common->wcmKeys, BTN_TOOL_DOUBLETAP) &&
		    !ISBITSET(common->wcmKeys, BTN_TOOL_TRIPLETAP))
			common->wcmProtocolLevel = WCM_PROTOCOL_GENERIC;
	}


	return Success;
}

static int usbDetectConfig(InputInfoPtr pInfo)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;

	DBG(10, common, "\n");
	if (IsPad (priv))
		priv->nbuttons = common->npadkeys;
	else
		priv->nbuttons = common->nbuttons;

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

/* Up to MAX_CHANNEL tools can be tracked concurrently by driver.
 * Chose a channel to use to track current batch of events.
 */
static int usbChooseChannel(WacomCommonPtr common)
{
	/* figure out the channel to use based on serial number */
	int i, channel = -1;
	wcmUSBData* private = common->private;
	int serial = private->wcmLastToolSerial;

	if (common->wcmProtocolLevel == WCM_PROTOCOL_GENERIC)
	{
		/* Generic Protocol devices do not use any form of
		 * serial #'s to multiplex events over a single input
		 * and so can always map to channel 0.  This means
		 * only 1 tool can ever been in proximity at one time
		 * (MT events are special case handled elsewhere).
		 * It also means all buttons must be associated with
		 * a single tool and can not send tablet buttons
		 * as part of a pad tool.
		 */
		channel = 0;
		serial = 1;

		/* Generic devices need to map stylus buttons to "channel"
		 * and all other button presses to PAD.  Hardcode PAD
		 * channel here.
		 */
		private->wcmBTNChannel = MAX_CHANNELS-1;
	}
	else if (common->wcmProtocolLevel == WCM_PROTOCOL_4)
	{
		/* Protocol 4 devices support only 2 devices being
		 * in proximity at the same time.  This includes
		 * the PAD device as well as 1 other tool
		 * (stylus, mouse, finger touch, etc).
		 * There is a special case of Tablet PC that also
		 * suport a 3rd tool (2nd finger touch) to also be
		 * in proximity at same time but this should eventually
		 * go away when its switched to MT events to fix loss of
		 * events.
		 *
		 * Protocol 4 send fixed serial numbers along with events.
		 * Events associated with PAD device
		 * will send serial number of 0xf0 always.
		 * Events associated with BTN_TOOL_TRIPLETAP (2nd finger
		 * touch) send a serial number of 0x02 always.
		 * Events associated with all other BTN_TOOL_*'s will
		 * either send a serial # of 0x01 or we can act as if
		 * they did send that value.
		 *
		 * Since its a fixed mapping, directly convert this to
		 * channels 0 to 2 with last channel always used for
		 * pad devices.
		 */
		if (serial == 0xf0)
			channel = MAX_CHANNELS-1;
		else if (serial)
			channel = serial-1;
		else
			channel = 0;
		/* All events go to same channel for Protocol 4 */
		private->wcmBTNChannel = channel;
	}
	else if (serial) /* serial number should never be 0 for V5 devices */
	{
		/* Protocol 5 devices can support tracking 2 or 3
		 * tools at once.  One is the PAD device
		 * as well as a stylus and/or mouse.
		 *
		 * Events associated with PAD device
		 * will send serial number of -1 (0xffffffff) always.
		 * Events associated with all other BTN_TOOL_*'s will
		 * send a dynamic serial #.
		 *
		 * Logic here is related to dynamically mapping
		 * serial numbers to a fixed channel #.
		 */
		if (TabletHasFeature(common, WCM_DUALINPUT))
		{
			/* find existing channel */
			for (i=0; i<MAX_CHANNELS; ++i)
			{
				if (common->wcmChannel[i].work.proximity &&
					common->wcmChannel[i].work.serial_num == serial)
				{
					channel = i;
					break;
				}
			}

			/* find an empty channel */
			if (channel < 0)
			{
				for (i=0; i<MAX_CHANNELS; ++i)
				{
					if (!common->wcmChannel[i].work.proximity)
					{
						channel = i;
						break;
					}
				}
			}
		}
		else  /* one transducer plus expresskey (pad) is supported */
		{
			if (serial == -1)  /* pad */
				channel = 1;
			else if ( (common->wcmChannel[0].work.proximity &&  /* existing transducer */
				    (common->wcmChannel[0].work.serial_num == serial)) ||
					!common->wcmChannel[0].work.proximity ) /* new transducer */
				channel = 0;
		}
		/* All events go to same channel for Protocol 5 */
		private->wcmBTNChannel = channel;
	}

	/* fresh out of channels */
	if (channel < 0)
	{
		/* This should never happen in normal use.
		 * Let's start over again. Force prox-out for all channels.
		 */
		for (i=0; i<MAX_CHANNELS; ++i)
		{
			if (common->wcmChannel[i].work.proximity &&
					(common->wcmChannel[i].work.serial_num != -1))
			{
				common->wcmChannel[i].work.proximity = 0;
				/* dispatch event */
				wcmEvent(common, i, &common->wcmChannel[i].work);
			}
		}
		DBG(1, common, "device with serial number: %u"
			" at %d: Exceeded channel count; ignoring the events.\n",
			serial, (int)GetTimeInMillis());
	}
	else
		private->wcmLastToolSerial = serial;

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
		xf86Msg(X_ERROR, "%s: usbParse: Exceeded event queue (%d) \n",
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
			xf86Msg(X_ERROR, "%s: usbParse: Ignoring event from invalid serial 0\n",
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

static int usbParseAbsEvent(WacomCommonPtr common,
			    struct input_event *event, WacomDeviceState *ds)
{
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
			ds->tiltx = event->value - common->wcmMaxtiltX/2;
			break;
		case ABS_TILT_Y:
			ds->tilty = event->value - common->wcmMaxtiltY/2;
			break;
		case ABS_PRESSURE:
			if (ds->device_type == TOUCH_ID)
				ds->capacity = event->value;
			else
				ds->pressure = event->value;
			break;
		case ABS_DISTANCE:
			ds->distance = event->value;
			break;
		case ABS_WHEEL:
			{
				double norm = event->value *
					MAX_ROTATION_RANGE /
					(double)MAX_ABS_WHEEL;
				ds->abswheel = (int)norm + MIN_ROTATION;
				break;
			}
		case ABS_Z:
			ds->abswheel = event->value;
			break;
		case ABS_THROTTLE:
			ds->throttle = event->value;
			break;
		case ABS_MISC:
			if (event->value)
				ds->device_id = event->value;
			break;
		default:
			change = 0;
	}
	return change;
}

static struct
{
	unsigned long device_type;
	unsigned long tool_key;
} wcmTypeToKey [] =
{
	{ STYLUS_ID, BTN_TOOL_PEN       },
	{ STYLUS_ID, BTN_TOOL_PENCIL    },
	{ STYLUS_ID, BTN_TOOL_BRUSH     },
	{ STYLUS_ID, BTN_TOOL_AIRBRUSH  },
	{ ERASER_ID, BTN_TOOL_RUBBER    },
	{ CURSOR_ID, BTN_TOOL_MOUSE     },
	{ CURSOR_ID, BTN_TOOL_LENS      },
	{ TOUCH_ID,  BTN_TOOL_DOUBLETAP },
	{ TOUCH_ID,  BTN_TOOL_TRIPLETAP },
	{ PAD_ID,    BTN_FORWARD        },
	{ PAD_ID,    BTN_0              }
};

#define MOD_BUTTONS(bit, value) do { \
	shift = 1<<bit; \
	ds->buttons = (((value) != 0) ? \
		       (ds->buttons | (shift)) : (ds->buttons & ~(shift))); \
        } while (0)

static int usbParseKeyEvent(WacomCommonPtr common,
			    struct input_event *event, WacomDeviceState *ds,
			    WacomDeviceState *dslast)
{
	int shift;
	int change = 1;

	/* BTN_TOOL_* are sent to indicate when a specific tool is going
	 * in our out of proximity.  When going out of proximity, ds
	 * is initialized to zeros elsewere.  When going in proximity,
	 * here we initialize tool specific values.
	 *
	 * This requires tools that map to same channel of an input
	 * device and that share events (ABS_X of PEN and ERASER for
	 * example) not to be in proximity at the same time.  Tools
	 * that map to different channels can be in proximity at same
	 * time with no confusion.
	 */
	switch (event->code)
	{
		case BTN_TOOL_PEN:
		case BTN_TOOL_PENCIL:
		case BTN_TOOL_BRUSH:
		case BTN_TOOL_AIRBRUSH:
			ds->device_type = STYLUS_ID;
			/* V5 tools use ABS_MISC to report device_id */
			if (common->wcmProtocolLevel == WCM_PROTOCOL_4)
				ds->device_id = STYLUS_DEVICE_ID;
			ds->proximity = (event->value != 0);
			DBG(6, common,
			    "USB stylus detected %x\n",
			    event->code);
			break;

		case BTN_TOOL_RUBBER:
			ds->device_type = ERASER_ID;
			/* V5 tools use ABS_MISC to report device_id */
			if (common->wcmProtocolLevel == WCM_PROTOCOL_4)
				ds->device_id = ERASER_DEVICE_ID;
			ds->proximity = (event->value != 0);
			if (ds->proximity)
				ds->proximity = ERASER_PROX;
			DBG(6, common,
			    "USB eraser detected %x (value=%d)\n",
			    event->code, event->value);
			break;

		case BTN_TOOL_MOUSE:
		case BTN_TOOL_LENS:
			DBG(6, common,
			    "USB mouse detected %x (value=%d)\n",
			    event->code, event->value);
			ds->device_type = CURSOR_ID;
			/* V5 tools use ABS_MISC to report device_id */
			if (common->wcmProtocolLevel == WCM_PROTOCOL_4)
				ds->device_id = CURSOR_DEVICE_ID;
			ds->proximity = (event->value != 0);
			break;

		case BTN_0:
		case BTN_FORWARD:
			DBG(6, common,
			    "USB Pad detected %x (value=%d)\n",
			    event->code, event->value);
			ds->device_type = PAD_ID;
			ds->device_id = PAD_DEVICE_ID;
			ds->proximity = (event->value != 0);
			break;

               case BTN_TOUCH:
			/* Treat BTN_TOUCH same as BTN_TOOL_DOUBLETAP
			 * for touchpads.
			 * TODO: Tablets that do not use wacom style
			 * multiplexing over a single input device
			 * also can report BTN_TOUCH same as
			 * BTN_TOOL_PEN would be used.  We should
			 * allow for that case as well.
			 */
			if (common->wcmProtocolLevel != WCM_PROTOCOL_GENERIC)
				break;

			/* fall through */
		case BTN_TOOL_DOUBLETAP:
			/* If a real double tap report, ignore. */
			if (common->wcmProtocolLevel == WCM_PROTOCOL_GENERIC &&
			    event->code == BTN_TOOL_DOUBLETAP)
				break;

			DBG(6, common,
			    "USB Touch detected %x (value=%d)\n",
			    event->code, event->value);
			ds->device_type = TOUCH_ID;
			ds->device_id = TOUCH_DEVICE_ID;
			ds->proximity = event->value;
			/* time stamp for 2FGT gesture events */
			if ((ds->proximity && !dslast->proximity) ||
			    (!ds->proximity && dslast->proximity))
				ds->sample = (int)GetTimeInMillis();
			/* left button is always pressed for
			 * touchscreen without capacity
			 * when the first finger touch event received.
			 * For touchscreen with capacity, left button
			 * event will be decided
			 * in wcmCommon.c by capacity threshold.
			 * Touchpads should not have button
			 * press.
			 */
			if (common->wcmCapacityDefault < 0 &&
			    (TabletHasFeature(common, WCM_TPC)))
				MOD_BUTTONS(0, event->value);
			break;

		case BTN_TOOL_TRIPLETAP:
			/* If a real triple tap report, ignore. */
			if (common->wcmProtocolLevel == WCM_PROTOCOL_GENERIC)
				break;

			DBG(6, common,
			    "USB Touch second finger detected %x (value=%d)\n",
			    event->code, event->value);
			ds->device_type = TOUCH_ID;
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

	if (change)
		return change;

	/* Rest back to non-default value for next switch statement */
	change = 1;

	/* From this point on, all BTN_* will be real button presses.
	 * Stylus buttons always go with *ds.  Handle remaining
	 * cases upon return.
	 */
	switch (event->code)
	{
		case BTN_STYLUS:
			MOD_BUTTONS(1, event->value);
			break;

		case BTN_STYLUS2:
			MOD_BUTTONS(2, event->value);
			break;

		default:
			change = 0;
	}

	return change;
}

/* Handle all button presses except for stylus buttons */
static int usbParseBTNEvent(WacomCommonPtr common,
			    struct input_event *event, WacomDeviceState *ds)
{
	int shift, nkeys;
	int change = 1;

	switch (event->code)
	{
		case BTN_LEFT:
			MOD_BUTTONS(0, event->value);
			break;

		case BTN_MIDDLE:
			MOD_BUTTONS(1, event->value);
			break;

		case BTN_RIGHT:
			MOD_BUTTONS(2, event->value);
			break;

		case BTN_SIDE:
			MOD_BUTTONS(3, event->value);
			break;

		case BTN_EXTRA:
			MOD_BUTTONS(4, event->value);
			break;

		default:
			for (nkeys = 0; nkeys < common->npadkeys; nkeys++)
			{
				if (event->code == common->padkey_code[nkeys])
				{
					MOD_BUTTONS(nkeys, event->value);
					break;
				}
			}
			if (nkeys >= common->npadkeys)
				change = 0;
	}
	return change;
}

static void usbDispatchEvents(InputInfoPtr pInfo)
{
	int i;
	WacomDeviceState *ds, *btn_ds;
	struct input_event* event;
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;
	int channel;
	int channel_change = 0, btn_channel_change = 0;
	WacomChannelPtr pChannel;
	WacomDeviceState dslast;
	wcmUSBData* private = common->private;

	DBG(6, common, "%d events received\n", private->wcmEventCnt);

	channel = usbChooseChannel(common);

	/* couldn't decide channel? invalid data */
	if (channel == -1) {
		private->wcmEventCnt = 0;
		return;
	}

	pChannel = common->wcmChannel + channel;
	dslast = pChannel->valid.state;

	/* Because of linux input filtering, the kernel driver can not
	 * always force total set of event data when new tool comes into
	 * proximity.  This includes simple case of flipping stylus
	 * from pen to eraser tool. Therefore, when new tool is in-prox
	 * we must initialize all shared event values to same as previous
	 * tool to account for filtered events.
	 *
	 * For Generic and Protocol 4 devices that have fixed channel
	 * mappings, this is no problem.  Protocol 5 devices are difficult
	 * because they dynamically assign channel #'s and even simple
	 * case above can switch from channel 1 to channel 0.
	 *
	 * To simplify things, we take advantage of fact wacom kernel
	 * drivers force all values to zero when going out of proximity so
	 * we take a short cut and memset() to align when going in-prox
	 * instead of a memcpy().
	 *
	 * TODO: Some non-wacom tablets send X/Y data right before coming
	 * in proximity. The following discards that data.
	 * Adding "&& dslast.proximimty" to check would probably help
	 * this case.
	 * Some non-wacom tablets may also never reset their values
	 * to zero when out-of-prox.  The memset() can loss this data.
	 * Adding a !WCM_PROTOCOL_GENERIC check would probably help this case.
	 */
	if (!common->wcmChannel[channel].work.proximity)
	{
		memset(&common->wcmChannel[channel],0,sizeof(WacomChannel));

		/* in case the in-prox event was missing */
		/* TODO: There are not valid times when in-prox
		 * events are not sent by a driver except:
		 *
		 * 1) Starting X while tool is already in prox.
		 * 2) Non-wacom tablet sends only BTN_TOUCH without
		 * BTN_TOOL_PEN since it only support 1 tool.
		 *
		 * Case 1) should be handled in same location as
		 * below check of (ds->device_type == 0) since its
		 * same reason.  It is better to query for real
		 * value instead of assuming in-prox.
		 * Case 2) should be handled in case statement that
		 * processes BTN_TOUCH for WCM_PROTOCOL_GENERIC devices.
		 *
		 * So we should not be forcing to in-prox here because
		 * it could cause cursor jump from (X,Y)=(0,0) if events
		 * are sent while out-of-prox; which can happen only
		 * with WCM_PROTOCOL_GENERIC devices. Hint: see TODO above.
		 */
		common->wcmChannel[channel].work.proximity = 1;
	}

	/* all USB data operates from previous context except relative values*/
	ds = &common->wcmChannel[channel].work;
	ds->relwheel = 0;
	ds->serial_num = private->wcmLastToolSerial;
	/* For protocol 4 and 5 devices, ds == btn_ds. */
	btn_ds = &common->wcmChannel[private->wcmBTNChannel].work;

	/* loop through all events in group */
	for (i=0; i<private->wcmEventCnt; ++i)
	{
		event = private->wcmEvents + i;
		DBG(11, common,
			"event[%d]->type=%d code=%d value=%d\n",
			i, event->type, event->code, event->value);

		/* absolute events */
		if (event->type == EV_ABS)
		{
			channel_change |= usbParseAbsEvent(common, event, ds);
		}
		else if (event->type == EV_REL)
		{
			if (event->code == REL_WHEEL)
			{
				ds->relwheel = -event->value;
				channel_change |= 1;
			}
			else
				xf86Msg(X_ERROR, "%s: rel event recv'd (%d)!\n",
					pInfo->name, event->code);
		}
		else if (event->type == EV_KEY)
		{
			if (usbParseKeyEvent(common, event, ds, &dslast))
				channel_change |= 1;
			else
				btn_channel_change |=
					usbParseBTNEvent(common, event,
							 btn_ds);
		}
	} /* next event */

	/* device type unknown? Tool may be on the tablet when X starts. */
	if (!ds->device_type && !dslast.proximity)
	{
		unsigned long keys[NBITS(KEY_MAX)] = { 0 };

		/* Retrieve the type by asking a resend from the kernel */
		ioctl(common->fd, EVIOCGKEY(sizeof(keys)), keys);

		for (i=0; i<sizeof(wcmTypeToKey) / sizeof(wcmTypeToKey[0]); i++)
		{
			if (ISBITSET(keys, wcmTypeToKey[i].tool_key))
			{
				ds->device_type = wcmTypeToKey[i].device_type;
				break;
			}
		}
	}

	/* don't send touch event when touch isn't enabled */
	if ((ds->device_type == TOUCH_ID) && !common->wcmTouch)
		return;

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

	/* dispatch events */
	if (channel_change ||
	    (private->wcmBTNChannel == channel && btn_channel_change))
		wcmEvent(common, channel, ds);

       /* dispatch butten events when re-routed */
	if (private->wcmBTNChannel != channel && btn_channel_change)
	{
		/* Force to in proximity for this special case */
		btn_ds->proximity = 1;
		btn_ds->device_type = PAD_ID;
		btn_ds->device_id = PAD_DEVICE_ID;
		btn_ds->serial_num = 0xf0;
		wcmEvent(common, private->wcmBTNChannel, btn_ds);
	}
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

	return wacom_id.product;
}


/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
