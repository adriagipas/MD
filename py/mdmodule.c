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
 *  mdmodule.c - Mòdul que implementa una MegaDrive en Python.
 *
 */

#include <Python.h>
#include <SDL/SDL.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "MD.h"




/**********/
/* MACROS */
/**********/

#define CHECK_INITIALIZED                                               \
  do {                                                                  \
    if ( !_initialized )                                                \
      {                                                                 \
        PyErr_SetString ( MDError, "Module must be initialized" );        \
        return NULL;                                                    \
      }                                                                 \
  } while(0)


#define CHECK_ROM                                                       \
  do {                                                                  \
    if ( _rom.bytes ==NULL )                                            \
      {                                                                 \
        PyErr_SetString ( MDError, "There is no ROM inserted"        	\
                          " into the simulator" );                      \
        return NULL;                                                    \
      }                                                                 \
  } while(0)

#define MAXWIDTH 640
#define MAXHEIGHT 480

#define NBUFF 4

#define AUDIO_FREQ_PAL                                          \
  (MD_CYCLES_PER_SEC_PAL/(double) MD_CPU_CYCLES_PER_FM_SAMPLE)

#define AUDIO_FREQ_NTSC                                         \
  (MD_CYCLES_PER_SEC_NTSC/(double) MD_CPU_CYCLES_PER_FM_SAMPLE)




/*********/
/* TIPUS */
/*********/

enum { FALSE= 0, TRUE };

typedef struct
{
  
  MDs16        *v;
  volatile int  full;
  
} buffer_t;




/*********/
/* ESTAT */
/*********/

/* Error. */
static PyObject *MDError;

/* Inicialitzat. */
static char _initialized;

/* Rom. */
static MD_Rom _rom;

/* Tracer. */
static struct
{
  PyObject *obj;
  int       has_cpu_step;
  int       has_cpu_step_z80;
  int       has_cpu_step_svp;
  int       has_mem_access;
  int       has_mem_access8;
  int       has_mem_access_z80;
} _tracer;

/* Pantalla. */
static struct
{
  
  int          width;
  int          height;
  SDL_Surface *surface;
  
} _screen;

/* Paleta de colors. */
static Uint32 _palette[0x600];

/* Control. */
static struct
{
  
  int          buttons;
  
} _control;

/* Estat so. */
static struct
{
  
  buffer_t buffers[NBUFF];
  int      buff_in;
  int      buff_out;
  char     silence;
  int      pos;
  int      size;
  int      nsamples;
  double   ratio;
  double   pos2;
  double   outfreq;
  
} _audio;

/* SRAM */
static MD_Word *_sram;

/* EEPROM */
static MDu8 *_eeprom;




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static int
has_method (
            PyObject   *obj,
            const char *name
            )
{
  
  PyObject *aux;
  int ret;
  
  if ( !PyObject_HasAttrString ( obj, name ) ) return 0;
  aux= PyObject_GetAttrString ( obj, name );
  ret= PyMethod_Check ( aux );
  Py_DECREF ( aux );
  
  return ret;
  
} /* end has_method */


static void
init_palette (void)
{
  
  MD_RGB ret;
  int i;
  
  
  for ( i= 0; i < 0x600; ++i )
    {
      ret= MD_color2rgb ( i );
      _palette[i]= SDL_MapRGB ( _screen.surface->format,
        			ret.r, ret.g, ret.b );
    }
  
} /* end init_palette */


static void
audio_callback (
                void  *userdata,
                Uint8 *stream,
                int    len
                )
{
  
  int i;
  const MDs16 *buffer;
  
  
  assert ( _audio.size == len );
  if ( _audio.buffers[_audio.buff_out].full )
    {
      buffer= _audio.buffers[_audio.buff_out].v;
      for ( i= 0; i < len; ++i )
        stream[i]= ((Uint8 *) buffer)[i];
      _audio.buffers[_audio.buff_out].full= 0;
      _audio.buff_out= (_audio.buff_out+1)%NBUFF;
    }
  else for ( i= 0; i < len; ++i ) stream[i]= _audio.silence;
  
} /* end audio_callback */


/* Torna 0 si tot ha anat bé. */
static const char *
init_audio (void)
{
  
  SDL_AudioSpec desired, obtained;
  int n;
  Uint8 *mem;
  
  
  /* Únic camp de l'estat que s'inicialitza abans. */
  _audio.buff_out= _audio.buff_in= 0;
  for ( n= 0; n < NBUFF; ++n ) _audio.buffers[n].full= 0;
  
  /* Inicialitza. */
  desired.freq= 44100;
  desired.format= AUDIO_S16;
  desired.channels= 2;
  desired.samples= 2048;
  desired.size= 8192;
  desired.callback= audio_callback;
  desired.userdata= NULL;
  if ( SDL_OpenAudio ( &desired, &obtained ) == -1 )
    return SDL_GetError ();
  if ( obtained.format != desired.format )
    {
      fprintf ( stderr, "Força format audio\n" );
      SDL_CloseAudio ();
      if ( SDL_OpenAudio ( &desired, NULL ) == -1 )
        return SDL_GetError ();
      obtained= desired;
    }
  
  /* Inicialitza estat. */
  mem= malloc ( obtained.size*NBUFF );
  for ( n= 0; n < NBUFF; ++n, mem+= obtained.size )
    _audio.buffers[n].v= (MDs16 *) mem;
  _audio.silence= (char) obtained.silence;
  _audio.pos= 0;
  _audio.size= obtained.size;
  _audio.nsamples= _audio.size/2;
  if ( obtained.freq >= AUDIO_FREQ_NTSC )
    {
      SDL_CloseAudio ();
      return "Freqüència massa gran";
    }
    // RATIO NTSC, cal canviar-lo
  _audio.outfreq= (double) obtained.freq;
  _audio.ratio= AUDIO_FREQ_NTSC / _audio.outfreq;
  _audio.pos2= 0.0;
  
  return NULL;
  
} /* end init_audio */


static void
close_audio (void)
{
  
  SDL_CloseAudio ();
  free ( _audio.buffers[0].v );
  
} /* end close_audio */




/************/
/* FRONTEND */
/************/

static void
warning (
         void       *udata,
         const char *format,
         ...
         )
{
  
  va_list ap;
  
  
  va_start ( ap, format );
  fprintf ( stderr, "Warning: " );
  vfprintf ( stderr, format, ap );
  putc ( '\n', stderr );
  va_end ( ap );
  
} /* end warning */


static void
update_screen (
               const int  fb[],
               void      *udata
               )
{
  
  Uint32 *data;
  int i;
  
  
  if ( _screen.surface == NULL ) return;
  if ( SDL_MUSTLOCK ( _screen.surface ) )
    SDL_LockSurface ( _screen.surface );
  
  data= _screen.surface->pixels;
  for ( i= 0; i < _screen.width*_screen.height; ++i )
    data[i]= _palette[fb[i]];
  
  if ( SDL_MUSTLOCK ( _screen.surface ) )
    SDL_UnlockSurface ( _screen.surface );
  
  if ( SDL_Flip ( _screen.surface ) == -1 )
    {
      fprintf ( stderr, "ERROR FATAL !!!: %s\n", SDL_GetError () );
      SDL_Quit ();
    }
  
} /* end update_screen */


static MD_Word *
get_static_ram (
        	const int  num_words,
        	void      *udata
        	)
{
  
  if ( _sram != NULL ) free ( _sram );
  _sram= (MD_Word *) calloc ( num_words, sizeof(MD_Word) );
  
  return _sram;
  
} /* end get_static_ram */


static MDu8 *
get_eeprom (
            const size_t  nbytes,
            const MDu8    init_val,
            void         *udata
            )
{

  size_t i;

  
  if ( _eeprom != NULL ) free ( _eeprom );
  _eeprom= (MDu8 *) malloc ( nbytes );
  if ( _eeprom == NULL )
    {
      fprintf ( stderr, "cannot allocate memory" );
      exit ( EXIT_FAILURE );
    }
  for ( i= 0; i < nbytes; ++i )
    _eeprom[i]= init_val;
  
  return _eeprom;
  
} /* end get_eeprom */


static void
play_sound (
            const MDs16  samples[MD_FM_BUFFER_SIZE*2],
            void        *udata
            )
{
  
  int nofull, j;
  MDs16 *buffer;
  
  
  for (;;)
    {
      
      while ( _audio.buffers[_audio.buff_in].full ) SDL_Delay ( 1 );
      buffer= _audio.buffers[_audio.buff_in].v;
      
      j= (int) (_audio.pos2 + 0.5);
      while ( (nofull= (_audio.pos != _audio.nsamples)) &&
              j < MD_FM_BUFFER_SIZE )
        {
          buffer[_audio.pos++]= samples[2*j];
          buffer[_audio.pos++]= samples[2*j+1];
          _audio.pos2+= _audio.ratio;
          j= (int) (_audio.pos2 + 0.5);
        }
      if ( !nofull )
        {
          _audio.pos= 0;
          _audio.buffers[_audio.buff_in].full= 1;
          _audio.buff_in= (_audio.buff_in+1)%NBUFF;
        }
      if ( j >= MD_FM_BUFFER_SIZE )
        {
          _audio.pos2-= MD_FM_BUFFER_SIZE;
          break;
        }
      
    }
  
} /* end play_sound */


