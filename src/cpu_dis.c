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
 *  cpu_dis.c - Implementació de les funcions per a decodificar el
 *              codi.
 *
 */


#include <stddef.h>
#include <stdlib.h>

#include "MD.h"
#include "unpack.h"




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

/* ADRECES ********************************************************************/
static MDu32
pd16an (
        MDu32                addr,
        MD_Inst * const      inst,
        MDu8 const           reg,
        MD_InstExtra * const extra
        )
{
  
  MD_Word word;
  
  
  word= MD_mem_read ( addr );
  inst->bytes[inst->nbytes++]= word.b.v1;
  inst->bytes[inst->nbytes++]= word.b.v0;
  extra->d16an.reg= reg;
  extra->d16an.dis= (MDs16) word.v;
  addr+= 2;
  
  return addr;
  
} /* end pd16an */


static MDu32
pw (
    MDu32                addr,
    MD_Inst * const      inst,
    MD_InstExtra * const extra
    )
{
  
  MD_Word word;
  
  
  word= MD_mem_read ( addr );
  inst->bytes[inst->nbytes++]= word.b.v1;
  inst->bytes[inst->nbytes++]= word.b.v0;
  extra->word= word.v;
  addr+= 2;
  
  return addr;
  
} /* end pw */


static MDu32
pl (
    MDu32                addr,
    MD_Inst * const      inst,
    MD_InstExtra * const extra
    )
{
  
  MD_Reg32 val;
  
  
  val.w.v1= MD_mem_read ( addr );
  inst->bytes[inst->nbytes++]= val.w.v1.b.v1;
  inst->bytes[inst->nbytes++]= val.w.v1.b.v0;
  addr+= 2;
  val.w.v0= MD_mem_read ( addr );
  inst->bytes[inst->nbytes++]= val.w.v0.b.v1;
  inst->bytes[inst->nbytes++]= val.w.v0.b.v0;
  addr+= 2;
  extra->longval= val.v;
  
  return addr;
  
} /* end pl */


static MDu32
pd8anxn (
         MDu32                addr,
         MD_Inst * const      inst,
         MDu8 const           reg,
         MD_InstExtra * const extra
         )
{
  
  MD_Word ew;
  
  
  ew= MD_mem_read ( addr );
  inst->bytes[inst->nbytes++]= ew.b.v1;
  inst->bytes[inst->nbytes++]= ew.b.v0;
  addr+= 2;
  extra->d8anxn.areg= reg;
  switch ( ew.b.v1>>3 ) /* D/A Reg.Num. W/L */
    {
    case 0x00:
      extra->d8anxn.xreg= 0;
      extra->d8anxn.isd= MD_TRUE;extra->d8anxn.isw= MD_TRUE; break;
    case 0x01:
      extra->d8anxn.xreg= 0;
      extra->d8anxn.isd= MD_TRUE;extra->d8anxn.isw= MD_FALSE; break;
    case 0x02:
      extra->d8anxn.xreg= 1;
      extra->d8anxn.isd= MD_TRUE;extra->d8anxn.isw= MD_TRUE; break;
    case 0x03:
      extra->d8anxn.xreg= 1;
      extra->d8anxn.isd= MD_TRUE;extra->d8anxn.isw= MD_FALSE; break;
    case 0x04:
      extra->d8anxn.xreg= 2;
      extra->d8anxn.isd= MD_TRUE;extra->d8anxn.isw= MD_TRUE; break;
    case 0x05:
      extra->d8anxn.xreg= 2;
      extra->d8anxn.isd= MD_TRUE;extra->d8anxn.isw= MD_FALSE; break;
    case 0x06:
      extra->d8anxn.xreg= 3;
      extra->d8anxn.isd= MD_TRUE;extra->d8anxn.isw= MD_TRUE; break;
    case 0x07:
      extra->d8anxn.xreg= 3;
      extra->d8anxn.isd= MD_TRUE;extra->d8anxn.isw= MD_FALSE; break;
    case 0x08:
      extra->d8anxn.xreg= 4;
      extra->d8anxn.isd= MD_TRUE;extra->d8anxn.isw= MD_TRUE; break;
    case 0x09:
      extra->d8anxn.xreg= 4;
      extra->d8anxn.isd= MD_TRUE;extra->d8anxn.isw= MD_FALSE; break;
    case 0x0A:
      extra->d8anxn.xreg= 5;
      extra->d8anxn.isd= MD_TRUE;extra->d8anxn.isw= MD_TRUE; break;
    case 0x0B:
      extra->d8anxn.xreg= 5;
      extra->d8anxn.isd= MD_TRUE;extra->d8anxn.isw= MD_FALSE; break;
    case 0x0C:
      extra->d8anxn.xreg= 6;
      extra->d8anxn.isd= MD_TRUE;extra->d8anxn.isw= MD_TRUE; break;
    case 0x0D:
      extra->d8anxn.xreg= 6;
      extra->d8anxn.isd= MD_TRUE;extra->d8anxn.isw= MD_FALSE; break;
    case 0x0E:
      extra->d8anxn.xreg= 7;
      extra->d8anxn.isd= MD_TRUE;extra->d8anxn.isw= MD_TRUE; break;
    case 0x0F:
      extra->d8anxn.xreg= 7;
      extra->d8anxn.isd= MD_TRUE;extra->d8anxn.isw= MD_FALSE; break;
    case 0x10:
      extra->d8anxn.xreg= 0;
      extra->d8anxn.isd= MD_FALSE;extra->d8anxn.isw= MD_TRUE; break;
    case 0x11:
      extra->d8anxn.xreg= 0;
      extra->d8anxn.isd= MD_FALSE;extra->d8anxn.isw= MD_FALSE; break;
    case 0x12:
      extra->d8anxn.xreg= 1;
      extra->d8anxn.isd= MD_FALSE;extra->d8anxn.isw= MD_TRUE; break;
    case 0x13:
      extra->d8anxn.xreg= 1;
      extra->d8anxn.isd= MD_FALSE;extra->d8anxn.isw= MD_FALSE; break;
    case 0x14:
      extra->d8anxn.xreg= 2;
      extra->d8anxn.isd= MD_FALSE;extra->d8anxn.isw= MD_TRUE; break;
    case 0x15:
      extra->d8anxn.xreg= 2;
      extra->d8anxn.isd= MD_FALSE;extra->d8anxn.isw= MD_FALSE; break;
    case 0x16:
      extra->d8anxn.xreg= 3;
      extra->d8anxn.isd= MD_FALSE;extra->d8anxn.isw= MD_TRUE; break;
    case 0x17:
      extra->d8anxn.xreg= 3;
      extra->d8anxn.isd= MD_FALSE;extra->d8anxn.isw= MD_FALSE; break;
    case 0x18:
      extra->d8anxn.xreg= 4;
      extra->d8anxn.isd= MD_FALSE;extra->d8anxn.isw= MD_TRUE; break;
    case 0x19:
      extra->d8anxn.xreg= 4;
      extra->d8anxn.isd= MD_FALSE;extra->d8anxn.isw= MD_FALSE; break;
    case 0x1A:
      extra->d8anxn.xreg= 5;
      extra->d8anxn.isd= MD_FALSE;extra->d8anxn.isw= MD_TRUE; break;
    case 0x1B:
      extra->d8anxn.xreg= 5;
      extra->d8anxn.isd= MD_FALSE;extra->d8anxn.isw= MD_FALSE; break;
    case 0x1C:
      extra->d8anxn.xreg= 6;
      extra->d8anxn.isd= MD_FALSE;extra->d8anxn.isw= MD_TRUE; break;
    case 0x1D:
      extra->d8anxn.xreg= 6;
      extra->d8anxn.isd= MD_FALSE;extra->d8anxn.isw= MD_FALSE; break;
    case 0x1E:
      extra->d8anxn.xreg= 7;
      extra->d8anxn.isd= MD_FALSE;extra->d8anxn.isw= MD_TRUE; break;
    case 0x1F:
      extra->d8anxn.xreg= 7;
      extra->d8anxn.isd= MD_FALSE;extra->d8anxn.isw= MD_FALSE; break;
    default: break;
    }
  /*extra->d8anxn.scale= 1<<((ew.b.v1>>1)&0x3);*/
  extra->d8anxn.dis= (MDs8) ew.b.v0;
  
  return addr;
  
} /* end pd8anxn */


static MDu32
dis16 (
       MDu32                addr,
       MD_Inst * const      inst,
       MD_InstExtra * const extra
       )
{
  
  MD_Word word;
  
  
  word= MD_mem_read ( addr );
  inst->bytes[inst->nbytes++]= word.b.v1;
  inst->bytes[inst->nbytes++]= word.b.v0;
  extra->dis16= (MDs16) word.v;
  addr+= 2;
  
  return addr;
  
} /* end dis16 */


/* I/O ************************************************************************/
static MDu32
inm_byte (
          MDu32                addr,
          MD_Inst * const      inst,
          MD_InstExtra * const extra
          )
{
  
  MD_Word word;
  
  
  word= MD_mem_read ( addr );
  addr+= 2;
  inst->bytes[inst->nbytes++]= word.b.v1;
  inst->bytes[inst->nbytes++]= word.b.v0;
  extra->byte= word.b.v0;
  
  return addr;
  
} /* end inm_byte */


static MDu32
inm_word (
          MDu32                addr,
          MD_Inst * const      inst,
          MD_InstExtra * const extra
          )
{
  
  MD_Word word;
  
  
  word= MD_mem_read ( addr );
  addr+= 2;
  inst->bytes[inst->nbytes++]= word.b.v1;
  inst->bytes[inst->nbytes++]= word.b.v0;
  extra->word= word.v;
  
  return addr;
  
} /* end inm_word */


static MDu32
inm_long (
          MDu32                addr,
          MD_Inst * const      inst,
          MD_InstExtra * const extra
          )
{
  
  MD_Reg32 long_;
  
  
  long_.w.v1= MD_mem_read ( addr );
  inst->bytes[inst->nbytes++]= long_.w.v1.b.v1;
  inst->bytes[inst->nbytes++]= long_.w.v1.b.v0;
  addr+= 2;
  long_.w.v0= MD_mem_read ( addr );
  inst->bytes[inst->nbytes++]= long_.w.v0.b.v1;
  inst->bytes[inst->nbytes++]= long_.w.v0.b.v0;
  addr+= 2;
  extra->longval= long_.v;
  
  return addr;
  
} /* end inm_long */


/* Funcions per decodificar ***************************************************/

static int
op1_0234567_01 (
        	MDu32 * const   addr,
        	MD_Inst * const inst,
        	MDu8 const      mode,
        	MDu8 const      reg
        	)
{
  
  switch ( mode )
    {
    case 0: inst->id.op1= MD_DN; inst->e1.reg= reg; return 0;
    case 2: inst->id.op1= MD_pAN; inst->e1.reg= reg; break;
    case 3: inst->id.op1= MD_pANi; inst->e1.reg= reg; break;
    case 4: inst->id.op1= MD_pANd; inst->e1.reg= reg; break;
    case 5:
      inst->id.op1= MD_pD16AN;
      *addr= pd16an ( *addr, inst, reg, &(inst->e1) );
      break;
    case 6:
      inst->id.op1= MD_pD8ANXN;
      *addr= pd8anxn ( *addr, inst, reg, &(inst->e1) );
      break;
    case 7:
      switch ( reg )
        {
        case 0:
          inst->id.op1= MD_pW;
          *addr= pw ( *addr, inst, &(inst->e1) );
          break;
        case 1:
          inst->id.op1= MD_pL;
          *addr= pl ( *addr, inst, &(inst->e1) );
          break;
        default: inst->id.name= MD_UNK; return -1;
        }
      break;
    default: inst->id.name= MD_UNK; return -1;
    }
  
  return 0;
  
} /* end op1_0234567_01 */


