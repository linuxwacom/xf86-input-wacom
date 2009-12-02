/*
 * Copyright 1995-2004 by Frederic Lepied, France. <Lepied@XFree86.org>
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


int xf86WcmReady(LocalDevicePtr local)
{
#ifdef DEBUG
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
#endif
	int n = xf86WaitForInput(local->fd, 0);
	DBG(10, priv->debugLevel, ErrorF("xf86WcmReady for %s with %d numbers of data\n", local->name, n));

	if (n >= 0) return n ? 1 : 0;
	xf86Msg(X_ERROR, "%s: select error: %s\n", local->name, strerror(errno));
	return 0;
}
/* vim: set noexpandtab shiftwidth=8: */
