/*
 * Copyright 2016-2022 Adrià Giménez Pastor.
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
 *  eeprom.c - Implementació del mòdul que simula la EEPROM.
 *
 */


#include <errno.h>
#include <stddef.h>
#include <stdio.h>
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

#define mem_alloc(type,size)                            \
  ((type *) mem_alloc_pvt_ ( sizeof(type) * (size) ))

#define EEPROM_CLASS        				\
  /* SDA */        					\
  MDu32 sda_in_addr;        				\
  int sda_in_desp;        				\
  MDu32 sda_out_addr;        				\
  int sda_out_desp;        				\
  MD_Bool sda_in;        				\
  MD_Bool sda_out;        				\
  /* SCL */        					\
  MDu32 scl_addr;        				\
  int scl_desp;        					\
  MD_Bool scl;        					\
  /* Estat. */        					\
  int  state;        					\
  MDu8 word;        					\
  int nbits;        					\
  /* Mètodes. */        				\
  void (*free) (eeprom_t *);        			\
  void (*ack_read) (eeprom_t *);        		\
  void (*write) (eeprom_t *);        			\
  void (*start) (eeprom_t *);        			\
  int (*save_state) (const eeprom_t *,FILE *f);        	\
  int (*load_state) (eeprom_t *,FILE *f);        	\
  void (*init_state) (eeprom_t *)

#define CHECKC(CHECKSUM,SERIAL)        				\
  (header.checksum == CHECKSUM && !strcmp ( SERIAL, header.type_snumber ))
   



/*********/
/* TIPUS */
/*********/

/* Per a state. */
enum
  {
    WAIT_START,
    WAIT_INIT_WRITE_ADDR,
    WRITE_WAIT_SET_NEXT_BIT,
    WRITE_WAIT_TRANSFER,
    WRITE_WAIT_SET_ACK,
    WRITE_WAIT_ACK,
    READ_WAIT_SET_NEXT_BIT,
    READ_WAIT_TRANSFER,
    READ_WAIT_SET_ACK,
    READ_WAIT_ACK
  };

typedef struct eeprom_ eeprom_t;
struct eeprom_
{
  EEPROM_CLASS;
};

typedef struct
{
  
  EEPROM_CLASS;
  enum {
    M1_WAIT_START,
    M1_WAIT_ADDR,
    M1_READING,
    M1_WRITING
  } mode_state;
  MDu8 size_mask;
  MDu8 page_mask;
  MDu8 addr;
  MDu8 base_addr;
  MDu8 *mem;
  
} eeprom_mode1_t;


typedef struct
{
  
  EEPROM_CLASS;
  enum {
    M2_WAIT_START,
    M2_WAIT_ADDR,
    M2_WAIT_ADDR2,
    M2_READING,
    M2_WRITING
  } mode_state;
  MDu16 size_mask;
  MDu8 page_mask;
  MDu16 addr;
  MDu16 base_addr;
  MDu8 *mem;
  
} eeprom_mode2_t;


typedef struct
{
  
  EEPROM_CLASS;
  enum {
    M3_WAIT_START,
    M3_WAIT_ADDR,
    M3_WAIT_ADDR2,
    M3_WAIT_ADDR3,
    M3_READING,
    M3_WRITING
  } mode_state;
  MDu32 size_mask;
  MDu8 page_mask;
  MDu32 addr;
  MDu16 base_addr;
  MDu8 *mem;
  
} eeprom_mode3_t;




/*********/
/* ESTAT */
/*********/

/* Callbacks. */
static MD_GetEEPROM *_get_eeprom;
static void *_udata;

/* EEPROM. */
static eeprom_t *_eeprom;

/* Model d'EEPROM. */
static enum
  {
    EEPROM_NULL,
    EEPROM_SEGA,
    EEPROM_CAPCOM,
    EEPROM_EA,
    EEPROM_CODEMASTERS_M1,
    EEPROM_ACCLAIM_T1,
    EEPROM_ACCLAIM_T2_A,
    EEPROM_ACCLAIM_T2_B,
    EEPROM_CODEMASTERS_M2,
    EEPROM_CODEMASTERS_M22,
    EEPROM_ACCLAIM_T2_C
  } _model;




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static void *
mem_alloc_pvt_ (
                size_t nbytes
                )
{
  
  void *new;
  
  
  new= malloc ( nbytes );
  if ( new == NULL )
    {
      fprintf ( stderr, "[EE] %s\n", strerror ( errno ) );
      exit ( EXIT_FAILURE );
    }
  
  return new;
  
} /* end mem_alloc_pvt_ */


static void
eeprom_init_state (
        	   eeprom_t *eeprom
        	   )
{

  eeprom->sda_in= MD_FALSE;
  eeprom->sda_out= MD_FALSE;
  eeprom->scl= MD_FALSE;
  eeprom->state= WAIT_START;
  eeprom->word= 0x00;
  eeprom->nbits= 0;
  
} /* eeprom_init_state */


static int
eeprom_save_state (
        	   const eeprom_t *eeprom,
        	   FILE           *f
        	   )
{

  SAVE ( eeprom->sda_in );
  SAVE ( eeprom->sda_out );
  SAVE ( eeprom->scl );
  SAVE ( eeprom->state );
  SAVE ( eeprom->word );
  SAVE ( eeprom->nbits );
  
  return 0;
  
} /* end eeprom_save_state */


