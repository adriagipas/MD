/*
 * Copyright 2012-2022 Adrià Giménez Pastor.
 *
 * This file is part of adriagipas/MD.
 *
 * adriagipas/MD is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * adriagipas/MD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with adriagipas/MD.  If not, see <https://www.gnu.org/licenses/>.
 */
/*
 *  unpack.h - Per a decodificar instruccions.
 *
 */

#ifndef __UNPACK_H__
#define __UNPACK_H__

typedef struct
{
  MDu8 op,v3,v2,v1,v0;
} unpack_t;

extern const unpack_t MD_unpack__[0x10000];

#endif /* __UNPACK_H__ */
