/*
 * drivers/usb/input/wacom_sys.c
 *
 *  USB Wacom Graphire and Wacom Intuos tablet support - system specific code
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "wacom.h"
#include "wacom_wac.h"

/* defines to get HID report descriptor */
#define HID_DEVICET_HID		(USB_TYPE_CLASS | 0x01)
#define HID_DEVICET_REPORT	(USB_TYPE_CLASS | 0x02)
#define HID_USAGE_PAGE			0x05
#define HID_USAGE_PAGE_DIGITIZER	0x0d
#define HID_USAGE_PAGE_DESKTOP		0x01
#define HID_USAGE			0x09
#define HID_USAGE_X			0x30
#define HID_USAGE_Y			0x31
#define HID_USAGE_X_TILT		0x3d
#define HID_USAGE_Y_TILT		0x3e
#define HID_USAGE_FINGER		0x22
#define HID_USAGE_STYLUS		0x20
#define HID_COLLECTION			0xc0

enum {
	WCM_UNDEFINED = 0,
	WCM_DESKTOP,
	WCM_DIGITIZER,
	MAX_USAGE_PAGE
};

struct hid_descriptor
{
	struct usb_descriptor_header header;
	__le16   bcdHID;
	u8       bCountryCode;
	u8       bNumDescriptors;
	u8       bDescriptorType;
	__le16   wDescriptorLength;
} __attribute__ ((packed));

/* defines to get/set USB message */
#define USB_REQ_GET_REPORT	0x01
#define USB_REQ_SET_REPORT	0x09

static int usb_get_report(struct usb_interface *intf, unsigned char type,
				unsigned char id, void *buf, int size)
{
	return usb_control_msg(interface_to_usbdev(intf),
		usb_rcvctrlpipe(interface_to_usbdev(intf), 0),
		USB_REQ_GET_REPORT, USB_TYPE_CLASS | USB_RECIP_INTERFACE,
		(type << 8) + id, intf->altsetting[0].desc.bInterfaceNumber,
		buf, size, 100);
}

static int usb_set_report(struct usb_interface *intf, unsigned char type,
				unsigned char id, void *buf, int size)
{
	return usb_control_msg(interface_to_usbdev(intf),
		usb_sndctrlpipe(interface_to_usbdev(intf), 0),
                USB_REQ_SET_REPORT, USB_TYPE_CLASS | USB_RECIP_INTERFACE,
                (type << 8) + id, intf->altsetting[0].desc.bInterfaceNumber,
		buf, size, 1000);
}

static struct input_dev * get_input_dev(struct wacom_combo *wcombo)
{
	return wcombo->wacom->dev;
}

static void wacom_sys_irq(struct urb *urb)
{
	struct wacom *wacom = urb->context;
	struct wacom_combo wcombo;
	int retval;

	switch (urb->status) {
	case 0:
		/* success */
		break;
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		/* this urb is terminated, clean up */
		dbg("%s - urb shutting down with status: %d", __FUNCTION__, urb->status);
		return;
	default:
		dbg("%s - nonzero urb status received: %d", __FUNCTION__, urb->status);
		goto exit;
	}

	wcombo.wacom = wacom;
	wcombo.urb = urb;

	if (wacom_wac_irq(wacom->wacom_wac, (void *)&wcombo))
		input_sync(get_input_dev(&wcombo));

 exit:
	retval = usb_submit_urb (urb, GFP_ATOMIC);
	if (retval)
		err ("%s - usb_submit_urb failed with result %d",
		     __FUNCTION__, retval);
}

void wacom_report_key(void *wcombo, unsigned int key_type, int key_data)
{
	input_report_key(get_input_dev((struct wacom_combo *)wcombo), key_type, key_data);
	return;
}

void wacom_report_abs(void *wcombo, unsigned int abs_type, int abs_data)
{
	input_report_abs(get_input_dev((struct wacom_combo *)wcombo), abs_type, abs_data);
	return;
}

void wacom_report_rel(void *wcombo, unsigned int rel_type, int rel_data)
{
	input_report_rel(get_input_dev((struct wacom_combo *)wcombo), rel_type, rel_data);
	return;
}

void wacom_input_event(void *wcombo, unsigned int type, unsigned int code, int value)
{
	input_event(get_input_dev((struct wacom_combo *)wcombo), type, code, value);
	return;
}

__u16 wacom_be16_to_cpu(unsigned char *data)
{
	__u16 value;
	value = be16_to_cpu(*(__be16 *) data);
	return value;
}

__u16 wacom_le16_to_cpu(unsigned char *data)
{
	__u16 value;
	value = le16_to_cpu(*(__le16 *) data);
	return value;
}

