/*
 * $Id: wacom.c,v 1.2 2005/09/16 19:21:04 pingc Exp $
 *
 *  Copyright (c) 2000-2002 Vojtech Pavlik  <vojtech@suse.cz>
 *  Copyright (c) 2000 Andreas Bach Aaen    <abach@stofanet.dk>
 *  Copyright (c) 2000 Clifford Wolf        <clifford@clifford.at>
 *  Copyright (c) 2000 Sam Mosel            <sam.mosel@computer.org>
 *  Copyright (c) 2000 James E. Blair       <corvus@gnu.org>
 *  Copyright (c) 2000 Daniel Egger         <egger@suse.de>
 *  Copyright (c) 2001 Frederic Lepied      <flepied@mandrakesoft.com>
 *  Copyright (c) 2002 Christer Nilsson     <christer.nilsson@kretskompaniet.se>
 *  Copyright (c) 2002-2005 Ping Cheng      <pingc@wacom.com>
 *  Copyright (c) 2002 John Joganic         <john@joganic.com>
 *
 *  USB Wacom Graphire and Intuos tablet support
 *
 *  Sponsored by SuSE
 *
 *  ChangeLog:
 *      v0.1 (vp)  - Initial release
 *      v0.2 (aba) - Support for all buttons / combinations
 *      v0.3 (vp)  - Support for Intuos added
 *	v0.4 (sm)  - Support for more Intuos models, menustrip
 *			relative mode, proximity.
 *	v0.5 (vp)  - Big cleanup, nifty features removed,
 * 			they belong in userspace
 *	v1.8 (vp)  - Submit URB only when operating, moved to CVS,
 *			use input_report_key instead of report_btn and
 *			other cleanups
 *	v1.11 (vp) - Add URB ->dev setting for new kernels
 *	v1.11 (jb) - Add support for the 4D Mouse & Lens
 *	v1.12 (de) - Add support for two more inking pen IDs
 *	v1.14 (vp) - Use new USB device id probing scheme.
 *		     Fix Wacom Graphire mouse wheel
 *	v1.18 (vp) - Fix mouse wheel direction
 *		     Make mouse relative
 *      v1.20 (fl) - Report tool id for Intuos devices
 *                 - Multi tools support
 *                 - Corrected Intuos protocol decoding (airbrush, 4D mouse, lens cursor...)
 *                 - Add PL models support
 *		   - Fix Wacom Graphire mouse wheel again
 *	v1.21 (vp) - Removed protocol descriptions
 *		   - Added MISC_SERIAL for tool serial numbers
 *	      (gb) - Identify version on module load.
 *    v1.21.1 (fl) - added Graphire2 support
 *    v1.21.2 (fl) - added Intuos2 support
 *                 - added all the PL ids
 *    v1.21.3 (fl) - added another eraser id from Neil Okamoto
 *                 - added smooth filter for Graphire from Peri Hankey
 *                 - added PenPartner support from Olaf van Es
 *                 - new tool ids from Ole Martin Bjoerndalen
 *
 *   WARNING: THIS IS NOT PART OF THE OFFICIAL KERNEL TREE
 *   THIS IS FOR TESTING PURPOSES
 *
 *    v2.4.30-pc0.6.9   - Supports x86_64 systems
 *    v2.4.30-pc0.7.0   - Added Volito2 support
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/autoconf.h>
#if defined(CONFIG_MODVERSIONS) && !defined(MODVERSIONS)
#   define MODVERSIONS
#endif

#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/smp_lock.h>
#include <linux/list.h>

/*
 * Version Information
 */
#define DRIVER_VERSION "v2.4.30-pc0.6.9"
#define DRIVER_AUTHOR "Vojtech Pavlik <vojtech@suse.cz>"
#ifndef __JEJ_DEBUG
#define DRIVER_DESC "USB Wacom Graphire and Wacom Intuos tablet driver (LINUXWACOM)"
#else
#define DRIVER_DESC "USB Wacom Graphire and Wacom Intuos tablet driver (LINUXWACOM-DEBUG)"
#endif

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

#define USB_VENDOR_ID_WACOM	0x056a

struct wacom_features {
	char *name;
	int pktlen;
	int x_max;
	int y_max;
	int pressure_max;
	int distance_max;
	void (*irq)(struct urb *urb);
	unsigned long evbit;
	unsigned long absbit;
	unsigned long relbit;
	unsigned long btnbit;
	unsigned long digibit;
};

struct wacom {
	signed char data[10];
	struct input_dev dev;
	struct usb_device *usbdev;
	struct urb irq;
	struct wacom_features *features;
	int tool[2];
	int open;
	__u32 serial[2];
	
	unsigned int ifnum;
};

