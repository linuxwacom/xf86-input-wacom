/*****************************************************************************
** wacserial.h
**
** Copyright (C) 2002,2003 - John E. Joganic
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Lesser General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
**
****************************************************************************/

#ifndef __LINUXWACOM_WACSERIAL_H
#define __LINUXWACOM_WACSERIAL_H

#include "wactablet.h"

int WacomGetSupportedSerialDeviceList(WACOMDEVICEREC** ppList, int* pnSize);
unsigned int WacomGetSerialDeviceFromName(const char* pszName);
WACOMTABLET WacomOpenSerialTablet(int fd, WACOMMODEL* pModel);

#endif /* __LINUXWACOM_WACSERIAL_H */