static int
op1_2567_0123 (
               MDu32 * const   addr,
               MD_Inst * const inst,
               MDu8 const      mode,
               MDu8 const      reg
               )
{
  
  switch ( mode )
    {
    case 2: inst->id.op1= MD_pAN; inst->e1.reg= reg; break;
    case 5:
      inst->id.op1= MD_pD16AN;
      *addr= pd16an ( *addr, inst, reg, &(inst->e1) );
      break;
    case 6:
      inst->id.op1= MD_pD8ANXN;
      *addr= pd8anxn ( *addr, inst, reg, &(inst->e1) );
      break;
    case 7:
      switch ( reg )
        {
        case 0:
          inst->id.op1= MD_pW;
          *addr= pw ( *addr, inst, &(inst->e1) );
          break;
        case 1:
          inst->id.op1= MD_pL;
          *addr= pl ( *addr, inst, &(inst->e1) );
          break;
        case 2:
          inst->id.op1= MD_pD16PC;
          *addr= pd16an ( *addr, inst, 0xFF, &(inst->e1) );
          break;
        case 3:
          inst->id.op1= MD_pD8PCXN;
          *addr= pd8anxn ( *addr, inst, 0xFF, &(inst->e1) );
          break;
        default: inst->id.name= MD_UNK; return -1;
        }
      break;
    default: inst->id.name= MD_UNK; return -1;
    }
  
  return 0;
  
} /* end op1_2567_0123 */


static int
op1_01234567_01234_w (
        	      MDu32 * const   addr,
        	      MD_Inst * const inst,
        	      MDu8 const      mode,
        	      MDu8 const      reg
        	      )
{
  
  switch ( mode )
    {
    case 0: inst->id.op1= MD_DN; inst->e1.reg= reg; break;
    case 1: inst->id.op1= MD_AN; inst->e1.reg= reg; break;
    case 2: inst->id.op1= MD_pAN; inst->e1.reg= reg; break;
    case 3: inst->id.op1= MD_pANi; inst->e1.reg= reg; break;
    case 4: inst->id.op1= MD_pANd; inst->e1.reg= reg; break;
    case 5:
      inst->id.op1= MD_pD16AN;
      *addr= pd16an ( *addr, inst, reg, &(inst->e1) );
      break;
    case 6:
      inst->id.op1= MD_pD8ANXN;
      *addr= pd8anxn ( *addr, inst, reg, &(inst->e1) );
      break;
    case 7:
      switch ( reg )
        {
        case 0:
          inst->id.op1= MD_pW;
          *addr= pw ( *addr, inst, &(inst->e1) );
          break;
        case 1:
          inst->id.op1= MD_pL;
          *addr= pl ( *addr, inst, &(inst->e1) );
          break;
        case 2:
          inst->id.op1= MD_pD16PC;
          *addr= pd16an ( *addr, inst, 0xFF, &(inst->e1) );
          break;
        case 3:
          inst->id.op1= MD_pD8PCXN;
          *addr= pd8anxn ( *addr, inst, 0xFF, &(inst->e1) );
          break;
        case 4:
          inst->id.op1= MD_INMw;
          *addr= inm_word ( *addr, inst, &(inst->e1) );
          break;
        default: inst->id.name= MD_UNK; return -1;
        }
      break;
    default: break;
    }
  
  return 0;
  
} /* end op1_01234567_01234_w */


static int
op1_01234567_01234_l (
        	      MDu32 * const   addr,
        	      MD_Inst * const inst,
        	      MDu8 const      mode,
        	      MDu8 const      reg
        	      )
{
  
  switch ( mode )
    {
    case 0: inst->id.op1= MD_DN; inst->e1.reg= reg; break;
    case 1: inst->id.op1= MD_AN; inst->e1.reg= reg; break;
    case 2: inst->id.op1= MD_pAN; inst->e1.reg= reg; break;
    case 3: inst->id.op1= MD_pANi; inst->e1.reg= reg; break;
    case 4: inst->id.op1= MD_pANd; inst->e1.reg= reg; break;
    case 5:
      inst->id.op1= MD_pD16AN;
      *addr= pd16an ( *addr, inst, reg, &(inst->e1) );
      break;
    case 6:
      inst->id.op1= MD_pD8ANXN;
      *addr= pd8anxn ( *addr, inst, reg, &(inst->e1) );
      break;
    case 7:
      switch ( reg )
        {
        case 0:
          inst->id.op1= MD_pW;
          *addr= pw ( *addr, inst, &(inst->e1) );
          break;
        case 1:
          inst->id.op1= MD_pL;
          *addr= pl ( *addr, inst, &(inst->e1) );
          break;
        case 2:
          inst->id.op1= MD_pD16PC;
          *addr= pd16an ( *addr, inst, 0xFF, &(inst->e1) );
          break;
        case 3:
          inst->id.op1= MD_pD8PCXN;
          *addr= pd8anxn ( *addr, inst, 0xFF, &(inst->e1) );
          break;
        case 4:
          inst->id.op1= MD_INMl;
          *addr= inm_long ( *addr, inst, &(inst->e1) );
          break;
        default: inst->id.name= MD_UNK; return -1;
        }
      break;
    default: break;
    }
  
  return 0;
  
} /* end op1_01234567_01234_l */


static int
op1_0234567_01234_b (
        	     MDu32 * const   addr,
        	     MD_Inst * const inst,
        	     MDu8 const      mode,
        	     MDu8 const      reg
        	     )
{
  
  switch ( mode )
    {
    case 0: inst->id.op1= MD_DN; inst->e1.reg= reg; break;
    case 2: inst->id.op1= MD_pAN; inst->e1.reg= reg; break;
    case 3: inst->id.op1= MD_pANi; inst->e1.reg= reg; break;
    case 4: inst->id.op1= MD_pANd; inst->e1.reg= reg; break;
    case 5:
      inst->id.op1= MD_pD16AN;
      *addr= pd16an ( *addr, inst, reg, &(inst->e1) );
      break;
    case 6:
      inst->id.op1= MD_pD8ANXN;
      *addr= pd8anxn ( *addr, inst, reg, &(inst->e1) );
      break;
    case 7:
      switch ( reg )
        {
        case 0:
          inst->id.op1= MD_pW;
          *addr= pw ( *addr, inst, &(inst->e1) );
          break;
        case 1:
          inst->id.op1= MD_pL;
          *addr= pl ( *addr, inst, &(inst->e1) );
          break;
        case 2:
          inst->id.op1= MD_pD16PC;
          *addr= pd16an ( *addr, inst, 0xFF, &(inst->e1) );
          break;
        case 3:
          inst->id.op1= MD_pD8PCXN;
          *addr= pd8anxn ( *addr, inst, 0xFF, &(inst->e1) );
          break;
        case 4:
          inst->id.op1= MD_INMb;
          *addr= inm_byte ( *addr, inst, &(inst->e1) );
          break;
        default: inst->id.name= MD_UNK; return -1;
        }
      break;
    default: inst->id.name= MD_UNK; return -1;
    }
  
  return 0;
  
} /* end op1_0234567_01234_b */


static int
op1_0234567_01234_w (
        	     MDu32 * const   addr,
        	     MD_Inst * const inst,
        	     MDu8 const      mode,
        	     MDu8 const      reg
        	     )
{
  
  switch ( mode )
    {
    case 0: inst->id.op1= MD_DN; inst->e1.reg= reg; break;
    case 2: inst->id.op1= MD_pAN; inst->e1.reg= reg; break;
    case 3: inst->id.op1= MD_pANi; inst->e1.reg= reg; break;
    case 4: inst->id.op1= MD_pANd; inst->e1.reg= reg; break;
    case 5:
      inst->id.op1= MD_pD16AN;
      *addr= pd16an ( *addr, inst, reg, &(inst->e1) );
      break;
    case 6:
      inst->id.op1= MD_pD8ANXN;
      *addr= pd8anxn ( *addr, inst, reg, &(inst->e1) );
      break;
    case 7:
      switch ( reg )
        {
        case 0:
          inst->id.op1= MD_pW;
          *addr= pw ( *addr, inst, &(inst->e1) );
          break;
        case 1:
          inst->id.op1= MD_pL;
          *addr= pl ( *addr, inst, &(inst->e1) );
          break;
        case 2:
          inst->id.op1= MD_pD16PC;
          *addr= pd16an ( *addr, inst, 0xFF, &(inst->e1) );
          break;
        case 3:
          inst->id.op1= MD_pD8PCXN;
          *addr= pd8anxn ( *addr, inst, 0xFF, &(inst->e1) );
          break;
        case 4:
          inst->id.op1= MD_INMw;
          *addr= inm_word ( *addr, inst, &(inst->e1) );
          break;
        default: inst->id.name= MD_UNK; return -1;
        }
      break;
    default: inst->id.name= MD_UNK; return -1;
    }
  
  return 0;
  
} /* end op1_0234567_01234_w */


static int
op2_0234567_01 (
        	MDu32 * const   addr,
        	MD_Inst * const inst,
        	MDu8 const      mode,
        	MDu8 const      reg
        	)
{
  
  switch ( mode )
    {
    case 0: inst->id.op2= MD_DN; inst->e2.reg= reg; return 0;
    case 2: inst->id.op2= MD_pAN; inst->e2.reg= reg; break;
    case 3: inst->id.op2= MD_pANi; inst->e2.reg= reg; break;
    case 4: inst->id.op2= MD_pANd; inst->e2.reg= reg; break;
    case 5:
      inst->id.op2= MD_pD16AN;
      *addr= pd16an ( *addr, inst, reg, &(inst->e2) );
      break;
    case 6:
      inst->id.op2= MD_pD8ANXN;
      *addr= pd8anxn ( *addr, inst, reg, &(inst->e2) );
      break;
    case 7:
      switch ( reg )
        {
        case 0:
          inst->id.op2= MD_pW;
          *addr= pw ( *addr, inst, &(inst->e2) );
          break;
        case 1:
          inst->id.op2= MD_pL;
          *addr= pl ( *addr, inst, &(inst->e2) );
          break;
        default: inst->id.name= MD_UNK; return -1;
        }
      break;
    default: inst->id.name= MD_UNK; return -1;
    }
  
  return 0;
  
} /* end op2_0234567_01 */


