/*
 * Copyright 2022-2023 Adrià Giménez Pastor.
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
 *  audio.c - Implementació del mòdul 'audio'.
 *
 *  Timings respecte al master clock (7*CLOCK_CPU_68K)
 *
 *    - FM:  1 mostra --> 7*144 = 1008
 *
 *    - PSG: 1 mostra --> 15*16 = 240
 *
 *  Aproximadament 4.2PSG mostres per FM mostra
 *
 *  L'objectiu és generar una eixida amb la freqüència del FM. El que
 *  vaig a fer és desar les mostres de PSG en un buffer múltiple de
 *  240 i cada 1008 mostres generar una mostra fm.
 *
 */


#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "MD.h"




/**********/
/* MACROS */
/**********/

#define PSG_STEPS 5
#define PSG_SAMPLES_PER_FM 5

#define PSG_CYCLES 240

#define FM_CYCLES 1008

// Per si de cas es generen de colp moltes mostres
#define BUF_SIZE 50
#define PSG_BUF_SIZE (BUF_SIZE*PSG_SAMPLES_PER_FM)


#define SAVE(VAR)                                               \
  if ( fwrite ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1

#define LOAD(VAR)                                               \
  if ( fread ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1

#define CHECK(COND)                             \
  if ( !(COND) ) return -1;




/*************/
/* CONSTANTS */
/*************/

// Cada mostra FM es corresponen amb 4.2 de PSG. Açò serveix per a
// passar de PSG a FM a partir de 5 mostres, consumint 4 mostres cada
// vegada.
static const struct
{
  int    discard;
  double ws[PSG_SAMPLES_PER_FM];
} PSG_STEP_WEIGHTS[PSG_STEPS]= {

  // STEP 0: 240 + 240 + 240 + 240 + 48
  { 4,
    {0.23809523809523808,0.23809523809523808,0.23809523809523808,
     0.23809523809523808,0.047619047619047616}
  },

  // STEP 1: 192 + 240 + 240 + 240 + 96
  { 4,
    {0.19047619047619047,0.23809523809523808,0.23809523809523808,
     0.23809523809523808,0.09523809523809523}
  },

  // STEP 2: 144 + 240 + 240 + 240 + 144
  { 4,
    {0.14285714285714285,0.23809523809523808,0.23809523809523808,
     0.23809523809523808,0.14285714285714285}
  },

  // STEP 3: 96 + 240 + 240 + 240 + 192
  { 4,
    {0.09523809523809523,0.23809523809523808,0.23809523809523808,
     0.23809523809523808,0.19047619047619047}
  },

  // STEP 4: 48 + 240 + 240 + 240 + 240
  { 5,
    {0.047619047619047616,0.23809523809523808,0.23809523809523808,
     0.23809523809523808,0.23809523809523808}
  }
};




/*********/
/* ESTAT */
/*********/

static MD_Warning *_warning;
static MD_PlaySound *_play_sound;
static void *_udata;

// Buffer mostres PSG transformades a master clock cycles.
static struct
{
  double v[PSG_BUF_SIZE];
  int    p;
  int    N;
  int    step;
} _psg;


// Buffer mostres FM sense transformar
static struct
{
  int16_t l[BUF_SIZE];
  int16_t r[BUF_SIZE];
  int     p;
  int     N;
} _fm;

// Buffer d'eixida
static struct
{
  MDs16 v[MD_FM_BUFFER_SIZE*2];
  int   N;
} _out;




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static void
render_samples (void)
{

  double sample;
  int i,pos;
  int32_t psg_sample,fm_left,fm_right,val;
  
  
  while ( _fm.N > 0 && _psg.N >= PSG_SAMPLES_PER_FM )
    {

      // Renderitza mostra PSG.
      sample= 0.0;
      for ( i= 0; i < PSG_SAMPLES_PER_FM; ++i )
        {
          pos= (_psg.p+i)%PSG_BUF_SIZE;
          sample+= _psg.v[pos]*PSG_STEP_WEIGHTS[_psg.step].ws[i];
        }
      _psg.N-= PSG_STEP_WEIGHTS[_psg.step].discard;
      _psg.p= (_psg.p+PSG_STEP_WEIGHTS[_psg.step].discard)%PSG_BUF_SIZE;
      _psg.step= (_psg.step+1)%PSG_STEPS;
      psg_sample= (int32_t) ((sample*8192*4) + 0.5);
      
      // Obté mostres FM.
      fm_left= (int32_t) _fm.l[_fm.p];
      fm_right= (int32_t) _fm.r[_fm.p];
      _fm.p= (_fm.p+1)%BUF_SIZE;
      --_fm.N;
      
      // Genera eixida
      val= (6*fm_left + psg_sample)/7;
      _out.v[_out.N++]= (MDs16) val;
      val= (6*fm_right + psg_sample)/7;
      _out.v[_out.N++]= (MDs16) val;
      if ( _out.N == MD_FM_BUFFER_SIZE*2 )
        {
          _play_sound ( _out.v, _udata );
          _out.N= 0;
        }
      
    }
  
} // end render_samples




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
MD_audio_init (
               MD_Warning   *warning,
               MD_PlaySound *play_sound,
               void         *udata
               )
{

  _warning= warning;
  _play_sound= play_sound;
  _udata= udata;
  MD_audio_init_state ();
  
} // end MD_audio_init


void
MD_audio_init_state (void)
{

  // Output buffer.
  memset ( _out.v, 0, sizeof(_out.v) );
  _out.N= 0;

  // FM buffer.
  memset ( _fm.l, 0, sizeof(_fm.l) );
  memset ( _fm.r, 0, sizeof(_fm.r) );
  _fm.N= 0;
  _fm.p= 0;

  // PSG buffer.
  memset ( _psg.v, 0, sizeof(_psg.v) );
  _psg.N= 0;
  _psg.p= 0;
  _psg.step= 0;
  
} // end MD_audio_init_state


void
MD_audio_fm_play (
                  const int16_t left,
        	  const int16_t right
        	  )
{

  int pos;
  
  
  // NOTA!!! Açò sols pot passar si es generen de colp moltes mostres.
  if ( _fm.N == BUF_SIZE )
    {
      _warning ( _udata,
                 "[AUDIO] El buffer de FM està ple, probablement calga"
                 " fer més gran el buffer" );
      return;
    }

  pos= (_fm.p+_fm.N)%BUF_SIZE;
  _fm.l[pos]= left;
  _fm.r[pos]= right;
  ++_fm.N;
  
  render_samples ();
  
} // end MD_audio_fm_play


void
MD_audio_psg_play (
                   const double sample
        	   )
{
  
  int pos;
  
  
  // NOTA!!! Açò sols pot passar si es generen de colp moltes mostres.
  if ( _psg.N == PSG_BUF_SIZE )
    {
      _warning ( _udata,
                 "[AUDIO] El buffer de PSG està ple, probablement calga"
                 " fer més gran el buffer" );
      return;
    }

  pos= (_psg.p+_psg.N)%PSG_BUF_SIZE;
  _psg.v[pos]= sample;
  ++_psg.N;
  
  render_samples ();
  
} // end MD_audio_psg_play


int
MD_audio_save_state (
        	     FILE *f
        	     )
{
  
  SAVE ( _psg );
  SAVE ( _fm );
  SAVE ( _out );
  
  return 0;
  
} // end MD_audio_save_state


int
MD_audio_load_state (
        	     FILE *f
        	     )
{
  
  LOAD ( _psg );
  CHECK ( _psg.p >= 0 && _psg.p < PSG_BUF_SIZE );
  CHECK ( _psg.N >= 0 && _psg.N <= PSG_BUF_SIZE );
  CHECK ( _psg.step >= 0 && _psg.step < PSG_STEPS );
  LOAD ( _fm );
  CHECK ( _fm.p >= 0 && _fm.p < BUF_SIZE );
  CHECK ( _fm.N >= 0 && _fm.N <= BUF_SIZE );
  LOAD ( _out );
  CHECK ( _out.N >= 0 && _out.N < MD_FM_BUFFER_SIZE*2 );
  
  return 0;
  
} // end MD_audio_load_state
