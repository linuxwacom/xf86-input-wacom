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

#pragma once

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define WACOM_TYPE_DRIVER (wacom_driver_get_type())
G_DECLARE_FINAL_TYPE (WacomDriver, wacom_driver, WACOM, DRIVER, GObject)


/**
 * wacom_driver_new:
 *
 * Use g_object_unref() to release the driver object
 *
 * Returns: (transfer full): a new WacomDriver object
 */
WacomDriver *wacom_driver_new(void);

/**
 * wacom_driver_get_devices:
 *
 * Returns: (element-type WacomDevice) (transfer container):
 */
GList *wacom_driver_get_devices(WacomDriver *driver);

G_END_DECLS