static void wacom_pl_irq(struct urb *urb)
{
	struct wacom *wacom = urb->context;
	unsigned char *data = wacom->data;
	struct input_dev *dev = &wacom->dev;
	int prox, pressure;

	if (urb->status) return;

	if (data[0] != 2 && data[0] != 5)
	{
		printk(KERN_INFO "wacom_pl_irq: received unknown report #%d\n", data[0]);
		return;
	}
	
	/* proximity and pressure */
	prox = data[1] & 0x40;
		
	if (prox) {
	        pressure = (signed char) ((data[7] <<1 ) | ((data[4] >> 2) & 1));
		if ( wacom->features->pressure_max > 350 ) {
		    pressure = (pressure << 1) | ((data[4] >> 6) & 1);
                } 
		pressure += (( wacom->features->pressure_max + 1 )/ 2);

		/*
		 * if going from out of proximity into proximity select between the eraser
		 * and the pen based on the state of the stylus2 button, choose eraser if
		 * pressed else choose pen. if not a proximity change from out to in, send
		 * an out of proximity for previous tool then a in for new tool.
		 */
		if (!wacom->tool[0]) {
			/* Going into proximity select tool */
			wacom->tool[1] = (data[4] & 0x20)? BTN_TOOL_RUBBER : BTN_TOOL_PEN;
		}
		else {
			/* was entered with stylus2 pressed */
			if (wacom->tool[1] == BTN_TOOL_RUBBER && !(data[4] & 0x20) ) {
				/* report out proximity for previous tool */
				input_report_key(dev, wacom->tool[1], 0);
				input_event(dev, EV_MSC, MSC_SERIAL, 0);
				wacom->tool[1] = BTN_TOOL_PEN;
				return;
			}
		}
		if (wacom->tool[1] != BTN_TOOL_RUBBER) {
			/* Unknown tool selected default to pen tool */
			wacom->tool[1] = BTN_TOOL_PEN;
		}
		input_report_key(dev, wacom->tool[1], prox); /* report in proximity for tool */

		input_report_abs(dev, ABS_X, data[3] | ((__u32)data[2] << 7) | ((__u32)(data[1] & 0x03) << 14));
		input_report_abs(dev, ABS_Y, data[6] | ((__u32)data[5] << 7) | ((__u32)(data[4] & 0x03) << 14));
		input_report_abs(dev, ABS_PRESSURE, pressure);
		input_report_key(dev, BTN_TOUCH, data[4] & 0x08);
		input_report_key(dev, BTN_STYLUS, data[4] & 0x10);
		/* Only allow the stylus2 button to be reported for the pen tool. */
		input_report_key(dev, BTN_STYLUS2, (wacom->tool[1] == BTN_TOOL_PEN) && (data[4] & 0x20));
	}
	else {
		/* report proximity-out of a (valid) tool */
		if (wacom->tool[1] != BTN_TOOL_RUBBER) {
			/* Unknown tool selected default to pen tool */
			wacom->tool[1] = BTN_TOOL_PEN;
		}
		input_report_key(dev, wacom->tool[1], prox);
	}

	wacom->tool[0] = prox; /* Save proximity state */
	/* end of proximity code */
	
	input_event(dev, EV_MSC, MSC_SERIAL, 0);
}

static void wacom_ptu_irq(struct urb *urb)
{
	struct wacom *wacom = urb->context;
	unsigned char *data = wacom->data;
	struct input_dev *dev = &wacom->dev;

	if (urb->status) return;

	if (data[0] != 2 && data[0] != 5)
	{
		printk(KERN_INFO "wacom_ptu_irq: received unknown report #%d\n", data[0]);
		return;
	}
	
	if (data[1] & 0x04) 
	{
		input_report_key(dev, BTN_TOOL_RUBBER, data[1] & 0x20);
		input_report_key(dev, BTN_TOUCH, data[1] & 0x08);
	}
	else
	{
		input_report_key(dev, BTN_TOOL_PEN, data[1] & 0x20);
		input_report_key(dev, BTN_TOUCH, data[1] & 0x01);
	}
	input_report_abs(dev, ABS_X, data[3] << 8 | data[2]);
	input_report_abs(dev, ABS_Y, data[5] << 8 | data[4]);
	input_report_abs(dev, ABS_PRESSURE, (data[6]|data[7] << 8));
	input_report_key(dev, BTN_STYLUS, data[1] & 0x02);
	input_report_key(dev, BTN_STYLUS2, data[1] & 0x10);

	input_event(dev, EV_MSC, MSC_SERIAL, 0);
}


static void wacom_penpartner_irq(struct urb *urb)
{
	struct wacom *wacom = urb->context;
	unsigned char *data = wacom->data;
	struct input_dev *dev = &wacom->dev;
	int leftmb = (((signed char)data[6] > -80) && !(data[5] &0x20));

	if (urb->status) return;

	if (data[0] != 2 && data[0] != 5)
	{
		printk(KERN_INFO "wacom_penpartner_irq: received unknown report #%d\n", data[0]);
		return;
	}

	input_report_key(dev, BTN_TOOL_PEN, 1);
	input_report_abs(dev, ABS_X, data[2] << 8 | data[1]);
	input_report_abs(dev, ABS_Y, data[4] << 8 | data[3]);
	input_report_abs(dev, ABS_PRESSURE, (signed char)data[6] + 127);
	input_report_key(dev, BTN_TOUCH, leftmb);
	input_report_key(dev, BTN_STYLUS, (data[5] & 0x40));

	input_event(dev, EV_MSC, MSC_SERIAL, leftmb);
}