static int
op2_01234567_01 (
        	 MDu32 * const   addr,
        	 MD_Inst * const inst,
        	 MDu8 const      mode,
        	 MDu8 const      reg
        	 )
{
  
  switch ( mode )
    {
    case 0: inst->id.op2= MD_DN; inst->e2.reg= reg; return 0;
    case 1: inst->id.op2= MD_AN; inst->e2.reg= reg; return 0;
    case 2: inst->id.op2= MD_pAN; inst->e2.reg= reg; break;
    case 3: inst->id.op2= MD_pANi; inst->e2.reg= reg; break;
    case 4: inst->id.op2= MD_pANd; inst->e2.reg= reg; break;
    case 5:
      inst->id.op2= MD_pD16AN;
      *addr= pd16an ( *addr, inst, reg, &(inst->e2) );
      break;
    case 6:
      inst->id.op2= MD_pD8ANXN;
      *addr= pd8anxn ( *addr, inst, reg, &(inst->e2) );
      break;
    case 7:
      switch ( reg )
        {
        case 0:
          inst->id.op2= MD_pW;
          *addr= pw ( *addr, inst, &(inst->e2) );
          break;
        case 1:
          inst->id.op2= MD_pL;
          *addr= pl ( *addr, inst, &(inst->e2) );
          break;
        default: inst->id.name= MD_UNK; return -1;
        }
      break;
    default: inst->id.name= MD_UNK; return -1;
    }
  
  return 0;
  
} /* end op2_01234567_01 */


static MDu32
opbi (
      MDu32           addr,
      MD_Inst * const inst,
      MDu8 const      mode,
      MDu8 const      reg,
      const int       mnemonic
      )
{
  
  addr= inm_byte ( addr, inst, &(inst->e1) );
  inst->id.op1= MD_INMb;
  if ( op2_0234567_01 ( &addr, inst, mode, reg ) == -1 ) return addr;
  inst->id.name= mnemonic;
  
  return addr;
  
} /* end opbi */


static MDu32
opwi (
      MDu32           addr,
      MD_Inst * const inst,
      MDu8 const      mode,
      MDu8 const      reg,
      const int       mnemonic
      )
{
  
  addr= inm_word ( addr, inst, &(inst->e1) );
  inst->id.op1= MD_INMw;
  if ( op2_0234567_01 ( &addr, inst, mode, reg ) == -1 ) return addr;
  inst->id.name= mnemonic;
  
  return addr;
  
} /* end opwi */


static MDu32
opli (
      MDu32           addr,
      MD_Inst * const inst,
      MDu8 const      mode,
      MDu8 const      reg,
      const int       mnemonic
      )
{
  
  addr= inm_long ( addr, inst, &(inst->e1) );
  inst->id.op1= MD_INMl;
  if ( op2_0234567_01 ( &addr, inst, mode, reg ) == -1 ) return addr;
  inst->id.name= mnemonic;
  
  return addr;
  
} /* end opli */


static MDu32
btst_inm (
           MDu32           addr,
           MD_Inst * const inst,
           MDu8 const      eamode,
           MDu8 const      eareg
          )
{
  
  addr= inm_byte ( addr, inst, &(inst->e1) );
  inst->id.op1= MD_INMb;
  
  switch ( eamode )
    {
    case 0: inst->id.op2= MD_DN; inst->e2.reg= eareg; break;
    case 2: inst->id.op2= MD_pAN; inst->e2.reg= eareg; break;
    case 3: inst->id.op2= MD_pANi; inst->e2.reg= eareg; break;
    case 4: inst->id.op2= MD_pANd; inst->e2.reg= eareg; break;
    case 5:
      inst->id.op2= MD_pD16AN;
      addr= pd16an ( addr, inst, eareg, &(inst->e2) );
      break;
    case 6:
      inst->id.op2= MD_pD8ANXN;
      addr= pd8anxn ( addr, inst, eareg, &(inst->e2) );
      break;  
    case 7:
      switch ( eareg )
        {
        case 0:
          inst->id.op2= MD_pW;
          addr= pw ( addr, inst, &(inst->e2) );
          break;
        case 1:
          inst->id.op2= MD_pL;
          addr= pl ( addr, inst, &(inst->e2) );
          break;
        case 2:
          inst->id.op2= MD_pD16PC;
          addr= pd16an ( addr, inst, 0xFF, &(inst->e2) );
          break;
        case 3:
          inst->id.op2= MD_pD8PCXN;
          addr= pd8anxn ( addr, inst, 0xFF, &(inst->e2) );
          break;
        default: inst->id.name= MD_UNK; return addr;
        }
      break;
    default: inst->id.name= MD_UNK; return addr;
    }
  
  inst->id.name= MD_BTST;
  
  return addr;
  
} /* end btst_inm */


static MDu32
movepw_mem_reg (
        	MDu32           addr,
        	MD_Inst * const inst,
                MDu8 const      dreg,
                MDu8 const      areg
                )
{
  
  inst->id.name= MD_MOVEPw;
  inst->id.op1= MD_pD16AN;
  addr= pd16an ( addr, inst, areg, &(inst->e1) );
  inst->id.op2= MD_DN;
  inst->e2.reg= dreg;
  
  return addr;
  
} /* end movepw_mem_reg */


static MDu32
btst_reg (
          MDu32           addr,
          MD_Inst * const inst,
          MDu8 const      reg,
          MDu8 const      eamode,
          MDu8 const      eareg
          )
{
  
  inst->id.name= MD_BTST;
  inst->id.op1= MD_DN;
  inst->e1.reg= reg;
  switch ( eamode )
    {
    case 0: inst->id.op2= MD_DN; inst->e2.reg= eareg; return addr;
    case 2: inst->id.op2= MD_pAN; inst->e2.reg= eareg; break;
    case 3: inst->id.op2= MD_pANi; inst->e2.reg= eareg; break;
    case 4: inst->id.op2= MD_pANd; inst->e2.reg= eareg; break;
    case 5: 
      inst->id.op2= MD_pD16AN;
      addr= pd16an ( addr, inst, eareg, &(inst->e2) );
      break;
    case 6:
      inst->id.op2= MD_pD8ANXN;
      addr= pd8anxn ( addr, inst, eareg, &(inst->e2) );
      break;
    case 7:
      switch ( eareg )
        {
        case 0:
          inst->id.op2= MD_pW;
          addr= pw ( addr, inst, &(inst->e2) );
          break;
        case 1:
          inst->id.op2= MD_pL;
          addr= pl ( addr, inst, &(inst->e2) );
          break;
        case 2:
          inst->id.op2= MD_pD16PC;
          addr= pd16an ( addr, inst, 0xFF, &(inst->e2) );
          break;
        case 3:
          inst->id.op2= MD_pD8PCXN;
          addr= pd8anxn ( addr, inst, 0xFF, &(inst->e2) );
          break;
        case 4:
          inst->id.op2= MD_INMb;
          addr= inm_byte ( addr, inst, &(inst->e2) );
          break;
        default: inst->id.name= MD_UNK; return addr;
        }
      break;
    default: inst->id.name= MD_UNK; return addr;
    }
  
  return addr;
  
} /* end btst_reg */


static int
movepl_mem_reg (
        	MDu32           addr,
                MD_Inst * const inst,
                MDu8 const      dreg,
                MDu8 const      areg
                )
{
  
  
  inst->id.name= MD_MOVEPl;
  inst->id.op1= MD_pD16AN;
  addr= pd16an ( addr, inst, areg, &(inst->e1) );
  inst->id.op2= MD_DN;
  inst->e2.reg= dreg;
  
  return addr;
  
} /* end movepl_mem_reg */


static MDu32
bop_reg (
         MDu32           addr,
         MD_Inst * const inst,
         MDu8 const      reg,
         MDu8 const      eamode,
         MDu8 const      eareg,
         const int       mnemonic
         )
{
  
  inst->id.op1= MD_DN;
  inst->e1.reg= reg;
  if ( op2_0234567_01 ( &addr, inst, eamode, eareg ) == -1 ) return addr;
  inst->id.name= mnemonic;
  
  return addr;
  
} /* end bop_reg */


static MDu32
movepw_reg_mem (
        	MDu32           addr,
                MD_Inst * const inst,
                MDu8 const      dreg,
                MDu8 const      areg
                )
{
  
  inst->id.name= MD_MOVEPw;
  inst->id.op1= MD_DN;
  inst->e1.reg= dreg;
  inst->id.op2= MD_pD16AN;
  addr= pd16an ( addr, inst, areg, &(inst->e2) );
  
  return addr;
  
} /* end movepw_reg_mem */


static MDu32
movepl_reg_mem (
        	MDu32           addr,
                MD_Inst * const inst,
                MDu8 const      dreg,
                MDu8 const      areg
                )
{
  
  inst->id.name= MD_MOVEPl;
  inst->id.op1= MD_DN;
  inst->e1.reg= dreg;
  inst->id.op2= MD_pD16AN;
  addr= pd16an ( addr, inst, areg, &(inst->e2) );
  
  return addr;
  
} /* end movepl_reg_mem */


static MDu32
as_negxb (
          MDu32           addr,
          MD_Inst * const inst,
          MDu8 const      mode,
          MDu8 const      reg,
          const int       mnemonic
          )
{
  
  inst->id.name= mnemonic;
  if ( op1_0234567_01 ( &addr, inst, mode, reg ) == -1 ) return addr;
  inst->id.op2= MD_NONE;
  
  return addr;
  
} /* end as_negxb */


static MDu32
swap (
      MDu32           addr,
      MD_Inst * const inst,
      MDu8 const      reg
      )
{
  
  inst->id.name= MD_SWAP;
  inst->id.op1= MD_DN;
  inst->e1.reg= reg;
  inst->id.op2= MD_NONE;
  
  return addr;
  
} /* end swap */


static MDu32
as_pea (
        MDu32           addr,
        MD_Inst * const inst,
        MDu8 const      mode,
        MDu8 const      reg,
        int const       mnemonic
        )
{
  
  inst->id.name= mnemonic;
  inst->id.op2= MD_NONE;
  if ( op1_2567_0123 ( &addr, inst, mode, reg ) == -1 ) return addr;
  
  return addr;
  
} /* end as_pea */


static MDu32
trap (
      MDu32           addr,
      MD_Inst * const inst,
      int const       vector
      )
{
  
  inst->id.name= MD_TRAP;
  inst->id.op1= MD_VECTOR;
  inst->e1.vector= vector;
  inst->id.op2= MD_NONE;
  
  return addr;
  
} /* end trap */


static MDu32
link (
      MDu32           addr,
      MD_Inst * const inst,
      MDu8 const      reg
      )
{
  
  inst->id.name= MD_LINK;
  inst->id.op1= MD_AN;
  inst->e1.reg= reg;
  addr= dis16 ( addr, inst, &(inst->e2) );
  inst->id.op2= MD_DIS16;
  
  return addr;
  
} /* end link */


static MDu32
unlk (
      MDu32           addr,
      MD_Inst * const inst,
      MDu8 const      reg
      )
{
  
  inst->id.name= MD_UNLK;
  inst->id.op1= MD_AN;
  inst->e1.reg= reg;
  inst->id.op2= MD_NONE;
  
  return addr;
  
} /* end unlk */


static MDu32
as_nop (
        MDu32           addr,
        MD_Inst * const inst,
        int const       mnemonic
        )
{
  
  inst->id.name= mnemonic;
  inst->id.op1= MD_NONE;
  inst->id.op2= MD_NONE;
  
  return addr;
  
} /* end as_nop */


static MDu16
revert_mask (
             MDu16 mask
             )
{
  
  MDu16 ret;
  int i;
  
  
  ret= 0x0000;
  for ( i= 0; i < 16; ++i )
    {
      ret<<= 1;
      if ( mask&0x1 ) ret|= 0x1;
      mask>>= 1;
    }
  
  return ret;
  
} /* end revert_mask */