void wacom_input_sync(void *wcombo)
{
	input_sync(get_input_dev((struct wacom_combo *)wcombo));
	return;
}

static int wacom_open(struct input_dev *dev)
{
	struct wacom *wacom = dev->private;

	wacom->irq->dev = wacom->usbdev;
	if (usb_submit_urb(wacom->irq, GFP_KERNEL))
		return -EIO;

	return 0;
}

static void wacom_close(struct input_dev *dev)
{
	struct wacom *wacom = dev->private;

	usb_kill_urb(wacom->irq);
}

void input_dev_mo(struct input_dev *input_dev, struct wacom_wac *wacom_wac)
{
	input_dev->keybit[LONG(BTN_LEFT)] |= BIT(BTN_1) | BIT(BTN_5);
	input_set_abs_params(input_dev, ABS_WHEEL, 0, 71, 0, 0);
}

void input_dev_g4(struct input_dev *input_dev, struct wacom_wac *wacom_wac)
{
	input_dev->evbit[0] |= BIT(EV_MSC);
	input_dev->mscbit[0] |= BIT(MSC_SERIAL);
	input_dev->keybit[LONG(BTN_DIGI)] |= BIT(BTN_TOOL_FINGER);
	input_dev->keybit[LONG(BTN_LEFT)] |= BIT(BTN_0) | BIT(BTN_4);
}

void input_dev_g(struct input_dev *input_dev, struct wacom_wac *wacom_wac)
{
	input_dev->evbit[0] |= BIT(EV_REL);
	input_dev->relbit[0] |= BIT(REL_WHEEL);
	input_dev->keybit[LONG(BTN_LEFT)] |= BIT(BTN_LEFT) | BIT(BTN_RIGHT) | BIT(BTN_MIDDLE);
	input_dev->keybit[LONG(BTN_DIGI)] |= BIT(BTN_TOOL_RUBBER) | BIT(BTN_TOOL_MOUSE) | BIT(BTN_STYLUS2);
	input_set_abs_params(input_dev, ABS_DISTANCE, 0, wacom_wac->features->distance_max, 0, 0);
}

void input_dev_i3s(struct input_dev *input_dev, struct wacom_wac *wacom_wac)
{
	input_dev->keybit[LONG(BTN_DIGI)] |= BIT(BTN_TOOL_FINGER);
	input_dev->keybit[LONG(BTN_LEFT)] |= BIT(BTN_0) | BIT(BTN_1) | BIT(BTN_2) | BIT(BTN_3);
	input_set_abs_params(input_dev, ABS_RX, 0, 4096, 0, 0);
	input_set_abs_params(input_dev, ABS_Z, -900, 899, 0, 0);
}

void input_dev_i3(struct input_dev *input_dev, struct wacom_wac *wacom_wac)
{
	input_dev->keybit[LONG(BTN_LEFT)] |= BIT(BTN_4) | BIT(BTN_5) | BIT(BTN_6) | BIT(BTN_7);
	input_set_abs_params(input_dev, ABS_RY, 0, 4096, 0, 0);
}

void input_dev_bee(struct input_dev *input_dev, struct wacom_wac *wacom_wac)
{
	input_dev->keybit[LONG(BTN_LEFT)] |= BIT(BTN_8) | BIT(BTN_9);
}

void input_dev_i(struct input_dev *input_dev, struct wacom_wac *wacom_wac)
{
	input_dev->evbit[0] |= BIT(EV_MSC) | BIT(EV_REL);
	input_dev->mscbit[0] |= BIT(MSC_SERIAL);
	input_dev->relbit[0] |= BIT(REL_WHEEL);
	input_dev->keybit[LONG(BTN_LEFT)] |= BIT(BTN_LEFT) | BIT(BTN_RIGHT) | BIT(BTN_MIDDLE) | BIT(BTN_SIDE) | BIT(BTN_EXTRA);
	input_dev->keybit[LONG(BTN_DIGI)] |= BIT(BTN_TOOL_RUBBER) | BIT(BTN_TOOL_MOUSE)	| BIT(BTN_TOOL_BRUSH)
		| BIT(BTN_TOOL_PENCIL) | BIT(BTN_TOOL_AIRBRUSH) | BIT(BTN_TOOL_LENS) | BIT(BTN_STYLUS2);
	input_set_abs_params(input_dev, ABS_DISTANCE, 0, wacom_wac->features->distance_max, 0, 0);
	input_set_abs_params(input_dev, ABS_WHEEL, 0, 1023, 0, 0);
	input_set_abs_params(input_dev, ABS_TILT_X, 0, 127, 0, 0);
	input_set_abs_params(input_dev, ABS_TILT_Y, 0, 127, 0, 0);
	input_set_abs_params(input_dev, ABS_RZ, -900, 899, 0, 0);
	input_set_abs_params(input_dev, ABS_THROTTLE, -1023, 1023, 0, 0);
}