static void wacom_graphire_irq(struct urb *urb)
{
	struct wacom *wacom = urb->context;
	unsigned char *data = wacom->data;
	struct input_dev *dev = &wacom->dev;
	int x, y;

	if (urb->status) return;

	if (data[0] != 2)
	{
		printk(KERN_INFO "wacom_graphire_irq: received unknown report #%d\n", data[0]);
		return;
	}

	if ( data[1] & 0x10 ) /* in prox */
	{
		switch ((data[1] >> 5) & 3) {

			case 0:	/* Pen */
				wacom->tool[0] = BTN_TOOL_PEN;
				break;

			case 1: /* Rubber */
				wacom->tool[0] = BTN_TOOL_RUBBER;
				break;

			case 2: /* Mouse with wheel */
				input_report_key(dev, BTN_MIDDLE, data[1] & 0x04);
				input_report_rel(dev, REL_WHEEL, (signed char) data[6]);
				/* fall through */

			case 3: /* Mouse without wheel */
				wacom->tool[0] = BTN_TOOL_MOUSE;
				input_report_key(dev, BTN_LEFT, data[1] & 0x01);
				input_report_key(dev, BTN_RIGHT, data[1] & 0x02);
				input_report_abs(dev, ABS_DISTANCE, data[7]);
				break;
		}
	}

	if (data[1] & 0x90) {
		x = data[2] | ((__u32)data[3] << 8);
		y = data[4] | ((__u32)data[5] << 8);
		input_report_abs(dev, ABS_X, x);
		input_report_abs(dev, ABS_Y, y);

		if (wacom->tool[0] != BTN_TOOL_MOUSE) {
			input_report_abs(dev, ABS_PRESSURE, data[6] | ((__u32)data[7] << 8));
			input_report_key(dev, BTN_TOUCH, data[1] & 0x01);
			input_report_key(dev, BTN_STYLUS, data[1] & 0x02);
			input_report_key(dev, BTN_STYLUS2, data[1] & 0x04);
		}
	}
	input_report_key(dev, wacom->tool[0], data[1] & 0x10);
	input_event(dev, EV_MSC, MSC_SERIAL, data[1] & 0x01);
}

static int wacom_intuos_inout(struct urb *urb)
{
	struct wacom *wacom = urb->context;
	unsigned char *data = wacom->data;
	struct input_dev *dev = &wacom->dev;
	int idx, type;

	/* tool number */
	idx = data[1] & 0x01;

	/* Enter report */
	if ((data[1] & 0xfc) == 0xc0)
	{
		/* serial number of the tool */
		wacom->serial[idx] = ((__u32)(data[3] & 0x0f) << 28) +
				((__u32)data[4] << 20) + ((__u32)data[5] << 12) +
				((__u32)data[6] << 4) + ((__u32)data[7] >> 4);

		#ifdef __JEJ_DEBUG
		printk(KERN_INFO "wacom_intuos_irq: tool change 0x%03X\n",
				(((__u32)data[2] << 4) | (data[3] >> 4)));
		#endif

		type = ((__u32)data[2] << 4) | (data[3] >> 4);
		switch (type)
		{
			case 0x812: /* Intuos2 ink pen XP-110-00A */
			case 0x801: /* Intuos3 Inking pen */
			case 0x012: /* Inking pen */
				wacom->tool[idx] = BTN_TOOL_PENCIL; break;

			case 0x822: /* Intuos Pen GP-300E-01H */
			case 0x852: /* Intuos2 Grip Pen XP-501E-00A */
			case 0x842: /* Designer Pen */
			case 0x823: /* Intuos3 Grip Pen */
			case 0x813: /* Intuos3 Classic Pen */
			case 0x885: /* Intuos3 Marker Pen */
			case 0x022:
				wacom->tool[idx] = BTN_TOOL_PEN; break;

			case 0x832: /* Intuos2 stroke pen XP-120-00A */
			case 0x032: /* Stroke pen */
				wacom->tool[idx] = BTN_TOOL_BRUSH; break;

			case 0x007: /* 2D Mouse */
			case 0x09C: /* ?? Mouse - not a valid code according to Wacom */
			case 0x094: /* 4D Mouse */
			case 0x017: /* Intuos3 2D Mouse */
				wacom->tool[idx] = BTN_TOOL_MOUSE; break;

			case 0x096: /* Lens cursor */
			case 0x097: /* Intuos3 Lens cursor */
				wacom->tool[idx] = BTN_TOOL_LENS; break;

			case 0x82A:
			case 0x85A:
			case 0x91A:
			case 0xD1A:
			case 0x0FA: /* Eraser */
			case 0x82B: /* Intuos3 Grip Pen Eraser */
			case 0x81B: /* Intuos3 Classic Pen Eraser */
			case 0x91B: /* Intuos3 Airbrush Eraser */
				wacom->tool[idx] = BTN_TOOL_RUBBER; break;

			case 0x112: /* Airbrush */
			case 0x912: /* Intuos2 Airbrush */
			case 0xD12: /* Intuos Airbrush */
			case 0x913: /* Intuos3 Airbrush */
				wacom->tool[idx] = BTN_TOOL_AIRBRUSH; break;

			default: /* Unknown tool */
				wacom->tool[idx] = BTN_TOOL_PEN; break;
		}
		input_report_key(dev, wacom->tool[idx], type);
		input_event(dev, EV_MSC, MSC_SERIAL, wacom->serial[idx]);
		return 1;
	}

	/* Exit report */
	if ((data[1] & 0xfe) == 0x80)
	{
		input_report_key(dev, wacom->tool[idx], 0);
		input_event(dev, EV_MSC, MSC_SERIAL, wacom->serial[idx]);
		return 1;
	}

	return 0;
}

