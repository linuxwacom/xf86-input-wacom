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

#include "xf86Wacom.h"

#if LINUX_INPUT

static Bool usbDetect(LocalDevicePtr);
static Bool usbInit(LocalDevicePtr pDev);

static void usbInitProtocol5(WacomCommonPtr common, int fd, const char* id,
	float version);
static void usbInitProtocol4(WacomCommonPtr common, int fd, const char* id,
	float version);
static int usbGetRanges(WacomCommonPtr common, int fd);
static int usbParse(WacomCommonPtr common, const unsigned char* data);
static void usbParseEvent(WacomCommonPtr common,
	const struct input_event* event);
static void usbParseChannel(WacomCommonPtr common, int channel, int serial);

	WacomDeviceClass gWacomUSBDevice =
	{
		usbDetect,
		usbInit,
		xf86WcmReadPacket,
	};

	static WacomModel usbUnknown =
	{
		"Unknown USB",
		usbInitProtocol5,     /* assume the best */
		NULL,                 /* resolution not queried */
		usbGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		NULL,                 /* input filtering not needed */
	};

	static WacomModel usbPenPartner =
	{
		"USB PenPartner",
		usbInitProtocol4,
		NULL,                 /* resolution not queried */
		usbGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		NULL,                 /* input filtering not needed */
	};

	static WacomModel usbGraphire =
	{
		"USB Graphire",
		usbInitProtocol4,
		NULL,                 /* resolution not queried */
		usbGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		NULL,                 /* input filtering not needed */
	};

	static WacomModel usbGraphire2 =
	{
		"USB Graphire2",
		usbInitProtocol4,
		NULL,                 /* resolution not queried */
		usbGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		NULL,                 /* input filtering not needed */
	};

	static WacomModel usbGraphire3 =
	{
		"USB Graphire3",
		usbInitProtocol4,
		NULL,                 /* resolution not queried */
		usbGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		NULL,                 /* input filtering not needed */
	};

	static WacomModel usbCintiq =
	{
		"USB Cintiq",
		usbInitProtocol4,
		NULL,                 /* resolution not queried */
		usbGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		NULL,                 /* input filtering not needed */
	};

	static WacomModel usbCintiqPartner =
	{
		"USB CintiqPartner",
		usbInitProtocol4,
		NULL,                 /* resolution not queried */
		usbGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		NULL,                 /* input filtering not needed */
	};

	static WacomModel usbIntuos =
	{
		"USB Intuos",
		usbInitProtocol5,
		NULL,                 /* resolution not queried */
		usbGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		NULL,                 /* input filtering not needed */
	};

	static WacomModel usbIntuos2 =
	{
		"USB Intuos2",
		usbInitProtocol5,
		NULL,                 /* resolution not queried */
		usbGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		NULL,                 /* input filtering not needed */
	};

	static WacomModel usbVolito =
	{
		"USB Volito",
		usbInitProtocol4,
		NULL,                 /* resolution not queried */
		usbGetRanges,
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		usbParse,
		NULL,                 /* input filtering not needed */
	};


/*****************************************************************************
 * usbDetect --
 *   Test if the attached device is USB.
 ****************************************************************************/

static Bool usbDetect(LocalDevicePtr local)
{
	int version;
	int err;

	DBG(1, ErrorF("usbDetect\n"));
    
	SYSCALL(err = ioctl(local->fd, EVIOCGVERSION, &version));
    
	if (!err)
	{
		ErrorF("%s Wacom Kernel Input driver version is %d.%d.%d\n",
				XCONFIG_PROBED, version >> 16,
				(version >> 8) & 0xff, version & 0xff);
		return 1;
	}

	return 0;
}

/*****************************************************************************
 * usbInit --
 ****************************************************************************/

static Bool usbInit(LocalDevicePtr local)
{
	short sID[4];
	char id[BUFFER_SIZE];
	WacomModelPtr model = NULL;
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	DBG(1, ErrorF("initializing USB tablet\n"));    

	/* fetch vendor, product, and model name */
	ioctl(local->fd, EVIOCGID, sID);
	ioctl(local->fd, EVIOCGNAME(sizeof(id)), id);

	/* vendor is wacom */
	if (sID[1] == 0x056A)
	{
		/* switch on product */
		switch (sID[2])
		{
			case 0x00: /* PenPartner */
				model = &usbPenPartner; break;

			case 0x10: /* Graphire */
				model = &usbGraphire; break;

			case 0x11: /* Graphire2 4x5 */
			case 0x12: /* Graphire2 5x7 */
				model = &usbGraphire2; break;

			case 0x13: /* Graphire2 4x5 */
			case 0x14: /* Graphire2 6x8 */
				model = &usbGraphire3; break;

			case 0x20: /* Intuos 4x5 */
			case 0x21: /* Intuos 6x8 */
			case 0x22: /* Intuos 9x12 */
			case 0x23: /* Intuos 12x12 */
			case 0x24: /* Intuos 12x18 */
				model = &usbIntuos; break;

			case 0x03: /* PTU600 */
				model = &usbCintiqPartner; break;

			case 0x30: /* PL400 */
			case 0x31: /* PL500 */
			case 0x32: /* PL600 */
			case 0x33: /* PL600SX */
			case 0x34: /* PL550 */
			case 0x35: /* PL800 */
				model = &usbCintiq; break;

			case 0x41: /* Intuos2 4x5 */
			case 0x42: /* Intuos2 6x8 */
			case 0x43: /* Intuos2 9x12 */
			case 0x44: /* Intuos2 12x12 */
			case 0x45: /* Intuos2 12x18 */
				model = &usbIntuos2; break;

			case 0x60: /* Volito */
				model = &usbVolito; break;
		}
	}

	if (!model)
		model = &usbUnknown;

	return xf86WcmInitTablet(common,model,local->fd,id,0.0);
}