void input_dev_pl(struct input_dev *input_dev, struct wacom_wac *wacom_wac)
{
	input_dev->keybit[LONG(BTN_DIGI)] |= BIT(BTN_STYLUS2);
}

void input_dev_pt(struct input_dev *input_dev, struct wacom_wac *wacom_wac)
{
	input_dev->keybit[LONG(BTN_DIGI)] |= BIT(BTN_TOOL_RUBBER);
}

static void wacom_paser_hid(struct usb_interface *intf, struct hid_descriptor *hid_desc, 
		struct wacom_wac *wacom_wac, char *report)
{
	struct usb_device *dev = interface_to_usbdev(intf);
	char limit = 0, result = 0;
	int i = 0, usage = WCM_UNDEFINED, finger = 0, pen = 0;

	/* retrive report descriptors */
	do {
		result = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
			USB_REQ_GET_DESCRIPTOR,
			USB_RECIP_INTERFACE | USB_DIR_IN,
			HID_DEVICET_REPORT << 8,
			intf->altsetting[0].desc.bInterfaceNumber, /* interface */
			report,
			hid_desc->wDescriptorLength,
			5000); /* 5 secs */
	} while (limit++ < 5);

	for (i=0; i<hid_desc->wDescriptorLength; i++) {
		if (report[i] == HID_USAGE_PAGE) {
			switch (report[i+1]) {
			    case HID_USAGE_PAGE_DIGITIZER:
				usage = WCM_DIGITIZER;
				i++;
				continue;
			    case HID_USAGE_PAGE_DESKTOP:
				usage = WCM_DESKTOP;
				i++;
				continue;
			}
		}

		if ((unsigned short)report[i] == HID_USAGE) {
			switch ((unsigned short)report[i+1]) {
			    case HID_USAGE_X:
				if (usage == WCM_DESKTOP) {
					if (finger) {
						wacom_wac->features->touch_x_max = 
						wacom_wac->features->touch_y_max = (unsigned short)
							(wacom_le16_to_cpu(&report[i+3]));
						wacom_wac->features->x_max = (unsigned short)
							(wacom_le16_to_cpu(&report[i+6]));
					} else if (pen) {
						wacom_wac->features->x_max = (unsigned short)
							(wacom_le16_to_cpu(&report[i+3]));
					}
				} else if (usage == WCM_DIGITIZER) {
					/* max pressure isn't reported 
					wacom_wac->features->pressure_max = (unsigned short)
							(report[i+4] << 8  | report[i+3]);
					*/
					wacom_wac->features->pressure_max = 255;
				}
				i += 3;
				break;
			    case HID_USAGE_Y:
				if (usage == WCM_DESKTOP) {
					wacom_wac->features->y_max = (unsigned short)
						(wacom_le16_to_cpu(&report[i+3]));
				}
				i += 4;
				break;
			    case HID_USAGE_FINGER:
				finger = 1;
				i++;
				break;
			    case HID_USAGE_STYLUS:
				pen = 1;
				i++;
				break;
			}
		}

		if ((unsigned short)report[i] == HID_COLLECTION) {
			/* capacity */
			if (finger && !report[i+1]) {
				wacom_wac->features->pressure_max = (unsigned short)report[i+4];
				i= hid_desc->wDescriptorLength;
			} else {
				finger = usage = 0;
			}
		}
	}
}