static int
eeprom_load_state (
        	   eeprom_t *eeprom,
        	   FILE     *f
        	   )
{

  LOAD ( eeprom->sda_in );
  LOAD ( eeprom->sda_out );
  LOAD ( eeprom->scl );
  LOAD ( eeprom->state );
  LOAD ( eeprom->word );
  LOAD ( eeprom->nbits );
  
  return 0;
  
} /* end eeprom_load_state */


static void
set_model (
           const MD_Rom *rom
           )
{

  MD_RomHeader header;


  MD_rom_get_header ( rom, &header );
  
  /* Wonder Boy in Monster World (UE) */
  if ( CHECKC ( 0x9d79, "GM G-4060  -00" ) ) _model= EEPROM_SEGA;

  /* Evander Holyfield's Real Deal Boxing (JU) */
  else if ( CHECKC ( 0x760f, "GM MK-1215 -00" ) ) _model= EEPROM_SEGA;

  /* Greatest Heavyweights (E) */
  else if ( CHECKC ( 0x95e7, "GM MK-1228 -00" ) ) _model= EEPROM_SEGA;

  /* Greatest Heavyweights (J) */
  else if ( CHECKC ( 0x0000, "GM G-5538  -00" ) ) _model= EEPROM_SEGA;

  /* Greatest Heavyweights (U) */
  else if ( CHECKC ( 0x7270, "GM MK-1228 -00" ) ) _model= EEPROM_SEGA;

  /* Honoo no Toukyuuji Dodge Danpei (J)  */
  else if ( CHECKC ( 0xbacc, "GM_00004076-00" ) ) _model= EEPROM_SEGA;

  /* MLBPA Sports Talk Baseball (U) */
  else if ( CHECKC ( 0xb939, "MK 00001211-00" ) ) _model= EEPROM_SEGA;

  /* Wonder Boy V - Monster World III (J) */
  else if ( CHECKC ( 0x0278, "GM G-4060  -00" ) ) _model= EEPROM_SEGA;

  /* Ninja Burai Densetsu (J) */
  else if ( CHECKC ( 0x487c, "GM G-4524  -00" ) ) _model= EEPROM_SEGA;

  /* Megaman - The Wily Wars (E) */
  else if ( CHECKC ( 0xad23, "GM T-12046 -00" ) ) _model= EEPROM_CAPCOM;

  /* Rockman Megaworld (J) [!].gen */
  else if ( CHECKC ( 0xea80, "GM T-12053 -00" ) ) _model= EEPROM_CAPCOM;

  /* Rings of Power (UE) */
  else if ( CHECKC ( 0x8b9f, "GM T-50176 -00" ) ) _model= EEPROM_EA;

  /* NHLPA Hockey '93 (UE) (REV01) */
  else if ( CHECKC ( 0xfa57, "GM T-50396 -01" ) ) _model= EEPROM_EA;

  /* John Madden Football '93 - Championship Edition (U) */
  else if ( CHECKC ( 0x5807, "GM T-50516 -00" ) ) _model= EEPROM_EA;
  
  /* John Madden Football '93 (UE) */
  else if ( CHECKC ( 0xa107, "GM T-50446 -00" ) ) _model= EEPROM_EA;

  /* Brian Lara Cricket (E) */
  else if ( CHECKC ( 0x7e65, "GM T-120106-50" ) ) _model= EEPROM_CODEMASTERS_M1;

  /* NBA Jam (UE) (REV00) */
  else if ( CHECKC ( 0x694f, "GM T-081326 00" ) ) _model= EEPROM_ACCLAIM_T1;

  /* NBA Jam (J) */
  else if ( CHECKC ( 0x5b9f, "GM T-81033  00" ) ) _model= EEPROM_ACCLAIM_T1;

  /* NBA Jam Tournament Edition (W) (REV01) */
  else if ( CHECKC ( 0x8ae1, "GM T-81406 -01" ) ) _model= EEPROM_ACCLAIM_T2_A;

  /* NFL Quarterback Club (W) */
  else if ( CHECKC ( 0x7651, "GM T-081276 00" ) ) _model= EEPROM_ACCLAIM_T2_A;
  
  /* NFL Quarterback Club 96 (UE) */
  else if ( CHECKC ( 0x3de6, "GM T-081586-00" ) ) _model= EEPROM_ACCLAIM_T2_B;

  /* Micro Machines 2 - Turbo Tournament (E) (J-Cart) */
  else if ( CHECKC ( 0x7e50, "GM T-120096-50" ) ) _model= EEPROM_CODEMASTERS_M2;

  /* Micro Machines 2 - Turbo Tournament (E) */
  else if ( CHECKC ( 0xef29, "GM T-120096-50" ) ) _model= EEPROM_CODEMASTERS_M2;
  
  /* Micro Machines Military - It's a Blast! (E) */
  else if ( CHECKC ( 0x168b, "GM 00000000-00" ) ) _model= EEPROM_CODEMASTERS_M2;
  
  /* Micro Machines - Turbo Tournament '96 (V1.1) (E) (J-Cart) */
  else if ( CHECKC ( 0x165e, "GM 00000000-00" ) )
    _model= EEPROM_CODEMASTERS_M22;

  /* College Slam (U) */
  else if ( CHECKC ( 0x6dd9, "GM T-81576 -00" ) ) _model= EEPROM_ACCLAIM_T2_C;

  /* Frank Thomas Big Hurt Baseball (UE) */
  else if ( CHECKC ( 0xcb78, "GM T-81476 -00" ) ) _model= EEPROM_ACCLAIM_T2_C;
  
  else _model= EEPROM_NULL;
  
} /* end set_model */