static void wacom_intuos_general(struct urb *urb)
{
	struct wacom *wacom = urb->context;
	unsigned char *data = wacom->data;
	struct input_dev *dev = &wacom->dev;
	unsigned int t;

	/* general pen packet */
	if ((data[1] & 0xb8) == 0xa0)
	{
		t = ((__u32)data[6] << 2) | ((data[7] >> 6) & 3);
		input_report_abs(dev, ABS_PRESSURE, t);
		input_report_abs(dev, ABS_TILT_X,
				((data[7] << 1) & 0x7e) | (data[8] >> 7));
		input_report_abs(dev, ABS_TILT_Y, data[8] & 0x7f);
		input_report_key(dev, BTN_STYLUS, data[1] & 2);
		input_report_key(dev, BTN_STYLUS2, data[1] & 4);
		input_report_key(dev, BTN_TOUCH, t > 10);
	}

	/* airbrush second packet */
	if ((data[1] & 0xbc) == 0xb4)
	{
		input_report_abs(dev, ABS_WHEEL,
				((__u32)data[6] << 2) | ((data[7] >> 6) & 3));
		input_report_abs(dev, ABS_TILT_X,
				((data[7] << 1) & 0x7e) | (data[8] >> 7));
		input_report_abs(dev, ABS_TILT_Y, data[8] & 0x7f);
	}
	return;
}

static void wacom_intuos_irq(struct urb *urb)
{
	struct wacom *wacom = urb->context;
	unsigned char *data = wacom->data;
	struct input_dev *dev = &wacom->dev;
	unsigned int t;
	int idx;

	if (urb->status) return;

	/* check for valid report */
	if (data[0] != 2 && data[0] != 5 && data[0] != 6 && data[0] != 12)
	{
		printk(KERN_INFO "wacom_intuos_irq: received unknown report #%d\n", data[0]);
		return;
	}
	
	/* tool index */
	idx = data[1] & 0x01;

	/* pad packets. Works as a second tool and is alway in prox */
	if (data[0] == 12 && (strstr(wacom->features->name, "Intuos3") ||
			strstr(wacom->features->name, "Cintiq")) )
	{
		/* initiate the pad as a device */
		if (wacom->tool[1] != BTN_TOOL_FINGER)
		{
			wacom->tool[1] = BTN_TOOL_FINGER;
			input_report_key(dev, wacom->tool[1], 1);
		}
		input_report_key(dev, BTN_0, (data[5] & 0x01));
		input_report_key(dev, BTN_1, (data[5] & 0x02));
		input_report_key(dev, BTN_2, (data[5] & 0x04));
		input_report_key(dev, BTN_3, (data[5] & 0x08));
		input_report_key(dev, BTN_4, (data[6] & 0x01));
		input_report_key(dev, BTN_5, (data[6] & 0x02));
		input_report_key(dev, BTN_6, (data[6] & 0x04));
		input_report_key(dev, BTN_7, (data[6] & 0x08));
		input_report_abs(dev, ABS_RX, ((data[1] & 0x1f) << 8) | data[2]);
		input_report_abs(dev, ABS_RY, ((data[3] & 0x1f) << 8) | data[4]);
		input_event(dev, EV_MSC, MSC_SERIAL, 0xffffffff);
		return;
	}

	/* process in/out prox events */
	if (wacom_intuos_inout(urb)) return;

	/* Cintiq doesn't send data when RDY bit isn't set */
	if (strstr(wacom->features->name, "Cintiq") && !(data[1] & 0x40)) return;

	if(strstr(wacom->features->name, "Intuos3") || strstr(wacom->features->name, "Cintiq"))
	{
		input_report_abs(dev, ABS_X, ((__u32)data[2] << 9) | ((__u32)data[3] << 1) | ((data[9] >> 1) & 1));
		input_report_abs(dev, ABS_Y, ((__u32)data[4] << 9) | ((__u32)data[5] << 1) | (data[9] & 1));
		input_report_abs(dev, ABS_DISTANCE, ((data[9] >> 2) & 0x3f));
	}
	else
	{
		input_report_abs(dev, ABS_X, ((__u32)data[2] << 8) | data[3]);
		input_report_abs(dev, ABS_Y, ((__u32)data[4] << 8) | data[5]);
		input_report_abs(dev, ABS_DISTANCE, ((data[9] >> 3) & 0x1f));
	}

	/* process general packets */
	wacom_intuos_general(urb);
	
	/* 4D mouse, 2D mouse, marker pen rotation, or Lens cursor packets */
	if ((data[1] & 0xbc) == 0xa8 || (data[1] & 0xbe) == 0xb0)
	{
		/* Rotation packet */
		if (data[1] & 0x02)
		{
			if(strstr(wacom->features->name, "Intuos3") ||
					strstr(wacom->features->name, "Cintiq"))
			{
				/* I3 marker pen rotation reported as wheel 
				 * due to valuator limitation 
				 */
				t = ((__u32)data[6] << 3) | ((data[7] >> 5) & 7);
				t = (data[7] & 0x20) ? ((t > 900) ? ((t-1) / 2 - 1350) :
					((t-1) / 2 + 450)) : (450 - t / 2) ;
				input_report_abs(dev, ABS_WHEEL, t);
			}
			else
			{
				/* 4D mouse rotation packet */
				t = ((__u32)data[6] << 3) | ((data[7] >> 5) & 7);
				input_report_abs(dev, ABS_RZ, (data[7] & 0x20) ?
					((t - 1) / 2) : -t / 2);
			}
		}

		/* 4D mouse packets */
		else if ( !(data[1] & 0x10) && !strstr(wacom->features->name, "Intuos3"))
		{
			input_report_key(dev, BTN_LEFT,   data[8] & 0x01);
			input_report_key(dev, BTN_MIDDLE, data[8] & 0x02);
			input_report_key(dev, BTN_RIGHT,  data[8] & 0x04);
			input_report_key(dev, BTN_SIDE,   data[8] & 0x20);
			input_report_key(dev, BTN_EXTRA,  data[8] & 0x10);
			/* throttle is positive when rolled backwards */
			t = ((__u32)data[6] << 2) | ((data[7] >> 6) & 3);
			input_report_abs(dev, ABS_THROTTLE, (data[8] & 0x08) ? -t : t);
		}

		/* 2D mouse packets */	
		else if (wacom->tool[idx] == BTN_TOOL_MOUSE)
		{
			input_report_key(dev, BTN_LEFT,   data[8] & 0x04);
			input_report_key(dev, BTN_MIDDLE, data[8] & 0x08);
			input_report_key(dev, BTN_RIGHT,  data[8] & 0x10);
			/* mouse wheel is positive when rolled backwards */
			input_report_rel(dev, REL_WHEEL, (__u32)((data[8] & 0x02) >> 1)
					 - (__u32)(data[8] & 0x01));

			/* I3 2D mouse side buttons */	
			if (strstr(wacom->features->name, "Intuos3"))
			{
				input_report_key(dev, BTN_SIDE,   data[8] & 0x40);
				input_report_key(dev, BTN_EXTRA,  data[8] & 0x20);
			}
		}
		/* lens cursor packets */
		else if ( !strstr(wacom->features->name, "Intuos3") )
		{
			input_report_key(dev, BTN_LEFT,   data[8] & 0x01);
			input_report_key(dev, BTN_MIDDLE, data[8] & 0x02);
			input_report_key(dev, BTN_RIGHT,  data[8] & 0x04);
			input_report_key(dev, BTN_SIDE,   data[8] & 0x10);
			input_report_key(dev, BTN_EXTRA,  data[8] & 0x08);
		}
	}

	input_report_key(dev, wacom->tool[idx], 1);
	input_event(dev, EV_MSC, MSC_SERIAL, wacom->serial[idx]);
}