static MDu32
as_movemw_reg_mem (
        	   MDu32           addr,
        	   MD_Inst * const inst,
        	   MDu8 const      mode,
        	   MDu8 const      reg,
        	   int const       mnemonic
        	   )
{
  
  MDu16 mask;
  
  
  inst->id.name= mnemonic;
  addr= inm_word ( addr, inst, &(inst->e1) ); /* e1 és provisional. */
  inst->id.op1= MD_LIST;
  mask= inst->e1.word;
  switch ( mode )
    {
    case 2: inst->id.op2= MD_pAN; inst->e2.reg= reg; break;
    case 4: inst->id.op2= MD_pANd; inst->e2.reg= reg;
      mask= revert_mask ( mask ); break;
    case 5:
      inst->id.op2= MD_pD16AN;
      addr= pd16an ( addr, inst, reg, &(inst->e2) );
      break;
    case 6:
      inst->id.op2= MD_pD8ANXN;
      addr= pd8anxn ( addr, inst, reg, &(inst->e2) );
      break;
    case 7:
      switch ( reg )
        {
        case 0:
          inst->id.op2= MD_pW;
          addr= pw ( addr, inst, &(inst->e2) );
          break;
        case 1:
          inst->id.op2= MD_pL;
          addr= pl ( addr, inst, &(inst->e2) );
          break;
        default: inst->id.name= MD_UNK; return -1;
        }
      break;
    default: inst->id.name= MD_UNK; return -1;
    }
  
  inst->e1.list= mask;
  
  return addr;
  
} /* end as_movemw_reg_mem */


static MDu32
as_movemw_mem_reg (
        	   MDu32           addr,
        	   MD_Inst * const inst,
        	   MDu8 const      mode,
        	   MDu8 const      reg,
        	   int const       mnemonic
        	   )
{
  
  MDu16 mask;
  
  
  inst->id.name= mnemonic;
  addr= inm_word ( addr, inst, &(inst->e2) ); /* e2 és provisional. */
  inst->id.op2= MD_LIST;
  mask= inst->e2.word;
  switch ( mode )
    {
    case 2: inst->id.op1= MD_pAN; inst->e1.reg= reg; break;
    case 3: inst->id.op1= MD_pANi; inst->e1.reg= reg; break;
    case 5:
      inst->id.op1= MD_pD16AN;
      addr= pd16an ( addr, inst, reg, &(inst->e1) );
      break;
    case 6:
      inst->id.op1= MD_pD8ANXN;
      addr= pd8anxn ( addr, inst, reg, &(inst->e1) );
      break;

    case 7:
      switch ( reg )
        {
        case 0:
          inst->id.op1= MD_pW;
          addr= pw ( addr, inst, &(inst->e1) );
          break;
        case 1:
          inst->id.op1= MD_pL;
          addr= pl ( addr, inst, &(inst->e1) );
          break;
        case 2:
          inst->id.op1= MD_pD16PC;
          addr= pd16an ( addr, inst, 0xFF, &(inst->e1) );
          break;
        case 3:
          inst->id.op1= MD_pD8PCXN;
          addr= pd8anxn ( addr, inst, 0xFF, &(inst->e1) );
          break;
        default: inst->id.name= MD_UNK; return addr;
        }
      break;
    default: inst->id.name= MD_UNK; return addr;
    }
  
  inst->e2.list= mask;
  
  return addr;
  
} /* end movemw_mem_reg */


static MDu32
move_from_sr (
              MDu32           addr,
              MD_Inst * const inst,
              MDu8 const      mode,
              MDu8 const      reg
              )
{
  
  inst->id.name= MD_MOVEw;
  inst->id.op1= MD_SR;
  if ( op2_0234567_01 ( &addr, inst, mode, reg ) == -1 ) return addr;
  
  return addr;
  
} /* end move_from_sr */


static MDu32
move_to_ccr (
             MDu32           addr,
             MD_Inst * const inst,
             MDu8 const      mode,
             MDu8 const      reg
             )
{
  
  inst->id.name= MD_MOVEw;
  if ( op1_01234567_01234_w ( &addr, inst, mode, reg ) == -1 ) return addr;
  inst->id.op2= MD_CCR;
  
  return addr;
  
} /* end move_to_ccr */


static MDu32
move_to_sr (
            MDu32           addr,
            MD_Inst * const inst,
            MDu8 const      mode,
            MDu8 const      reg
            )
{
  
  inst->id.name= MD_MOVEw;
  if ( op1_0234567_01234_w ( &addr, inst, mode, reg ) == -1 ) return addr;
  inst->id.op2= MD_SR;
  
  return addr;
  
} /* end move_to_sr */


static MDu32
chk (
     MDu32           addr,
     MD_Inst * const inst,
     MDu8 const      reg,
     MDu8 const      eamode,
     MDu8 const      eareg
     )
{
  
  inst->id.name= MD_CHK;
  if ( op1_0234567_01234_w ( &addr, inst, eamode, eareg ) == -1 ) return addr;
  inst->id.op2= MD_DN;
  inst->e2.reg= reg;
  
  return addr;
  
} /* end chk */


static MDu32
lea (
     MDu32           addr,
     MD_Inst * const inst,
     MDu8 const      reg,
     MDu8 const      eamode,
     MDu8 const      eareg
     )
{
  
  inst->id.name= MD_LEA;
  if ( op1_2567_0123 ( &addr, inst, eamode, eareg ) == -1 ) return addr;
  inst->id.op2= MD_AN;
  inst->e2.reg= reg;
  
  return addr;
  
} /* end lea */


static MDu32
opbq (
      MDu32           addr,
      MD_Inst * const inst,
      MDu8            data,
      MDu8 const      mode,
      MDu8 const      reg,
      const int       mnemonic
      )
{
  
  if ( data == 0 ) data= 8;
  inst->id.op1= MD_INMb;
  inst->e1.byte= data;
  if ( op2_0234567_01 ( &addr, inst, mode, reg ) == -1 ) return addr;
  inst->id.name= mnemonic;
  
  return addr;
  
} /* end opbq */


static MDu32
opwq (
      MDu32           addr,
      MD_Inst * const inst,
      MDu8            data,
      MDu8 const      mode,
      MDu8 const      reg,
      const int       mnemonic
      )
{
  
  if ( data == 0 ) data= 8;
  inst->id.op1= MD_INMb;
  inst->e1.byte= data;
  if ( op2_01234567_01 ( &addr, inst, mode, reg ) == -1 ) return addr;
  inst->id.name= mnemonic;
  
  return addr;
  
} /* end opwq */


static MDu32
dbcc (
      MDu32           addr,
      MD_Inst * const inst,
      MDu8 const      cond,
      MDu8 const      reg
      )
{
  
  MDu32 aux;
  MDs16 disp;
  
  
  inst->id.op1= MD_DN;
  inst->e1.reg= reg;
  aux= addr;
  addr= inm_word ( addr, inst, &(inst->e2) ); /* e2 és provisional */
  disp= (MDs16) inst->e2.word;
  inst->id.op2= MD_LABEL;
  inst->e2.label.dis= disp;
  inst->e2.label.addr= aux + disp;
  switch ( cond )
    {
    case 0x0: inst->id.name= MD_DBT; break;
    case 0x1: inst->id.name= MD_DBF; break;
    case 0x2: inst->id.name= MD_DBHI; break;
    case 0x3: inst->id.name= MD_DBLS; break;
    case 0x4: inst->id.name= MD_DBCC; break;
    case 0x5: inst->id.name= MD_DBCS; break;
    case 0x6: inst->id.name= MD_DBNE; break;
    case 0x7: inst->id.name= MD_DBEQ; break;
    case 0x8: inst->id.name= MD_DBVC; break;
    case 0x9: inst->id.name= MD_DBVS; break;
    case 0xA: inst->id.name= MD_DBPL; break;
    case 0xB: inst->id.name= MD_DBMI; break;
    case 0xC: inst->id.name= MD_DBGE; break;
    case 0xD: inst->id.name= MD_DBLT; break;
    case 0xE: inst->id.name= MD_DBGT; break;
    case 0xF: inst->id.name= MD_DBLE; break;
    default: inst->id.name= MD_UNK; break;
    }
  
  return addr;
  
} /* end dbcc */


static MDu32
scc (
     MDu32           addr,
     MD_Inst * const inst,
     MDu8 const      cond,
     MDu8 const      mode,
     MDu8 const      reg
     )
{
  
  if ( op1_0234567_01 ( &addr, inst, mode, reg ) == -1 ) return addr;
  inst->id.op2= MD_NONE;
  switch ( cond )
    {
    case 0x0: inst->id.name= MD_ST; break;
    case 0x1: inst->id.name= MD_SF; break;
    case 0x2: inst->id.name= MD_SHI; break;
    case 0x3: inst->id.name= MD_SLS; break;
    case 0x4: inst->id.name= MD_SCC; break;
    case 0x5: inst->id.name= MD_SCS; break;
    case 0x6: inst->id.name= MD_SNE; break;
    case 0x7: inst->id.name= MD_SEQ; break;
    case 0x8: inst->id.name= MD_SVC; break;
    case 0x9: inst->id.name= MD_SVS; break;
    case 0xA: inst->id.name= MD_SPL; break;
    case 0xB: inst->id.name= MD_SMI; break;
    case 0xC: inst->id.name= MD_SGE; break;
    case 0xD: inst->id.name= MD_SLT; break;
    case 0xE: inst->id.name= MD_SGT; break;
    case 0xF: inst->id.name= MD_SLE; break;
    default: inst->id.name= MD_UNK; break;
    }
  
  return addr;
  
} /* end scc */


static MDu32
as_bra (
        MDu32           addr,
        MD_Inst * const inst,
        MDu8 const      disp,
        const int       mnemonic
        )
{
  
  MDs16 disp16;
  MDu32 addr2;
  
 
  inst->id.name= mnemonic;
  inst->id.op2= MD_NONE;
  inst->id.op1= MD_LABEL;
  if ( disp==0x00 )
    {
      addr2= addr;
      addr= inm_word ( addr, inst, &(inst->e1) ); /* Provisional */
      disp16= (MDs16) inst->e1.word;
      inst->e1.label.addr= addr2 + disp16;
      inst->e1.label.dis= disp16;
    }
  else
    {
      inst->e1.label.addr= addr + (MDs8) disp;
      inst->e1.label.dis= (MDs8) disp;
    }
  
  return addr;
  
} /* end as_bra */


static MDu32
bcc (
     MDu32           addr,
     MD_Inst * const inst,
     MDu8 const      cond,
     MDu8 const      disp
     )
{
  
  MDs16 disp16;
  MDu32 addr2;
  
 
  inst->id.op2= MD_NONE;
  inst->id.op1= MD_LABEL;
  if ( disp == 0x00 )
    {
      addr2= addr;
      addr= inm_word ( addr, inst, &(inst->e1) ); /* Provisional */
      addr2+= disp16= (MDs16) inst->e1.word;
    }
  if ( disp==0x00 )
    {
      inst->e1.label.addr= addr2;
      inst->e1.label.dis= disp16;
    }
  else
    {
      inst->e1.label.addr= addr + (MDs8) disp;
      inst->e1.label.dis= (MDs8) disp;
    }
  switch ( cond )
    {
    case 0x2: inst->id.name= MD_BHI; break;
    case 0x3: inst->id.name= MD_BLS; break;
    case 0x4: inst->id.name= MD_BCC; break;
    case 0x5: inst->id.name= MD_BCS; break;
    case 0x6: inst->id.name= MD_BNE; break;
    case 0x7: inst->id.name= MD_BEQ; break;
    case 0x8: inst->id.name= MD_BVC; break;
    case 0x9: inst->id.name= MD_BVS; break;
    case 0xA: inst->id.name= MD_BPL; break;
    case 0xB: inst->id.name= MD_BMI; break;
    case 0xC: inst->id.name= MD_BGE; break;
    case 0xD: inst->id.name= MD_BLT; break;
    case 0xE: inst->id.name= MD_BGT; break;
    case 0xF: inst->id.name= MD_BLE; break;
    default: inst->id.name= MD_UNK; break;
    }
  
  return addr;
  
} /* end bcc */