/**********/
/* MODE 1 */
/**********/

#define MODE1(PTR) ((eeprom_mode1_t *) (PTR))

static void
eeprom_mode1_free (
        	   eeprom_t *eeprom
        	   )
{
  free ( eeprom );
} /* end eeprom_mode1_free */


static void
eeprom_mode1_ack_read (
        	       eeprom_t *self
        	       )
{
  
  /* Si no és correcte o absurd reseteja. */
  if ( MODE1(self)->mode_state != M1_READING )
    {
      MODE1(self)->mode_state= M1_WAIT_START;
      self->state= WAIT_START;
      return;
    }
  
  /* Prepara per a la següent lectura. */
  self->state= READ_WAIT_SET_NEXT_BIT;
  self->word= MODE1(self)->mem[MODE1(self)->addr];
  self->sda_out= (self->word&0x80)!=0;
  MODE1(self)->addr= (MODE1(self)->addr+1)&MODE1(self)->size_mask;
  self->nbits= 0;
  
} /* end eeprom_mode1_ack_read */


static void
eeprom_mode1_write (
        	    eeprom_t *self
        	    )
{
  
  switch ( MODE1(self)->mode_state )
    {
      
    case M1_WAIT_ADDR:
      /* Fixa l'adreça. */
      MODE1(self)->addr= (self->word>>1)&MODE1(self)->size_mask;
      MODE1(self)->base_addr= MODE1(self)->addr&(~(MODE1(self)->page_mask));
      /* Prepara operació. */
      if ( self->word&0x1 ) /* Read */
        {
          self->state= READ_WAIT_SET_NEXT_BIT;
          self->word= MODE1(self)->mem[MODE1(self)->addr];
          self->sda_out= (self->word&0x80)!=0;
          MODE1(self)->addr= (MODE1(self)->addr+1)&MODE1(self)->size_mask;
          self->nbits= 0;
          MODE1(self)->mode_state= M1_READING;
        }
      else /* Write */
        {
          self->state= WRITE_WAIT_SET_NEXT_BIT;
          self->word= 0;
          self->nbits= 0;
          MODE1(self)->mode_state= M1_WRITING;
        }
      break;

    case M1_WRITING:
      MODE1(self)->mem[MODE1(self)->addr]= self->word;
      MODE1(self)->addr=
        MODE1(self)->base_addr |
        ((MODE1(self)->addr+1)&(MODE1(self)->page_mask));
      self->state= WRITE_WAIT_SET_NEXT_BIT;
      self->word= 0;
      self->nbits= 0;
      break;
      
    default: /* En cas absurd resteja. */
      MODE1(self)->mode_state= M1_WAIT_START;
      self->state= WAIT_START;
      break;
      
    }
  
} /* end eeprom_mode1_write */


static void
eeprom_mode1_start (
        	    eeprom_t *self
        	    )
{
  MODE1(self)->mode_state= M1_WAIT_ADDR;
} /* end eeprom_mode1_start */


static void
eeprom_mode1_init_state (
        		 eeprom_t *self
        		 )
{

  eeprom_init_state ( self );
  MODE1(self)->addr= 0;
  MODE1(self)->base_addr= 0;
  MODE1(self)->mode_state= M1_WAIT_START;
  
} /* end eeprom_mode1_init_state */


static int
eeprom_mode1_save_state (
        		 const eeprom_t *self,
        		 FILE           *f
        		 )
{

  if ( eeprom_save_state ( self, f ) != 0 )
    return -1;
  SAVE ( MODE1(self)->mode_state );
  SAVE ( MODE1(self)->addr );
  SAVE ( MODE1(self)->base_addr );
  if ( fwrite ( MODE1(self)->mem, MODE1(self)->size_mask+1, 1, f ) != 1 )
    return -1;
  
  return 0;
  
} /* end eeprom_mode1_save_state */


static int
eeprom_mode1_load_state (
        		 eeprom_t *self,
        		 FILE     *f
        		 )
{

  if ( eeprom_load_state ( self, f ) != 0 )
    return -1;
  LOAD ( MODE1(self)->mode_state );
  LOAD ( MODE1(self)->addr );
  CHECK ( MODE1(self)->addr >= 0 &&
          MODE1(self)->addr <= MODE1(self)->size_mask );
  LOAD ( MODE1(self)->base_addr );
  CHECK ( MODE1(self)->base_addr >= 0 &&
          MODE1(self)->base_addr <= MODE1(self)->size_mask );
  if ( fread ( MODE1(self)->mem, MODE1(self)->size_mask+1, 1, f ) != 1 )
    return -1;
  
  return 0;
  
} /* end eeprom_mode1_load_state */


