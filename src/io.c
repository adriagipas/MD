/*
 * Copyright 2013-2022 Adrià Giménez Pastor.
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
 *  io.c - Implementació del mòdul I/O.
 *
 */


#include <stddef.h>
#include <stdlib.h>

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

#define IODEV(DEV) ((iodev_t *) (DEV))

#define iodev_free(DEV) (DEV)->free ( IODEV(DEV) );

#define PAD(PTR) ((pad_t *) (PTR))

#define PAD6B(PTR) ((pad6b_t *) (PTR))




/*********/
/* TIPUS */
/*********/

#define CLASS_IODEVICE        					\
  void (*free) (iodev_t *dev);        				\
  MDu8 (*read) (iodev_t * dev);        				\
  void (*end_frame) (iodev_t *dev);        			\
  /* Escriu data&mask */        				\
  void (*write) (iodev_t * dev,MDu8 mask,MDu8 data);        \
  int (*save_state) (iodev_t *dev,FILE *f);        		\
  int (*load_state) (iodev_t *dev,FILE *f);        		\
  void (*init_state) (iodev_t *dev)

/* Dispositiu extern que es pot conectar a una MegaDrive. */
typedef struct iodev_t_ iodev_t;
struct iodev_t_
{
  CLASS_IODEVICE;
};


/* NOTA: De moment no implemente el serial control. */
typedef struct
{
  
  MDu8         control;
  MDu8         latch;
  iodev_t     *dev;
  MD_IODevice  type;
  
} port_t;


typedef int (pad_callback_t) (void);

/* Tipus pad normal. */
typedef struct
{
  
  CLASS_IODEVICE;
  MD_Bool         TH;
  pad_callback_t *callback;
  
} pad_t;


/* Tipus pad 6 botons. */
typedef struct
{
  
  CLASS_IODEVICE;
  MD_Bool         TH;
  int             counter;
  pad_callback_t *callback;
  
} pad6b_t;




/*********/
/* ESTAT */
/*********/

static port_t _port1, _port2, _port_exp;

/* Callbacks. */
static MD_CheckButtons *_check_buttons;
static void *_udata;

/* Inicialitzat. */
static MD_Bool _initialised= MD_FALSE;




/**************/
/* NORMAL PAD */
/**************/

static void
pad_free (
          iodev_t *dev
          )
{
  free ( dev );
} /* end pad_free */


static MDu8
pad_read (
          iodev_t *dev
          )
{
  
  int buttons;
  MDu8 ret;
  
  
  buttons= PAD(dev)->callback ();
  if ( PAD(dev)->TH )
    {
      ret= 0x00; /* TH a 0 per a que siga 1. */
      if ( buttons&MD_C ) ret|= 0x20;
      if ( buttons&MD_B ) ret|= 0x10;
      if ( buttons&MD_RIGHT ) ret|= 0x08;
      if ( buttons&MD_LEFT ) ret|= 0x04;
      if ( buttons&MD_DOWN ) ret|= 0x02;
      if ( buttons&MD_UP ) ret|= 0x01;
    }
  else
    {
      ret= 0x4C; /* TH, 3 i 2 a 1 per a que siga 0 al fer ~ */
      if ( buttons&MD_START ) ret|= 0x20;
      if ( buttons&MD_A ) ret|= 0x10;
      if ( buttons&MD_DOWN ) ret|= 0x02;
      if ( buttons&MD_UP ) ret|= 0x01;
    }
  
  return ~ret;
  
} /* end pad_read */


static void
pad_end_frame (
               iodev_t *dev
               )
{
} /* end pad_end_frame */


static void
pad_write (
           iodev_t *dev,
           MDu8     mask,
           MDu8     data
           )
{
  PAD(dev)->TH= ((mask&data&0x40)!=0);
} /* end pad_write */


static int
pad_save_state (
        	iodev_t *dev,
        	FILE    *f
        	)
{

  SAVE ( PAD(dev)->TH );
  
  return 0;
  
} /* end pad_save_state */


static int
pad_load_state (
        	iodev_t *dev,
        	FILE    *f
        	)
{

  LOAD ( PAD(dev)->TH );
  
  return 0;
  
} /* end pad_load_state */


static void
pad_init_state (
        	iodev_t *dev
        	)
{
  PAD(dev)->TH= MD_FALSE;
} /* end pad_init_state */


