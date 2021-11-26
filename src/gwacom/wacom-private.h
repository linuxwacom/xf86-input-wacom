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

#include "wacom-driver.h"
#include "wacom-device.h"


WacomDriver *wacom_device_get_driver(WacomDevice *device);
void *wacom_device_get_impl(WacomDevice *device);

void wacom_driver_add_device(WacomDriver *driver, WacomDevice *device);
void wacom_driver_remove_device(WacomDriver *driver, WacomDevice *device);
