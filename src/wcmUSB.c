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
 * GNU Lesser General Public License for more details.
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

#include <sys/utsname.h>

#ifndef BTN_TASK
#define BTN_TASK 0x117
#endif

#ifndef BTN_TOOL_DOUBLETAP
#define BTN_TOOL_DOUBLETAP 0x14d
#endif

#ifndef BTN_TOOL_TRIPLETAP
#define BTN_TOOL_TRIPLETAP 0x14e
#endif

static Bool usbDetect(LocalDevicePtr);
Bool usbWcmInit(LocalDevicePtr pDev, char* id, float *version);

static void usbInitProtocol5(WacomCommonPtr common, const char* id,
	float version);
static void usbInitProtocol4(WacomCommonPtr common, const char* id,
	float version);
int usbWcmGetRanges(LocalDevicePtr local);
static int usbParse(LocalDevicePtr local, const unsigned char* data);
static int usbDetectConfig(LocalDevicePtr local);
static void usbParseEvent(LocalDevicePtr local,
	const struct input_event* event);
static void usbParseChannel(LocalDevicePtr local, int channel, int serial);

	WacomDeviceClass gWacomUSBDevice =
	{
		usbDetect,
		usbWcmInit,
		xf86WcmReadPacket,
	};

	static WacomModel usbUnknown =
	{
		"Unknown USB",
		usbInitProtocol5,     /* assume the best */
		NULL,                 /* resolution not queried */
		usbWcmGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		NULL,                 /* input filtering not needed */
		usbDetectConfig,      /* detect hardware buttons etc */
	};

	static WacomModel usbPenPartner =
	{
		"USB PenPartner",
		usbInitProtocol4,
		NULL,                 /* resolution not queried */
		usbWcmGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		xf86WcmFilterCoord,   /* input filtering */
		usbDetectConfig,      /* detect hardware buttons etc */
	};

	static WacomModel usbGraphire =
	{
		"USB Graphire",
		usbInitProtocol4,
		NULL,                 /* resolution not queried */
		usbWcmGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		xf86WcmFilterCoord,   /* input filtering */
		usbDetectConfig,      /* detect hardware buttons etc */
	};

	static WacomModel usbGraphire2 =
	{
		"USB Graphire2",
		usbInitProtocol4,
		NULL,                 /* resolution not queried */
		usbWcmGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		xf86WcmFilterCoord,   /* input filtering */
		usbDetectConfig,      /* detect hardware buttons etc */
	};

	static WacomModel usbGraphire3 =
	{
		"USB Graphire3",
		usbInitProtocol4,
		NULL,                 /* resolution not queried */
		usbWcmGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		xf86WcmFilterCoord,   /* input filtering */
		usbDetectConfig,      /* detect hardware buttons etc */
	};

	static WacomModel usbGraphire4 =
	{
		"USB Graphire4",
		usbInitProtocol4,
		NULL,                 /* resolution not queried */
		usbWcmGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		xf86WcmFilterCoord,   /* input filtering */
		usbDetectConfig,      /* detect hardware buttons etc */
	};

	static WacomModel usbBamboo =
	{
		"USB Bamboo",
		usbInitProtocol4,
		NULL,                 /* resolution not queried */
		usbWcmGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		xf86WcmFilterCoord,   /* input filtering */
		usbDetectConfig,      /* detect hardware buttons etc */
	};

	static WacomModel usbBamboo1 =
	{
		"USB Bamboo1",
		usbInitProtocol4,
		NULL,                 /* resolution not queried */
		usbWcmGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		xf86WcmFilterCoord,   /* input filtering */
		usbDetectConfig,      /* detect hardware buttons etc */
	};

	static WacomModel usbBambooFun =
	{
		"USB BambooFun",
		usbInitProtocol4,
		NULL,                 /* resolution not queried */
		usbWcmGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		xf86WcmFilterCoord,   /* input filtering */
		usbDetectConfig,      /* detect hardware buttons etc */
	};

	static WacomModel usbCintiq =
	{
		"USB PL/Cintiq",
		usbInitProtocol4,
		NULL,                 /* resolution not queried */
		usbWcmGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		NULL,                 /* input filtering */
		usbDetectConfig,      /* detect hardware buttons etc */
	};

	static WacomModel usbCintiqPartner =
	{
		"USB CintiqPartner",
		usbInitProtocol4,
		NULL,                 /* resolution not queried */
		usbWcmGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		NULL,                 /* input filtering */
		usbDetectConfig,      /* detect hardware buttons etc */
	};

	static WacomModel usbIntuos =
	{
		"USB Intuos1",
		usbInitProtocol5,
		NULL,                 /* resolution not queried */
		usbWcmGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		xf86WcmFilterIntuos,  /* input filtering recommended */
		usbDetectConfig,      /* detect hardware buttons etc */
	};

	static WacomModel usbIntuos2 =
	{
		"USB Intuos2",
		usbInitProtocol5,
		NULL,                 /* resolution not queried */
		usbWcmGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		xf86WcmFilterIntuos,  /* input filtering recommended */
		usbDetectConfig,      /* detect hardware buttons etc */
	};

	static WacomModel usbIntuos3 =
	{
		"USB Intuos3",
		usbInitProtocol5,
		NULL,                 /* resolution not queried */
		usbWcmGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		xf86WcmFilterIntuos,  /* input filtering recommended */
		usbDetectConfig,      /* detect hardware buttons etc */
	};

	static WacomModel usbIntuos4 =
	{
		"USB Intuos4",
		usbInitProtocol5,
		NULL,                 /* resolution not queried */
		usbWcmGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		xf86WcmFilterIntuos,  /* input filtering recommended */
		usbDetectConfig,      /* detect hardware buttons etc */
	};

	static WacomModel usbVolito =
	{
		"USB Volito",
		usbInitProtocol4,
		NULL,                 /* resolution not queried */
		usbWcmGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		xf86WcmFilterCoord,   /* input filtering */
		usbDetectConfig,      /* detect hardware buttons etc */
	};

	static WacomModel usbVolito2 =
	{
		"USB Volito2",
		usbInitProtocol4,
		NULL,                 /* resolution not queried */
		usbWcmGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		xf86WcmFilterCoord,   /* input filtering */
		usbDetectConfig,      /* detect hardware buttons etc */
	};

	static WacomModel usbCintiqV5 =
	{
		"USB CintiqV5",
		usbInitProtocol5,
		NULL,                 /* resolution not queried */
		usbWcmGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		xf86WcmFilterIntuos,  /* input filtering recommended */
		usbDetectConfig,      /* detect hardware buttons etc */
	};

	static WacomModel usbTabletPC =
	{
		"USB TabletPC",
		usbInitProtocol4,
		NULL,                 /* resolution not queried */
		usbWcmGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		NULL,                 /* input filtering */
		usbDetectConfig,      /* detect hardware buttons etc */
	};