static MDu32
opb_easrc (
           MDu32           addr,
           MD_Inst * const inst,
           MDu8 const      reg,
           MDu8 const      eamode,
           MDu8 const      eareg,
           int const       mnemonic
           )
{
  
  
  inst->id.name= mnemonic;
  if ( op1_0234567_01234_b ( &addr, inst, eamode, eareg ) == -1 ) return addr;
  inst->id.op2= MD_DN;
  inst->e2.reg= reg;
  
  return addr;
  
} /* end opb_easrc */


static MDu32
opw_easrc (
           MDu32           addr,
           MD_Inst * const inst,
           MDu8 const      reg,
           MDu8 const      eamode,
           MDu8 const      eareg,
           int const       mnemonic
           )
{
  
  
  inst->id.name= mnemonic;
  if ( op1_01234567_01234_w ( &addr, inst, eamode, eareg ) == -1 ) return addr;
  inst->id.op2= MD_DN;
  inst->e2.reg= reg;
  
  return addr;
  
} /* end opw_easrc */


static MDu32
opw_easrc_A (
             MDu32           addr,
             MD_Inst * const inst,
             MDu8 const      reg,
             MDu8 const      eamode,
             MDu8 const      eareg,
             int const       mnemonic
             )
{
  
  
  inst->id.name= mnemonic;
  if ( op1_01234567_01234_w ( &addr, inst, eamode, eareg ) == -1 ) return addr;
  inst->id.op2= MD_AN;
  inst->e2.reg= reg;
  
  return addr;
  
} /* end opw_easrc_A */


static MDu32
opl_easrc (
           MDu32           addr,
           MD_Inst * const inst,
           MDu8 const      reg,
           MDu8 const      eamode,
           MDu8 const      eareg,
           int const       mnemonic
           )
{
  
  
  inst->id.name= mnemonic;
  if ( op1_01234567_01234_l ( &addr, inst, eamode, eareg ) == -1 ) return addr;
  inst->id.op2= MD_DN;
  inst->e2.reg= reg;
  
  return addr;
  
} /* end opl_easrc */


static MDu32
opl_easrc_A (
             MDu32           addr,
             MD_Inst * const inst,
             MDu8 const      reg,
             MDu8 const      eamode,
             MDu8 const      eareg,
             int const       mnemonic
             )
{
  
  
  inst->id.name= mnemonic;
  if ( op1_01234567_01234_l ( &addr, inst, eamode, eareg ) == -1 ) return addr;
  inst->id.op2= MD_AN;
  inst->e2.reg= reg;
  
  return addr;
  
} /* end opl_easrc_A */


static MDu32
as_divu (
         MDu32           addr,
         MD_Inst * const inst,
         MDu8 const      reg,
         MDu8 const      eamode,
         MDu8 const      eareg,
         int const       mnemonic
         )
{
  
  inst->id.name= mnemonic;
  if ( op1_0234567_01234_w ( &addr, inst, eamode, eareg ) == -1 ) return addr;
  inst->id.op2= MD_DN;
  inst->e2.reg= reg;
  
  return addr;
  
} /* end as_divu */


static MDu32
opb_eadst (
           MDu32           addr,
           MD_Inst * const inst,
           MDu8 const      reg,
           MDu8 const      eamode,
           MDu8 const      eareg,
           int const       mnemonic
           )
{
  
  inst->id.name= mnemonic;
  inst->id.op1= MD_DN;
  inst->e1.reg= reg;
  switch ( eamode )
    {
    case 2: inst->id.op2= MD_pAN; inst->e2.reg= eareg; break;
    case 3: inst->id.op2= MD_pANi; inst->e2.reg= eareg; break;
    case 4: inst->id.op2= MD_pANd; inst->e2.reg= eareg; break;
    case 5:
      inst->id.op2= MD_pD16AN;
      addr= pd16an ( addr, inst, eareg, &(inst->e2) );
      break;
    case 6:
      inst->id.op2= MD_pD8ANXN;
      addr= pd8anxn ( addr, inst, eareg, &(inst->e2) );
      break;
    case 7:
      switch ( eareg )
        {
        case 0:
          inst->id.op2= MD_pW;
          addr= pw ( addr, inst, &(inst->e2) );
          break;
        case 1:
          inst->id.op2= MD_pL;
          addr= pl ( addr, inst, &(inst->e2) );
          break;
        default: inst->id.name= MD_UNK; return -1;
        }
      break;
    default: inst->id.name= MD_UNK; return -1;
    }
  
  return addr;
  
} /* end opb_eadst */


static MDu32
as_eorb (
         MDu32           addr,
         MD_Inst * const inst,
         MDu8 const      reg,
         MDu8 const      eamode,
         MDu8 const      eareg,
         int const       mnemonic
         )
{
  
  inst->id.name= mnemonic;
  inst->id.op1= MD_DN;
  inst->e1.reg= reg;
  if ( op2_0234567_01 ( &addr, inst, eamode, eareg ) == -1 ) return addr;
  
  return addr;
  
} /* end as_eorb */


static MDu32
sopb_inm (
          MDu32           addr,
          MD_Inst * const inst,
          MDu8            count,
          MDu8 const      reg,
          int const       mnemonic
          )
{
  
  inst->id.name= mnemonic;
  inst->id.op1= MD_COUNT;
  inst->e1.count= (count==0) ? 8 : count;
  inst->id.op2= MD_DN;
  inst->e2.reg= reg;
  
  return addr;
  
} /* end sopb_inm */


static MDu32
sopb_reg (
          MDu32           addr,
          MD_Inst * const inst,
          MDu8 const      regc,
          MDu8 const      reg,
          int const       mnemonic
          )
{
  
  inst->id.name= mnemonic;
  inst->id.op1= MD_DN;
  inst->e1.reg= regc;
  inst->id.op2= MD_DN;
  inst->e2.reg= reg;
  
  return addr;
  
} /* end sopb_reg */


static MDu32
sop_mem (
         MDu32           addr,
         MD_Inst * const inst,
         MDu8 const      mode,
         MDu8 const      reg,
         int const       mnemonic
         )
{
  
  inst->id.name= mnemonic;
  inst->id.op2= MD_NONE;
  switch ( mode )
    {
    case 2: inst->id.op1= MD_pAN; inst->e1.reg= reg; break;
    case 3: inst->id.op1= MD_pANi; inst->e1.reg= reg; break;
    case 4: inst->id.op1= MD_pANd; inst->e1.reg= reg; break;
    case 5:
      inst->id.op1= MD_pD16AN;
      addr= pd16an ( addr, inst, reg, &(inst->e1) );
      break;
    case 6:
      inst->id.op1= MD_pD8ANXN;
      addr= pd8anxn ( addr, inst, reg, &(inst->e1) );
      break;
    case 7:
      switch ( reg )
        {
        case 0:
          inst->id.op1= MD_pW;
          addr= pw ( addr, inst, &(inst->e1) );
          break;
        case 1:
          inst->id.op1= MD_pL;
          addr= pl ( addr, inst, &(inst->e1) );
          break;
        default: inst->id.name= MD_UNK; return -1;
        }
      break;
    default: inst->id.name= MD_UNK; return -1;
    }
  
  return addr;
  
} /* end sop_mem */


/* Taules de bot **************************************************************/
static MDu32
bit_movep_inm (
               MDu32           addr,
               MD_Inst * const inst,
               MDu8 const      v3,
               MDu8 const      v2,
               MDu8 const      v1,
               MDu8 const      v0
               )
{
  
  switch ( v2 )
    {
    case 0:
      switch ( v3 )
        {
        case 0:
          switch ( v1 )
            {
            case 7:
              switch ( v0 )
        	{
        	case 4:
        	  inst->id.name= MD_ORIb;
        	  inst->id.op1= MD_INMb; inst->id.op2= MD_CCR;
        	  return inm_byte ( addr, inst, &(inst->e1) );
        	default: return opbi ( addr, inst, v1, v0, MD_ORIb );
        	}
              break;
            default: return opbi ( addr, inst, v1, v0, MD_ORIb );
            }
          break;
        case 1:
          switch ( v1 )
            {
            case 7:
              switch ( v0 )
        	{
        	case 4: 
        	  inst->id.name= MD_ANDIb;
        	  inst->id.op1= MD_INMb; inst->id.op2= MD_CCR;
        	  return inm_byte ( addr, inst, &(inst->e1) );
        	default: return opbi ( addr, inst, v1, v0, MD_ANDIb );
        	}
              break;
            default: return opbi ( addr, inst, v1, v0, MD_ANDIb );
            }
          break;
        case 2: return opbi ( addr, inst, v1, v0, MD_SUBIb );
        case 3: return opbi ( addr, inst, v1, v0, MD_ADDIb );
        case 4: return btst_inm ( addr, inst, v1, v0 );
        case 5:
          switch ( v1 )
            {
            case 7:
              switch ( v0 )
        	{
        	case 4:
        	  inst->id.name= MD_EORIb;
        	  inst->id.op1= MD_INMb; inst->id.op2= MD_CCR;
        	  return inm_byte ( addr, inst, &(inst->e1) );
        	default: return opbi ( addr, inst, v1, v0, MD_EORIb );
        	}
              break;
            default: return opbi ( addr, inst, v1, v0, MD_EORIb );
            }
          break;
        case 6: return opbi ( addr, inst, v1, v0, MD_CMPIb );
        default: break;
        }
      break;
    case 1:
      switch ( v3 )
        {
        case 0:
          switch ( v1 )
            {
            case 7:
              switch ( v0 )
                {
        	case 4:
        	  inst->id.name= MD_ORIw;
        	  inst->id.op1= MD_INMw; inst->id.op2= MD_SR;
        	  return inm_word ( addr, inst, &(inst->e1) );
                default: return opwi ( addr, inst, v1, v0, MD_ORIw );
                }
              break;
            default: return opwi ( addr, inst, v1, v0, MD_ORIw );
            }
          break;
        case 1:
          switch ( v1 )
            {
            case 7:
              switch ( v0 )
                {
        	case 4:
        	  inst->id.name= MD_ANDIw;
        	  inst->id.op1= MD_INMw; inst->id.op2= MD_SR;
        	  return inm_word ( addr, inst, &(inst->e1) );
                default: return opwi ( addr, inst, v1, v0, MD_ANDIw );
                }
              break;
            default: return opwi ( addr, inst, v1, v0, MD_ANDIw );
            }
          break;
        case 2: return opwi ( addr, inst, v1, v0, MD_SUBIw );
        case 3: return opwi ( addr, inst, v1, v0, MD_ADDIw );
        case 4: return opbi ( addr, inst, v1, v0, MD_BCHG );
        case 5:
          switch ( v1 )
            {
            case 7:
              switch ( v0 )
                {
        	case 4:
        	  inst->id.name= MD_EORIw;
        	  inst->id.op1= MD_INMw; inst->id.op2= MD_SR;
        	  return inm_word ( addr, inst, &(inst->e1) );
                default: return opwi ( addr, inst, v1, v0, MD_EORIw );
                }
              break;
            default: return opwi ( addr, inst, v1, v0, MD_EORIw );
            }
          break;
        case 6: return opwi ( addr, inst, v1, v0, MD_CMPIw );
        default: break;
        }
      break;
    case 2:
      switch ( v3 )
        {
        case 0: return opli ( addr, inst, v1, v0, MD_ORIl );
        case 1: return opli ( addr, inst, v1, v0, MD_ANDIl );
        case 2: return opli ( addr, inst, v1, v0, MD_SUBIl );
        case 3: return opli ( addr, inst, v1, v0, MD_ADDIl );
        case 4: return opbi ( addr, inst, v1, v0, MD_BCLR );
        case 5: return opli ( addr, inst, v1, v0, MD_EORIl );
        case 6: return opli ( addr, inst, v1, v0, MD_CMPIl );
        default: break;
        }
      break;
    case 3:
      switch ( v3 )
        {
        case 4: return opbi ( addr, inst, v1, v0, MD_BSET );
        default: break;
        }
      break;
    case 4:
      switch ( v1 )
        {
        case 1: return movepw_mem_reg ( addr, inst, v3, v0 );
        default: return btst_reg ( addr, inst, v3, v1, v0 );
        }
      break;
    case 5:
      switch ( v1 )
        {
        case 1: return movepl_mem_reg ( addr, inst, v3, v0 );
        default: return bop_reg ( addr, inst, v3, v1, v0, MD_BCHG );
        }
      break;
    case 6:
      switch ( v1 )
        {
        case 1: return movepw_reg_mem ( addr, inst, v3, v0 );
        default: return bop_reg ( addr, inst, v3, v1, v0, MD_BCLR );
        }
      break;
    case 7:
      switch ( v1 )
        {
        case 1: return movepl_reg_mem ( addr, inst, v3, v0 );
        default: return bop_reg ( addr, inst, v3, v1, v0, MD_BSET );
        }
      break;
    default: break;
    }
  
  inst->id.name= MD_UNK;
  
  return addr;
  
} /* end bit_movep_inm */