static void
check_signals (
               MD_Bool *stop,
               MD_Bool *reset,
               void    *udata
               )
{
  
  SDL_Event event;
  
  
  *stop= *reset= MD_FALSE;
  while ( SDL_PollEvent ( &event ) )
    switch ( event.type )
      {
      case SDL_ACTIVEEVENT:
        if ( event.active.state&SDL_APPINPUTFOCUS &&
             !event.active.gain )
          _control.buttons= 0;
        break;
      case SDL_KEYDOWN:
        if ( event.key.keysym.mod&KMOD_CTRL )
          {
            switch ( event.key.keysym.sym )
              {
              case SDLK_q: *stop= MD_TRUE; break;
              case SDLK_r: *reset= MD_TRUE;
              default: break;
              }
          }
        else
          {
            switch ( event.key.keysym.sym )
              {
              case SDLK_SPACE: _control.buttons|= MD_START; break;
              case SDLK_w: _control.buttons|= MD_UP; break;
              case SDLK_s: _control.buttons|= MD_DOWN; break;
              case SDLK_a: _control.buttons|= MD_LEFT; break;
              case SDLK_d: _control.buttons|= MD_RIGHT; break;
              case SDLK_i: _control.buttons|= MD_A; break;
              case SDLK_o: _control.buttons|= MD_B; break;
              case SDLK_p: _control.buttons|= MD_C; break;
              default: break;
              }
          }
        break;
      case SDL_KEYUP:
        switch ( event.key.keysym.sym )
          {
          case SDLK_SPACE: _control.buttons&= ~MD_START; break;
          case SDLK_w: _control.buttons&= ~MD_UP; break;
          case SDLK_s: _control.buttons&= ~MD_DOWN; break;
          case SDLK_a: _control.buttons&= ~MD_LEFT; break;
          case SDLK_d: _control.buttons&= ~MD_RIGHT; break;
          case SDLK_i: _control.buttons&= ~MD_A; break;
          case SDLK_o: _control.buttons&= ~MD_B; break;
          case SDLK_p: _control.buttons&= ~MD_C; break;
          default: break;
          }
        break;
      default: break;
      }
  
} /* end check_signals */


static int
check_buttons (
               const int  pad,
               void      *udata
               )
{
  return _control.buttons;
} /* end check_buttons */


static void 
sres_changed (
              const int  width,
              const int  height,
              void      *udata
              )
{
  
  SDL_Surface *prev;
  
  
  /* Nou surface. */
  _screen.width= width;
  _screen.height= height;
  prev= _screen.surface;
  _screen.surface= SDL_SetVideoMode ( width, height, 32,
                                      SDL_HWSURFACE | SDL_GL_DOUBLEBUFFER );
  if ( prev == NULL ) init_palette ();
  if ( _screen.surface == NULL )
    {
      fprintf ( stderr, "FATAL ERROR!!!: %s", SDL_GetError () );
      SDL_Quit ();
      return;
    }
  SDL_WM_SetCaption ( "MD", "MD" );
 
} /* end sres_changed */


static void
mem_access (
            const MD_MemAccessType  type,
            const MDu32             addr,
            const MD_Word           data,
            void                   *udata
            )
{
  
  PyObject *ret;
  
  
  if ( _tracer.obj == NULL ||
       !_tracer.has_mem_access ||
       PyErr_Occurred () != NULL ) return;
  ret= PyObject_CallMethod ( _tracer.obj, "mem_access",
                             "iIH", type, addr, data.v );
  Py_XDECREF ( ret );
  
} /* end mem_access */


static void
mem_access8 (
             const MD_MemAccessType  type,
             const MDu32             addr,
             const MDu8              data,
             void                   *udata
             )
{
  
  PyObject *ret;
  
  
  if ( _tracer.obj == NULL ||
       !_tracer.has_mem_access8 ||
       PyErr_Occurred () != NULL ) return;
  ret= PyObject_CallMethod ( _tracer.obj, "mem_access8",
                             "iIB", type, addr, data );
  Py_XDECREF ( ret );
  
} /* end mem_access8 */


static void
mem_access_z80 (
        	const MD_MemAccessType  type,
        	const Z80u16            addr,
        	const Z80u8             data,
        	void                   *udata
        	)
{
  
  PyObject *ret;
  
  
  if ( _tracer.obj == NULL ||
       !_tracer.has_mem_access ||
       PyErr_Occurred () != NULL ) return;
  ret= PyObject_CallMethod ( _tracer.obj, "mem_access_z80",
                             "iHB", type, addr, data );
  Py_XDECREF ( ret );
  
} /* end mem_access_z80 */


static void
cpu_step (
          const MD_Step *step,
          const MDu32    nextaddr,
          void          *udata
          )
{
  
  PyObject *ret;
  
  
  if ( _tracer.obj == NULL ||
       !_tracer.has_cpu_step ||
       PyErr_Occurred () != NULL ) return;
  switch ( step->type )
    {
    case MD_STEP_INST:
      ret= PyObject_CallMethod ( _tracer.obj, "cpu_step",
                                 "iHiiiy#"
        			 "(bb(bh)(bbbbb)HIihH(Ih)b)"
        			 "(bb(bh)(bbbbb)HIihH(Ih)b)",
                                 MD_STEP_INST,
                                 nextaddr,
                                 step->val.inst.id.name,
                                 step->val.inst.id.op1,
                                 step->val.inst.id.op2,
                                 step->val.inst.bytes,
                                 step->val.inst.nbytes,
        			 step->val.inst.e1.byte,
        			 step->val.inst.e1.reg,
        			 step->val.inst.e1.d16an.reg,
        			 step->val.inst.e1.d16an.dis,
        			 step->val.inst.e1.d8anxn.areg,
        			 step->val.inst.e1.d8anxn.xreg,
        			 step->val.inst.e1.d8anxn.dis,
        			 step->val.inst.e1.d8anxn.isd,
        			 step->val.inst.e1.d8anxn.isw,
        			 step->val.inst.e1.word,
        			 step->val.inst.e1.longval,
        			 step->val.inst.e1.vector,
        			 step->val.inst.e1.dis16,
        			 step->val.inst.e1.list,
        			 step->val.inst.e1.label.addr,
        			 step->val.inst.e1.label.dis,
        			 step->val.inst.e1.count,
        			 step->val.inst.e2.byte,
        			 step->val.inst.e2.reg,
        			 step->val.inst.e2.d16an.reg,
        			 step->val.inst.e2.d16an.dis,
        			 step->val.inst.e2.d8anxn.areg,
        			 step->val.inst.e2.d8anxn.xreg,
        			 step->val.inst.e2.d8anxn.dis,
        			 step->val.inst.e2.d8anxn.isd,
        			 step->val.inst.e2.d8anxn.isw,
        			 step->val.inst.e2.word,
        			 step->val.inst.e2.longval,
        			 step->val.inst.e2.vector,
        			 step->val.inst.e2.dis16,
        			 step->val.inst.e2.list,
        			 step->val.inst.e2.label.addr,
        			 step->val.inst.e2.label.dis,
        			 step->val.inst.e2.count );
      break;
    case MD_STEP_STOP:
    case MD_STEP_RESET:
      ret= PyObject_CallMethod ( _tracer.obj, "cpu_step", "iH",
                                 step->type, nextaddr );
      break;
    case MD_STEP_AUTOVECTOR:
      ret= PyObject_CallMethod ( _tracer.obj, "cpu_step", "iHI",
                                 MD_STEP_AUTOVECTOR, nextaddr,
        			 step->val.priority );
      break;
    default: ret= NULL;
    }
  Py_XDECREF ( ret );
  
} /* end cpu_step */