/*****************************************************************************
 * usbDetect --
 *   Test if the attached device is USB.
 ****************************************************************************/

static Bool usbDetect(LocalDevicePtr local)
{
	int version;
	int err;
#ifdef DEBUG
	WacomDevicePtr priv = (WacomDevicePtr)local->private;

	DBG(1, priv->debugLevel, ErrorF("usbDetect\n"));
#endif

	SYSCALL(err = ioctl(local->fd, EVIOCGVERSION, &version));

	if (err < 0)
	{
		xf86Msg(X_ERROR, "%s: usbDetect: can not ioctl version\n", local->name);
		return 0;
	}
#ifdef EVIOCGRAB
	/* Try to grab the event device so that data don't leak to /dev/input/mice */
	SYSCALL(err = ioctl(local->fd, EVIOCGRAB, (pointer)1));

	if (err < 0) 
		xf86Msg(X_ERROR, "%s: Wacom X driver can't grab event device, errno=%d\n",
				local->name, errno);
#endif
	return 1;
}

/*****************************************************************************
 * wcmusbInit --
 ****************************************************************************/

/* Key codes used to mark tablet buttons -- must be in sync
 * with the keycode array in wacom.c kernel driver.
 */
static unsigned short padkey_codes [] = {
	BTN_0, BTN_1, BTN_2, BTN_3, BTN_4,
	BTN_5, BTN_6, BTN_7, BTN_8, BTN_9,
	BTN_A, BTN_B, BTN_C, BTN_X, BTN_Y, BTN_Z,
	BTN_BASE, BTN_BASE2, BTN_BASE3,
	BTN_BASE4, BTN_BASE5, BTN_BASE6,
	BTN_TL, BTN_TR, BTN_TL2, BTN_TR2, BTN_SELECT
};

