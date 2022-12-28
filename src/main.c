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
 *  main.c - Implementació del mòdul principal.
 *
 */


#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "MD.h"




/*************/
/* CONSTANTS */
/*************/

/* Un valor un poc arreu. Si cada T és correspon amb un cicle de
   rellotge que va a 7.61MHz (7.67MHz NTSC) approx, tenim que és
   comprova cada 1/100 segons. */
static const int CCTOCHECK= 76100;

static const char MDSTATE[]= "MDSTATE\n";




/*********/
/* ESTAT */
/*********/

/* Senyals. */
static MD_Bool _stop;
static MD_Bool _reset;

/* Frontend. */
static MD_CheckSignals *_check;
static MD_Warning *_warning;
static void *_udata;

/* Callback per a la UCP. */
static MD_CPUStep *_cpu_step;

/* Versió. */
static MDu8 _version_no;

// Indica que el SVP està actiu (no s'ha de guardar en l'estat)
static MD_Bool _svp_enabled;




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static void
reset (void)
{
  
  /*
   * NOTA: Al fer el RESET seguint aquest patró estem resetejant també
   * el FM.
   */
  
  MD_cpu_set_reset ();
  MD_z80_busreq_write ( 0x01 ); /* Agarra el bus. */
  MD_z80_reset ( 0x00 ); /* Activa senyal del Z80. */
  MD_z80_busreq_write ( 0x00 ); /* Torna el bus. */
  MD_z80_reset ( 0x01 ); /* Fromalitza el reset. */
  MD_z80_busreq_write ( 0x01 ); /* Agarra el bus per a que estiga parat. */
  if ( _svp_enabled ) MD_svp_reset ();
  _reset= MD_FALSE;
  
} /* end reset */




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
MD_close (void)
{
  
  MD_eeprom_close ();
  MD_io_close ();
  
} /* end MD_close */


void
MD_init (
         const MD_Rom      *rom,
         const MDu8         model_flags,
         const MD_Frontend *frontend,
         void              *udata
         )
{
  
  /* Callbacks. */
  _check= frontend->check;
  _warning= frontend->warning;
  _udata= udata;
  _cpu_step= frontend->trace!=NULL ?
    frontend->trace->cpu_step:NULL;
  
  /* VERSION NO. */
  /* Si dixe VER a 0 m'assegure de que no fa la tonteria del TMMS. */
  _version_no= 0x00;
  if ( model_flags&MD_MODEL_OVERSEAS ) _version_no|= 0x80;
  if ( model_flags&MD_MODEL_PAL ) _version_no|= 0x40;
  
  /* Mòduls. */
  _svp_enabled= MD_svp_init ( rom,
                              frontend->warning,
                              frontend->trace!=NULL?
                              frontend->trace->cpu_step_svp : NULL,
                              udata );
  MD_eeprom_init ( rom, frontend->get_eeprom, udata );
  MD_mem_init ( rom,
                _svp_enabled,
        	frontend->get_static_ram,
        	frontend->trace!=NULL?
        	frontend->trace->mem_access:NULL,
        	frontend->trace!=NULL?
        	frontend->trace->mem_access8:NULL,
                frontend->warning,
        	udata );
  MD_cpu_init ( frontend->warning, udata );
  MD_z80_init ( frontend->trace!=NULL ?
        	frontend->trace->cpu_step_z80 : NULL,
        	frontend->trace!=NULL ?
        	frontend->trace->mem_access_z80 : NULL,
        	frontend->warning, udata );
  MD_vdp_init ( (model_flags&MD_MODEL_PAL)!=0,
        	frontend->sres_changed,
        	frontend->update_screen,
        	frontend->warning, udata );
  MD_vdp_set_dma_lag ( _svp_enabled ? 2 : 0 );
  MD_io_init ( frontend->plugged_devs, frontend->check_buttons, udata );
  MD_fm_init ( frontend->warning, udata );
  MD_psg_init ();
  MD_audio_init ( frontend->warning, frontend->play_sound, udata );
  
} /* end MD_init */


