/*
 * Copyright 1995-2003 by Frederic Lepied, France. <Lepied@XFree86.org>
 * Copyright 2002-2007 by Ping Cheng, Wacom. <pingc@wacom.com> 
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

#ifndef __XF86_WCMFILTER_H
#define __XF86_WCMFILTER_H

#include "xf86Wacom.h"

/****************************************************************************/

void wcmSetPressureCurve(WacomDevicePtr pDev, int x0, int y0,
	int x1, int y1);
int wcmFilterIntuos(WacomCommonPtr common, WacomChannelPtr pChannel,
	WacomDeviceStatePtr ds);
int wcmFilterCoord(WacomCommonPtr common, WacomChannelPtr pChannel,
	WacomDeviceStatePtr ds);

/****************************************************************************/
#endif /* __XF86_WCMFILTER_H */
