/*
	crc.h

	CRC (MD4) prototypes

	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/

#ifndef __crc_h
#define __crc_h

/** \defgroup crc Checksum generation.
	\ingroup utils
*/
//@{

#include "QF/qtypes.h"

void CRC_Init(unsigned short *crcvalue);
void CRC_ProcessByte(unsigned short *crcvalue, byte data);
void CRC_ProcessBlock (const byte *start, unsigned short *crcvalue, int count);
unsigned short CRC_Value(unsigned short crcvalue) __attribute__((const));
unsigned short CRC_Block (const byte *start, int count) __attribute__((pure));

//@}

#endif // __crc_h