int
MD_iter (
         MD_Bool *stop
         )
{

  static int CC= 0;
  int cc, ret;
  MD_Bool dma_mem2vram;
  
  
  ret= cc= MD_cpu_run ();
  MD_z80_clock ( cc );
  if ( _svp_enabled ) MD_svp_clock ( cc );
  MD_fm_clock ( cc );
  MD_psg_clock ( cc );
  CC+= cc;
  while ( (dma_mem2vram= MD_vdp_clock ( cc )) )
    {
      ret+= cc= MD_vdp_dma_mem2vram_step ();
      MD_z80_clock ( cc );
      if ( _svp_enabled ) MD_svp_clock ( cc );
      MD_fm_clock ( cc );
      MD_psg_clock ( cc );
      CC+= cc;
    }
  if ( CC >= CCTOCHECK && _check != NULL )
    {
      CC-= CCTOCHECK;
      _check ( stop, &_reset, _udata );
      if ( _reset ) reset ();
    }

  return ret;
  
} // end MD_iter


MD_RGB
MD_color2rgb (
              const int color
              )
{
  
  MD_RGB ret;
  int r,g,b;
  const double frac= 255.0/7.0;
  const double frac2= 127.0/7.0;

  
  r= (color&0x7);
  g= (color&0x38)>>3;
  b= (color&0x1C0)>>6;
  switch ( (color&0x600)>>9 )
    {
    case 1: /* Shadow */
      ret.r= (MDu8) (r*frac2+0.5);
      ret.g= (MDu8) (g*frac2+0.5);
      ret.b= (MDu8) (b*frac2+0.5);
      break;
    case 2: /* Highlight */
      ret.r= ((MDu8) (r*frac2+0.5)) + 128;
      ret.g= ((MDu8) (g*frac2+0.5)) + 128;
      ret.b= ((MDu8) (b*frac2+0.5)) + 128;
      break;
    default: /* Normal. */
      ret.r= (MDu8) (r*frac+0.5);
      ret.g= (MDu8) (g*frac+0.5);
      ret.b= (MDu8) (b*frac+0.5);
      break;
    }

  return ret;
  
#if 0
  /* Codi vell*/
  ret.r= (color&0x7)<<1;
  ret.g= (color&0x38)>>2;
  ret.b= (color&0x1C0)>>5;
  switch ( (color&0x600)>>9 )
    {
    case 1: /* Shadow */
      ret.r>>= 1;
      ret.g>>= 1;
      ret.b>>= 1;
      break;
    case 2: /* Highlight */
      ret.r= (ret.r>>1)+7;
      ret.g= (ret.g>>1)+7;
      ret.b= (ret.b>>1)+7;
      break;
    default: break;
    }
  ret.r|= ret.r<<4;
  ret.g|= ret.g<<4;
  ret.b|= ret.b<<4;

  return ret;
#endif
  
} /* end MD_color2rgb */


void
MD_loop (void)
{
  
  int cc, CC;
  MD_Bool dma_mem2vram;
  
  
  _stop= _reset= MD_FALSE;
  if ( _check == NULL )
    {
      while ( !_stop )
        {
          if ( _reset ) reset ();
          cc= MD_cpu_run ();
          MD_z80_clock ( cc );
          if ( _svp_enabled ) MD_svp_clock ( cc );
          MD_fm_clock ( cc );
          MD_psg_clock ( cc );
          while ( (dma_mem2vram= MD_vdp_clock ( cc )) )
            {
              cc= MD_vdp_dma_mem2vram_step ();
              MD_z80_clock ( cc );
              if ( _svp_enabled ) MD_svp_clock ( cc );
              MD_fm_clock ( cc );
              MD_psg_clock ( cc );
            }
        }
    }
  else
    {
      CC= 0;
      for (;;)
        {
          cc= MD_cpu_run ();
          MD_z80_clock ( cc );
          if ( _svp_enabled ) MD_svp_clock ( cc );
          MD_fm_clock ( cc );
          MD_psg_clock ( cc );
          CC+= cc;
          while ( (dma_mem2vram= MD_vdp_clock ( cc )) )
            {
              cc= MD_vdp_dma_mem2vram_step ();
              MD_z80_clock ( cc );
              if ( _svp_enabled ) MD_svp_clock ( cc );
              MD_fm_clock ( cc );
              MD_psg_clock ( cc );
              CC+= cc;
            }
          if ( CC >= CCTOCHECK )
            {
              CC-= CCTOCHECK;
              _check ( &_stop, &_reset, _udata );
              if ( _stop ) break;
              if ( _reset ) reset ();
            }
        }
    }
  _stop= _reset= MD_FALSE;
  
} /* end MD_loop */