static void usbInitProtocol5(WacomCommonPtr common, int fd, const char* id,
	float version)
{
	DBG(2, ErrorF("detected a protocol 5 model (%s)\n",id));
	common->wcmResolX = common->wcmResolY = 2540;
	common->wcmProtocolLevel = 5;
	common->wcmChannelCnt = 2;
	common->wcmPktLength = sizeof(struct input_event);
}

static void usbInitProtocol4(WacomCommonPtr common, int fd, const char* id,
	float version)
{
	DBG(2, ErrorF("detected a protocol 4 model (%s)\n",id));
	common->wcmResolX = common->wcmResolY = 1016;
	common->wcmProtocolLevel = 4;
	common->wcmPktLength = sizeof(struct input_event);
}

#define BIT(x) (1<<(x))
#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define ISBITSET(x,y) ((x)[LONG(y)] & BIT(y))
#define OFF(x)   ((x)%BITS_PER_LONG)
#define LONG(x)  ((x)/BITS_PER_LONG)

static int usbGetRanges(WacomCommonPtr common, int fd)
{
	int nValues[5];
	unsigned long ev[NBITS(EV_MAX)];
	unsigned long abs[NBITS(ABS_MAX)];

	if (ioctl(fd, EVIOCGBIT(0 /*EV*/, sizeof(ev)), ev) < 0)
	{
		ErrorF("WACOM: unable to ioctl event bits.\n");
		return !Success;
	}

	/* absolute values */
	if (ISBITSET(ev,EV_ABS))
	{
		if (ioctl(fd, EVIOCGBIT(EV_ABS,sizeof(abs)),abs) < 0)
		{
			ErrorF("WACOM: unable to ioctl abs bits.\n");
			return !Success;
		}

		/* max x */
		if (common->wcmMaxX == 0)
		{
			if (ioctl(fd, EVIOCGABS(ABS_X), nValues) < 0)
			{
				ErrorF("WACOM: unable to ioctl xmax value.\n");
				return !Success;
			}
			common->wcmMaxX = nValues[2];
		}

		/* max y */
		if (common->wcmMaxY == 0)
		{
			if (ioctl(fd, EVIOCGABS(ABS_Y), nValues) < 0)
			{
				ErrorF("WACOM: unable to ioctl ymax value.\n");
				return !Success;
			}
			common->wcmMaxY = nValues[2];
		}

		/* max z cannot be configured */
		if (ioctl(fd, EVIOCGABS(ABS_PRESSURE), nValues) < 0)
		{
			ErrorF("WACOM: unable to ioctl press max value.\n");
			return !Success;
		}
		common->wcmMaxZ = nValues[2];
	}

	return Success;
}

static int usbParse(WacomCommonPtr common, const unsigned char* data)
{
	usbParseEvent(common,(const struct input_event*)data);
	return common->wcmPktLength;
}

static void usbParseEvent(WacomCommonPtr common,
	const struct input_event* event)
{
	int i, serial, channel;

	/* store events until we receive the MSC_SERIAL containing
	 * the serial number; without it we cannot determine the
	 * correct channel. */

	/* space left? bail if not. */
	if (common->wcmEventCnt >=
		(sizeof(common->wcmEvents)/sizeof(*common->wcmEvents)))
	{
		common->wcmEventCnt = 0;
		DBG(1, ErrorF("usbParse: Exceeded event queue (%d)\n",
				common->wcmEventCnt));
		return;
	}

	/* save it for later */
	common->wcmEvents[common->wcmEventCnt++] = *event;

	/* is it the all-important MSC_SERIAL? maybe next time. */
	if ((event->type != EV_MSC) || (event->code != MSC_SERIAL))
		return;

	/* serial number is key for channel */
	serial = event->value;
	channel = -1;

	/* one channel only? must be it. */
	if (common->wcmChannelCnt == 1)
		channel = 0;

	/* otherwise, find the channel */
	else
	{
		/* find existing channel */
		for (i=0; i<common->wcmChannelCnt; ++i)
		{
			if (common->wcmChannel[i].work.serial_num == serial)
			{
				channel = i;
				break;
			}
		}

		/* find an empty channel */
		if (channel < 0)
		{
			for (i=0; i<common->wcmChannelCnt; ++i)
			{
				if (common->wcmChannel[i].work.proximity == 0)
				{
					/* clear out channel */
					memset(&common->wcmChannel[i],0,
						sizeof(WacomChannel));
					channel = i;
					break;
				}
			}
		}

		/* fresh out of channels */
		if (channel < 0)
		{
			/* this should never happen in normal use */
			DBG(1, ErrorF("usbParse: Exceeded channel count; "
				"ignoring.\n"));
			return;
		}
	}

	usbParseChannel(common,channel,serial);

	common->wcmEventCnt = 0;
}