#define WACOM_GRAPHIRE_BITS	(BIT(EV_REL))
#define WACOM_GRAPHIRE_REL	(BIT(REL_WHEEL))
#define WACOM_INTUOS_TOOLS	(BIT(BTN_TOOL_BRUSH) | BIT(BTN_TOOL_PENCIL) | BIT(BTN_TOOL_AIRBRUSH) | BIT(BTN_TOOL_LENS))
#define WACOM_INTUOS3_TOOLS	(WACOM_INTUOS_TOOLS | BIT(BTN_TOOL_FINGER))
#define WACOM_INTUOS_BUTTONS	(BIT(BTN_SIDE) | BIT(BTN_EXTRA))
#define WACOM_INTUOS3_BUTTONS	(WACOM_INTUOS_BUTTONS | BIT(BTN_0) | BIT(BTN_1) | BIT(BTN_2) | BIT(BTN_3) | BIT(BTN_4) | BIT(BTN_5) | BIT(BTN_6) | BIT(BTN_7))
#define WACOM_INTUOS_BITS	(BIT(EV_REL))
#define WACOM_INTUOS_REL	(BIT(REL_WHEEL))
#define WACOM_INTUOS_ABS	(BIT(ABS_TILT_X) | BIT(ABS_TILT_Y) | BIT(ABS_RZ) | BIT(ABS_THROTTLE))
#define WACOM_INTUOS3_ABS	(WACOM_INTUOS_ABS | BIT(ABS_RX) | BIT(ABS_RY))

struct wacom_features wacom_features[] = {

	/* PenPartner */
	/*  0 */ { "Wacom Penpartner",	   7,   5040,  3780,   255, 32,
			wacom_penpartner_irq, 0, 0, 0, 0 },

	/* Graphire */
	/*  1 */ { "Wacom Graphire",       8,  10206,  7422,   511, 32,
			wacom_graphire_irq, WACOM_GRAPHIRE_BITS, 0, WACOM_GRAPHIRE_REL, 0 },
	/*  2 */ { "Wacom Graphire2 4x5",  8,  10206,  7422,   511, 32,
			wacom_graphire_irq, WACOM_GRAPHIRE_BITS, 0, WACOM_GRAPHIRE_REL, 0 },
	/*  3 */ { "Wacom Graphire2 5x7",  8,  13918, 10206,   511, 32,
			wacom_graphire_irq, WACOM_GRAPHIRE_BITS, 0, WACOM_GRAPHIRE_REL, 0 },