static struct
{
	unsigned char model_id;
	int yRes; /* tablet Y resolution in points/inch */
	int xRes; /* tablet X resolution in points/inch */
	WacomModelPtr model;
} WacomModelDesc [] =
{
	{ 0x00, 1000, 1000, &usbPenPartner }, /* PenPartner */
	{ 0x10, 2032, 2032, &usbGraphire   }, /* Graphire */
	{ 0x11, 2032, 2032, &usbGraphire2  }, /* Graphire2 4x5 */
	{ 0x12, 2032, 2032, &usbGraphire2  }, /* Graphire2 5x7 */
	{ 0x13, 2032, 2032, &usbGraphire3  }, /* Graphire3 4x5 */
	{ 0x14, 2032, 2032, &usbGraphire3  }, /* Graphire3 6x8 */
	{ 0x15, 2032, 2032, &usbGraphire4  }, /* Graphire4 4x5 */
	{ 0x16, 2032, 2032, &usbGraphire4  }, /* Graphire4 6x8 */ 
	{ 0x17, 2540, 2540, &usbBambooFun  }, /* BambooFun 4x5 */
	{ 0x18, 2540, 2540, &usbBambooFun  }, /* BambooFun 6x8 */
	{ 0x19, 2032, 2032, &usbBamboo1    }, /* Bamboo1 Medium*/ 
	{ 0x81, 2032, 2032, &usbGraphire4  }, /* Graphire4 6x8 BlueTooth */

	{ 0x20, 2540, 2540, &usbIntuos     }, /* Intuos 4x5 */
	{ 0x21, 2540, 2540, &usbIntuos     }, /* Intuos 6x8 */
	{ 0x22, 2540, 2540, &usbIntuos     }, /* Intuos 9x12 */
	{ 0x23, 2540, 2540, &usbIntuos     }, /* Intuos 12x12 */
	{ 0x24, 2540, 2540, &usbIntuos     }, /* Intuos 12x18 */

	{ 0x03,  508,  508, &usbCintiqPartner }, /* PTU600 */

	{ 0x30,  508,  508, &usbCintiq     }, /* PL400 */
	{ 0x31,  508,  508, &usbCintiq     }, /* PL500 */
	{ 0x32,  508,  508, &usbCintiq     }, /* PL600 */
	{ 0x33,  508,  508, &usbCintiq     }, /* PL600SX */
	{ 0x34,  508,  508, &usbCintiq     }, /* PL550 */
	{ 0x35,  508,  508, &usbCintiq     }, /* PL800 */
	{ 0x37,  508,  508, &usbCintiq     }, /* PL700 */
	{ 0x38,  508,  508, &usbCintiq     }, /* PL510 */
	{ 0x39,  508,  508, &usbCintiq     }, /* PL710 */ 
	{ 0xC0,  508,  508, &usbCintiq     }, /* DTF720 */
	{ 0xC2,  508,  508, &usbCintiq     }, /* DTF720a */
	{ 0xC4,  508,  508, &usbCintiq     }, /* DTF521 */ 
	{ 0xC7, 2540, 2540, &usbCintiq     }, /* DTU1931 */

	{ 0x41, 2540, 2540, &usbIntuos2    }, /* Intuos2 4x5 */
	{ 0x42, 2540, 2540, &usbIntuos2    }, /* Intuos2 6x8 */
	{ 0x43, 2540, 2540, &usbIntuos2    }, /* Intuos2 9x12 */
	{ 0x44, 2540, 2540, &usbIntuos2    }, /* Intuos2 12x12 */
	{ 0x45, 2540, 2540, &usbIntuos2    }, /* Intuos2 12x18 */
	{ 0x47, 2540, 2540, &usbIntuos2    }, /* Intuos2 6x8  */

	{ 0x60, 1016, 1016, &usbVolito     }, /* Volito */ 

	{ 0x61, 1016, 1016, &usbVolito2    }, /* PenStation */
	{ 0x62, 1016, 1016, &usbVolito2    }, /* Volito2 4x5 */
	{ 0x63, 1016, 1016, &usbVolito2    }, /* Volito2 2x3 */
	{ 0x64, 1016, 1016, &usbVolito2    }, /* PenPartner2 */

	{ 0x65, 2540, 2540, &usbBamboo     }, /* Bamboo */
	{ 0x69, 1012, 1012, &usbBamboo1    }, /* Bamboo1 */ 

	{ 0xB0, 5080, 5080, &usbIntuos3    }, /* Intuos3 4x5 */
	{ 0xB1, 5080, 5080, &usbIntuos3    }, /* Intuos3 6x8 */
	{ 0xB2, 5080, 5080, &usbIntuos3    }, /* Intuos3 9x12 */
	{ 0xB3, 5080, 5080, &usbIntuos3    }, /* Intuos3 12x12 */
	{ 0xB4, 5080, 5080, &usbIntuos3    }, /* Intuos3 12x19 */
	{ 0xB5, 5080, 5080, &usbIntuos3    }, /* Intuos3 6x11 */
	{ 0xB7, 5080, 5080, &usbIntuos3    }, /* Intuos3 4x6 */

	{ 0xB8, 5080, 5080, &usbIntuos4    }, /* Intuos4 4x6 */
	{ 0xB9, 5080, 5080, &usbIntuos4    }, /* Intuos4 6x9 */
	{ 0xBA, 5080, 5080, &usbIntuos4    }, /* Intuos4 8x13 */
	{ 0xBB, 5080, 5080, &usbIntuos4    }, /* Intuos4 12x19*/

	{ 0x3F, 5080, 5080, &usbCintiqV5   }, /* Cintiq 21UX */ 
	{ 0xC5, 5080, 5080, &usbCintiqV5   }, /* Cintiq 20WSX */ 
	{ 0xC6, 5080, 5080, &usbCintiqV5   }, /* Cintiq 12WX */ 

	{ 0x90, 2540, 2540, &usbTabletPC   }, /* TabletPC 0x90 */ 
	{ 0x93, 2540, 2540, &usbTabletPC   }, /* TabletPC 0x93 */
	{ 0x9A, 2540, 2540, &usbTabletPC   }  /* TabletPC 0x9A */
};