static eeprom_t *
create_eeprom_mode1 (
        	     const MDu8  init_val,
        	     const MDu32 sda_in_addr,
        	     const int   sda_in_desp,
        	     const MDu32 sda_out_addr,
        	     const int   sda_out_desp,
        	     const MDu32 scl_addr,
        	     const int   scl_desp,
        	     const MDu8  size_mask,
        	     const MDu8  page_mask
        	     )
{

  eeprom_mode1_t *new;


  /* Inicialitza. */
  new= mem_alloc ( eeprom_mode1_t, 1 );
  new->sda_in_addr= sda_in_addr;
  new->sda_in_desp= sda_in_desp;
  new->sda_out_addr= sda_out_addr;
  new->sda_out_desp= sda_out_desp;
  new->scl_addr= scl_addr;
  new->scl_desp= scl_desp;
  new->size_mask= size_mask;
  new->page_mask= page_mask;
  
  /* Mètodes. */
  new->free= eeprom_mode1_free;
  new->ack_read= eeprom_mode1_ack_read;
  new->write= eeprom_mode1_write;
  new->start= eeprom_mode1_start;
  new->init_state= eeprom_mode1_init_state;
  new->save_state= eeprom_mode1_save_state;
  new->load_state= eeprom_mode1_load_state;
  
  /* Memòria i estat. */
  new->mem= _get_eeprom ( size_mask+1, init_val, _udata );
  eeprom_mode1_init_state ( (eeprom_t *) new );

  return (eeprom_t *) new;
  
} /* end create_eeprom_mode1 */




/**********/
/* MODE 2 */
/**********/

#define MODE2(PTR) ((eeprom_mode2_t *) (PTR))

static void
eeprom_mode2_free (
                   eeprom_t *eeprom
                   )
{
  free ( eeprom );
} /* end eeprom_mode2_free */


static void
eeprom_mode2_ack_read (
                       eeprom_t *self
                       )
{
  
  /* Si no és correcte o absurd reseteja. */
  if ( MODE2(self)->mode_state != M2_READING )
    {
      MODE2(self)->mode_state= M2_WAIT_START;
      self->state= WAIT_START;
      return;
    }

  /* NOTA!!! No tinc clar si al incrementar l'adreça torna al principi
     o sols al principi del dispositiu. De moment implemente al
     principi. */
  /* Prepara per a la següent lectura. */
  self->state= READ_WAIT_SET_NEXT_BIT;
  self->word= MODE2(self)->mem[MODE2(self)->addr];
  self->sda_out= (self->word&0x80)!=0;
  MODE2(self)->addr= (MODE2(self)->addr+1)&MODE2(self)->size_mask;
  self->nbits= 0;
  
} /* end eeprom_mode2_ack_read */


static void
eeprom_mode2_write (
                    eeprom_t *self
                    )
{
  
  switch ( MODE2(self)->mode_state )
    {
      
    case M2_WAIT_ADDR:
      /* NOTA!!! No tinc clar lo de l'adreça es fa en cas de
         lectura. */
      /* Fixa la part alta de l'adreça. */
      MODE2(self)->addr=
        ((MODE2(self)->addr&0x00FF) |
         (((MDu16) (self->word>>1))<<8))&MODE2(self)->size_mask;
      MODE2(self)->base_addr= MODE2(self)->addr&(~(MODE2(self)->page_mask));
      /* Prepara operació. */
      if ( self->word&0x1 ) /* Read */
        {
          self->state= READ_WAIT_SET_NEXT_BIT;
          self->word= MODE2(self)->mem[MODE2(self)->addr];
          /* NOTA: No tinc clar que l'increment en lectura siga així !!! */
          MODE2(self)->addr= (MODE2(self)->addr+1)&MODE2(self)->size_mask;
          self->nbits= 0;
          MODE2(self)->mode_state= M2_READING;
        }
      else /* Write */
        {
          self->state= WRITE_WAIT_SET_NEXT_BIT;
          self->word= 0;
          self->nbits= 0;
          MODE2(self)->mode_state= M2_WAIT_ADDR2;
        }
      break;

    case M2_WAIT_ADDR2:
      MODE2(self)->addr=
        ((MODE2(self)->addr&0xFF00) |
         ((MDu16) self->word))&MODE2(self)->size_mask;
      MODE2(self)->base_addr= MODE2(self)->addr&(~(MODE2(self)->page_mask));
      self->state= WRITE_WAIT_SET_NEXT_BIT;
      self->word= 0;
      self->nbits= 0;
      MODE2(self)->mode_state= M2_WRITING;
      break;
      
    case M2_WRITING:
      MODE2(self)->mem[MODE2(self)->addr]= self->word;
      MODE2(self)->addr=
        MODE2(self)->base_addr |
        ((MODE2(self)->addr+1)&(MODE2(self)->page_mask));
      self->state= WRITE_WAIT_SET_NEXT_BIT;
      self->word= 0;
      self->nbits= 0;
      break;
      
    default: /* En cas absurd resteja. */
      MODE2(self)->mode_state= M2_WAIT_START;
      self->state= WAIT_START;
      break;
      
    }
  
} /* end eeprom_mode2_write */


