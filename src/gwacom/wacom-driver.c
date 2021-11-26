/*
 * Copyright 2021 Red Hat, Inc.
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

#include "config.h"

#include "wacom-driver.h"
#include "wacom-private.h"

#include "xf86Wacom.h"

struct _WacomDriver {
	GObject parent_instance;

	GList *devices;
};

enum {
	SIGNAL_DEVICE_ADDED,
	SIGNAL_DEVICE_REMOVED,
	LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (WacomDriver, wacom_driver, G_TYPE_OBJECT)

int wcmForeachDevice(WacomDevicePtr priv, WacomDeviceCallback func, void *data)
{
	WacomDevice *device = priv->frontend;
	WacomDriver *driver = wacom_device_get_driver(device);
	GList *elem = driver->devices;
	int nmatch = 0;

	while (elem)
	{
		WacomDevice *d = elem->data;
		int rc = func(wacom_device_get_impl(d), data);
		elem = elem->next;
		if (rc == -ENODEV)
			continue;

		if (rc < 0)
			return -rc;
		nmatch += 1;
		if (rc == 0)
			break;
	}

	return nmatch;
}

WacomDriver*
wacom_driver_new(void)
{
	return g_object_new(WACOM_TYPE_DRIVER, NULL);
}

GList *
wacom_driver_get_devices(WacomDriver *driver)
{
	return g_list_copy(driver->devices);
}

void
wacom_driver_add_device(WacomDriver *driver, WacomDevice *device)
{
	driver->devices = g_list_append(driver->devices, g_object_ref(device));
	g_signal_emit(driver, signals[SIGNAL_DEVICE_ADDED], 0, device);
}

void
wacom_driver_remove_device(WacomDriver *driver, WacomDevice *device)
{
	if (g_list_find(driver->devices, device)) {
		driver->devices = g_list_remove(driver->devices, device);
		g_signal_emit(driver, signals[SIGNAL_DEVICE_REMOVED], 0, device);
		g_object_unref(device);
	}
}

static void
wacom_driver_dispose(GObject *gobject)
{
	G_OBJECT_CLASS (wacom_driver_parent_class)->dispose (gobject);
}

static void
wacom_driver_finalize(GObject *gobject)
{
	G_OBJECT_CLASS (wacom_driver_parent_class)->finalize (gobject);
}


static void
wacom_driver_class_init(WacomDriverClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->dispose = wacom_driver_dispose;
	object_class->finalize = wacom_driver_finalize;

	/**
	 * WacomDriver::device-added:
	 * @driver: the driver instance
	 * @device: the device that was added
	 *
	 * The device-added signal is emitted whenever a device is added. A
	 * caller should connect to the device's signals as required and
	 * initialize and enable the device.
	 */
	signals[SIGNAL_DEVICE_ADDED] =
		g_signal_new("device-added",
			     G_TYPE_FROM_CLASS(klass),
			     G_SIGNAL_RUN_FIRST,
			     0, NULL, NULL, NULL, G_TYPE_NONE,
			     1, WACOM_TYPE_DEVICE);

	/**
	 * WacomDriver::device-removed:
	 * @driver: the driver instance
	 * @device: the device that was removed
	 */
	signals[SIGNAL_DEVICE_REMOVED] =
		g_signal_new("device-removed",
			     G_TYPE_FROM_CLASS(klass),
			     G_SIGNAL_RUN_FIRST,
			     0, NULL, NULL, NULL, G_TYPE_NONE,
			     1, WACOM_TYPE_DEVICE);
}

static void
wacom_driver_init(WacomDriver *self)
{
}