static iodev_t *
pad_new (
         pad_callback_t *callback
         )
{
  
  pad_t *new;
  
  
  new= (pad_t *) malloc ( sizeof(pad_t) );
  new->TH= MD_FALSE;
  new->free= pad_free;
  new->read= pad_read;
  new->end_frame= pad_end_frame;
  new->write= pad_write;
  new->save_state= pad_save_state;
  new->load_state= pad_load_state;
  new->init_state= pad_init_state;
  new->callback= callback;
  
  return (iodev_t *) new;
  
} /* end pad_new */




/****************/
/* PAD 6 BOTONS */
/****************/

static void
pad6b_free (
            iodev_t *dev
            )
{
  free ( dev );
} /* end pad6b_free */


static MDu8
pad6b_read (
            iodev_t *dev
            )
{
  
  int buttons;
  MDu8 ret;
  

  buttons= PAD6B(dev)->callback ();
  ++(PAD6B(dev)->counter);
  if ( PAD(dev)->TH )
    {
      ret= 0x00; /* TH a 0 per a que siga 1. */
      if ( PAD6B(dev)->counter == 7 )
        {
          if ( buttons&MD_C ) ret|= 0x20;
          if ( buttons&MD_B ) ret|= 0x10;
          if ( buttons&MD_X ) ret|= 0x04;
          if ( buttons&MD_Y ) ret|= 0x02;
          if ( buttons&MD_Z ) ret|= 0x01;
        }
      else
        {
          if ( buttons&MD_C ) ret|= 0x20;
          if ( buttons&MD_B ) ret|= 0x10;
          if ( buttons&MD_RIGHT ) ret|= 0x08;
          if ( buttons&MD_LEFT ) ret|= 0x04;
          if ( buttons&MD_DOWN ) ret|= 0x02;
          if ( buttons&MD_UP ) ret|= 0x01;
        }
    }
  else
    {
      if ( PAD6B(dev)->counter == 8 )
        {
          ret= 0x40; /* TH a 1, i 3, 2, 1 i 0 a 0 per a que siga ¿? al fer ~ */
          if ( buttons&MD_START ) ret|= 0x20;
          if ( buttons&MD_A ) ret|= 0x10;
        }
      else if ( PAD6B(dev)->counter == 6 )
        {
          ret= 0x4F; /* TH, 3, 2, 1 i 0 a 1 per a que siga 0 al fer ~ */
          if ( buttons&MD_START ) ret|= 0x20;
          if ( buttons&MD_A ) ret|= 0x10;
        }
      else
        {
          ret= 0x4C; /* TH, 3 i 2 a 1 per a que siga 0 al fer ~ */
          if ( buttons&MD_START ) ret|= 0x20;
          if ( buttons&MD_A ) ret|= 0x10;
          if ( buttons&MD_DOWN ) ret|= 0x02;
          if ( buttons&MD_UP ) ret|= 0x01;
        }
    }
  if ( PAD6B(dev)->counter == 8 ) PAD6B(dev)->counter= 0;
  
  return ~ret;
  
} /* end pad6b_read */


static void
pad6b_end_frame (
        	 iodev_t *dev
        	 )
{
  PAD6B(dev)->counter= 0;
} /* end pad6b_end_frame */


static void
pad6b_write (
             iodev_t *dev,
             MDu8     mask,
             MDu8     data
           )
{
  PAD6B(dev)->TH= ((mask&data&0x40)!=0);
} /* end pad6b_write */


static int
pad6b_save_state (
        	  iodev_t *dev,
        	  FILE    *f
        	  )
{

  SAVE ( PAD6B(dev)->TH );
  SAVE ( PAD6B(dev)->counter );
  
  return 0;
  
} /* end pad6b_save_state */


static int
pad6b_load_state (
        	  iodev_t *dev,
        	  FILE    *f
        	  )
{

  LOAD ( PAD6B(dev)->TH );
  LOAD ( PAD6B(dev)->counter );
  CHECK ( PAD6B(dev)->counter >= 0 && PAD6B(dev)->counter < 8 );
  
  return 0;
  
} /* end pad6b_load_state */


static void
pad6b_init_state (
        	  iodev_t *dev
        	  )
{
  
  PAD6B(dev)->TH= MD_FALSE;
  PAD6B(dev)->counter= 0;
  
} /* end pad6b_init_state */


static iodev_t *
pad6b_new (
           pad_callback_t *callback
           )
{
  
  pad6b_t *new;
  
  
  new= (pad6b_t *) malloc ( sizeof(pad6b_t) );
  new->TH= MD_FALSE;
  new->counter= 0;
  new->free= pad6b_free;
  new->read= pad6b_read;
  new->end_frame= pad6b_end_frame;
  new->write= pad6b_write;
  new->save_state= pad6b_save_state;
  new->load_state= pad6b_load_state;
  new->init_state= pad6b_init_state;
  new->callback= callback;
  
  return (iodev_t *) new;
  
} /* end pad6b_new */




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static int
pad1_callback (void)
{
  return _check_buttons!=NULL ? _check_buttons ( 0, _udata ) : 0;
} /* end pad1_callback */