static void
eeprom_mode2_start (
                    eeprom_t *self
                    )
{
  MODE2(self)->mode_state= M2_WAIT_ADDR;
} /* end eeprom_mode2_start */


static void
eeprom_mode2_init_state (
                         eeprom_t *self
                         )
{

  eeprom_init_state ( self );
  MODE2(self)->addr= 0;
  MODE2(self)->base_addr= 0;
  MODE2(self)->mode_state= M2_WAIT_START;
  
} /* end eeprom_mode2_init_state */


static int
eeprom_mode2_save_state (
                         const eeprom_t *self,
                         FILE           *f
                         )
{

  if ( eeprom_save_state ( self, f ) != 0 )
    return -1;
  SAVE ( MODE2(self)->mode_state );
  SAVE ( MODE2(self)->addr );
  SAVE ( MODE2(self)->base_addr );
  if ( fwrite ( MODE2(self)->mem, MODE2(self)->size_mask+1, 1, f ) != 1 )
    return -1;
  
  return 0;
  
} /* end eeprom_mode2_save_state */


static int
eeprom_mode2_load_state (
                         eeprom_t *self,
                         FILE     *f
                         )
{

  if ( eeprom_load_state ( self, f ) != 0 )
    return -1;
  LOAD ( MODE2(self)->mode_state );
  LOAD ( MODE2(self)->addr );
  CHECK ( MODE2(self)->addr >= 0 &&
          MODE2(self)->addr <= MODE2(self)->size_mask );
  LOAD ( MODE2(self)->base_addr );
  CHECK ( MODE2(self)->base_addr >= 0 &&
          MODE2(self)->base_addr <= MODE2(self)->size_mask );
  if ( fread ( MODE2(self)->mem, MODE2(self)->size_mask+1, 1, f ) != 1 )
    return -1;
  
  return 0;
  
} /* end eeprom_mode2_load_state */


static eeprom_t *
create_eeprom_mode2 (
                     const MDu8  init_val,
                     const MDu32 sda_in_addr,
                     const int   sda_in_desp,
                     const MDu32 sda_out_addr,
                     const int   sda_out_desp,
                     const MDu32 scl_addr,
                     const int   scl_desp,
                     const MDu16 size_mask,
                     const MDu8  page_mask
                     )
{

  eeprom_mode2_t *new;


  /* Inicialitza. */
  new= mem_alloc ( eeprom_mode2_t, 1 );
  new->sda_in_addr= sda_in_addr;
  new->sda_in_desp= sda_in_desp;
  new->sda_out_addr= sda_out_addr;
  new->sda_out_desp= sda_out_desp;
  new->scl_addr= scl_addr;
  new->scl_desp= scl_desp;
  new->size_mask= size_mask;
  new->page_mask= page_mask;
  
  /* Mètodes. */
  new->free= eeprom_mode2_free;
  new->ack_read= eeprom_mode2_ack_read;
  new->write= eeprom_mode2_write;
  new->start= eeprom_mode2_start;
  new->init_state= eeprom_mode2_init_state;
  new->save_state= eeprom_mode2_save_state;
  new->load_state= eeprom_mode2_load_state;
  
  /* Memòria i estat. */
  new->mem= _get_eeprom ( size_mask+1, init_val, _udata );
  eeprom_mode2_init_state ( (eeprom_t *) new );

  return (eeprom_t *) new;
  
} /* end create_eeprom_mode2 */




/**********/
/* MODE 3 */
/**********/

#define MODE3(PTR) ((eeprom_mode3_t *) (PTR))

static void
eeprom_mode3_free (
                   eeprom_t *eeprom
                   )
{
  free ( eeprom );
} /* end eeprom_mode3_free */


static void
eeprom_mode3_ack_read (
                       eeprom_t *self
                       )
{
  
  /* Si no és correcte o absurd reseteja. */
  if ( MODE3(self)->mode_state != M3_READING )
    {
      MODE3(self)->mode_state= M3_WAIT_START;
      self->state= WAIT_START;
      return;
    }

  /* NOTA!!! No tinc clar si al incrementar l'adreça torna al principi
     o sols al principi del dispositiu. De moment implemente al
     principi. */
  /* Prepara per a la següent lectura. */
  self->state= READ_WAIT_SET_NEXT_BIT;
  self->word= MODE3(self)->mem[MODE3(self)->addr];
  self->sda_out= (self->word&0x80)!=0;
  MODE3(self)->addr= (MODE3(self)->addr+1)&MODE3(self)->size_mask;
  self->nbits= 0;
  
} /* end eeprom_mode3_ack_read */