MDu8
MD_version_no (void)
{
  return _version_no;
} /* end MD_version_no */


void
MD_reset (void)
{
  _reset= MD_TRUE;
} /* end MD_reset */


void
MD_stop (void)
{
  _stop= MD_TRUE;
} /* end MD_stop */


int
MD_trace (void)
{
  
  int cc;
  MDu32 addr;
  MD_Step step;
  MD_Bool dma_mem2vram;
  
  
  if ( _cpu_step != NULL )
    {
      addr= MD_cpu_decode_next_step ( &step );
      _cpu_step ( &step, addr, _udata );
    }
  MD_mem_set_mode_trace ( MD_TRUE );
  cc= MD_cpu_run ();
  MD_z80_trace ( cc );
  if ( _svp_enabled ) MD_svp_trace ( cc );
  MD_fm_clock ( cc );
  MD_psg_clock ( cc );
  while ( (dma_mem2vram= MD_vdp_clock ( cc )) )
    {
      cc= MD_vdp_dma_mem2vram_step ();
      MD_z80_trace ( cc );
      if ( _svp_enabled ) MD_svp_trace ( cc );
      MD_fm_clock ( cc );
      MD_psg_clock ( cc );
    }
  MD_mem_set_mode_trace ( MD_FALSE );
  
  return cc;
  
} /* end MD_trace */


int
MD_load_state (
               FILE *f
               )
{

  static char buf[sizeof(MDSTATE)];

  
  _stop= _reset= MD_FALSE;
  
  /* MDSTATE. */
  if ( fread ( buf, sizeof(MDSTATE)-1, 1, f ) != 1 ) goto error;
  buf[sizeof(MDSTATE)-1]= '\0';
  if ( strcmp ( buf, MDSTATE ) ) goto error;
  
  /* Carrega. */
  if ( MD_cpu_load_state ( f ) != 0 ) goto error;
  if ( MD_z80_load_state ( f ) != 0 ) goto error;
  if ( MD_eeprom_load_state ( f ) != 0 ) goto error;
  if ( MD_svp_load_state ( f ) != 0 ) goto error;
  if ( MD_mem_load_state ( f ) != 0 ) goto error;
  if ( MD_io_load_state ( f ) != 0 ) goto error;
  if ( MD_psg_load_state ( f ) != 0 ) goto error;
  if ( MD_fm_load_state ( f ) != 0 ) goto error;
  if ( MD_audio_load_state ( f ) != 0 ) goto error;
  if ( MD_vdp_load_state ( f ) != 0 ) goto error;
  
  return 0;
  
 error:
  _warning ( _udata,
             "error al carregar l'estat del simulador des d'un fitxer" );
  MD_cpu_init_state ();
  MD_z80_init_state ();
  MD_eeprom_init_state ();
  MD_svp_init_state ();
  MD_mem_init_state ();
  MD_io_init_state ();
  MD_psg_init_state ();
  MD_fm_init_state ();
  MD_audio_init_state ();
  MD_vdp_init_state ();
  return -1;
  
} // end MD_load_state


int
MD_save_state (
               FILE *f
               )
{
  
  if ( fwrite ( MDSTATE, sizeof(MDSTATE)-1, 1, f ) != 1 ) return -1;
  if ( MD_cpu_save_state ( f ) != 0 ) return -1;
  if ( MD_z80_save_state ( f ) != 0 ) return -1;
  if ( MD_eeprom_save_state ( f ) != 0 ) return -1;
  if ( MD_svp_save_state ( f ) != 0 ) return -1;
  if ( MD_mem_save_state ( f ) != 0 ) return -1;
  if ( MD_io_save_state ( f ) != 0 ) return -1;
  if ( MD_psg_save_state ( f ) != 0 ) return -1;
  if ( MD_fm_save_state ( f ) != 0 ) return -1;
  if ( MD_audio_save_state ( f ) != 0 ) return -1;
  if ( MD_vdp_save_state ( f ) != 0 ) return -1;
  
  return 0;
  
} // end MD_save_state