Bool usbWcmInit(LocalDevicePtr local, char* id, float *version)
{
	int i;
	struct input_id sID;
	unsigned long keys[NBITS(KEY_MAX)];
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	DBG(1, priv->debugLevel, ErrorF("initializing USB tablet\n"));
	*version = 0.0;

	/* fetch vendor, product, and model name */
	ioctl(local->fd, EVIOCGID, &sID);
	ioctl(local->fd, EVIOCGNAME(sizeof(id)), id);

	/* vendor is wacom */
	if (sID.vendor == WACOM_VENDOR_ID)
	{
		common->tablet_id = sID.product;

		for (i = 0; i < sizeof (WacomModelDesc) / sizeof (WacomModelDesc [0]); i++)
			if (common->tablet_id == WacomModelDesc [i].model_id)
			{
				common->wcmModel = WacomModelDesc [i].model;
				common->wcmResolX = WacomModelDesc [i].xRes;
				common->wcmResolY = WacomModelDesc [i].yRes;
			}

		if (strstr(common->wcmModel->name, "TabletPC"))
		{
			if (common->tablet_id != 0x90)
			{
				/* TouchDefault was off for all devices */
				/* except when touch is supported */
				common->wcmTouchDefault = 1;

				/* check if touch was turned off in xorg.conf */
				common->wcmTouch = xf86SetBoolOption(local->options, 
						"Touch", common->wcmTouchDefault);
				if ( common->wcmTouch )
					xf86Msg(X_CONFIG, "%s: Touch is enabled \n", local->name);
			}

			/* Tablet PC button applied to the whole tablet. Not just one tool */
			common->wcmTPCButtonDefault = 1; /* Tablet PC buttons on by default */
			if ( priv->flags & STYLUS_ID )
			{
				common->wcmTPCButton = xf86SetBoolOption(local->options, 
					"TPCButton", common->wcmTPCButtonDefault);
				if ( common->wcmTPCButton )
					xf86Msg(X_CONFIG, "%s: Tablet PC buttons are on \n", 
						local->name);
			}
		}
	}

	if (!common->wcmModel)
	{
		common->wcmModel = &usbUnknown;
		common->wcmResolX = common->wcmResolY = 1016;
	}

	/* Determine max number of buttons */
	if (ioctl(local->fd, EVIOCGBIT(EV_KEY,sizeof(keys)),keys) < 0)
	{
		xf86Msg(X_ERROR, "%s: unable to ioctl key bits.\n", local->name);
		return FALSE;
	}

	/* Find out supported button codes - except mouse button codes
	 * BTN_LEFT and BTN_RIGHT, which are always fixed. */
	common->npadkeys = 0;
	for (i = 0; i < sizeof (padkey_codes) / sizeof (padkey_codes [0]); i++)
		if (ISBITSET (keys, padkey_codes [i]))
			common->padkey_code [common->npadkeys++] = padkey_codes [i];

	if (ISBITSET (keys, BTN_TASK))
		common->nbuttons = 10;
	else if (ISBITSET (keys, BTN_BACK))
		common->nbuttons = 9;
	else if (ISBITSET (keys, BTN_FORWARD))
		common->nbuttons = 8;
	else if (ISBITSET (keys, BTN_EXTRA))
		common->nbuttons = 7;
	else if (ISBITSET (keys, BTN_SIDE))
		common->nbuttons = 6;
	else
		common->nbuttons = 5;

	return Success;
}

static void usbInitProtocol5(WacomCommonPtr common, const char* id,
	float version)
{
	common->wcmProtocolLevel = 5;
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
	common->wcmProtocolLevel = 4;
	common->wcmPktLength = sizeof(struct input_event);
	common->wcmCursorProxoutDistDefault 
			= PROXOUT_GRAPHIRE_DISTANCE;

	/* tilt disabled */
	common->wcmFlags &= ~TILT_ENABLED_FLAG;
}