static void
eeprom_mode3_write (
                    eeprom_t *self
                    )
{
  
  switch ( MODE3(self)->mode_state )
    {
      
    case M3_WAIT_ADDR:
      /* NOTA!!! No tinc clar lo de l'adreça es fa en cas de
         lectura. */
      /* Fixa la part alta de l'adreça. */
      MODE3(self)->addr=
        ((MODE3(self)->addr&0x00FFFF) |
         (((MDu32) (self->word>>1))<<16))&MODE3(self)->size_mask;
      MODE3(self)->base_addr= MODE3(self)->addr&(~(MODE3(self)->page_mask));
      /* Prepara operació. */
      if ( self->word&0x1 ) /* Read */
        {
          self->state= READ_WAIT_SET_NEXT_BIT;
          self->word= MODE3(self)->mem[MODE3(self)->addr];
          /* NOTA: No tinc clar que l'increment en lectura siga així !!! */
          MODE3(self)->addr= (MODE3(self)->addr+1)&MODE3(self)->size_mask;
          self->nbits= 0;
          MODE3(self)->mode_state= M3_READING;
        }
      else /* Write */
        {
          self->state= WRITE_WAIT_SET_NEXT_BIT;
          self->word= 0;
          self->nbits= 0;
          MODE3(self)->mode_state= M3_WAIT_ADDR2;
        }
      break;

    case M3_WAIT_ADDR2:
      MODE3(self)->addr=
        ((MODE3(self)->addr&0xFF00FF) |
         (((MDu32) self->word)<<8))&MODE3(self)->size_mask;
      MODE3(self)->base_addr= MODE3(self)->addr&(~(MODE3(self)->page_mask));
      self->state= WRITE_WAIT_SET_NEXT_BIT;
      self->word= 0;
      self->nbits= 0;
      MODE3(self)->mode_state= M3_WAIT_ADDR3;
      break;
      
    case M3_WAIT_ADDR3:
      MODE3(self)->addr=
        ((MODE3(self)->addr&0xFFFF00) |
         ((MDu32) self->word))&MODE3(self)->size_mask;
      MODE3(self)->base_addr= MODE3(self)->addr&(~(MODE3(self)->page_mask));
      self->state= WRITE_WAIT_SET_NEXT_BIT;
      self->word= 0;
      self->nbits= 0;
      MODE3(self)->mode_state= M3_WRITING;
      break;
      
    case M3_WRITING:
      MODE3(self)->mem[MODE3(self)->addr]= self->word;
      MODE3(self)->addr=
        MODE3(self)->base_addr |
        ((MODE3(self)->addr+1)&(MODE3(self)->page_mask));
      self->state= WRITE_WAIT_SET_NEXT_BIT;
      self->word= 0;
      self->nbits= 0;
      break;
      
    default: /* En cas absurd resteja. */
      MODE3(self)->mode_state= M3_WAIT_START;
      self->state= WAIT_START;
      break;
      
    }
  
} /* end eeprom_mode3_write */


static void
eeprom_mode3_start (
                    eeprom_t *self
                    )
{
  MODE3(self)->mode_state= M3_WAIT_ADDR;
} /* end eeprom_mode3_start */


static void
eeprom_mode3_init_state (
                         eeprom_t *self
                         )
{

  eeprom_init_state ( self );
  MODE3(self)->addr= 0;
  MODE3(self)->base_addr= 0;
  MODE3(self)->mode_state= M3_WAIT_START;
  
} /* end eeprom_mode3_init_state */


static int
eeprom_mode3_save_state (
                         const eeprom_t *self,
                         FILE           *f
                         )
{

  if ( eeprom_save_state ( self, f ) != 0 )
    return -1;
  SAVE ( MODE3(self)->mode_state );
  SAVE ( MODE3(self)->addr );
  SAVE ( MODE3(self)->base_addr );
  if ( fwrite ( MODE3(self)->mem, MODE3(self)->size_mask+1, 1, f ) != 1 )
    return -1;
  
  return 0;
  
} /* end eeprom_mode3_save_state */


static int
eeprom_mode3_load_state (
                         eeprom_t *self,
                         FILE     *f
                         )
{

  if ( eeprom_load_state ( self, f ) != 0 )
    return -1;
  LOAD ( MODE3(self)->mode_state );
  LOAD ( MODE3(self)->addr );
  CHECK ( MODE3(self)->addr >= 0 &&
          MODE3(self)->addr <= MODE3(self)->size_mask );
  LOAD ( MODE3(self)->base_addr );
  CHECK ( MODE3(self)->base_addr >= 0 &&
          MODE3(self)->base_addr <= MODE3(self)->size_mask );
  if ( fread ( MODE3(self)->mem, MODE3(self)->size_mask+1, 1, f ) != 1 )
    return -1;
  
  return 0;
  
} /* end eeprom_mode3_load_state */