static int
pad2_callback (void)
{
  return _check_buttons!=NULL ? _check_buttons ( 1, _udata ) : 0;
} /* end pad2_callback */


static void
port1_create (
              const MD_IODevice type
              )
{

  _port1.type= type;
  switch ( type )
    {
    case MD_IODEV_PAD: _port1.dev= pad_new ( pad1_callback ); break;
    case MD_IODEV_PAD6B: _port1.dev= pad6b_new ( pad1_callback ); break;
    case MD_IODEV_NONE:
    default: _port1.type= MD_IODEV_NONE; _port1.dev= NULL;
    }
  
} /* end port1_create */


static void
port2_create (
              const MD_IODevice type
              )
{
  
  _port2.type= type;
  switch ( type )
    {
    case MD_IODEV_PAD: _port2.dev= pad_new ( pad2_callback ); break;
    case MD_IODEV_PAD6B: _port2.dev= pad6b_new ( pad2_callback ); break;
    case MD_IODEV_NONE:
    default: _port2.type= MD_IODEV_NONE; _port2.dev= NULL;
    }
  
} /* end port2_create */


static void
port_exp_create (
        	 const MD_IODevice type
        	 )
{

  _port_exp.type= type;
  switch ( type )
    {
    case MD_IODEV_NONE:
    default: _port_exp.type= MD_IODEV_NONE; _port_exp.dev= NULL;
    }
  
} /* end port_exp_create */


static MDu8
data_read (
           port_t *port
           )
{
  
  MDu8 val;
  
  
  val= (port->dev!=NULL) ? port->dev->read ( port->dev ) : 0xFF;
  
  return
    ((port->control|0x80)&port->latch) |
    (((~port->control)&0x7F)&val);
  
} /* end data_read */


static void
data_write (
            port_t     *port,
            const MDu8  data
            )
{
  
  if ( port->dev != NULL )
    port->dev->write ( port->dev, port->control&0x7F, data );
  port->latch= data;
  
} /* end data_write */


#include <stdio.h>
static void
control_write (
               port_t     *port,
               const MDu8  data
               )
{
  
  port->control= data;
  if ( data&0x80 ) printf ( "io: TH-INT not supported\n" ); 
  
} /* end control_write */


static int
save_state (
            port_t *port,
            FILE   *f
            )
{

  SAVE ( port->control );
  SAVE ( port->latch );
  SAVE ( port->type );
  if ( port->dev != NULL )
    {
      if ( port->dev->save_state ( port->dev, f ) != 0 )
        return -1;
    }
  
  return 0;
  
} /* end save_state */


static int
load_state (
            port_t *port,
            FILE   *f
            )
{

  MD_IODevice dev_type;

  
  LOAD ( port->control );
  LOAD ( port->latch );
  LOAD ( dev_type );
  if ( dev_type < 0 || dev_type > MD_IODEV_NONE )
    return -1;
  if ( dev_type != port->type )
    {
      if ( port->dev != NULL ) iodev_free ( port->dev );
      if ( port == &_port1 ) port1_create ( dev_type );
      else if ( port == &_port2 ) port2_create ( dev_type );
      else /*if ( port == &_port_exp ) */ port_exp_create ( dev_type );
    }
  if ( port->dev != NULL )
    {
      if ( port->dev->load_state ( port->dev, f ) != 0 )
        return -1;
    }
  
  return 0;
  
} /* end load_state */


static void
free_devs (void)
{
  
  if ( _port1.dev != NULL ) iodev_free ( _port1.dev );
  if ( _port2.dev != NULL ) iodev_free ( _port2.dev );
  if ( _port_exp.dev != NULL ) iodev_free ( _port_exp.dev );
  
} /* end free_devs */




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
MD_io_close (void)
{
  
  free_devs ();
  _initialised= MD_FALSE;
  
} /* end MD_io_close */


MDu8
MD_io_data_read_1 (void)
{
  return data_read ( &_port1 );
} /* end MD_io_data_read_1 */


MDu8
MD_io_data_read_2 (void)
{
  return data_read ( &_port2 );
} /* end MD_io_data_read_2 */


MDu8
MD_io_data_read_exp (void)
{
  return data_read ( &_port_exp );
} /* end MD_io_data_read_exp */