int usbWcmGetRanges(LocalDevicePtr local)
{
	struct input_absinfo absinfo;
	unsigned long ev[NBITS(EV_MAX)];
	unsigned long abs[NBITS(ABS_MAX)];
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common =	priv->common;

	if (ioctl(local->fd, EVIOCGBIT(0 /*EV*/, sizeof(ev)), ev) < 0)
	{
		xf86Msg(X_ERROR, "%s: unable to ioctl event bits.\n", local->name);
		return !Success;
	}

        common->wcmFlags |= USE_SYN_REPORTS_FLAG;

        if (ioctl(local->fd, EVIOCGBIT(EV_ABS,sizeof(abs)),abs) < 0)
	{
		xf86Msg(X_ERROR, "%s: unable to ioctl abs bits.\n", local->name);
		return !Success;
	}

	/* absolute values */
	if (!ISBITSET(ev,EV_ABS))
	{
		xf86Msg(X_ERROR, "%s: unable to ioctl max values.\n", local->name);
		return !Success;
	}

	/* max x */
	if (ioctl(local->fd, EVIOCGABS(ABS_X), &absinfo) < 0)
	{
		xf86Msg(X_ERROR, "%s: unable to ioctl xmax value.\n", local->name);
		return !Success;
	}

	if (absinfo.maximum <= 0)
	{
		xf86Msg(X_ERROR, "%s: xmax value is wrong.\n", local->name);
		return !Success;
	}
	if (!IsTouch(priv))
		common->wcmMaxX = absinfo.maximum;
	else
		common->wcmMaxTouchX = absinfo.maximum;

	/* max y */
	if (ioctl(local->fd, EVIOCGABS(ABS_Y), &absinfo) < 0)
	{
		xf86Msg(X_ERROR, "%s: unable to ioctl ymax value.\n", local->name);
		return !Success;
	}

	if (absinfo.maximum <= 0)
	{
		xf86Msg(X_ERROR, "%s: ymax value is wrong.\n", local->name);
		return !Success;
	}
	if (!IsTouch(priv))
		common->wcmMaxY = absinfo.maximum;
	else
		common->wcmMaxTouchY = absinfo.maximum;

	/* max finger strip X for tablets with Expresskeys
	 * or max touch logical X for TabletPCs with touch */
	if (ioctl(local->fd, EVIOCGABS(ABS_RX), &absinfo) == 0)
	{
		if (IsTouch(priv))
			common->wcmTouchResolX = absinfo.maximum;
		else
			common->wcmMaxStripX = absinfo.maximum;
	}

	/* max finger strip Y for tablets with Expresskeys
	 * or max touch logical Y for TabletPCs with touch */
	if (ioctl(local->fd, EVIOCGABS(ABS_RY), &absinfo) == 0)
	{
		if (IsTouch(priv))
			common->wcmTouchResolY = absinfo.maximum;
		else
			common->wcmMaxStripY = absinfo.maximum;
	}

	if (IsTouch(priv) && common->wcmTouchResolX && common->wcmMaxTouchX)
	{
		common->wcmTouchResolX = (int)(((double)common->wcmTouchResolX)
			 / ((double)common->wcmMaxTouchX) + 0.5);
		common->wcmTouchResolY = (int)(((double)common->wcmTouchResolY)
			 / ((double)common->wcmMaxTouchY) + 0.5);

		if (!common->wcmTouchResolX || !common->wcmTouchResolY)
		{
			xf86Msg(X_ERROR, "%s: touch resolution value(s) was wrong TouchResolX"
				" = %d MaxTouchY = %d.\n", local->name, common->wcmTouchResolX,
				common->wcmTouchResolY);
			return !Success;
		}

	}

	/* max z cannot be configured */
	if (ioctl(local->fd, EVIOCGABS(ABS_PRESSURE), &absinfo) == 0)
		common->wcmMaxZ = absinfo.maximum;

	/* max distance */
	if (ioctl(local->fd, EVIOCGABS(ABS_DISTANCE), &absinfo) == 0)
		common->wcmMaxDist = absinfo.maximum;

	return Success;
}