static void
cpu_step_z80 (
              const Z80_Step *step,
              const Z80u16    nextaddr,
              void           *udata
              )
{
  
  PyObject *ret;
  
  
  if ( _tracer.obj == NULL ||
       !_tracer.has_cpu_step_z80 ||
       PyErr_Occurred () != NULL ) return;
  switch ( step->type )
    {
    case Z80_STEP_INST:
      ret= PyObject_CallMethod ( _tracer.obj, "cpu_step_z80",
                                 "iHiiiy#(BbH(bH))(BbH(bH))",
                                 Z80_STEP_INST,
                                 nextaddr,
                                 step->val.inst.id.name,
                                 step->val.inst.id.op1,
                                 step->val.inst.id.op2,
                                 step->val.inst.bytes,
                                 step->val.inst.nbytes,
                                 step->val.inst.e1.byte,
                                 step->val.inst.e1.desp,
                                 step->val.inst.e1.addr_word,
                                 step->val.inst.e1.branch.desp,
                                 step->val.inst.e1.branch.addr,
                                 step->val.inst.e2.byte,
                                 step->val.inst.e2.desp,
                                 step->val.inst.e2.addr_word,
                                 step->val.inst.e2.branch.desp,
                                 step->val.inst.e2.branch.addr );
      break;
    case Z80_STEP_NMI:
      ret= PyObject_CallMethod ( _tracer.obj, "cpu_step_z80", "iH",
                                 Z80_STEP_NMI, nextaddr );
      break;
    case Z80_STEP_IRQ:
      ret= PyObject_CallMethod ( _tracer.obj, "cpu_step_z80", "iHB",
                                 Z80_STEP_IRQ, nextaddr, step->val.bus );
      break;
    default: ret= NULL;
    }
  Py_XDECREF ( ret );
  
} /* end cpu_step_z80 */


static void
cpu_step_svp (
              const MD_SVPInst *inst,
              const uint16_t    addr,
              void             *udata
              )
{
  
  PyObject *ret;
  
  
  if ( _tracer.obj == NULL ||
       !_tracer.has_cpu_step_svp ||
       PyErr_Occurred () != NULL ) return;

  ret= PyObject_CallMethod ( _tracer.obj, "cpu_step_svp",
                             "Hiii[HH]i(ii)(ii)bB(Bi)",
                             addr,
                             inst->mnemonic,
                             inst->op[0],
                             inst->op[1],
                             inst->inst[0],
                             inst->inst[1],
                             inst->nwords,
                             inst->ri[0].m,
                             inst->ri[0].reg,
                             inst->ri[1].m,
                             inst->ri[1].reg,
                             inst->cond_f,
                             inst->simm,
                             inst->adr.pos,
                             inst->adr.ram_ind );
  Py_XDECREF ( ret );
  
} // end cpu_step_svp




/******************/
/* FUNCIONS MÒDUL */
/******************/

static PyObject *
MD_check_signals (
        	  PyObject *self,
        	  PyObject *args
        	  )
{
  
  MD_Bool stop, reset;
  
  
  CHECK_INITIALIZED;
  CHECK_ROM;
  
  check_signals ( &stop, &reset, NULL );
  
  Py_RETURN_NONE;
  
} /* end MD_check_signals */


static PyObject *
MD_close_ (
           PyObject *self,
           PyObject *args
           )
{
  
  if ( !_initialized ) Py_RETURN_NONE;

  close_audio ();
  SDL_Quit ();
  if ( _rom.bytes != NULL ) MD_rom_free ( &_rom );
  if ( _sram != NULL ) free ( _sram );
  if ( _eeprom != NULL ) free ( _eeprom );
  _initialized= FALSE;
  Py_XDECREF ( _tracer.obj );
  
  Py_RETURN_NONE;
  
} /* end MD_close_ */


static PyObject *
MD_get_cram (
             PyObject *self,
             PyObject *args
             )
{
  
  PyObject *ret, *aux;
  const MDu16 *cram;
  int i;
  
  
  CHECK_INITIALIZED;
  CHECK_ROM;
  
  cram= MD_vdp_get_cram ();
  ret= PyTuple_New ( 64 );
  if ( ret == NULL ) return NULL;
  for ( i= 0; i < 64; ++i )
    {
      aux= PyLong_FromLong ( cram[i] );
      if ( aux == NULL ) goto error;
      PyTuple_SET_ITEM ( ret, i, aux );
    }
  
  return ret;
  
 error:
  Py_DECREF ( ret );
  return NULL;
  
} /* end MD_get_cram */


static PyObject *
MD_get_vram (
             PyObject *self,
             PyObject *args
             )
{
  
  PyObject *ret;
  const MDu8 *vram;
  
  
  CHECK_INITIALIZED;
  CHECK_ROM;
  
  vram= MD_vdp_get_vram ();
  ret= PyBytes_FromStringAndSize ( (const char *) vram, 0x10000 );
  
  return ret;
  
} /* end MD_get_vram */


static PyObject *
MD_get_ram (
            PyObject *self,
            PyObject *args
            )
{
  
  PyObject *ret;
  const MD_Word *ram;
  
  
  CHECK_INITIALIZED;
  CHECK_ROM;
  
  ram= MD_mem_get_ram ();
  ret= PyBytes_FromStringAndSize ( (const char *) ram, 32768*2 );
  
  return ret;
  
} /* end MD_get_ram */


static PyObject *
MD_get_rom (
            PyObject *self,
            PyObject *args
            )
{
  
  PyObject *dict, *aux;
  MD_RomHeader header;
  
  
  CHECK_INITIALIZED;
  CHECK_ROM;
  
  dict= PyDict_New ();
  if ( dict == NULL ) return NULL;
  
  /* Número de Paraules. */
  aux= PyLong_FromLong ( _rom.nwords );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "nwords", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );

  /* Bytes. */
  aux= PyBytes_FromStringAndSize ( ((const char *) _rom.bytes),
        			   _rom.nwords*2 );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "bytes", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );
  
  /* Obté la capçalera. */
  MD_rom_get_header ( &_rom, &header );
  
  /* Consola. */
  aux= PyByteArray_FromStringAndSize ( header.console,
        			       strlen ( header.console ) );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "console", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );
  
  /* Firma i build. */
  aux= PyByteArray_FromStringAndSize ( header.firm_build,
        			       strlen ( header.firm_build )  );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "firm_build", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );
  
  /* Nom domèstic. */
  aux= PyByteArray_FromStringAndSize ( header.dom_name,
        			       strlen ( header.dom_name ) );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "dom_name", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );
  
  /* Nom internacional. */
  aux= PyByteArray_FromStringAndSize ( header.int_name,
        			       strlen ( header.int_name ) );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "int_name", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );
  
  /* Type and snumber. */
  aux= PyByteArray_FromStringAndSize ( header.type_snumber,
        			       strlen ( header.type_snumber ) );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "type_snumber", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );
  
  /* Checksum. */
  aux= PyLong_FromLong ( header.checksum );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "checksum", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );
  
  /* I/O support. */
  aux= PyByteArray_FromStringAndSize ( header.io, strlen(header.io) );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "io", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );
  
  /* Start. */
  aux= PyLong_FromLong ( header.start );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "start", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );
  
  /* End. */
  aux= PyLong_FromLong ( header.end );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "end", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );
  
  /* Start RAM. */
  aux= PyLong_FromLong ( header.start_ram );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "start_ram", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );
  
  /* End RAM. */
  aux= PyLong_FromLong ( header.end_ram );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "end_ram", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );
  
  /* SRAM id. */
  aux= PyByteArray_FromStringAndSize ( header.sramid, strlen(header.sramid) );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "sramid", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );
  
  /* SRAM FLAGS. */
  aux= PyLong_FromLong ( header.sram_flags );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "sram_flags", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );
  
  /* Start SRAM. */
  aux= PyLong_FromLong ( header.start_sram );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "start_sram", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );
  
  /* End SRAM. */
  aux= PyLong_FromLong ( header.end_sram );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "end_sram", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );
  
  /* Modem. */
  aux= PyByteArray_FromStringAndSize ( header.modem, strlen(header.modem) );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "modem", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );
  
  /* Notes. */
  aux= PyByteArray_FromStringAndSize ( header.notes, strlen(header.notes) );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "notes", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );
  
  /* Country codes. */
  aux= PyByteArray_FromStringAndSize ( header.ccodes, strlen(header.ccodes) );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "ccodes", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );
  
  /* Comprovació checksum. */
  aux= PyBool_FromLong ( MD_rom_check_checksum ( &_rom ) );
  if ( PyDict_SetItemString ( dict, "checksum_ok", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );
  
  return dict;
  
 error:
  Py_XDECREF ( dict );
  return NULL;
  
} /* end MD_get_rom */


static PyObject *
MD_init_module (
        	PyObject *self,
        	PyObject *args
        	)
{
  
  const char *err;
  
  
  if ( _initialized ) Py_RETURN_NONE;
  
  /* SDL */
  if ( SDL_Init ( SDL_INIT_VIDEO |
                  SDL_INIT_NOPARACHUTE |
                  SDL_INIT_AUDIO ) == -1 )
    {
      PyErr_SetString ( MDError, SDL_GetError () );
      return NULL;
    }
  _screen.surface= NULL;
  if ( (err= init_audio ()) != NULL )
    {
      PyErr_SetString ( MDError, err );
      SDL_Quit ();
      return NULL; 
    }
  
  /* ROM */
  _rom.bytes= NULL;
  
  /* SRAM. */
  _sram= NULL;

  /* EEPROM. */
  _eeprom= NULL;
  
  /* Tracer. */
  _tracer.obj= NULL;
  
  /* Pad. */
  _initialized= TRUE;
  
  Py_RETURN_NONE;
  
} /* end MD_init_module */