static void usbParseChannel(WacomCommonPtr common, int channel, int serial)
{
	int i;
	WacomDeviceState* ds;
	struct input_event* event;

	#define MOD_BUTTONS(bit, value) do { \
		ds->buttons = (((value) != 0) ? \
		(ds->buttons | (bit)) : (ds->buttons & ~(bit))); \
		} while (0)

	/* all USB data operates from previous context except relative values*/
	ds = &common->wcmChannel[channel].work;
	ds->relwheel = 0;
	ds->serial_num = serial;

	/* loop through all events in group */
	for (i=0; i<common->wcmEventCnt; ++i)
	{
		event = common->wcmEvents + i;

		DBG(11, ErrorF("usbParseChannel event[%d]->type=%d "
			"code=%d value=%d\n", i, event->type,
			event->code, event->value));

		/* absolute events */
		if (event->type == EV_ABS)
		{
			if (event->code == ABS_X)
				ds->x = event->value;
			else if (event->code == ABS_Y)
				ds->y = event->value;
			else if (event->code == ABS_RZ)
				ds->rotation = event->value;
			else if (event->code == ABS_TILT_X)
				ds->tiltx = event->value - 64;
			else if (event->code ==  ABS_TILT_Y)
				ds->tilty = event->value - 64;
			else if (event->code == ABS_PRESSURE)
			{
				ds->pressure = event->value;
				MOD_BUTTONS (1, event->value >
					common->wcmThreshold ? 1 : 0);
				/* pressure button should be downstream */
			}
			else if (event->code == ABS_DISTANCE)
			{
			}
			else if (event->code == ABS_WHEEL)
				ds->abswheel = event->value;
			else if (event->code == ABS_THROTTLE)
				ds->throttle = event->value;
		}

		else if (event->type == EV_REL)
		{
			if (event->code == REL_WHEEL)
				ds->relwheel = event->value;
			else
			{
				ErrorF("wacom: rel event recv'd (%d)!\n",
					event->code);
			}
		}

		else if (event->type == EV_KEY)
		{
			if ((event->code == BTN_TOOL_PEN) ||
				(event->code == BTN_TOOL_PENCIL) ||
				(event->code == BTN_TOOL_BRUSH) ||
				(event->code == BTN_TOOL_AIRBRUSH))
			{
				ds->device_type = STYLUS_ID;
				ds->proximity = (event->value != 0);
				DBG(6, ErrorF("USB stylus detected %x\n",
					event->code));
			}
			else if (event->code == BTN_TOOL_RUBBER)
			{
				ds->device_type = ERASER_ID;
				ds->proximity = (event->value != 0);
				if (ds->proximity) 
					ds->proximity = ERASER_PROX;
				DBG(6, ErrorF("USB eraser detected %x\n",
					event->code));
			}
			else if ((event->code == BTN_TOOL_MOUSE) ||
				(event->code == BTN_TOOL_LENS))
			{
				DBG(6, ErrorF("USB mouse detected %x\n",
					event->code));
				ds->device_type = CURSOR_ID;
				ds->proximity = (event->value != 0);
			}
			else if (event->code == BTN_TOUCH)
			{
				/* we use the pressure to determine
				 * the button 1 for now */
			}
			else if ((event->code == BTN_STYLUS) ||
				(event->code == BTN_MIDDLE))
			{
				MOD_BUTTONS (2, event->value);
			}
			else if ((event->code == BTN_STYLUS2) ||
				(event->code == BTN_RIGHT))
			{
				MOD_BUTTONS (4, event->value);
			}
			else if (event->code == BTN_LEFT)
				MOD_BUTTONS (1, event->value);
			else if (event->code == BTN_SIDE)
				MOD_BUTTONS (8, event->value);
			else if (event->code == BTN_EXTRA)
				MOD_BUTTONS (16, event->value);
		}
	} /* next event */

	/* dispatch event */
	xf86WcmEvent(common, channel, ds);
}

#endif /* LINUX_INPUT */