	/* Intuos */
	/*  4 */ { "Wacom Intuos 4x5",    10,  12700, 10600,  1023, 15,
			wacom_intuos_irq, WACOM_INTUOS_BITS, WACOM_INTUOS_ABS,
			WACOM_INTUOS_REL, WACOM_INTUOS_BUTTONS, WACOM_INTUOS_TOOLS },
	/* JEJ - confirmed X and Y range from test tablet */
	/*  5 */ { "Wacom Intuos 6x8",    10,  20320, 16240,  1023, 15,
			wacom_intuos_irq, WACOM_INTUOS_BITS, WACOM_INTUOS_ABS,
			WACOM_INTUOS_REL, WACOM_INTUOS_BUTTONS, WACOM_INTUOS_TOOLS },
	/*  6 */ { "Wacom Intuos 9x12",   10,  30480, 24060,  1023, 15,
			wacom_intuos_irq, WACOM_INTUOS_BITS, WACOM_INTUOS_ABS,
			WACOM_INTUOS_REL, WACOM_INTUOS_BUTTONS, WACOM_INTUOS_TOOLS },
	/*  7 */ { "Wacom Intuos 12x12",  10,  30480, 31680,  1023, 15,
			wacom_intuos_irq, WACOM_INTUOS_BITS, WACOM_INTUOS_ABS,
			WACOM_INTUOS_REL, WACOM_INTUOS_BUTTONS, WACOM_INTUOS_TOOLS },
	/*  8 */ { "Wacom Intuos 12x18",  10,  45720, 31680,  1023, 15,
			wacom_intuos_irq, WACOM_INTUOS_BITS, WACOM_INTUOS_ABS,
			WACOM_INTUOS_REL, WACOM_INTUOS_BUTTONS, WACOM_INTUOS_TOOLS },

	/* PL - Cintiq */
	/*  9 */ { "Wacom PL400",          8,   5408,  4056,   255, 32,
			wacom_pl_irq, 0,  0, 0, 0 },
	/* 10 */ { "Wacom PL500",          8,   6144,  4608,   255, 32,
			wacom_pl_irq, 0,  0, 0, 0 },
	/* 11 */ { "Wacom PL600",          8,   6126,  4604,   255, 32,
			wacom_pl_irq, 0,  0, 0, 0 },
	/* 12 */ { "Wacom PL600SX",        8,   6260,  5016,   255, 32,
			wacom_pl_irq, 0,  0, 0, 0 },
	/* 13 */ { "Wacom PL550",          8,   6144,  4608,   511, 32,
			wacom_pl_irq, 0,  0, 0, 0 },
	/* 14 */ { "Wacom PL800",          8,   7220,  5780,   511, 32,
			wacom_pl_irq, 0,  0, 0, 0 },
	/* 15 */ { "Wacom PL700",          8,   6758,  5406,   511, 32,
			wacom_pl_irq, 0,  0, 0, 0 },
	/* 16 */ { "Wacom PL510",          8,   6282,  4762,   511, 32,
			wacom_pl_irq, 0,  0, 0, 0 },