static PyObject *
MD_load_state_ (
        	PyObject *self,
        	PyObject *args
        	)
{

  const char *fn;
  FILE *f;
  int ret;
  
  
  CHECK_INITIALIZED;
  CHECK_ROM;

  if ( !PyArg_ParseTuple ( args, "s", &fn ) )
    return NULL;

  f= fopen ( fn, "rb" );
  if ( f == NULL )
    {
      PyErr_Format ( PyExc_RuntimeError, "cannot open '%s'", fn );
      return NULL;
    }
  ret= MD_load_state ( f );
  fclose ( f );
  if ( ret != 0 )
    {
      PyErr_Format ( PyExc_RuntimeError, "cannot load state from '%s'", fn );
      return NULL;
    }

  Py_RETURN_NONE;
  
} /* end MD_load_state_ */


static PyObject *
MD_loop_module (
        	PyObject *self,
        	PyObject *args
        	)
{
  
  int n;
  
  
  CHECK_INITIALIZED;
  CHECK_ROM;
  
  for ( n= 0; n < NBUFF; ++n ) _audio.buffers[n].full= 0;
  SDL_PauseAudio ( 0 );
  MD_loop ();
  SDL_PauseAudio ( 1 );
  
  Py_RETURN_NONE;
  
} /* end MD_loop_module */


static PyObject *
MD_set_rom (
            PyObject *self,
            PyObject *args
            )
{
  
  static const MD_TraceCallbacks trace_callbacks=
    {
      mem_access,
      mem_access8,
      mem_access_z80,
      cpu_step,
      cpu_step_z80,
      cpu_step_svp
    };
  static const MD_Frontend frontend=
    {
      warning,
      check_signals,
      sres_changed,
      update_screen,
      play_sound,
      get_static_ram,
      get_eeprom,
      &trace_callbacks,
      { MD_IODEV_PAD, MD_IODEV_NONE, MD_IODEV_NONE },
      check_buttons
    };
  
  PyObject *bytes;
  Py_ssize_t size, n;
  MD_Error err;
  const char *data;
  int pal_model;
  MDu8 flags;
  
  
  CHECK_INITIALIZED;
  if ( !PyArg_ParseTuple ( args, "O!i", &PyBytes_Type, &bytes, &pal_model ) )
    return NULL;
  
  size= PyBytes_Size ( bytes );
  if ( size <= 0 || size%2 != 0 )
    {
      PyErr_SetString ( MDError, "Invalid ROM size" );
      return NULL;
    }
  if ( _rom.bytes != NULL )
    {
      MD_close ();
      MD_rom_free ( &_rom );
    }
  _rom.nwords= size/2;
  MD_rom_alloc ( _rom );
  if ( _rom.bytes == NULL ) return PyErr_NoMemory ();
  data= PyBytes_AS_STRING ( bytes );
  for ( n= 0; n < size; ++n ) _rom.bytes[n]= (MDu8) data[n];
  err= MD_rom_prepare ( &_rom );
  if ( err != MD_NOERROR )
    {
      MD_rom_free ( &_rom );
      _rom.bytes= NULL;
      return PyErr_NoMemory ();
    }
  
  /* Inicialitza el simulador. */
  _control.buttons= 0;
  flags= MD_MODEL_OVERSEAS;
  if ( pal_model ) flags|= MD_MODEL_PAL;
  if ( flags&MD_MODEL_PAL )
    _audio.ratio= AUDIO_FREQ_PAL / _audio.outfreq;
  else
    _audio.ratio= AUDIO_FREQ_NTSC / _audio.outfreq;
  MD_init ( &_rom, flags, &frontend, NULL );
  
  Py_RETURN_NONE;
  
} /* end MD_set_rom */


static PyObject *
MD_set_tracer (
               PyObject *self,
               PyObject *args
               )
{
  
  PyObject *aux;
  
  
  CHECK_INITIALIZED;
  if ( !PyArg_ParseTuple ( args, "O", &aux ) )
    return NULL;
  Py_XDECREF ( _tracer.obj );
  _tracer.obj= aux;
  Py_INCREF ( _tracer.obj );
  
  if ( _tracer.obj != NULL )
    {
      _tracer.has_mem_access= has_method ( _tracer.obj, "mem_access" );
      _tracer.has_mem_access8= has_method ( _tracer.obj, "mem_access8" );
      _tracer.has_mem_access_z80= has_method ( _tracer.obj, "mem_access_z80" );
      _tracer.has_cpu_step= has_method ( _tracer.obj, "cpu_step" );
      _tracer.has_cpu_step_z80= has_method ( _tracer.obj, "cpu_step_z80" );
      _tracer.has_cpu_step_svp= has_method ( _tracer.obj, "cpu_step_svp" );
    }
  
  Py_RETURN_NONE;
  
} /* end MD_set_tracer */


static PyObject *
MD_trace_module (
        	 PyObject *self,
        	 PyObject *args
        	 )
{
  
  int cc;
  
  
  CHECK_INITIALIZED;
  CHECK_ROM;
  
  SDL_PauseAudio ( 0 );
  cc= MD_trace ();
  SDL_PauseAudio ( 1 );
  if ( PyErr_Occurred () != NULL ) return NULL;
  
  return PyLong_FromLong ( cc );
  
} /* end MD_trace_module */




/************************/
/* INICIALITZACIÓ MÒDUL */
/************************/

static PyMethodDef MDMethods[]=
  {
    { "check_signals", MD_check_signals, METH_VARARGS,
      "Force signals checking" },
    { "close", MD_close_, METH_VARARGS,
      "Free module resources and close the module" },
    { "get_rom", MD_get_rom, METH_VARARGS,
      "Get the ROM structured into a dictionary" },
    { "get_cram", MD_get_cram, METH_VARARGS,
      "Get a copy of the CRAM" },
    { "get_vram", MD_get_vram, METH_VARARGS,
      "Get a copy of the VRAM" },
    { "get_ram", MD_get_ram, METH_VARARGS,
      "Get a copy of the RAM" },
    { "init", MD_init_module, METH_VARARGS,
      "Initialize the module" },
    { "load_state", MD_load_state_, METH_VARARGS,
      "Load state from file" },
    { "loop", MD_loop_module, METH_VARARGS,
      "Run the simulator into a loop and block" },
    { "set_rom", MD_set_rom, METH_VARARGS,
      "Set a ROM into the simulator. The ROM should be of type bytes."
      " The second argument is a boolean: true/false (PAL/NTSC)"},
    { "set_tracer", MD_set_tracer, METH_VARARGS,
      "Set a python object to trace the execution. The object can"
      " implement one of these methods:\n"
      " - cpu_step_z80: To trace the Z80 cpu execution. The first and second"
      " parameter are always the step identifier (Z80_INST|Z80_NMI|Z80_IRQ) and"
      " the next memory address. The other parameters depends on the"
      " identifier:\n"
      " -- Z80_INST: instruction id., operator 1 id., operator 2 id., bytes,"
      " tuple with extra info containing: (byte,desp,address/word,"
      "(branch.desp,branch.address))\n"
      " -- Z80_NMI: nothing\n"
      " -- Z80_IRQ: The bus value\n"
      " - cpu_step_svp: To trace the SVP execution.\n"
      " - cpu_step: To trace the cpu execution. The first and second"
      " parameter are always the step identifier (STEP_INST|STEP_STOP|"
      "STEP_RESET|STEP_AUTOVECTOR)"
      " and the next memory address. The other parameters depends on the"
      " identifier:\n"
      " -- STEP_INST: instruction id., operator 1 id., operator 2 id., bytes,"
      " tuple with extra info containing: (byte,reg,(reg,dis),d8anxn,word,"
      " longval,vector,dis16,list,(addr,dis),count)\n"
      " -- STEP_STOP: nothing\n"
      " -- STEP_RESET: nothing\n"
      " -- STEP_AUTOVECTOR: The vector\n"
      " - mem_access: Called every time a memory access is done. The type"
      " of access, the address and the transmitted data "
      "is passed as arguments\n"
      " - mem_access8: Called every time a byte memory access is done. The type"
      " of access, the address and the transmitted data "
      "is passed as arguments\n"
      " - mem_access_z88: Called every time a Z80 memory access is done. The"
      " type of access, the address and the transmitted data "
      "is passed as arguments"},
    { "trace", MD_trace_module, METH_VARARGS,
      "Executes the next instruction or interruption in trace mode" },
    { NULL, NULL, 0, NULL }
  };


static struct PyModuleDef MDmodule=
  {
    PyModuleDef_HEAD_INIT,
    "MD",
    NULL,
    -1,
    MDMethods
  };