static MDu32
moveb (
       MDu32           addr,
       MD_Inst * const inst,
       MDu8 const      dreg,
       MDu8 const      dmode,
       MDu8 const      smode,
       MDu8 const      sreg
       )
{
  
  inst->id.name= MD_MOVEb;
  
  /* Source. */
  switch ( smode )
    {
    case 0: inst->id.op1= MD_DN; inst->e1.reg= sreg; break;
    case 1: inst->id.name= MD_UNK; return addr;
    case 2: inst->id.op1= MD_pAN; inst->e1.reg= sreg; break;
    case 3: inst->id.op1= MD_pANi; inst->e1.reg= sreg; break;
    case 4: inst->id.op1= MD_pANd; inst->e1.reg= sreg; break;
    case 5:
      inst->id.op1= MD_pD16AN;
      addr= pd16an ( addr, inst, sreg, &(inst->e1) );
      break;
    case 6:
      inst->id.op1= MD_pD8ANXN;
      addr= pd8anxn ( addr, inst, sreg, &(inst->e1) );
      break;
    case 7:
      switch ( sreg )
        {
        case 0:
          inst->id.op1= MD_pW;
          addr= pw ( addr, inst, &(inst->e1) );
          break;
        case 1:
          inst->id.op1= MD_pL;
          addr= pl ( addr, inst, &(inst->e1) );
          break;
        case 2:
          inst->id.op1= MD_pD16PC;
          addr= pd16an ( addr, inst, 0xFF, &(inst->e1) );
          break;
        case 3:
          inst->id.op1= MD_pD8PCXN;
          addr= pd8anxn ( addr, inst, 0xFF, &(inst->e1) );
          break;
        case 4:
          inst->id.op1= MD_INMb;
          addr= inm_byte ( addr, inst, &(inst->e1) );
          break;
        default: inst->id.name= MD_UNK; return addr;
        }
      break;
    default: break;
    }
  
  /* Destination. */
  if ( op2_0234567_01 ( &addr, inst, dmode, dreg ) == -1 ) return addr;
  
  return addr;
  
} /* end moveb */


static MDu32
movel (
       MDu32           addr,
       MD_Inst * const inst,
       MDu8 const      dreg,
       MDu8 const      dmode,
       MDu8 const      smode,
       MDu8 const      sreg
       )
{
  
  inst->id.name= MD_MOVEl;
  
  /* Source. */
  switch ( smode )
    {
    case 0: inst->id.op1= MD_DN; inst->e1.reg= sreg; break;
    case 1: inst->id.op1= MD_AN; inst->e1.reg= sreg; break;
    case 2: inst->id.op1= MD_pAN; inst->e1.reg= sreg; break;
    case 3: inst->id.op1= MD_pANi; inst->e1.reg= sreg; break;
    case 4: inst->id.op1= MD_pANd; inst->e1.reg= sreg; break;
    case 5:
      inst->id.op1= MD_pD16AN;
      addr= pd16an ( addr, inst, sreg, &(inst->e1) );
      break;
    case 6:
      inst->id.op1= MD_pD8ANXN;
      addr= pd8anxn ( addr, inst, sreg, &(inst->e1) );
      break;
    case 7:
      switch ( sreg )
        {
        case 0:
          inst->id.op1= MD_pW;
          addr= pw ( addr, inst, &(inst->e1) );
          break;
        case 1:
          inst->id.op1= MD_pL;
          addr= pl ( addr, inst, &(inst->e1) );
          break;
        case 2:
          inst->id.op1= MD_pD16PC;
          addr= pd16an ( addr, inst, 0xFF, &(inst->e1) );
          break;
        case 3:
          inst->id.op1= MD_pD8PCXN;
          addr= pd8anxn ( addr, inst, 0xFF, &(inst->e1) );
          break;
        case 4:
          inst->id.op1= MD_INMl;
          addr= inm_long ( addr, inst, &(inst->e1) );
          break;
        default: inst->id.name= MD_UNK; return addr;
        }
      break;
    default: break;
    }
  
  /* Destination. */
  if ( op2_01234567_01 ( &addr, inst, dmode, dreg ) == -1 ) return addr;
  
  return addr;
  
} /* end movel */


static MDu32
movew (
       MDu32           addr,
       MD_Inst * const inst,
       MDu8 const      dreg,
       MDu8 const      dmode,
       MDu8 const      smode,
       MDu8 const      sreg
       )
{
  
  inst->id.name= MD_MOVEw;
  
  /* Source. */
  if ( op1_01234567_01234_w ( &addr, inst, smode, sreg ) == -1 ) return addr;
  
  /* Destination. */
  if ( op2_01234567_01 ( &addr, inst, dmode, dreg ) == -1 ) return addr;
  
  return addr;
  
} /* end movew */


static MDu32
miscellaneous (
               MDu32           addr,
               MD_Inst * const inst,
               MDu8 const      v3,
               MDu8 const      v2,
               MDu8 const      v1,
               MDu8 const      v0
               )
{
  
  switch ( v2 )
    {
    case 0:
      switch ( v3 )
        {
        case 0: return as_negxb ( addr, inst, v1, v0, MD_NEGXb );
        case 1: return as_negxb ( addr, inst, v1, v0, MD_CLRb );
        case 2: return as_negxb ( addr, inst, v1, v0, MD_NEGb );
        case 3: return as_negxb ( addr, inst, v1, v0, MD_NOTb );
        case 4: return as_negxb ( addr, inst, v1, v0, MD_NBCD );
        case 5: return as_negxb ( addr, inst, v1, v0, MD_TSTb );
        default: break;
        }
      break;
    case 1:
      switch ( v3 )
        {
        case 0: return as_negxb ( addr, inst, v1, v0, MD_NEGXw );
        case 1: return as_negxb ( addr, inst, v1, v0, MD_CLRw );
        case 2: return as_negxb ( addr, inst, v1, v0, MD_NEGw );
        case 3: return as_negxb ( addr, inst, v1, v0, MD_NOTw );
        case 4:
          switch ( v1 )
            {
            case 0: return swap ( addr, inst, v0 );
            default: return as_pea ( addr, inst, v1, v0, MD_PEA );
            }
          break;
        case 5: return as_negxb ( addr, inst, v1, v0, MD_TSTw );
        case 7:
          switch ( v1 )
            {
            case 0: return trap ( addr, inst, v0+32 );
            case 1: return trap ( addr, inst, (v0|0x8)+32 );
            case 2: return link ( addr, inst, v0 );
            case 3: return unlk ( addr, inst, v0 );
            case 4:
              inst->id.name= MD_MOVEl; inst->id.op1= MD_AN; inst->e1.reg= v0;
              inst->id.op2= MD_USP; return addr;
            case 5:
              inst->id.name= MD_MOVEl; inst->id.op1= MD_USP;
              inst->id.op2= MD_AN; inst->e2.reg= v0; return addr;
            case 6:
              switch ( v0 )
                {
                case 0: return as_nop ( addr, inst, MD_RESET );
                case 1: return as_nop ( addr, inst, MD_NOP );
                case 2: return as_nop ( addr, inst, MD_STOP );
                case 3: return as_nop ( addr, inst, MD_RTE );
                case 5: return as_nop ( addr, inst, MD_RTS );
                case 7: return as_nop ( addr, inst, MD_RTR );
                default: break;
                }
              break;
            default: break;
            }
          break;
        default: break;
        }
      break;
    case 2:
      switch ( v3 )
        {
        case 0: return as_negxb ( addr, inst, v1, v0, MD_NEGXl );
        case 1: return as_negxb ( addr, inst, v1, v0, MD_CLRl );
        case 2: return as_negxb ( addr, inst, v1, v0, MD_NEGl );
        case 3: return as_negxb ( addr, inst, v1, v0, MD_NOTl );
        case 4:
          switch ( v1 )
            {
            case 0:
              inst->id.name= MD_EXTw; inst->id.op1= MD_DN; inst->e1.reg= v0;
              inst->id.op2= MD_NONE; return addr;
            default: return as_movemw_reg_mem ( addr, inst, v1, v0, MD_MOVEMw );
            }
          break;
        case 5: return as_negxb ( addr, inst, v1, v0, MD_TSTl );
        case 6: return as_movemw_mem_reg ( addr, inst, v1, v0, MD_MOVEMw );
        case 7: return as_pea ( addr, inst, v1, v0, MD_JSR );
        default: break;
        }
      break;
    case 3:
      switch ( v3 )
        {
        case 0: return move_from_sr ( addr, inst, v1, v0 );
        case 2: return move_to_ccr ( addr, inst, v1, v0 );
        case 3: return move_to_sr ( addr, inst, v1, v0 );
        case 4:
          switch ( v1 )
            {
            case 0:
              inst->id.name= MD_EXTl; inst->id.op1= MD_DN; inst->e1.reg= v0;
              inst->id.op2= MD_NONE; return addr;
            default: return as_movemw_reg_mem ( addr, inst, v1, v0, MD_MOVEMl );
            }
          break;
        case 5:
          switch ( v1 )
            {
            case 7:
              switch ( v0 )
                {
                case 4: return as_nop ( addr, inst, MD_ILLEGAL );
                default: break;
                }
              break;
            default: break;
            }
          break;
        case 6: return as_movemw_mem_reg ( addr, inst, v1, v0, MD_MOVEMl );
        case 7: return as_pea ( addr, inst, v1, v0, MD_JMP );
        default: break;
        }
      break;
    case 6: return chk ( addr, inst, v3, v1, v0 );
    case 7: return lea ( addr, inst, v3, v1, v0 );
    default: break;
    }
  
  inst->id.name= MD_UNK;
  
  return addr;
  
} /* end miscellaneous */