	/* Intuos2 */
	/* JEJ - confirmed X and Y range from J.N. tablet */
	/* 17 */ { "Wacom Intuos2 4x5",   10,  12700, 10600,  1023, 15,
			wacom_intuos_irq, WACOM_INTUOS_BITS, WACOM_INTUOS_ABS,
			WACOM_INTUOS_REL, WACOM_INTUOS_BUTTONS, WACOM_INTUOS_TOOLS },
	/* JEJ - confirmed X and Y range from R.T. and J.S. tablets */
	/* 18 */ { "Wacom Intuos2 6x8",   10,  20320, 16240,  1023, 15,
			wacom_intuos_irq, WACOM_INTUOS_BITS, WACOM_INTUOS_ABS,
			WACOM_INTUOS_REL, WACOM_INTUOS_BUTTONS, WACOM_INTUOS_TOOLS },
	/* JEJ - values from serial 9x12 */
	/* 19 */ { "Wacom Intuos2 9x12",  10,  30480, 24060,  1023, 15,
			wacom_intuos_irq, WACOM_INTUOS_BITS, WACOM_INTUOS_ABS,
			WACOM_INTUOS_REL, WACOM_INTUOS_BUTTONS, WACOM_INTUOS_TOOLS },
	/* JEJ - confirmed X and Y range from J.J. tablet */
	/* 20 */ { "Wacom Intuos2 12x12", 10,  30480, 31680,  1023, 15,
			wacom_intuos_irq, WACOM_INTUOS_BITS, WACOM_INTUOS_ABS,
			WACOM_INTUOS_REL, WACOM_INTUOS_BUTTONS, WACOM_INTUOS_TOOLS },
	/* 21 */ { "Wacom Intuos2 12x18", 10,  45720, 31680,  1023, 15,
			wacom_intuos_irq, WACOM_INTUOS_BITS, WACOM_INTUOS_ABS,
			WACOM_INTUOS_REL, WACOM_INTUOS_BUTTONS, WACOM_INTUOS_TOOLS },
	/* Volito - (Graphire2 4x5 no mouse wheel) */
	/* 22 */ { "Wacom Volito",         8,   5104,  3712,   511, 32,
			wacom_graphire_irq, WACOM_GRAPHIRE_BITS, 0, 0, 0 },
	/* Volito2 - PenPartner - PenStation */
	/* 23 */ { "Wacom PenPartner2",    8,   3250,  2320,   255, 32,
			wacom_graphire_irq, WACOM_GRAPHIRE_BITS, 0, 0, 0 },
	/* 24 */ { "Wacom Volito2 4x5",    8,   5104,  3712,   511, 32,
			wacom_graphire_irq, WACOM_GRAPHIRE_BITS, 0, 0, 0 },
	/* 25 */ { "Wacom Volito2 2x3",    8,   3248,  2320,   511, 32,
			wacom_graphire_irq, WACOM_GRAPHIRE_BITS, 0, 0, 0 },
	/* 26 */ { "Wacom Graphire3 4x5",  8,   10208, 7424,   511, 32,
			wacom_graphire_irq, WACOM_GRAPHIRE_BITS, 0, WACOM_GRAPHIRE_REL, 0 },
	/* 27 */ { "Wacom Graphire3 6x8",  8,   16704, 12064,  511, 32,
			wacom_graphire_irq, WACOM_GRAPHIRE_BITS, 0, WACOM_GRAPHIRE_REL, 0 },
	/* 28 */ { "Wacom Cintiq Partner", 8,   20480, 15360,  511, 32,
			wacom_ptu_irq, 0, 0, 0, 0 },
	/* Intuos3 */
	/* 29 */ { "Wacom Intuos3 4x5",   10,  25400, 20320,  1023, 15,
			wacom_intuos_irq, WACOM_INTUOS_BITS, WACOM_INTUOS3_ABS,
			WACOM_INTUOS_REL, WACOM_INTUOS3_BUTTONS, WACOM_INTUOS3_TOOLS },
	/* 30 */ { "Wacom Intuos3 6x8",   10,  40640, 30480,  1023, 15,
			wacom_intuos_irq, WACOM_INTUOS_BITS, WACOM_INTUOS3_ABS,
			WACOM_INTUOS_REL, WACOM_INTUOS3_BUTTONS, WACOM_INTUOS3_TOOLS },
	/* 31 */ { "Wacom Intuos3 9x12",  10,  60960, 45720,  1023, 15,
			wacom_intuos_irq, WACOM_INTUOS_BITS, WACOM_INTUOS3_ABS,
			WACOM_INTUOS_REL, WACOM_INTUOS3_BUTTONS, WACOM_INTUOS3_TOOLS },
	/* Protocol 5 Cintiq */
	/* 32 */ { "Wacom Cintiq 21UX",   10,  87200, 65600,  1023, 15,
			wacom_intuos_irq, WACOM_INTUOS_BITS, WACOM_INTUOS3_ABS,
			WACOM_INTUOS_REL, WACOM_INTUOS3_BUTTONS, WACOM_INTUOS3_TOOLS },

	{ NULL , 0 }
};

struct usb_device_id wacom_ids[] = {
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x00), driver_info: 0 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x10), driver_info: 1 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x11), driver_info: 2 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x12), driver_info: 3 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x20), driver_info: 4 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x21), driver_info: 5 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x22), driver_info: 6 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x23), driver_info: 7 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x24), driver_info: 8 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x30), driver_info: 9 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x31), driver_info: 10 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x32), driver_info: 11 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x33), driver_info: 12 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x34), driver_info: 13 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x35), driver_info: 14 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x37), driver_info: 15 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x38), driver_info: 16 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x41), driver_info: 17 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x42), driver_info: 18 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x43), driver_info: 19 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x44), driver_info: 20 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x45), driver_info: 21 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x60), driver_info: 22 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x61), driver_info: 23 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x62), driver_info: 24 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x63), driver_info: 25 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x13), driver_info: 26 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x14), driver_info: 27 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x03), driver_info: 28 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0xB0), driver_info: 29 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0xB1), driver_info: 30 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0xB2), driver_info: 31 },

	/* some Intuos2 6x8's erroneously report as 0x47;
	 * multiple confirmed examples exist. */
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x47), driver_info: 18 },

	{ }
};

MODULE_DEVICE_TABLE(usb, wacom_ids);

static int wacom_open(struct input_dev *dev)
{
	struct wacom *wacom = dev->private;
	
	if (wacom->open++)
		return 0;

	wacom->irq.dev = wacom->usbdev;
	if (usb_submit_urb(&wacom->irq))
		return -EIO;

	return 0;
}

static void wacom_close(struct input_dev *dev)
{
	struct wacom *wacom = dev->private;

	if (!--wacom->open)
		usb_unlink_urb(&wacom->irq);
}