static int usbDetectConfig(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	DBG(10, common->debugLevel, ErrorF("usbDetectConfig \n"));
	if (IsPad (priv))
	{
		priv->nbuttons = common->npadkeys;

/* This code will be used when we are ready to report valuators in tablet and tool 
 * specific form, which will need to clean InitValuatorAxisStruct() in xf86Wacom.c
 * and all the calls to X that are related to valuators, such as xf86PostButtonEvent and 
 * xf86PostButtonEvent, etc. Code under util directory will need to be updated as well.
 * This will take some time. We put it in the to-do list for now.  Ping 
		unsigned long abs[NBITS(ABS_MAX)];
		priv->naxes = 0;
		if (ioctl(local->fd, EVIOCGBIT(EV_ABS, sizeof(abs)), abs) >= 0)
		{
			if (ISBITSET (abs, ABS_RX))
				priv->naxes++;
			if (ISBITSET (abs, ABS_RY))
				priv->naxes++;
			if (!priv->naxes)
				priv->flags |= BUTTONS_ONLY_FLAG;
		}
*/	}
	else
		priv->nbuttons = common->nbuttons;

	if (!common->wcmCursorProxoutDist)
		common->wcmCursorProxoutDist
			= common->wcmCursorProxoutDistDefault;
	return TRUE;
}

static int usbParse(LocalDevicePtr local, const unsigned char* data)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	usbParseEvent(local, (const struct input_event*)data);
	return common->wcmPktLength;
}

