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
 *  z80.c - Implementació del control del Z80.
 *
 *  NOTA: No sé si és el funcionament correcte però vaig a assumir el
 *  següent:
 *
 *  - busreq i reset van cadascun per una banda.
 *  - Quan busreq està actiu el processador està parat.
 *  - Quan reset passa de 1 -> 0 es produëix un reset.
 *
 */


#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "MD.h"




/**********/
/* MACROS */
/**********/

#define SAVE(VAR)                                               \
  if ( fwrite ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1

#define LOAD(VAR)                                               \
  if ( fread ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1

#define CHECK(COND)                             \
  if ( !(COND) ) return -1;




/*********/
/* ESTAT */
/*********/

static Z80u8 _ram[8192]; /* 8K */

/* Per a llegir de la memòria del 68K. */
static struct
{
  
  MDu32 addr;
  MDu32 addr_tmp;
  MDu32 bit;
  
} _bank_select;

/* Per al timing. */
static int _cc;

/* Callbacks. */
static MD_CPUStepZ80 *_cpu_step;
static MD_MemAccessZ80 *_mem_access;
static void *_udata;

/* Contorl. */
static struct
{
  
  MD_Bool busreq;
  MD_Bool reset;
  
} _control;

/* Funcions per a accedir a memòria. */
static Z80u8 (*_mem_read) (Z80u16 addr);
static void (*_mem_write) (Z80u16 addr,Z80u8 data);




/*********************/
/* FUNCIONS PRIVADES */
/*********************/
#include <stdio.h>
static Z80u8
mem_read (
          Z80u16 addr
          )
{
  
  /* RAM */
  if ( addr < 0x2000 ) return _ram[addr];
  
  /* Registres */
  else if ( addr < 0x8000 )
    {
      switch ( addr )
        {
        case 0x4000:
        case 0x4001:
        case 0x4002:
        case 0x4003: return MD_fm_status ();
        case 0x7F11: return 0x00;
        default: return 0x00;
        }
    }
  
  /* 68K memory. */
  else return (Z80u8) MD_mem_read8 ( _bank_select.addr | (addr&0x7FFF) );
  
} /* end mem_read */


static Z80u8
mem_read_trace (
        	const Z80u16 addr
        	)
{
  
  MDu8 data;
  
  
  data= mem_read ( addr );
  _mem_access ( MD_READ, addr, data, _udata );
  
  return data;
  
} /* end mem_read_trace */


static void
mem_write (
           Z80u16 addr,
           Z80u8  data
           )
{
  
  /* RAM */
  if ( addr < 0x2000 ) _ram[addr]= data;
  
  /* Registres */
  else if ( addr < 0x8000 )
    {
      switch ( addr )
        {
        case 0x4000: MD_fm_part1_set_addr ( (MDu8) data ); break;
        case 0x4001: MD_fm_part1_write_data ( (MDu8) data ); break;
        case 0x4002: MD_fm_part2_set_addr ( (MDu8) data ); break;
        case 0x4003: MD_fm_part2_write_data ( (MDu8) data ); break;
        case 0x6000:
          _bank_select.addr_tmp=
            (((MDu32) (data&0x1))<<23) | ((_bank_select.addr_tmp>>1)&0xFF8000);
          if ( ++_bank_select.bit == 9 )
            {
              _bank_select.bit= 0;
              _bank_select.addr= _bank_select.addr_tmp;
            }
          break;
        case 0x7F11: MD_psg_control ( data ); break;
        default: break;
        }
    }
  
  /* 68K memory. */
  else MD_mem_write8 ( _bank_select.addr | (addr&0x7FFF), (Z80u8) data );
  
} /* end mem_write */


static void
mem_write_trace (
        	 Z80u16 addr,
        	 Z80u8  data
        	 )
{
  
  mem_write ( addr, data );
  _mem_access ( MD_WRITE, addr, data, _udata );
  
} /* end mem_write_trace */


static void
set_mode_mem_trace (
        	    const MD_Bool val
        	    )
{
  
  if ( _mem_access != NULL )
    {
      if ( val )
        {
          _mem_read= mem_read_trace;
          _mem_write= mem_write_trace;
        }
      else
        {
          _mem_read= mem_read;
          _mem_write= mem_write;
        }
    }
  
} /* end set_mode_mem_trace */




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

MDu8
MD_z80_busreq_read (void)
{
  return _control.busreq ? 0x00 : 0x01;
} /* end MD_z80_busreq_read */


void
MD_z80_busreq_write (
        	     const MDu8 data
        	     )
{
  _control.busreq= ((data&0x1)!=0);
} /* end MD_z80_busreq_write */


void
MD_z80_clock (
              const int cc
              )
{

  // NOTA!! La fórmula per a obtindre els cicles del Z80 és: (cc*7)/15
  
  if ( _control.busreq ) return;
  
  _cc+= 7*cc;
  while ( _cc >= 15 )
    _cc-= 15 * Z80_run ();
  
} // end MD_z80_clock


void
MD_z80_init (
             MD_CPUStepZ80   *cpu_step,
             MD_MemAccessZ80 *mem_access,
             MD_Warning      *warning,
             void            *udata
             )
{
  
  /* Inicialitza processador. */
  Z80_init ( warning, udata );
  
  /* callback. */
  _cpu_step= cpu_step;
  _mem_access= mem_access;
  _udata= udata;
  
  /* Funcions. */
  _mem_read= mem_read;
  _mem_write= mem_write;

  MD_z80_init_state ();
  
} /* end MD_z80_init */


void
MD_z80_init_state (void)
{
  
  /* Inicialitza memòria. */
  memset ( _ram, 0, 8192 );
  _bank_select.addr= 0;
  _bank_select.addr_tmp= 0;
  _bank_select.bit= 0;
  
  /* Inicialitza processador. */
  Z80_init_state ();
  
  /* Inicialitza timing. */
  _cc= 0;
  
  /* control. */
  _control.busreq= MD_TRUE;
  _control.reset= MD_TRUE;
  
} /* end MD_z80_init_state */


void
MD_z80_reset (
              const MDu8 data
              )
{
  
  MD_Bool reset;
  
  
  reset= (data&0x01)!=0;
  if ( _control.reset && !reset )
    {
      Z80_reset ();
      MD_fm_reset ();
    }
  _control.reset= reset;
  
} /* end MD_z80_reset */


void
MD_z80_trace (
              const int cc
              )
{
  
  Z80u16 addr;
  Z80_Step step;
  

  if ( _control.busreq ) return;
  
  _cc+= 7*cc;
  while ( _cc >= 15 )
    {
      if ( _cpu_step != NULL )
        {
          addr= Z80_decode_next_step ( &step );
          _cpu_step ( &step, addr, _udata );
        }
      set_mode_mem_trace ( MD_TRUE );
      _cc-= 15 * Z80_run ();
      set_mode_mem_trace ( MD_FALSE );
    }
  
} /* end MD_z80_trace */


Z80u8
Z80_io_read (
             Z80u8 port
             )
{
  return 0x00;
} /* end Z80_io_read */


void
Z80_io_write (
              Z80u8 port,
              Z80u8 data
              )
{
} /* end Z80_io_write */


Z80u8 Z80_read (
        	Z80u16 addr
        	)
{
  return _mem_read ( addr );
} /* end Z80_read */

void
Z80_reti_signal (void)
{
} /* end Z80_reti_signal */


void Z80_write (
        	Z80u16 addr,
        	Z80u8  data
        	)
{
  _mem_write ( addr, data );
} /* end Z80_write */


int
MD_z80_save_state (
        	   FILE *f
        	   )
{

  if ( Z80_save_state ( f ) != 0 ) return -1;
  SAVE ( _ram );
  SAVE ( _bank_select );
  SAVE ( _cc );
  SAVE ( _control );

  return 0;
  
} /* end MD_z80_save_state */


int
MD_z80_load_state (
        	   FILE *f
        	   )
{
  
  if ( Z80_load_state ( f ) != 0 ) return -1;
  LOAD ( _ram );
  LOAD ( _bank_select );
  CHECK ( (_bank_select.addr&0xFF8000) == _bank_select.addr );
  CHECK ( (_bank_select.addr_tmp&0xFF8000) == _bank_select.addr_tmp );
  CHECK ( _bank_select.bit >= 0 && _bank_select.bit <= 8 );
  LOAD ( _cc );
  LOAD ( _control );
  
  return 0;
  
} /* end MD_z80_load_state */