PyMODINIT_FUNC
PyInit_MD (void)
{
  
  PyObject *m;
  
  
  m= PyModule_Create ( &MDmodule );
  if ( m == NULL ) return NULL;
  
  _initialized= FALSE;
  MDError= PyErr_NewException ( "MD.error", NULL, NULL );
  Py_INCREF ( MDError );
  PyModule_AddObject ( m, "error", MDError );
  
  /* Tipus de pasos. */
  PyModule_AddIntConstant ( m, "STEP_INST", MD_STEP_INST );
  PyModule_AddIntConstant ( m, "STEP_STOP", MD_STEP_STOP );
  PyModule_AddIntConstant ( m, "STEP_RESET", MD_STEP_RESET );
  PyModule_AddIntConstant ( m, "STEP_AUTOVECTOR", MD_STEP_AUTOVECTOR );
  
  /* Tipus de pasos Z80. */
  PyModule_AddIntConstant ( m, "Z80_INST", Z80_STEP_INST );
  PyModule_AddIntConstant ( m, "Z80_NMI", Z80_STEP_NMI );
  PyModule_AddIntConstant ( m, "Z80_IRQ", Z80_STEP_IRQ );
  
  /* Mnemonics. */
  PyModule_AddIntConstant ( m, "UNK", MD_UNK );
  PyModule_AddIntConstant ( m, "ABCD", MD_ABCD );
  PyModule_AddIntConstant ( m, "ADDb", MD_ADDb );
  PyModule_AddIntConstant ( m, "ADDw", MD_ADDw );
  PyModule_AddIntConstant ( m, "ADDl", MD_ADDl );
  PyModule_AddIntConstant ( m, "ADDIb", MD_ADDIb );
  PyModule_AddIntConstant ( m, "ADDIw", MD_ADDIw );
  PyModule_AddIntConstant ( m, "ADDIl", MD_ADDIl );
  PyModule_AddIntConstant ( m, "ADDQb", MD_ADDQb );
  PyModule_AddIntConstant ( m, "ADDQl", MD_ADDQl );
  PyModule_AddIntConstant ( m, "ADDQw", MD_ADDQw );
  PyModule_AddIntConstant ( m, "ADDXb", MD_ADDXb );
  PyModule_AddIntConstant ( m, "ADDXw", MD_ADDXw );
  PyModule_AddIntConstant ( m, "ADDXl", MD_ADDXl );
  PyModule_AddIntConstant ( m, "ANDb", MD_ANDb );
  PyModule_AddIntConstant ( m, "ANDw", MD_ANDw );
  PyModule_AddIntConstant ( m, "ANDl", MD_ANDl );
  PyModule_AddIntConstant ( m, "ANDIb", MD_ANDIb );
  PyModule_AddIntConstant ( m, "ANDIw", MD_ANDIw );
  PyModule_AddIntConstant ( m, "ANDIl", MD_ANDIl );
  PyModule_AddIntConstant ( m, "ASLb", MD_ASLb );
  PyModule_AddIntConstant ( m, "ASLw", MD_ASLw );
  PyModule_AddIntConstant ( m, "ASLl", MD_ASLl );
  PyModule_AddIntConstant ( m, "ASRb", MD_ASRb );
  PyModule_AddIntConstant ( m, "ASRw", MD_ASRw );
  PyModule_AddIntConstant ( m, "ASRl", MD_ASRl );
  PyModule_AddIntConstant ( m, "BCC", MD_BCC );
  PyModule_AddIntConstant ( m, "BCS", MD_BCS );
  PyModule_AddIntConstant ( m, "BEQ", MD_BEQ );
  PyModule_AddIntConstant ( m, "BGE", MD_BGE );
  PyModule_AddIntConstant ( m, "BGT", MD_BGT );
  PyModule_AddIntConstant ( m, "BHI", MD_BHI );
  PyModule_AddIntConstant ( m, "BLE", MD_BLE );
  PyModule_AddIntConstant ( m, "BLS", MD_BLS );
  PyModule_AddIntConstant ( m, "BLT", MD_BLT );
  PyModule_AddIntConstant ( m, "BMI", MD_BMI );
  PyModule_AddIntConstant ( m, "BNE", MD_BNE );
  PyModule_AddIntConstant ( m, "BPL", MD_BPL );
  PyModule_AddIntConstant ( m, "BVC", MD_BVC );
  PyModule_AddIntConstant ( m, "BVS", MD_BVS );
  PyModule_AddIntConstant ( m, "BCHG", MD_BCHG );
  PyModule_AddIntConstant ( m, "BCLR", MD_BCLR );
  PyModule_AddIntConstant ( m, "BRA", MD_BRA );
  PyModule_AddIntConstant ( m, "BSET", MD_BSET );
  PyModule_AddIntConstant ( m, "BSR", MD_BSR );
  PyModule_AddIntConstant ( m, "BTST", MD_BTST );
  PyModule_AddIntConstant ( m, "CHK", MD_CHK );
  PyModule_AddIntConstant ( m, "CLRb", MD_CLRb );
  PyModule_AddIntConstant ( m, "CLRw", MD_CLRw );
  PyModule_AddIntConstant ( m, "CLRl", MD_CLRl );
  PyModule_AddIntConstant ( m, "CMPb", MD_CMPb );
  PyModule_AddIntConstant ( m, "CMPw", MD_CMPw );
  PyModule_AddIntConstant ( m, "CMPl", MD_CMPl );
  PyModule_AddIntConstant ( m, "CMPIb", MD_CMPIb );
  PyModule_AddIntConstant ( m, "CMPIw", MD_CMPIw );
  PyModule_AddIntConstant ( m, "CMPIl", MD_CMPIl );
  PyModule_AddIntConstant ( m, "CMPMb", MD_CMPMb );
  PyModule_AddIntConstant ( m, "CMPMw", MD_CMPMw );
  PyModule_AddIntConstant ( m, "CMPMl", MD_CMPMl );
  PyModule_AddIntConstant ( m, "DBCC", MD_DBCC );
  PyModule_AddIntConstant ( m, "DBCS", MD_DBCS );
  PyModule_AddIntConstant ( m, "DBEQ", MD_DBEQ );
  PyModule_AddIntConstant ( m, "DBF", MD_DBF );
  PyModule_AddIntConstant ( m, "DBGE", MD_DBGE );
  PyModule_AddIntConstant ( m, "DBGT", MD_DBGT );
  PyModule_AddIntConstant ( m, "DBHI", MD_DBHI );
  PyModule_AddIntConstant ( m, "DBLE", MD_DBLE );
  PyModule_AddIntConstant ( m, "DBLS", MD_DBLS );
  PyModule_AddIntConstant ( m, "DBLT", MD_DBLT );
  PyModule_AddIntConstant ( m, "DBMI", MD_DBMI );
  PyModule_AddIntConstant ( m, "DBNE", MD_DBNE );
  PyModule_AddIntConstant ( m, "DBPL", MD_DBPL );
  PyModule_AddIntConstant ( m, "DBT", MD_DBT );
  PyModule_AddIntConstant ( m, "DBVC", MD_DBVC );
  PyModule_AddIntConstant ( m, "DBVS", MD_DBVS );
  PyModule_AddIntConstant ( m, "DIVS", MD_DIVS );
  PyModule_AddIntConstant ( m, "DIVU", MD_DIVU );
  PyModule_AddIntConstant ( m, "EORb", MD_EORb );
  PyModule_AddIntConstant ( m, "EORw", MD_EORw );
  PyModule_AddIntConstant ( m, "EORl", MD_EORl );
  PyModule_AddIntConstant ( m, "EORIb", MD_EORIb );
  PyModule_AddIntConstant ( m, "EORIw", MD_EORIw );
  PyModule_AddIntConstant ( m, "EORIl", MD_EORIl );
  PyModule_AddIntConstant ( m, "EXG", MD_EXG );
  PyModule_AddIntConstant ( m, "EXTl", MD_EXTl );
  PyModule_AddIntConstant ( m, "EXTw", MD_EXTw );
  PyModule_AddIntConstant ( m, "ILLEGAL", MD_ILLEGAL );
  PyModule_AddIntConstant ( m, "JMP", MD_JMP );
  PyModule_AddIntConstant ( m, "JSR", MD_JSR );
  PyModule_AddIntConstant ( m, "LEA", MD_LEA );
  PyModule_AddIntConstant ( m, "LINK", MD_LINK );
  PyModule_AddIntConstant ( m, "LSLb", MD_LSLb );
  PyModule_AddIntConstant ( m, "LSLw", MD_LSLw );
  PyModule_AddIntConstant ( m, "LSLl", MD_LSLl );
  PyModule_AddIntConstant ( m, "LSRb", MD_LSRb );
  PyModule_AddIntConstant ( m, "LSRw", MD_LSRw );
  PyModule_AddIntConstant ( m, "LSRl", MD_LSRl );
  PyModule_AddIntConstant ( m, "MOVEb", MD_MOVEb );
  PyModule_AddIntConstant ( m, "MOVEw", MD_MOVEw );
  PyModule_AddIntConstant ( m, "MOVEl", MD_MOVEl );
  PyModule_AddIntConstant ( m, "MOVEMl", MD_MOVEMl );
  PyModule_AddIntConstant ( m, "MOVEMw", MD_MOVEMw );
  PyModule_AddIntConstant ( m, "MOVEPw", MD_MOVEPw );
  PyModule_AddIntConstant ( m, "MOVEPl", MD_MOVEPl );
  PyModule_AddIntConstant ( m, "MOVEQ", MD_MOVEQ );
  PyModule_AddIntConstant ( m, "MULS", MD_MULS );
  PyModule_AddIntConstant ( m, "MULU", MD_MULU );
  PyModule_AddIntConstant ( m, "NBCD", MD_NBCD );
  PyModule_AddIntConstant ( m, "NEGb", MD_NEGb );
  PyModule_AddIntConstant ( m, "NEGl", MD_NEGl );
  PyModule_AddIntConstant ( m, "NEGw", MD_NEGw );
  PyModule_AddIntConstant ( m, "NEGXb", MD_NEGXb );
  PyModule_AddIntConstant ( m, "NEGXl", MD_NEGXl );
  PyModule_AddIntConstant ( m, "NEGXw", MD_NEGXw );
  PyModule_AddIntConstant ( m, "NOP", MD_NOP );
  PyModule_AddIntConstant ( m, "NOTb", MD_NOTb );
  PyModule_AddIntConstant ( m, "NOTl", MD_NOTl );
  PyModule_AddIntConstant ( m, "NOTw", MD_NOTw );
  PyModule_AddIntConstant ( m, "ORb", MD_ORb );
  PyModule_AddIntConstant ( m, "ORl", MD_ORl );
  PyModule_AddIntConstant ( m, "ORw", MD_ORw );
  PyModule_AddIntConstant ( m, "ORIb", MD_ORIb );
  PyModule_AddIntConstant ( m, "ORIl", MD_ORIl );
  PyModule_AddIntConstant ( m, "ORIw", MD_ORIw );
  PyModule_AddIntConstant ( m, "PEA", MD_PEA );
  PyModule_AddIntConstant ( m, "RESET", MD_RESET );
  PyModule_AddIntConstant ( m, "ROLb", MD_ROLb );
  PyModule_AddIntConstant ( m, "ROLw", MD_ROLw );
  PyModule_AddIntConstant ( m, "ROLl", MD_ROLl );
  PyModule_AddIntConstant ( m, "RORb", MD_RORb );
  PyModule_AddIntConstant ( m, "RORw", MD_RORw );
  PyModule_AddIntConstant ( m, "RORl", MD_RORl );
  PyModule_AddIntConstant ( m, "ROXLb", MD_ROXLb );
  PyModule_AddIntConstant ( m, "ROXLw", MD_ROXLw );
  PyModule_AddIntConstant ( m, "ROXLl", MD_ROXLl );
  PyModule_AddIntConstant ( m, "ROXRb", MD_ROXRb );
  PyModule_AddIntConstant ( m, "ROXRw", MD_ROXRw );
  PyModule_AddIntConstant ( m, "ROXRl", MD_ROXRl );
  PyModule_AddIntConstant ( m, "RTE", MD_RTE );
  PyModule_AddIntConstant ( m, "RTR", MD_RTR );
  PyModule_AddIntConstant ( m, "RTS", MD_RTS );
  PyModule_AddIntConstant ( m, "SBCD", MD_SBCD );
  PyModule_AddIntConstant ( m, "SCC", MD_SCC );
  PyModule_AddIntConstant ( m, "SCS", MD_SCS );
  PyModule_AddIntConstant ( m, "SEQ", MD_SEQ );
  PyModule_AddIntConstant ( m, "SF", MD_SF );
  PyModule_AddIntConstant ( m, "SGE", MD_SGE );
  PyModule_AddIntConstant ( m, "SGT", MD_SGT );
  PyModule_AddIntConstant ( m, "SHI", MD_SHI );
  PyModule_AddIntConstant ( m, "SLE", MD_SLE );
  PyModule_AddIntConstant ( m, "SLS", MD_SLS );
  PyModule_AddIntConstant ( m, "SLT", MD_SLT );
  PyModule_AddIntConstant ( m, "SMI", MD_SMI );
  PyModule_AddIntConstant ( m, "SNE", MD_SNE );
  PyModule_AddIntConstant ( m, "SPL", MD_SPL );
  PyModule_AddIntConstant ( m, "ST", MD_ST );
  PyModule_AddIntConstant ( m, "SVC", MD_SVC );
  PyModule_AddIntConstant ( m, "SVS", MD_SVS );
  PyModule_AddIntConstant ( m, "STOP", MD_STOP );
  PyModule_AddIntConstant ( m, "SUBb", MD_SUBb );
  PyModule_AddIntConstant ( m, "SUBw", MD_SUBw );
  PyModule_AddIntConstant ( m, "SUBl", MD_SUBl );
  PyModule_AddIntConstant ( m, "SUBIb", MD_SUBIb );
  PyModule_AddIntConstant ( m, "SUBIw", MD_SUBIw );
  PyModule_AddIntConstant ( m, "SUBIl", MD_SUBIl );
  PyModule_AddIntConstant ( m, "SUBQb", MD_SUBQb );
  PyModule_AddIntConstant ( m, "SUBQl", MD_SUBQl );
  PyModule_AddIntConstant ( m, "SUBQw", MD_SUBQw );
  PyModule_AddIntConstant ( m, "SUBXb", MD_SUBXb );
  PyModule_AddIntConstant ( m, "SUBXl", MD_SUBXl );
  PyModule_AddIntConstant ( m, "SUBXw", MD_SUBXw );
  PyModule_AddIntConstant ( m, "SWAP", MD_SWAP );
  PyModule_AddIntConstant ( m, "TRAP", MD_TRAP );
  PyModule_AddIntConstant ( m, "TSTb", MD_TSTb );
  PyModule_AddIntConstant ( m, "TSTw", MD_TSTw );
  PyModule_AddIntConstant ( m, "TSTl", MD_TSTl );
  PyModule_AddIntConstant ( m, "UNLK", MD_UNLK );
  
  /* Mnemonics Z80. */
  PyModule_AddIntConstant ( m, "Z80_UNK", Z80_UNK );
  PyModule_AddIntConstant ( m, "Z80_LD", Z80_LD );
  PyModule_AddIntConstant ( m, "Z80_PUSH", Z80_PUSH );
  PyModule_AddIntConstant ( m, "Z80_POP", Z80_POP );
  PyModule_AddIntConstant ( m, "Z80_EX", Z80_EX );
  PyModule_AddIntConstant ( m, "Z80_EXX", Z80_EXX );
  PyModule_AddIntConstant ( m, "Z80_LDI", Z80_LDI );
  PyModule_AddIntConstant ( m, "Z80_LDIR", Z80_LDIR );
  PyModule_AddIntConstant ( m, "Z80_LDD", Z80_LDD );
  PyModule_AddIntConstant ( m, "Z80_LDDR", Z80_LDDR );
  PyModule_AddIntConstant ( m, "Z80_CPI", Z80_CPI );
  PyModule_AddIntConstant ( m, "Z80_CPIR", Z80_CPIR );
  PyModule_AddIntConstant ( m, "Z80_CPD", Z80_CPD );
  PyModule_AddIntConstant ( m, "Z80_CPDR", Z80_CPDR );
  PyModule_AddIntConstant ( m, "Z80_ADD", Z80_ADD );
  PyModule_AddIntConstant ( m, "Z80_ADC", Z80_ADC );
  PyModule_AddIntConstant ( m, "Z80_SUB", Z80_SUB );
  PyModule_AddIntConstant ( m, "Z80_SBC", Z80_SBC );
  PyModule_AddIntConstant ( m, "Z80_AND", Z80_AND );
  PyModule_AddIntConstant ( m, "Z80_OR", Z80_OR );
  PyModule_AddIntConstant ( m, "Z80_XOR", Z80_XOR );
  PyModule_AddIntConstant ( m, "Z80_CP", Z80_CP );
  PyModule_AddIntConstant ( m, "Z80_INC", Z80_INC );
  PyModule_AddIntConstant ( m, "Z80_DEC", Z80_DEC );
  PyModule_AddIntConstant ( m, "Z80_DAA", Z80_DAA );
  PyModule_AddIntConstant ( m, "Z80_CPL", Z80_CPL );
  PyModule_AddIntConstant ( m, "Z80_NEG", Z80_NEG );
  PyModule_AddIntConstant ( m, "Z80_CCF", Z80_CCF );
  PyModule_AddIntConstant ( m, "Z80_SCF", Z80_SCF );
  PyModule_AddIntConstant ( m, "Z80_NOP", Z80_NOP );
  PyModule_AddIntConstant ( m, "Z80_HALT", Z80_HALT );
  PyModule_AddIntConstant ( m, "Z80_DI", Z80_DI );
  PyModule_AddIntConstant ( m, "Z80_EI", Z80_EI );
  PyModule_AddIntConstant ( m, "Z80_IM0", Z80_IM0 );
  PyModule_AddIntConstant ( m, "Z80_IM1", Z80_IM1 );
  PyModule_AddIntConstant ( m, "Z80_IM2", Z80_IM2 );
  PyModule_AddIntConstant ( m, "Z80_RLCA", Z80_RLCA );
  PyModule_AddIntConstant ( m, "Z80_RLA", Z80_RLA );
  PyModule_AddIntConstant ( m, "Z80_RRCA", Z80_RRCA );
  PyModule_AddIntConstant ( m, "Z80_RRA", Z80_RRA );
  PyModule_AddIntConstant ( m, "Z80_RLC", Z80_RLC );
  PyModule_AddIntConstant ( m, "Z80_RL", Z80_RL );
  PyModule_AddIntConstant ( m, "Z80_RRC", Z80_RRC );
  PyModule_AddIntConstant ( m, "Z80_RR", Z80_RR );
  PyModule_AddIntConstant ( m, "Z80_SLA", Z80_SLA );
  PyModule_AddIntConstant ( m, "Z80_SRA", Z80_SRA );
  PyModule_AddIntConstant ( m, "Z80_SRL", Z80_SRL );
  PyModule_AddIntConstant ( m, "Z80_RLD", Z80_RLD );
  PyModule_AddIntConstant ( m, "Z80_RRD", Z80_RRD );
  PyModule_AddIntConstant ( m, "Z80_BIT", Z80_BIT );
  PyModule_AddIntConstant ( m, "Z80_SET", Z80_SET );
  PyModule_AddIntConstant ( m, "Z80_RES", Z80_RES );
  PyModule_AddIntConstant ( m, "Z80_JP", Z80_JP );
  PyModule_AddIntConstant ( m, "Z80_JR", Z80_JR );
  PyModule_AddIntConstant ( m, "Z80_DJNZ", Z80_DJNZ );
  PyModule_AddIntConstant ( m, "Z80_CALL", Z80_CALL );
  PyModule_AddIntConstant ( m, "Z80_RET", Z80_RET );
  PyModule_AddIntConstant ( m, "Z80_RETI", Z80_RETI );
  PyModule_AddIntConstant ( m, "Z80_RETN", Z80_RETN );
  PyModule_AddIntConstant ( m, "Z80_RST00", Z80_RST00 );
  PyModule_AddIntConstant ( m, "Z80_RST08", Z80_RST08 );
  PyModule_AddIntConstant ( m, "Z80_RST10", Z80_RST10 );
  PyModule_AddIntConstant ( m, "Z80_RST18", Z80_RST18 );
  PyModule_AddIntConstant ( m, "Z80_RST20", Z80_RST20 );
  PyModule_AddIntConstant ( m, "Z80_RST28", Z80_RST28 );
  PyModule_AddIntConstant ( m, "Z80_RST30", Z80_RST30 );
  PyModule_AddIntConstant ( m, "Z80_RST38", Z80_RST38 );
  PyModule_AddIntConstant ( m, "Z80_IN", Z80_IN );
  PyModule_AddIntConstant ( m, "Z80_INI", Z80_INI );
  PyModule_AddIntConstant ( m, "Z80_INIR", Z80_INIR );
  PyModule_AddIntConstant ( m, "Z80_IND", Z80_IND );
  PyModule_AddIntConstant ( m, "Z80_INDR", Z80_INDR );
  PyModule_AddIntConstant ( m, "Z80_OUT", Z80_OUT );
  PyModule_AddIntConstant ( m, "Z80_OUTI", Z80_OUTI );
  PyModule_AddIntConstant ( m, "Z80_OTIR", Z80_OTIR );
  PyModule_AddIntConstant ( m, "Z80_OUTD", Z80_OUTD );
  PyModule_AddIntConstant ( m, "Z80_OTDR", Z80_OTDR );

  // MNEMONICS SVP
  PyModule_AddIntConstant ( m, "SVP_UNK", MD_SVP_UNK );
  PyModule_AddIntConstant ( m, "SVP_SUB", MD_SVP_SUB );
  PyModule_AddIntConstant ( m, "SVP_CMP", MD_SVP_CMP );
  PyModule_AddIntConstant ( m, "SVP_ADD", MD_SVP_ADD );
  PyModule_AddIntConstant ( m, "SVP_AND", MD_SVP_AND );
  PyModule_AddIntConstant ( m, "SVP_OR", MD_SVP_OR );
  PyModule_AddIntConstant ( m, "SVP_EOR", MD_SVP_EOR );
  PyModule_AddIntConstant ( m, "SVP_MOD", MD_SVP_MOD );
  PyModule_AddIntConstant ( m, "SVP_LD", MD_SVP_LD );
  PyModule_AddIntConstant ( m, "SVP_CALL", MD_SVP_CALL );
  PyModule_AddIntConstant ( m, "SVP_BRA", MD_SVP_BRA );
  PyModule_AddIntConstant ( m, "SVP_MLD", MD_SVP_MLD );
  PyModule_AddIntConstant ( m, "SVP_MPYA", MD_SVP_MPYA );
  PyModule_AddIntConstant ( m, "SVP_MPYS", MD_SVP_MPYS );
  
  /* MODES */
  PyModule_AddIntConstant ( m, "NONE", MD_NONE );
  PyModule_AddIntConstant ( m, "INMb", MD_INMb );
  PyModule_AddIntConstant ( m, "INMw", MD_INMw );
  PyModule_AddIntConstant ( m, "INMl", MD_INMl );
  PyModule_AddIntConstant ( m, "DN", MD_DN );
  PyModule_AddIntConstant ( m, "AN", MD_AN );
  PyModule_AddIntConstant ( m, "pAN", MD_pAN );
  PyModule_AddIntConstant ( m, "pANi", MD_pANi );
  PyModule_AddIntConstant ( m, "pANd", MD_pANd );
  PyModule_AddIntConstant ( m, "pD16AN", MD_pD16AN );
  PyModule_AddIntConstant ( m, "pD8ANXN", MD_pD8ANXN );
  PyModule_AddIntConstant ( m, "pW", MD_pW );
  PyModule_AddIntConstant ( m, "pL", MD_pL );
  PyModule_AddIntConstant ( m, "pD16PC", MD_pD16PC );
  PyModule_AddIntConstant ( m, "pD8PCXN", MD_pD8PCXN );
  PyModule_AddIntConstant ( m, "SR", MD_SR );
  PyModule_AddIntConstant ( m, "VECTOR", MD_VECTOR );
  PyModule_AddIntConstant ( m, "DIS16", MD_DIS16 );
  PyModule_AddIntConstant ( m, "USP", MD_USP );
  PyModule_AddIntConstant ( m, "LIST", MD_LIST );
  PyModule_AddIntConstant ( m, "CCR", MD_CCR );
  PyModule_AddIntConstant ( m, "LABEL", MD_LABEL );
  PyModule_AddIntConstant ( m, "COUNT", MD_COUNT );
  
  /* MODES Z80 */
  PyModule_AddIntConstant ( m, "Z80_NONE", Z80_NONE );
  PyModule_AddIntConstant ( m, "Z80_A", Z80_A );
  PyModule_AddIntConstant ( m, "Z80_B", Z80_B );
  PyModule_AddIntConstant ( m, "Z80_C", Z80_C );
  PyModule_AddIntConstant ( m, "Z80_D", Z80_D );
  PyModule_AddIntConstant ( m, "Z80_E", Z80_E );
  PyModule_AddIntConstant ( m, "Z80_H", Z80_H );
  PyModule_AddIntConstant ( m, "Z80_L", Z80_L );
  PyModule_AddIntConstant ( m, "Z80_I", Z80_I );
  PyModule_AddIntConstant ( m, "Z80_R", Z80_R );
  PyModule_AddIntConstant ( m, "Z80_BYTE", Z80_BYTE );
  PyModule_AddIntConstant ( m, "Z80_pHL", Z80_pHL );
  PyModule_AddIntConstant ( m, "Z80_pBC", Z80_pBC );
  PyModule_AddIntConstant ( m, "Z80_pDE", Z80_pDE );
  PyModule_AddIntConstant ( m, "Z80_pSP", Z80_pSP );
  PyModule_AddIntConstant ( m, "Z80_pIX", Z80_pIX );
  PyModule_AddIntConstant ( m, "Z80_pIY", Z80_pIY );
  PyModule_AddIntConstant ( m, "Z80_pIXd", Z80_pIXd );
  PyModule_AddIntConstant ( m, "Z80_pIYd", Z80_pIYd );
  PyModule_AddIntConstant ( m, "Z80_ADDR", Z80_ADDR );
  PyModule_AddIntConstant ( m, "Z80_BC", Z80_BC );
  PyModule_AddIntConstant ( m, "Z80_DE", Z80_DE );
  PyModule_AddIntConstant ( m, "Z80_HL", Z80_HL );
  PyModule_AddIntConstant ( m, "Z80_SP", Z80_SP );
  PyModule_AddIntConstant ( m, "Z80_IX", Z80_IX );
  PyModule_AddIntConstant ( m, "Z80_IXL", Z80_IXL );
  PyModule_AddIntConstant ( m, "Z80_IXH", Z80_IXH );
  PyModule_AddIntConstant ( m, "Z80_IY", Z80_IY );
  PyModule_AddIntConstant ( m, "Z80_IYL", Z80_IYL );
  PyModule_AddIntConstant ( m, "Z80_IYH", Z80_IYH );
  PyModule_AddIntConstant ( m, "Z80_AF", Z80_AF );
  PyModule_AddIntConstant ( m, "Z80_AF2", Z80_AF2 );
  PyModule_AddIntConstant ( m, "Z80_B0", Z80_B0 );
  PyModule_AddIntConstant ( m, "Z80_B1", Z80_B1 );
  PyModule_AddIntConstant ( m, "Z80_B2", Z80_B2 );
  PyModule_AddIntConstant ( m, "Z80_B3", Z80_B3 );
  PyModule_AddIntConstant ( m, "Z80_B4", Z80_B4 );
  PyModule_AddIntConstant ( m, "Z80_B5", Z80_B5 );
  PyModule_AddIntConstant ( m, "Z80_B6", Z80_B6 );
  PyModule_AddIntConstant ( m, "Z80_B7", Z80_B7 );
  PyModule_AddIntConstant ( m, "Z80_WORD", Z80_WORD );
  PyModule_AddIntConstant ( m, "Z80_F_NZ", Z80_F_NZ );
  PyModule_AddIntConstant ( m, "Z80_F_Z", Z80_F_Z );
  PyModule_AddIntConstant ( m, "Z80_F_NC", Z80_F_NC );
  PyModule_AddIntConstant ( m, "Z80_F_C", Z80_F_C );
  PyModule_AddIntConstant ( m, "Z80_F_PO", Z80_F_PO );
  PyModule_AddIntConstant ( m, "Z80_F_PE", Z80_F_PE );
  PyModule_AddIntConstant ( m, "Z80_F_P", Z80_F_P );
  PyModule_AddIntConstant ( m, "Z80_F_M", Z80_F_M );
  PyModule_AddIntConstant ( m, "Z80_BRANCH", Z80_BRANCH );
  PyModule_AddIntConstant ( m, "Z80_pB", Z80_pB );
  PyModule_AddIntConstant ( m, "Z80_pC", Z80_pC );
  PyModule_AddIntConstant ( m, "Z80_pD", Z80_pD );
  PyModule_AddIntConstant ( m, "Z80_pE", Z80_pE );
  PyModule_AddIntConstant ( m, "Z80_pH", Z80_pH );
  PyModule_AddIntConstant ( m, "Z80_pL", Z80_pL );
  PyModule_AddIntConstant ( m, "Z80_pA", Z80_pA );
  PyModule_AddIntConstant ( m, "Z80_pBYTE", Z80_pBYTE );

  // MODES SVP
  PyModule_AddIntConstant ( m, "SVP_NONE", MD_SVP_NONE );
  PyModule_AddIntConstant ( m, "SVP__", MD_SVP__ );
  PyModule_AddIntConstant ( m, "SVP_X", MD_SVP_X );
  PyModule_AddIntConstant ( m, "SVP_Y", MD_SVP_Y );
  PyModule_AddIntConstant ( m, "SVP_A", MD_SVP_A );
  PyModule_AddIntConstant ( m, "SVP_ST", MD_SVP_ST );
  PyModule_AddIntConstant ( m, "SVP_STACK", MD_SVP_STACK );
  PyModule_AddIntConstant ( m, "SVP_PC", MD_SVP_PC );
  PyModule_AddIntConstant ( m, "SVP_P", MD_SVP_P );
  PyModule_AddIntConstant ( m, "SVP_PM0", MD_SVP_PM0 );
  PyModule_AddIntConstant ( m, "SVP_PM1", MD_SVP_PM1 );
  PyModule_AddIntConstant ( m, "SVP_PM2", MD_SVP_PM2 );
  PyModule_AddIntConstant ( m, "SVP_XST", MD_SVP_XST );
  PyModule_AddIntConstant ( m, "SVP_PM4", MD_SVP_PM4 );
  PyModule_AddIntConstant ( m, "SVP_EXT5", MD_SVP_EXT5 );
  PyModule_AddIntConstant ( m, "SVP_PMC", MD_SVP_PMC );
  PyModule_AddIntConstant ( m, "SVP_AL", MD_SVP_AL );
  PyModule_AddIntConstant ( m, "SVP_PRI", MD_SVP_PRI );
  PyModule_AddIntConstant ( m, "SVP_PRI_MVAL", MD_SVP_PRI_MVAL );
  PyModule_AddIntConstant ( m, "SVP_PRI_INC", MD_SVP_PRI_INC );
  PyModule_AddIntConstant ( m, "SVP_PRI_MODINC", MD_SVP_PRI_MODINC );
  PyModule_AddIntConstant ( m, "SVP_PRI_MODDEC", MD_SVP_PRI_MODDEC );
  PyModule_AddIntConstant ( m, "SVP_ADR", MD_SVP_ADR );
  PyModule_AddIntConstant ( m, "SVP_IMM", MD_SVP_IMM );
  PyModule_AddIntConstant ( m, "SVP_PPRI", MD_SVP_PPRI );
  PyModule_AddIntConstant ( m, "SVP_PPRI_MVAL", MD_SVP_PPRI_MVAL );
  PyModule_AddIntConstant ( m, "SVP_PPRI_INC", MD_SVP_PPRI_INC );
  PyModule_AddIntConstant ( m, "SVP_PPRI_MODINC", MD_SVP_PPRI_MODINC );
  PyModule_AddIntConstant ( m, "SVP_PPRI_MODDEC", MD_SVP_PPRI_MODDEC );
  PyModule_AddIntConstant ( m, "SVP_RI", MD_SVP_RI );
  PyModule_AddIntConstant ( m, "SVP_SIMM", MD_SVP_SIMM );
  PyModule_AddIntConstant ( m, "SVP_COND_UNK", MD_SVP_COND_UNK );
  PyModule_AddIntConstant ( m, "SVP_COND_TRUE", MD_SVP_COND_TRUE );
  PyModule_AddIntConstant ( m, "SVP_COND_Z", MD_SVP_COND_Z );
  PyModule_AddIntConstant ( m, "SVP_COND_N", MD_SVP_COND_N );
  PyModule_AddIntConstant ( m, "SVP_ADDR", MD_SVP_ADDR );
  PyModule_AddIntConstant ( m, "SVP_PA", MD_SVP_PA );
  PyModule_AddIntConstant ( m, "SVP_OP_SR", MD_SVP_OP_SR );
  PyModule_AddIntConstant ( m, "SVP_OP_SL", MD_SVP_OP_SL );
  PyModule_AddIntConstant ( m, "SVP_OP_NEG", MD_SVP_OP_NEG );
  PyModule_AddIntConstant ( m, "SVP_OP_ABS", MD_SVP_OP_ABS );
  PyModule_AddIntConstant ( m, "SVP_OP_UNK", MD_SVP_OP_UNK );
  
  /* TIPUS D'ACCESSOS A MEMÒRIA. */
  PyModule_AddIntConstant ( m, "READ", MD_READ );
  PyModule_AddIntConstant ( m, "WRITE", MD_WRITE );
  
  /* TIPUS SRAM INFO. */
  PyModule_AddIntConstant ( m, "SRAMINFO_AVAILABLE", MD_SRAMINFO_AVAILABLE );
  PyModule_AddIntConstant ( m, "SRAMINFO_ISFORBACKUP",
        		    MD_SRAMINFO_ISFORBACKUP );
  PyModule_AddIntConstant ( m, "SRAMINFO_ODDBYTES", MD_SRAMINFO_ODDBYTES );
  PyModule_AddIntConstant ( m, "SRAMINFO_EVENBYTES", MD_SRAMINFO_EVENBYTES );
  
  return m;

} /* end PyInit_MD */


/* PROVISIONAL */
void
MD_cpu_reset_external_devices_signal (void)
{
  printf("RESET EXTERNAL DEVICES\n");
}