static MDu32
addq_subq__ (
             MDu32           addr,
             MD_Inst * const inst,
             MDu8 const      v3,
             MDu8 const      v2,
             MDu8 const      v1,
             MDu8 const      v0
             )
{
  
  switch ( v2 )
    {
    case 0: return opbq ( addr, inst, v3, v1, v0, MD_ADDQb );
    case 1: return opwq ( addr, inst, v3, v1, v0, MD_ADDQw );
    case 2: return opwq ( addr, inst, v3, v1, v0, MD_ADDQl );
    case 3:
      switch ( v1 )
        {
        case 1: return dbcc ( addr, inst, v3<<1, v0 );
        default: return scc ( addr, inst, v3<<1, v1, v0 );
        }
      break;
    case 4: return opbq ( addr, inst, v3, v1, v0, MD_SUBQb );
    case 5: return opwq ( addr, inst, v3, v1, v0, MD_SUBQw );
    case 6: return opwq ( addr, inst, v3, v1, v0, MD_SUBQl );
    case 7:
      switch ( v1 )
        {
        case 1: return dbcc ( addr, inst, (v3<<1)|1, v0 );
        default: return scc ( addr, inst, (v3<<1)|1, v1, v0 );
        }
      break;
    default: break;
    }
  
  inst->id.name= MD_UNK;
  
  return addr;
  
} /* end addq_subq__ */


static MDu32
bcc_bsr_bra (
             MDu32           addr,
             MD_Inst * const inst,
             MDu8 const      cond,
             MDu8 const      disp
             )
{
  
  switch ( cond )
    {
    case 0: return as_bra ( addr, inst, disp, MD_BRA );
    case 1: return as_bra ( addr, inst, disp, MD_BSR );
    default: return bcc ( addr, inst, cond, disp );
    }
  
  inst->id.name= MD_UNK;
  
  return addr;
  
  
} /* end bcc_bsr_bra */


static MDu32
or_div_sbcd (
             MDu32           addr,
             MD_Inst * const inst,
             MDu8 const      v3,
             MDu8 const      v2,
             MDu8 const      v1,
             MDu8 const      v0
             )
{
  
  switch ( v2 )
    {
    case 0:
      switch ( v1 )
        {
        case 1: break;
        default: return opb_easrc ( addr, inst, v3, v1, v0, MD_ORb );
        }
      break;
    case 1:
      switch ( v1 )
        {
        case 1: break;
        default: return opw_easrc ( addr, inst, v3, v1, v0, MD_ORw );
        }
      break;
    case 2:
      switch ( v1 )
        {
        case 1: break;
        default: return opl_easrc ( addr, inst, v3, v1, v0, MD_ORl );
        }
      break;
    case 3: return as_divu ( addr, inst, v3, v1, v0, MD_DIVU );
    case 4:
      switch ( v1 )
        {
        case 0:
          inst->id.name= MD_SBCD;
          inst->id.op1= MD_DN; inst->e1.reg= v0;
          inst->id.op2= MD_DN; inst->e2.reg= v3;
          return addr;
        case 1:
          inst->id.name= MD_SBCD;
          inst->id.op1= MD_pANd; inst->e1.reg= v0;
          inst->id.op2= MD_pANd; inst->e2.reg= v3;
          return addr;
        default: return opb_eadst ( addr, inst, v3, v1, v0, MD_ORb );
        }
      break;
    case 5: return opb_eadst ( addr, inst, v3, v1, v0, MD_ORw );
    case 6: return opb_eadst ( addr, inst, v3, v1, v0, MD_ORl );
    case 7: return as_divu ( addr, inst, v3, v1, v0, MD_DIVS );
    default: break;
    }
  
  inst->id.name= MD_UNK;
  
  return addr;
  
} /* end or_div_sbcd */


static MDu32
sub_subx (
          MDu32           addr,
          MD_Inst * const inst,
          MDu8 const      v3,
          MDu8 const      v2,
          MDu8 const      v1,
          MDu8 const      v0
          )
{
  
  switch ( v2 )
    {
    case 0: return opb_easrc ( addr, inst, v3, v1, v0, MD_SUBb );
    case 1: return opw_easrc ( addr, inst, v3, v1, v0, MD_SUBw );
    case 2: return opl_easrc ( addr, inst, v3, v1, v0, MD_SUBl );
    case 3: return opw_easrc_A ( addr, inst, v3, v1, v0, MD_SUBw );
    case 4:
      switch ( v1 )
        {
        case 0:
          inst->id.name= MD_SUBXb;
          inst->id.op1= MD_DN; inst->e1.reg= v0;
          inst->id.op2= MD_DN; inst->e2.reg= v3;
          return addr;
        case 1:
          inst->id.name= MD_SUBXb;
          inst->id.op1= MD_pANd; inst->e1.reg= v0;
          inst->id.op2= MD_pANd; inst->e2.reg= v3;
          return addr;
        default: return opb_eadst ( addr, inst, v3, v1, v0, MD_SUBb );
        }
      break;
    case 5:
      switch ( v1 )
        {
        case 0:
          inst->id.name= MD_SUBXw;
          inst->id.op1= MD_DN; inst->e1.reg= v0;
          inst->id.op2= MD_DN; inst->e2.reg= v3;
          return addr;
        case 1:
          inst->id.name= MD_SUBXw;
          inst->id.op1= MD_pANd; inst->e1.reg= v0;
          inst->id.op2= MD_pANd; inst->e2.reg= v3;
          return addr;
        default: return opb_eadst ( addr, inst, v3, v1, v0, MD_SUBw );
        }
      break;
    case 6:
      switch ( v1 )
        {
        case 0:
          inst->id.name= MD_SUBXl;
          inst->id.op1= MD_DN; inst->e1.reg= v0;
          inst->id.op2= MD_DN; inst->e2.reg= v3;
          return addr;
        case 1:
          inst->id.name= MD_SUBXl;
          inst->id.op1= MD_pANd; inst->e1.reg= v0;
          inst->id.op2= MD_pANd; inst->e2.reg= v3;
          return addr;
        default: return opb_eadst ( addr, inst, v3, v1, v0, MD_SUBl );
        }
      break;
    case 7: return opl_easrc_A ( addr, inst, v3, v1, v0, MD_SUBl );
    default: break;
    }
  
  inst->id.name= MD_UNK;
  
  return addr;
  
} /* end sub_subx */


static MDu32
cmp_eor (
         MDu32           addr,
         MD_Inst * const inst,
         MDu8 const      v3,
         MDu8 const      v2,
         MDu8 const      v1,
         MDu8 const      v0
         )
{
  
  switch ( v2 )
    {
    case 0: return opb_easrc ( addr, inst, v3, v1, v0, MD_CMPb );
    case 1: return opw_easrc ( addr, inst, v3, v1, v0, MD_CMPw );
    case 2: return opl_easrc ( addr, inst, v3, v1, v0, MD_CMPl );
    case 3: return opw_easrc_A ( addr, inst, v3, v1, v0, MD_CMPw );
    case 4:
      switch ( v1 )
        {
        case 1:
          inst->id.name= MD_CMPMb;
          inst->id.op1= MD_pANi; inst->e1.reg= v0;
          inst->id.op2= MD_pANi; inst->e2.reg= v3;
          return addr;
        default: return as_eorb ( addr, inst, v3, v1, v0, MD_EORb );
        }
      break;
    case 5:
      switch ( v1 )
        {
        case 1:
          inst->id.name= MD_CMPMw;
          inst->id.op1= MD_pANi; inst->e1.reg= v0;
          inst->id.op2= MD_pANi; inst->e2.reg= v3;
          return addr;
        default: return as_eorb ( addr, inst, v3, v1, v0, MD_EORw );
        }
      break;
    case 6:
      switch ( v1 )
        {
        case 1:
          inst->id.name= MD_CMPMl;
          inst->id.op1= MD_pANi; inst->e1.reg= v0;
          inst->id.op2= MD_pANi; inst->e2.reg= v3;
          return addr;
        default: return as_eorb ( addr, inst, v3, v1, v0, MD_EORl );
        }
      break;
    case 7: return opl_easrc_A ( addr, inst, v3, v1, v0, MD_CMPl );
    default: break;
    }
  
  inst->id.name= MD_UNK;
  
  return addr;
  
} /* end cmp_eor */


static MDu32
and_mul_abcd_exg (
        	  MDu32           addr,
        	  MD_Inst * const inst,
        	  MDu8 const      v3,
        	  MDu8 const      v2,
                  MDu8 const      v1,
                  MDu8 const      v0
                  )
{
  
  
  switch ( v2 )
    {
    case 0:
      switch ( v1 )
        {
        case 1: break;
        default: return opb_easrc ( addr, inst, v3, v1, v0, MD_ANDb );
        }
      break;
    case 1:
      switch ( v1 )
        {
        case 1: break;
        default: return opw_easrc ( addr, inst, v3, v1, v0, MD_ANDw );
        }
      break;
    case 2:
      switch ( v1 )
        {
        case 1: break;
        default: return opl_easrc ( addr, inst, v3, v1, v0, MD_ANDl );
        }
      break;
    case 3: return as_divu ( addr, inst, v3, v1, v0, MD_MULU );
    case 4:
      switch ( v1 )
        {
        case 0:
          inst->id.name= MD_ABCD;
          inst->id.op1= MD_DN; inst->e1.reg= v0;
          inst->id.op2= MD_DN; inst->e2.reg= v3;
          return addr;
        case 1:
          inst->id.name= MD_ABCD;
          inst->id.op1= MD_pANd; inst->e1.reg= v0;
          inst->id.op2= MD_pANd; inst->e2.reg= v3;
          return addr;
        default: return opb_eadst ( addr, inst, v3, v1, v0, MD_ANDb );
        }
      break;
    case 5:
      switch ( v1 )
        {
        case 0: /* EXG Dx,Dy */
          inst->id.name= MD_EXG;
          inst->id.op1= MD_DN; inst->e1.reg= v3;
          inst->id.op2= MD_DN; inst->e2.reg= v0;
          return addr;
        case 1: /* EXG Ax,Ay */
          inst->id.name= MD_EXG;
          inst->id.op1= MD_AN; inst->e1.reg= v3;
          inst->id.op2= MD_AN; inst->e2.reg= v0;
          return addr;
        default: return opb_eadst ( addr, inst, v3, v1, v0, MD_ANDw );
        }
      break;
    case 6:
      switch ( v1 )
        {
        case 1: /* EXG Dx,Ay */
          inst->id.name= MD_EXG;
          inst->id.op1= MD_DN; inst->e1.reg= v3;
          inst->id.op2= MD_AN; inst->e2.reg= v0;
          return addr;
        default: return opb_eadst ( addr, inst, v3, v1, v0, MD_ANDl );
        }
      break;
    case 7: return as_divu ( addr, inst, v3, v1, v0, MD_MULS );
    default: break;
    }
  
  inst->id.name= MD_UNK;
  
  return addr;
  
} /* end and_mul_abcd_exg */