static void wacom_reset(struct wacom* wacom)
{
	unsigned char edata[2], limit=0;
	#ifdef __JEJ_DEBUG
	printk(KERN_INFO __FILE__ ": Setting tablet report for tablet data\n");
	#endif

	/* ask the tablet to report tablet data. repeats until it succeeds */
	do {
		edata[0] = 2;
		edata[1] = 2;
		usb_set_report(wacom->usbdev, wacom->ifnum, 3, 2, edata, 2);
		usb_get_report(wacom->usbdev, wacom->ifnum, 3, 2, edata, 2);
	} while (edata[1] != 2 && limit++ < 5);
}

static void *wacom_probe(struct usb_device *dev, unsigned int ifnum, const struct usb_device_id *id)
{
	struct usb_endpoint_descriptor *endpoint;
	struct wacom *wacom;

	if (!(wacom = kmalloc(sizeof(struct wacom), GFP_KERNEL))) return NULL;
	memset(wacom, 0, sizeof(struct wacom));

	wacom->features = wacom_features + id->driver_info;

	wacom->dev.evbit[0] |= BIT(EV_KEY) | BIT(EV_ABS) | BIT(EV_MSC) |
			wacom->features->evbit;
	wacom->dev.absbit[0] |= BIT(ABS_X) | BIT(ABS_Y) | BIT(ABS_PRESSURE) |
			BIT(ABS_DISTANCE) | BIT(ABS_WHEEL) | wacom->features->absbit;
	wacom->dev.relbit[0] |= wacom->features->relbit;
	wacom->dev.keybit[LONG(BTN_LEFT)] |= BIT(BTN_LEFT) | BIT(BTN_RIGHT) |
			BIT(BTN_MIDDLE) | wacom->features->btnbit;
	wacom->dev.keybit[LONG(BTN_DIGI)] |= BIT(BTN_TOOL_PEN) | 
			BIT(BTN_TOOL_RUBBER) | BIT(BTN_TOOL_MOUSE) | 
			BIT(BTN_TOUCH) | BIT(BTN_STYLUS) | BIT(BTN_STYLUS2) |
			wacom->features->digibit;
	wacom->dev.mscbit[0] |= BIT(MSC_SERIAL);

	#ifdef __JEJ_DEBUG
	printk(KERN_INFO __FILE__ ": Reporting max %d, %d\n",
		wacom->features->x_max, wacom->features->y_max);
	#endif

	wacom->dev.absmax[ABS_X] = wacom->features->x_max;
	wacom->dev.absmax[ABS_Y] = wacom->features->y_max;
	wacom->dev.absmax[ABS_PRESSURE] = wacom->features->pressure_max;
	wacom->dev.absmax[ABS_DISTANCE] = wacom->features->distance_max;
	wacom->dev.absmax[ABS_TILT_X] = 127;
	wacom->dev.absmax[ABS_TILT_Y] = 127;
	wacom->dev.absmax[ABS_WHEEL] = 1023;

	wacom->dev.absmax[ABS_RX] = 4097;
	wacom->dev.absmax[ABS_RY] = 4097;
	wacom->dev.absmin[ABS_RZ] = -900;
	wacom->dev.absmax[ABS_RZ] = 899;
	wacom->dev.absmin[ABS_THROTTLE] = -1023;
	wacom->dev.absmax[ABS_THROTTLE] = 1023;

	wacom->dev.absfuzz[ABS_X] = 4;
	wacom->dev.absfuzz[ABS_Y] = 4;

	wacom->dev.private = wacom;
	wacom->dev.open = wacom_open;
	wacom->dev.close = wacom_close;

	wacom->dev.name = wacom->features->name;
	wacom->dev.idbus = BUS_USB;
	wacom->dev.idvendor = dev->descriptor.idVendor;
	wacom->dev.idproduct = dev->descriptor.idProduct;
	wacom->dev.idversion = dev->descriptor.bcdDevice;
	wacom->usbdev = dev;
	wacom->ifnum = ifnum;

	endpoint = dev->config[0].interface[ifnum].altsetting[0].endpoint + 0;

	usb_set_idle(dev, dev->config[0].interface[ifnum].altsetting[0].bInterfaceNumber, 0, 0);

	FILL_INT_URB(&wacom->irq, dev, usb_rcvintpipe(dev,
			endpoint->bEndpointAddress), wacom->data, wacom->features->pktlen,
			wacom->features->irq, wacom, endpoint->bInterval);

	input_register_device(&wacom->dev);

	wacom_reset(wacom);

	printk(KERN_INFO __FILE__ ": input%d: %s on usb%d:%d.%d\n",
			wacom->dev.number, wacom->features->name, dev->bus->busnum,
			dev->devnum, ifnum);

	return wacom;
}

static void wacom_disconnect(struct usb_device *dev, void *ptr)
{
	struct wacom *wacom = ptr;

	usb_unlink_urb(&wacom->irq);
	input_unregister_device(&wacom->dev);
	kfree(wacom);
}

static struct usb_driver wacom_driver = {
	name:		"wacom",
	probe:		wacom_probe,
	disconnect:	wacom_disconnect,
	id_table:	wacom_ids,
};

static int __init wacom_init(void)
{
	usb_register(&wacom_driver);
	info(DRIVER_VERSION " " DRIVER_AUTHOR);
	info(DRIVER_DESC);
	return 0;
}

static void __exit wacom_exit(void)
{
	usb_deregister(&wacom_driver);
}

module_init(wacom_init);
module_exit(wacom_exit);
