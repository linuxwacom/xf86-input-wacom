/*
 * wacomxi.h -- Add X11 extended input handling capability for wacomcpl.
 *
 * Author		: Ping Cheng
 * Creation date	: 04/05/2003
 *
 */

/*
 *  Based on xi.h 1998-99 Patrick Lecoanet --
 *
 * This code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this code; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef _WacomXi_H
#define _WacomXi_H

#include <X11/Xlib.h>
#include <tk.h>


int Tk_CreateXiEventHandler(Tk_Window, Tk_Uid, Tk_Uid, Tk_EventProc *,
			    ClientData);
void Tk_DeleteXiEventHandler(Tk_Window, Tk_Uid, Tk_Uid, Tk_EventProc *,
			     ClientData);
int Tk_DispatchXiEvent(XEvent *);
int WacomXi_Init(Tcl_Interp *interp);

#endif