static MDu32
add_addx (
          MDu32           addr,
          MD_Inst * const inst,
          MDu8 const      v3,
          MDu8 const      v2,
          MDu8 const      v1,
          MDu8 const      v0
          )
{
  
  switch ( v2 )
    {
    case 0: return opb_easrc ( addr, inst, v3, v1, v0, MD_ADDb );
    case 1: return opw_easrc ( addr, inst, v3, v1, v0, MD_ADDw );
    case 2: return opl_easrc ( addr, inst, v3, v1, v0, MD_ADDl );
    case 3: return opw_easrc_A ( addr, inst, v3, v1, v0, MD_ADDw );
    case 4:
      switch ( v1 )
        {
        case 0:
          inst->id.name= MD_ADDXb;
          inst->id.op1= MD_DN; inst->e1.reg= v0;
          inst->id.op2= MD_DN; inst->e2.reg= v3;
          return addr;
        case 1:
          inst->id.name= MD_ADDXb;
          inst->id.op1= MD_pANd; inst->e1.reg= v0;
          inst->id.op2= MD_pANd; inst->e2.reg= v3;
          return addr;
        default: return opb_eadst ( addr, inst, v3, v1, v0, MD_ADDb );
        }
      break;
    case 5:
      switch ( v1 )
        {
        case 0:
          inst->id.name= MD_ADDXw;
          inst->id.op1= MD_DN; inst->e1.reg= v0;
          inst->id.op2= MD_DN; inst->e2.reg= v3;
          return addr;
        case 1:
          inst->id.name= MD_ADDXw;
          inst->id.op1= MD_pANd; inst->e1.reg= v0;
          inst->id.op2= MD_pANd; inst->e2.reg= v3;
          return addr;
        default: return opb_eadst ( addr, inst, v3, v1, v0, MD_ADDw );
        }
      break;
    case 6:
      switch ( v1 )
        {
        case 0:
          inst->id.name= MD_ADDXl;
          inst->id.op1= MD_DN; inst->e1.reg= v0;
          inst->id.op2= MD_DN; inst->e2.reg= v3;
          return addr;
        case 1:
          inst->id.name= MD_ADDXl;
          inst->id.op1= MD_pANd; inst->e1.reg= v0;
          inst->id.op2= MD_pANd; inst->e2.reg= v3;
          return addr;
        default: return opb_eadst ( addr, inst, v3, v1, v0, MD_ADDl );
        }
      break;
    case 7: return opl_easrc_A ( addr, inst, v3, v1, v0, MD_ADDl );
    default: break;
    }
  
  inst->id.name= MD_UNK;
  
  return addr;
  
} /* end add_addx */


static MDu32
shift_rot_bit (
               MDu32           addr,
               MD_Inst * const inst,
               MDu8 const      v3,
               MDu8 const      v2,
               MDu8 const      v1,
               MDu8 const      v0
               )
{
  
  switch ( v2 )
    {
    case 0:
      switch ( v1 )
        {
        case 0: return sopb_inm ( addr, inst, v3, v0, MD_ASRb );
        case 1: return sopb_inm ( addr, inst, v3, v0, MD_LSRb );
        case 2: return sopb_inm ( addr, inst, v3, v0, MD_ROXRb );
        case 3: return sopb_inm ( addr, inst, v3, v0, MD_RORb );
        case 4: return sopb_reg ( addr, inst, v3, v0, MD_ASRb );
        case 5: return sopb_reg ( addr, inst, v3, v0, MD_LSRb );
        case 6: return sopb_reg ( addr, inst, v3, v0, MD_ROXRb );
        case 7: return sopb_reg ( addr, inst, v3, v0, MD_RORb );
        default: break;
        }
      break;
    case 1:
      switch ( v1 )
        {
        case 0: return sopb_inm ( addr, inst, v3, v0, MD_ASRw );
        case 1: return sopb_inm ( addr, inst, v3, v0, MD_LSRw );
        case 2: return sopb_inm ( addr, inst, v3, v0, MD_ROXRw );
        case 3: return sopb_inm ( addr, inst, v3, v0, MD_RORw );
        case 4: return sopb_reg ( addr, inst, v3, v0, MD_ASRw );
        case 5: return sopb_reg ( addr, inst, v3, v0, MD_LSRw );
        case 6: return sopb_reg ( addr, inst, v3, v0, MD_ROXRw );
        case 7: return sopb_reg ( addr, inst, v3, v0, MD_RORw );
        default: break;
        }
      break;
    case 2:
      switch ( v1 )
        {
        case 0: return sopb_inm ( addr, inst, v3, v0, MD_ASRl );
        case 1: return sopb_inm ( addr, inst, v3, v0, MD_LSRl );
        case 2: return sopb_inm ( addr, inst, v3, v0, MD_ROXRl );
        case 3: return sopb_inm ( addr, inst, v3, v0, MD_RORl );
        case 4: return sopb_reg ( addr, inst, v3, v0, MD_ASRl );
        case 5: return sopb_reg ( addr, inst, v3, v0, MD_LSRl );
        case 6: return sopb_reg ( addr, inst, v3, v0, MD_ROXRl );
        case 7: return sopb_reg ( addr, inst, v3, v0, MD_RORl );
        default: break;
        }
      break;
    case 3:
      switch ( v3 )
        {
        case 0: return sop_mem ( addr, inst, v1, v0, MD_ASRw );
        case 1: return sop_mem ( addr, inst, v1, v0, MD_LSRw );
        case 2: return sop_mem ( addr, inst, v1, v0, MD_ROXRw );
        case 3: return sop_mem ( addr, inst, v1, v0, MD_RORw );
        default: break;
        }
      break;
    case 4:
      switch ( v1 )
        {
        case 0: return sopb_inm ( addr, inst, v3, v0, MD_ASLb );
        case 1: return sopb_inm ( addr, inst, v3, v0, MD_LSLb );
        case 2: return sopb_inm ( addr, inst, v3, v0, MD_ROXLb );
        case 3: return sopb_inm ( addr, inst, v3, v0, MD_ROLb );
        case 4: return sopb_reg ( addr, inst, v3, v0, MD_ASLb );
        case 5: return sopb_reg ( addr, inst, v3, v0, MD_LSLb );
        case 6: return sopb_reg ( addr, inst, v3, v0, MD_ROXLb );
        case 7: return sopb_reg ( addr, inst, v3, v0, MD_ROLb );
        default: break;
        }
      break;
    case 5:
      switch ( v1 )
        {
        case 0: return sopb_inm ( addr, inst, v3, v0, MD_ASLw );
        case 1: return sopb_inm ( addr, inst, v3, v0, MD_LSLw );
        case 2: return sopb_inm ( addr, inst, v3, v0, MD_ROXLw );
        case 3: return sopb_inm ( addr, inst, v3, v0, MD_ROLw );
        case 4: return sopb_reg ( addr, inst, v3, v0, MD_ASLw );
        case 5: return sopb_reg ( addr, inst, v3, v0, MD_LSLw );
        case 6: return sopb_reg ( addr, inst, v3, v0, MD_ROXLw );
        case 7: return sopb_reg ( addr, inst, v3, v0, MD_ROLw );
        default: break;
        }
      break;
    case 6:
      switch ( v1 )
        {
        case 0: return sopb_inm ( addr, inst, v3, v0, MD_ASLl );
        case 1: return sopb_inm ( addr, inst, v3, v0, MD_LSLl );
        case 2: return sopb_inm ( addr, inst, v3, v0, MD_ROXLl );
        case 3: return sopb_inm ( addr, inst, v3, v0, MD_ROLl );
        case 4: return sopb_reg ( addr, inst, v3, v0, MD_ASLl );
        case 5: return sopb_reg ( addr, inst, v3, v0, MD_LSLl );
        case 6: return sopb_reg ( addr, inst, v3, v0, MD_ROXLl );
        case 7: return sopb_reg ( addr, inst, v3, v0, MD_ROLl );
        default: break;
        }
      break;
    case 7:
      switch ( v3 )
        {
        case 0: return sop_mem ( addr, inst, v1, v0, MD_ASLw );
        case 1: return sop_mem ( addr, inst, v1, v0, MD_LSLw );
        case 2: return sop_mem ( addr, inst, v1, v0, MD_ROXLw );
        case 3: return sop_mem ( addr, inst, v1, v0, MD_ROLw );
        default: break;
        }
      break;
    default: break;
    }
  
  inst->id.name= MD_UNK;
  
  return addr;
  
} /* end shift_rot_bit */




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

MDu32
MD_cpu_decode (
               MDu32    addr,
               MD_Inst *inst
               )
{
  
  MD_Word opword;
  unpack_t uword;
  
  
  opword= MD_mem_read ( addr );
  inst->nbytes= 2;
  inst->bytes[0]= opword.b.v1; inst->bytes[1]= opword.b.v0;
  addr+= 2;
  uword= MD_unpack__[opword.v];
  switch ( uword.op )
    {
    case 0x0: return bit_movep_inm ( addr, inst, uword.v3,
        			     uword.v2, uword.v1, uword.v0 );
    case 0x1: return moveb ( addr, inst, uword.v3, uword.v2,
        		     uword.v1, uword.v0 );
    case 0x2: return movel ( addr, inst, uword.v3, uword.v2,
        		     uword.v1, uword.v0 );
    case 0x3: return movew ( addr, inst, uword.v3, uword.v2,
        		     uword.v1, uword.v0 );
    case 0x4: return miscellaneous ( addr, inst, uword.v3, uword.v2,
        			     uword.v1, uword.v0 );
    case 0x5: return addq_subq__ ( addr, inst, uword.v3, uword.v2,
        			   uword.v1, uword.v0 );
    case 0x6: return bcc_bsr_bra ( addr, inst, opword.b.v1&0xF, opword.b.v0 );
    case 0x7: /* MOVEQ */
      if ( opword.v&0x0100 ) { inst->id.name= MD_UNK; return addr; }
      inst->id.name= MD_MOVEQ;
      inst->id.op1= MD_INMl;
      inst->e1.longval= (MDs8) opword.b.v0;
      inst->id.op2= MD_DN;
      inst->e2.reg= uword.v3;
      return addr;
    case 0x8: return or_div_sbcd ( addr, inst, uword.v3, uword.v2,
        			   uword.v1, uword.v0 );
    case 0x9: return sub_subx ( addr, inst, uword.v3, uword.v2,
        			uword.v1, uword.v0 );
    case 0xB: return cmp_eor ( addr, inst, uword.v3, uword.v2,
        		       uword.v1, uword.v0 );
    case 0xC: return and_mul_abcd_exg ( addr, inst, uword.v3, uword.v2,
        				uword.v1, uword.v0 );
    case 0xD: return add_addx ( addr, inst, uword.v3, uword.v2,
        			uword.v1, uword.v0 );
    case 0xE: return shift_rot_bit ( addr, inst, uword.v3, uword.v2,
        			     uword.v1, uword.v0 );
    default: inst->id.name= MD_UNK;
    }
  
  return addr;
  
} /* end MD_cpu_decode */