static void usbParseEvent(LocalDevicePtr local,
	const struct input_event* event)
{
	int i, channel;
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	DBG(10, common->debugLevel, ErrorF("usbParseEvent \n"));
	/* store events until we receive the MSC_SERIAL containing
	 * the serial number; without it we cannot determine the
	 * correct channel. */

	/* space left? bail if not. */
	if (common->wcmEventCnt >=
		(sizeof(common->wcmEvents)/sizeof(*common->wcmEvents)))
	{
		xf86Msg(X_ERROR, "%s: usbParse: Exceeded event queue (%d) \n",
			local->name, common->wcmEventCnt);
		goto skipEvent;
	}

	/* save it for later */
	common->wcmEvents[common->wcmEventCnt++] = *event;

	if ((event->type == EV_MSC) && (event->code == MSC_SERIAL))
	{
		/* we don't report serial numbers for some tools
		 * but we never report a serial number with a value of 0 */
		if (event->value == 0)
		{
			xf86Msg(X_ERROR, "%s: usbParse: Ignoring event from invalid serial 0\n",
				local->name);
			goto skipEvent;
		}

		/* save the serial number so we can look up the channel number later */
		common->wcmLastToolSerial = event->value;

		/* if SYN_REPORT is end of record indicator, we are done */
		if (USE_SYN_REPORTS(common))
			return;

		/* fall through to deliver the X event */
	} else if ((event->type == EV_SYN) && (event->code == SYN_REPORT))
	{
		/* if we got a SYN_REPORT but weren't expecting one, change over to
		   using SYN_REPORT as the end of record indicator */
		if (! USE_SYN_REPORTS(common))
		{
			xf86Msg(X_ERROR, "%s: Got unexpected SYN_REPORT, changing mode\n",
				local->name);

			/* we can expect SYN_REPORT's from now on */
			common->wcmFlags |= USE_SYN_REPORTS_FLAG;
		}

		/* end of record. fall through to deliver the X event */
	}
	else
	{
		/* not an SYN_REPORT and not an SYN_REPORT, bail out */
		return;
	}

	/* figure out the channel to use based on serial number */
	channel = -1;
	if (common->wcmProtocolLevel == 4)
	{
		/* Protocol 4 doesn't support tool serial numbers */
		if (common->wcmLastToolSerial == 0xf0)
			channel = 1;
		else
			channel = 0;
	}
	else if (common->wcmLastToolSerial) /* serial number should never be 0 */
	{
		/* ignore events without information */
		if (common->wcmEventCnt <= 2) 
		{
			DBG(3, common->debugLevel, ErrorF("%s - usbParse: dropping empty event "
				"for serial %d\n", local->name, common->wcmLastToolSerial));
			goto skipEvent;
		}

		/* dual input is supported */
		if ( strstr(common->wcmModel->name, "Intuos1") || strstr(common->wcmModel->
name, "Intuos2") )
		{
			/* find existing channel */
			for (i=0; i<MAX_CHANNELS; ++i)
			{
				if (common->wcmChannel[i].work.proximity && 
			  		common->wcmChannel[i].work.serial_num == common->wcmLastToolSerial)
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
			if (common->wcmLastToolSerial == -1)  /* pad */
				channel = 1;
			else if ( (common->wcmChannel[0].work.proximity &&  /* existing transducer */
				    (common->wcmChannel[0].work.serial_num == common->wcmLastToolSerial)) ||
					!common->wcmChannel[0].work.proximity ) /* new transducer */
				channel = 0;
			
		}
	}
	else
		goto skipEvent;

	/* fresh out of channels */
	if (channel < 0)
	{
		/* This should never happen in normal use.
		 * Let's start over again. Force prox-out for all channels.
		 */
		for (i=0; i<MAX_CHANNELS; ++i)
		{
			if (common->wcmChannel[i].work.proximity && (common->wcmChannel[i].work.serial_num != -1))
			{
				common->wcmChannel[i].work.proximity = 0;
				/* dispatch event */
				xf86WcmEvent(common, i, &common->wcmChannel[i].work);
			}
		}
		DBG(1, common->debugLevel, ErrorF("usbParse (%s with serial number: %u) at %d: "
			"Exceeded channel count; ignoring the events.\n", local->name, 
			common->wcmLastToolSerial, (int)GetTimeInMillis()));
		goto skipEvent;
	}

	if (!common->wcmChannel[channel].work.proximity)
	{
		memset(&common->wcmChannel[channel],0,sizeof(WacomChannel));
		/* in case the in-prox event was missing */
		common->wcmChannel[channel].work.proximity = 1;
	}

	/* dispatch event */
	usbParseChannel(local,channel,common->wcmLastToolSerial);

skipEvent:
	common->wcmLastToolSerial = 0;
	common->wcmEventCnt = 0;
}

static void usbParseChannel(LocalDevicePtr local, int channel, int serial)
{
	int i, shift, nkeys;
	WacomDeviceState* ds;
	struct input_event* event;
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	DBG(6, common->debugLevel, ErrorF("usbParseChannel %d events received\n", common->wcmEventCnt));
	#define MOD_BUTTONS(bit, value) do { \
		shift = 1<<bit; \
		ds->buttons = (((value) != 0) ? \
		(ds->buttons | (shift)) : (ds->buttons & ~(shift))); \
		} while (0)

	if (common->wcmEventCnt == 1 && !common->wcmEvents->type) {
		DBG(6, common->debugLevel, ErrorF("usbParseChannel no real events received\n"));
		return;
	}
	DBG(6, common->debugLevel, ErrorF("usbParseChannel %d events received\n", common->wcmEventCnt));

	/* all USB data operates from previous context except relative values*/
	ds = &common->wcmChannel[channel].work;
	ds->relwheel = 0;
	ds->serial_num = serial;

	/* loop through all events in group */
	for (i=0; i<common->wcmEventCnt; ++i)
	{
		event = common->wcmEvents + i;
		DBG(11, common->debugLevel, ErrorF("usbParseChannel "
			"event[%d]->type=%d code=%d value=%d\n",
			i, event->type, event->code, event->value));

		/* absolute events */
		if (event->type == EV_ABS)
		{
			if (event->code == ABS_X)
				ds->x = event->value;
			else if (event->code == ABS_Y)
				ds->y = event->value;
			else if (event->code == ABS_RX)
				ds->stripx = event->value; 
			else if (event->code == ABS_RY)
				ds->stripy = event->value;
			else if (event->code == ABS_RZ) 
				ds->rotation = event->value;
			else if (event->code == ABS_TILT_X)
				ds->tiltx = event->value - common->wcmMaxtiltX/2;
			else if (event->code ==  ABS_TILT_Y)
				ds->tilty = event->value - common->wcmMaxtiltY/2;
			else if (event->code == ABS_PRESSURE) {
				if (ds->device_type == TOUCH_ID)
					ds->capacity = event->value;
				else
					ds->pressure = event->value;
			} else if (event->code == ABS_DISTANCE)
				ds->distance = event->value;
			else if (event->code == ABS_WHEEL || 
				    event->code == ABS_Z)
				ds->abswheel = event->value;
			else if (event->code == ABS_THROTTLE)
				ds->throttle = event->value;
			else if (event->code == ABS_MISC && event->value)
				ds->device_id = event->value;
		}
		else if (event->type == EV_REL)
		{
			if (event->code == REL_WHEEL)
				ds->relwheel = -event->value;
			else
				xf86Msg(X_ERROR, "%s: rel event recv'd (%d)!\n",
					local->name, event->code);
		}

		else if (event->type == EV_KEY)
		{
			if ((event->code == BTN_TOOL_PEN) ||
				(event->code == BTN_TOOL_PENCIL) ||
				(event->code == BTN_TOOL_BRUSH) ||
				(event->code == BTN_TOOL_AIRBRUSH))
			{
				ds->device_type = STYLUS_ID;
				/* V5 tools use ABS_MISC to report device_id */
				if (common->wcmProtocolLevel == 4)
					ds->device_id = STYLUS_DEVICE_ID;
				ds->proximity = (event->value != 0);
				DBG(6, common->debugLevel, ErrorF(
					"USB stylus detected %x\n",
					event->code));
			}
			else if (event->code == BTN_TOOL_RUBBER)
			{
				ds->device_type = ERASER_ID;
				/* V5 tools use ABS_MISC to report device_id */
				if (common->wcmProtocolLevel == 4)
					ds->device_id = ERASER_DEVICE_ID;
				ds->proximity = (event->value != 0);
				if (ds->proximity)
					ds->proximity = ERASER_PROX;
				DBG(6, common->debugLevel, ErrorF(
					"USB eraser detected %x (value=%d)\n",
					event->code, event->value));
			}
			else if ((event->code == BTN_TOOL_MOUSE) ||
				(event->code == BTN_TOOL_LENS))
			{
				DBG(6, common->debugLevel, ErrorF(
					"USB mouse detected %x (value=%d)\n",
					event->code, event->value));
				ds->device_type = CURSOR_ID;
				/* V5 tools use ABS_MISC to report device_id */
				if (common->wcmProtocolLevel == 4)
					ds->device_id = CURSOR_DEVICE_ID;
				ds->proximity = (event->value != 0);
			}
			else if (event->code == BTN_TOOL_FINGER)
			{
				DBG(6, common->debugLevel, ErrorF(
					"USB Pad detected %x (value=%d)\n",
					event->code, event->value));
				ds->device_type = PAD_ID;
				ds->device_id = PAD_DEVICE_ID;
				ds->proximity = (event->value != 0);
			}
			else if (event->code == BTN_TOOL_DOUBLETAP)
			{
				WacomChannelPtr pChannel = common->wcmChannel + channel;
				WacomDeviceState dslast = pChannel->valid.state;
				DBG(6, common->debugLevel, ErrorF(
					"USB Touch detected %x (value=%d)\n",
					event->code, event->value));
				ds->device_type = TOUCH_ID;
				ds->device_id = TOUCH_DEVICE_ID;
				ds->proximity = event->value;
				/* time stamp for 2FGT gesture events */
				if ((ds->proximity && !dslast.proximity) ||
					    (!ds->proximity && dslast.proximity))
					ds->sample = (int)GetTimeInMillis();
				/* left button is always pressed for touch without capacity 
				 * For touch with capacity, left button event will be decided
				 * in wcmCommon.c by capacity threshold
				 */
				if (common->wcmCapacityDefault < 0)
					MOD_BUTTONS (0, event->value);
			}
			else if (event->code == BTN_TOOL_TRIPLETAP)
			{
				WacomChannelPtr pChannel = common->wcmChannel + channel;
				WacomDeviceState dslast = pChannel->valid.state;
				DBG(6, common->debugLevel, ErrorF(
					"USB Touch second finger detected %x (value=%d)\n",
					event->code, event->value));
				ds->device_type = TOUCH_ID;
				ds->device_id = TOUCH_DEVICE_ID;
				ds->proximity = event->value;
				/* time stamp for 2GT gesture events */
				if ((ds->proximity && !dslast.proximity) ||
					    (!ds->proximity && dslast.proximity))
					ds->sample = (int)GetTimeInMillis();
				/* Second finger events will be considered in
				 * combination with the first finger data */
			}
			else if ((event->code == BTN_STYLUS) ||
				(event->code == BTN_MIDDLE))
			{
				MOD_BUTTONS (1, event->value);
			}
			else if ((event->code == BTN_STYLUS2) ||
				(event->code == BTN_RIGHT))
			{
				MOD_BUTTONS (2, event->value);
			}
			else if (event->code == BTN_LEFT)
				MOD_BUTTONS (0, event->value);
			else if (event->code == BTN_SIDE)
				MOD_BUTTONS (3, event->value);
			else if (event->code == BTN_EXTRA)
				MOD_BUTTONS (4, event->value);
			else
			{
				for (nkeys = 0; nkeys < common->npadkeys; nkeys++)
					if (event->code == common->padkey_code [nkeys])
					{
						MOD_BUTTONS (nkeys, event->value);
						break;
					}
			}
		}
	} /* next event */

	/* don't send touch event when touch isn't enabled */
	if ((ds->device_type == TOUCH_ID) && !common->wcmTouch)
		return;

	/* DTF720 and DTF720a don't support eraser */
	if (((common->tablet_id == 0xC0) || (common->tablet_id == 0xC2)) && 
		(ds->device_type == ERASER_ID)) 
	{
		DBG(10, common->debugLevel, ErrorF("usbParseChannel "
			"DTF 720 doesn't support eraser "));
		return;
	}

	/* dispatch event */
	xf86WcmEvent(common, channel, ds);
}
/* vim: set noexpandtab shiftwidth=8: */