static eeprom_t *
create_eeprom_mode3 (
                     const MDu8  init_val,
                     const MDu32 sda_in_addr,
                     const int   sda_in_desp,
                     const MDu32 sda_out_addr,
                     const int   sda_out_desp,
                     const MDu32 scl_addr,
                     const int   scl_desp,
                     const MDu32 size_mask,
                     const MDu8  page_mask
                     )
{

  eeprom_mode3_t *new;


  /* Inicialitza. */
  new= mem_alloc ( eeprom_mode3_t, 1 );
  new->sda_in_addr= sda_in_addr;
  new->sda_in_desp= sda_in_desp;
  new->sda_out_addr= sda_out_addr;
  new->sda_out_desp= sda_out_desp;
  new->scl_addr= scl_addr;
  new->scl_desp= scl_desp;
  new->size_mask= size_mask;
  new->page_mask= page_mask;
  
  /* Mètodes. */
  new->free= eeprom_mode3_free;
  new->ack_read= eeprom_mode3_ack_read;
  new->write= eeprom_mode3_write;
  new->start= eeprom_mode3_start;
  new->init_state= eeprom_mode3_init_state;
  new->save_state= eeprom_mode3_save_state;
  new->load_state= eeprom_mode3_load_state;
  
  /* Memòria i estat. */
  new->mem= _get_eeprom ( size_mask+1, init_val, _udata );
  eeprom_mode3_init_state ( (eeprom_t *) new );

  return (eeprom_t *) new;
   
} /* end create_eeprom_mode3 */




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
MD_eeprom_close (void)
{
  if ( _eeprom != NULL ) _eeprom->free ( _eeprom );
} /* end MD_eeprom_close */


void
MD_eeprom_init (
        	const MD_Rom *rom,
        	MD_GetEEPROM *get_eeprom,
        	void         *udata
        	)
{

  /* Callbacks. */
  _get_eeprom= get_eeprom;
  _udata= udata;

  /* Crea la eeprom. */
  set_model ( rom );
  switch ( _model )
    {
    case EEPROM_SEGA:
    case EEPROM_CAPCOM:
      _eeprom= create_eeprom_mode1 ( 0x00, 0x200001, 0, 0x200001, 0,
        			     0x200001, 1, 0x7F, 0x03 );
      break;
    case EEPROM_EA:
      _eeprom= create_eeprom_mode1 ( 0x00, 0x200001, 7, 0x200001, 7,
        			     0x200001, 6, 0x7F, 0x03 );
      break;
    case EEPROM_CODEMASTERS_M1:
      _eeprom= create_eeprom_mode1 ( 0x00, 0x300000, 0, 0x380001, 7,
        			     0x300000, 1, 0x7F, 0x03 );
      break;
    case EEPROM_ACCLAIM_T1:
      _eeprom= create_eeprom_mode2 ( 0x00, 0x200001, 0, 0x200001, 1,
        			     0x200001, 1, 0xFF, 0x03 );
      break;
    case EEPROM_ACCLAIM_T2_A:
      _eeprom= create_eeprom_mode2 ( 0x00, 0x200001, 0, 0x200001, 0,
        			     0x200000, 0, 0xFF, 0x03 );
      break;
    case EEPROM_ACCLAIM_T2_B:
      _eeprom= create_eeprom_mode2 ( 0x00, 0x200001, 0, 0x200001, 0,
        			     0x200000, 0, 0x7FF, 0x07 );
      break;
    case EEPROM_CODEMASTERS_M2:
      _eeprom= create_eeprom_mode2 ( 0xFF, 0x300000, 0, 0x380001, 7,
        			     0x300000, 1, 0x3FF, 0x0F );
      break;
    case EEPROM_CODEMASTERS_M22:
      _eeprom= create_eeprom_mode2 ( 0x00, 0x300000, 0, 0x380001, 7,
        			     0x300000, 1, 0x7FF, 0x0F );
      break;
    case EEPROM_ACCLAIM_T2_C:
      _eeprom= create_eeprom_mode3 ( 0xFF, 0x200001, 0, 0x200001, 0,
        			     0x200000, 0, 0x1FFF, 0x07 );
      break; 
    default: _eeprom= NULL;
    }
  
} /* end MD_eeprom_init */


void
MD_eeprom_init_state (void)
{
  if ( _eeprom != NULL ) _eeprom->init_state ( _eeprom );
} /* end MD_eeprom_init_state */


MDu8
MD_eeprom_read (
        	const MDu32 addr
        	)
{

  if ( _eeprom == NULL ) return 0x00;
  
  if ( _eeprom->sda_out_addr != addr ) return 0x00;
  
  return (_eeprom->sda_out ? 0x1 : 0x0)<<_eeprom->sda_out_desp;
  
} /* end MD_eeprom_read */