void
MD_io_data_write_1 (
        	    const MDu8 data
        	    )
{
  data_write ( &_port1, data );
} /* end MD_io_data_write_1 */


void
MD_io_data_write_2 (
        	    const MDu8 data
        	    )
{
  data_write ( &_port2, data );
} /* end MD_io_data_write_2 */


void
MD_io_data_write_exp (
        	      const MDu8 data
        	      )
{
  data_write ( &_port_exp, data );
} /* end MD_io_data_write_exp */


void
MD_io_end_frame_1 (void)
{
  if ( _port1.dev != NULL ) _port1.dev->end_frame ( _port1.dev );
} /* end MD_io_end_frame_1 */

void
MD_io_end_frame_2 (void)
{
  if ( _port2.dev != NULL ) _port2.dev->end_frame ( _port2.dev );
} /* end MD_io_end_frame_2 */


MDu8
MD_io_control_read_1 (void)
{
  return _port1.control;
} /* end MD_io_control_read_1 */


MDu8
MD_io_control_read_2 (void)
{
  return _port2.control;
} /* end MD_io_control_read_2 */


MDu8
MD_io_control_read_exp (void)
{
  return _port_exp.control;
} /* end MD_io_control_read_exp */


void
MD_io_control_write_1 (
        	       const MDu8 data
        	       )
{
  control_write ( &_port1, data );
} /* end MD_io_control_write_1 */


void
MD_io_control_write_2 (
        	       const MDu8 data
        	       )
{
  control_write ( &_port2, data );
} /* end MD_io_control_write_2 */


void
MD_io_control_write_exp (
        		 const MDu8 data
        		 )
{
  control_write ( &_port_exp, data );
} /* end MD_io_control_write_exp */


void
MD_io_init (
            const MD_IOPluggedDevices  devices,
            MD_CheckButtons           *check_buttons,
            void                      *udata
            )
{

  /* Callbacks. */
  _check_buttons= check_buttons;
  _udata= udata;
  
  /* Crea ports. */
  port1_create ( devices.dev1 );
  port2_create ( devices.dev2 );
  port_exp_create ( devices.dev_exp );

  /* Inicialitza. */
  MD_io_init_state ();

  _initialised= MD_TRUE;
  
} /* end MD_io_init */


void
MD_io_init_state (void)
{
  
  /* Port 1. */
  _port1.control= _port1.latch= 0x00;
  if ( _port1.dev != NULL )
    _port1.dev->init_state ( _port1.dev );
  
  /* Port 2. */
  _port2.control= _port2.latch= 0x00;
  if ( _port2.dev != NULL )
    _port2.dev->init_state ( _port2.dev );
  
  /* Port expansió. */
  _port_exp.control= _port_exp.latch= 0x00;
  if ( _port_exp.dev != NULL )
    _port_exp.dev->init_state ( _port_exp.dev );
  
} /* end MD_io_init_state */


int
MD_io_save_state (
        	  FILE *f
        	  )
{

  if ( save_state ( &_port1, f ) != 0 ) return -1;
  if ( save_state ( &_port2, f ) != 0 ) return -1;
  if ( save_state ( &_port_exp, f ) != 0 ) return -1;
  
  return 0;
  
} /* end MD_io_save_state */


int
MD_io_load_state (
        	  FILE *f
        	  )
{

  if ( load_state ( &_port1, f ) != 0 ) return -1;
  if ( load_state ( &_port2, f ) != 0 ) return -1;
  if ( load_state ( &_port_exp, f ) != 0 ) return -1;
  
  return 0;
  
} /* end MD_io_load_state */


void
MD_get_io_devs (
        	MD_IOPluggedDevices *devs
        	)
{

  if ( !_initialised ) return;
  
  devs->dev1= _port1.type;
  devs->dev2= _port2.type;
  devs->dev_exp= _port_exp.type;
  
} /* end MD_get_io_devs */


void
MD_set_io_devs (
        	const MD_IOPluggedDevices devs
        	)
{

  if ( !_initialised ) return;
  
  free_devs ();
  port1_create ( devs.dev1 );
  port2_create ( devs.dev2 );
  port_exp_create ( devs.dev_exp );

  /* Reinicialitza l'estat però sols l'intern. */
  if ( _port1.dev != NULL )
    _port1.dev->init_state ( _port1.dev );
  if ( _port2.dev != NULL )
    _port2.dev->init_state ( _port2.dev );
  if ( _port_exp.dev != NULL )
    _port_exp.dev->init_state ( _port_exp.dev );
  
} /* end MD_set_io_devs */