static int wacom_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usb_device *dev = interface_to_usbdev(intf);
	struct usb_host_interface *interface = intf->cur_altsetting;
	struct usb_endpoint_descriptor *endpoint;
	struct wacom *wacom;
	struct wacom_wac *wacom_wac;
	struct input_dev *input_dev;
	char limit = 0, rep_data[2], mode = 2, *report = NULL;
	struct hid_descriptor *hid_desc;

	wacom = kzalloc(sizeof(struct wacom), GFP_KERNEL);
	wacom_wac = kzalloc(sizeof(struct wacom_wac), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!wacom || !input_dev || !wacom_wac)
		goto fail1;

	wacom_wac->data = usb_buffer_alloc(dev, 10, GFP_KERNEL, &wacom->data_dma);
	if (!wacom_wac->data)
		goto fail1;

	wacom->irq = usb_alloc_urb(0, GFP_KERNEL);
	if (!wacom->irq)
		goto fail2;

	wacom->usbdev = dev;
	wacom->dev = input_dev;
	usb_make_path(dev, wacom->phys, sizeof(wacom->phys));
	strlcat(wacom->phys, "/input0", sizeof(wacom->phys));

	wacom_wac->features = get_wacom_feature(id);
	BUG_ON(wacom_wac->features->pktlen > 10);

	input_dev->name = wacom_wac->features->name;
	wacom->wacom_wac = wacom_wac;
	usb_to_input_id(dev, &input_dev->id);

	input_dev->cdev.dev = &intf->dev;
	input_dev->private = wacom;
	input_dev->open = wacom_open;
	input_dev->close = wacom_close;

	endpoint = &intf->cur_altsetting->endpoint[0].desc;

	/* TabletPC need to retrieve the physical and logical maximum from report descriptor */
	if (wacom_wac->features->type == TABLETPC) {
		if (usb_get_extra_descriptor(interface, HID_DEVICET_HID, &hid_desc)) {
	    		if (usb_get_extra_descriptor(&interface->endpoint[0], 
				HID_DEVICET_REPORT, &hid_desc)) {
				printk("wacom: can not retrive extra class descriptor\n");
				goto fail2;
			}
		}
		report = kzalloc(hid_desc->wDescriptorLength, GFP_KERNEL);
		if (!report) {
			goto fail2;
		}
		wacom_paser_hid(intf, hid_desc, wacom_wac, report);
	}

	input_dev->evbit[0] |= BIT(EV_KEY) | BIT(EV_ABS);
	input_dev->keybit[LONG(BTN_DIGI)] |= BIT(BTN_TOOL_PEN) | BIT(BTN_TOUCH) | BIT(BTN_STYLUS);
	input_set_abs_params(input_dev, ABS_X, 0, wacom_wac->features->x_max, 4, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, wacom_wac->features->y_max, 4, 0);
	input_set_abs_params(input_dev, ABS_PRESSURE, 0, wacom_wac->features->pressure_max, 0, 0);
	if (wacom_wac->features->type == TABLETPC) {
		input_set_abs_params(input_dev, ABS_RX, 0, wacom_wac->features->touch_x_max, 4, 0);
		input_set_abs_params(input_dev, ABS_RY, 0, wacom_wac->features->touch_y_max, 4, 0);
	}
	input_dev->absbit[LONG(ABS_MISC)] |= BIT(ABS_MISC);

	wacom_init_input_dev(input_dev, wacom_wac);

	usb_fill_int_urb(wacom->irq, dev,
			 usb_rcvintpipe(dev, endpoint->bEndpointAddress),
			 wacom_wac->data, wacom_wac->features->pktlen,
			 wacom_sys_irq, wacom, endpoint->bInterval);
	wacom->irq->transfer_dma = wacom->data_dma;
	wacom->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	input_register_device(wacom->dev);

	/* TabletPC second bit 0 is for stylus mode*/
	if (wacom_wac->features->type == TABLETPC)
		mode = 0;

	/* Ask the tablet to report tablet data. Repeat until it succeeds */
	do {
		rep_data[0] = 2;
		rep_data[1] = mode;
		/* TabletPC doesn't need set report call */
		if (wacom_wac->features->type != TABLETPC)
			usb_set_report(intf, USB_DT_STRING, 2, rep_data, 2);
		usb_get_report(intf, USB_DT_STRING, 2, rep_data, 2);
	} while (rep_data[1] != mode && limit++ < 5);

	usb_set_intfdata(intf, wacom);
	kfree(report);
	return 0;

fail2:	usb_buffer_free(dev, 10, wacom_wac->data, wacom->data_dma);
fail1:	input_free_device(input_dev);
	kfree(wacom);
	kfree(wacom_wac);
	return -ENOMEM;
}

static void wacom_disconnect(struct usb_interface *intf)
{
	struct wacom *wacom = usb_get_intfdata (intf);

	usb_set_intfdata(intf, NULL);
	if (wacom) {
		usb_kill_urb(wacom->irq);
		input_unregister_device(wacom->dev);
		usb_free_urb(wacom->irq);
		usb_buffer_free(interface_to_usbdev(intf), 10, wacom->wacom_wac->data, wacom->data_dma);
		kfree(wacom->wacom_wac);
		kfree(wacom);
	}
}

static struct usb_driver wacom_driver = {
	.name =		"wacom",
	.probe =	wacom_probe,
	.disconnect =	wacom_disconnect,
};

static int __init wacom_init(void)
{
	int result;
	wacom_driver.id_table = get_device_table();
	result = usb_register(&wacom_driver);
	if (result == 0)
		info(DRIVER_VERSION ":" DRIVER_DESC);
	return result;
}

static void __exit wacom_exit(void)
{
	usb_deregister(&wacom_driver);
}

module_init(wacom_init);
module_exit(wacom_exit);