void
MD_eeprom_write (
        	 const MDu32 addr,
        	 const MDu8  data
        	 )
{

  MD_Bool val;

  
  if ( _eeprom == NULL ) return;
  
  

  /* SCL */
  if ( _eeprom->scl_addr == addr )
    {
      val= ((data>>_eeprom->scl_desp)&0x1)!=0;
      switch ( _eeprom->state )
        {
        case WAIT_START: _eeprom->scl= val; break;
        case WAIT_INIT_WRITE_ADDR:
          if ( _eeprom->scl && !val )
            {
              _eeprom->nbits= 0;
              _eeprom->word= 0x00;
              _eeprom->sda_out= MD_TRUE; /* For WRITE ACK. */
              _eeprom->state= WRITE_WAIT_SET_NEXT_BIT;
            }
          _eeprom->scl= val;
          break;
        case WRITE_WAIT_SET_NEXT_BIT:
          if ( !(_eeprom->scl) && val )
            {
              _eeprom->word<<= 1;
              if ( _eeprom->sda_in ) _eeprom->word|= 0x1;
              _eeprom->state= WRITE_WAIT_TRANSFER;
            }
          _eeprom->scl= val;
          break;
        case WRITE_WAIT_TRANSFER:
          if ( _eeprom->scl && !val )
            {
              if ( ++(_eeprom->nbits) == 8 )
        	{
        	  _eeprom->state= WRITE_WAIT_SET_ACK;
        	  _eeprom->sda_out= MD_FALSE; /* ACK. */
        	}
              else
        	_eeprom->state= WRITE_WAIT_SET_NEXT_BIT;
            }
          _eeprom->scl= val;
          break;
        case WRITE_WAIT_SET_ACK:
          if ( !(_eeprom->scl) && val )
            _eeprom->state= WRITE_WAIT_ACK;
          _eeprom->scl= val;
          break;
        case WRITE_WAIT_ACK:
          if ( _eeprom->scl && !val )
            {
              _eeprom->sda_out= MD_TRUE;
              _eeprom->write ( _eeprom ); /* La implementació
        				     s'encarrega de canviar
        				     l'estat. */
            }
          _eeprom->scl= val;
          break;
        case READ_WAIT_SET_NEXT_BIT:
          if ( !(_eeprom->scl) && val )
            {
              _eeprom->word<<= 1;
              _eeprom->state= READ_WAIT_TRANSFER;
            }
          _eeprom->scl= val;
          break;
        case READ_WAIT_TRANSFER:
          if ( _eeprom->scl && !val )
            {
              if ( ++(_eeprom->nbits) == 8 )
        	_eeprom->state= READ_WAIT_SET_ACK;
              else /* A vegades es llig just abans del SCL1, cal avançar. */
        	{
        	  _eeprom->sda_out= (_eeprom->word&0x80)!=0;
        	  _eeprom->state= READ_WAIT_SET_NEXT_BIT;
        	}
            }
          _eeprom->scl= val;
          break;
        case READ_WAIT_SET_ACK:
          if ( !(_eeprom->scl) && val )
            _eeprom->state= READ_WAIT_ACK;
          _eeprom->scl= val;
          break;
        case READ_WAIT_ACK:
          if ( _eeprom->scl && !val )
            {
              if ( !(_eeprom->sda_in) )
        	_eeprom->ack_read ( _eeprom ); /* La implementació
        					  s'encarrega de
        					  canviar l'estat. */
            }
          _eeprom->scl= val;
          break;
        }
    }

  /* SDA_IN */
  if ( _eeprom->sda_in_addr == addr )
    {
      val= ((data>>_eeprom->sda_in_desp)&0x1)!=0;
      switch ( _eeprom->state )
        {
        case WAIT_START:
          if ( _eeprom->scl && _eeprom->sda_in && !val )
            {
              _eeprom->start ( _eeprom );
              _eeprom->state= WAIT_INIT_WRITE_ADDR;
            }
          _eeprom->sda_in= val;
          break;
        case WAIT_INIT_WRITE_ADDR: _eeprom->sda_in= val; break;
        case WRITE_WAIT_SET_NEXT_BIT: _eeprom->sda_in= val; break;
        case WRITE_WAIT_TRANSFER:
          /* En principi sols canvia per a fer un stop o un start. */
          if ( _eeprom->nbits==0 && _eeprom->scl )
            {
              if ( !(_eeprom->sda_in) && val ) _eeprom->state= WAIT_START;
              else if ( _eeprom->sda_in && !val )
        	{
        	  _eeprom->start ( _eeprom );
        	  _eeprom->state= WAIT_INIT_WRITE_ADDR;
        	}
              _eeprom->sda_in= val;
            }
          break;
        case WRITE_WAIT_SET_ACK: _eeprom->sda_in= val; break;
        case WRITE_WAIT_ACK: break; /* no permet que es canvie res. */
        case READ_WAIT_SET_NEXT_BIT: _eeprom->sda_in= val; break;
        case READ_WAIT_TRANSFER: /* Pot produïr-se un stop. */
          if ( _eeprom->nbits==0 && _eeprom->scl && !(_eeprom->sda_in) && val )
            _eeprom->state= WAIT_START;
          _eeprom->sda_in= val;
          break;
        case READ_WAIT_SET_ACK: _eeprom->sda_in= val; break;
        case READ_WAIT_ACK: /* Es pot produir directament un stop. */
          if ( _eeprom->scl && !(_eeprom->sda_in) && val )
            _eeprom->state= WAIT_START;
          _eeprom->sda_in= val;
          break;
        }
    }
  
} /* end MD_eeprom_write */


int
MD_eeprom_save_state (
        	      FILE *f
        	      )
{

  SAVE ( _model );
  if ( _eeprom != NULL && _eeprom->save_state ( _eeprom, f ) != 0 )
    return -1;
  
  return 0;
  
} /* end MD_eeprom_save_state */


int
MD_eeprom_load_state (
        	      FILE *f
        	      )
{

  int tmp;


  LOAD ( tmp );
  CHECK ( tmp == (int) _model );
  if ( _eeprom != NULL && _eeprom->load_state ( _eeprom, f ) != 0 )
    return -1;
  
  return 0;
  
} /* end MD_eeprom_load_state */
