/*****************************************************************************
 * wcm-beta.h
 *
 * Copyright 2002 by John Joganic <john@joganic.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * REVISION HISTORY
 *
 ****************************************************************************/

#ifndef __XF86_WCM_BETA_H
#define __XF86_WCM_BETA_H

#include "xf86Version.h"
#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(3,9,0,0,0)
#error This code will not work on XFree86 version 3.x, yet
#endif

/* FOO **********************************************************************/

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
#endif /* LINUX_INPUT */

/* BAR ***********************************************************************/

#include <xf86Xinput.h>

/***************************************************************************/

InputInfoPtr wcmBetaNewDevice(InputDriverPtr pDriver, IDevPtr pConfig,
		int nFlags);
void wcmBetaDeleteDevice(InputDriverPtr pDriver, LocalDevicePtr pDevice,
		int nFlags);

/***************************************************************************/
#endif /* __XF86_WCM_BETA_H */
