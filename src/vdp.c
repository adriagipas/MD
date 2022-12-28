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
 *  vdp.c - Implementació del mòdul VDP.
 *
 *  NOTA IMPORTANT !!
 *
 *  Després de llegir diversos estudis sobre el VInt, pareix clar que
 *  l'única manera de desactivar-lo és amb un ACK del processador. No
 *  obstant, en algns jocs (Sonic i Sonic 2) he tingut problemes al
 *  fer-ho així. En aquestos jocs al principi, amb el display
 *  desactivat, les interrupcions estana ctivades en la part del VDP
 *  però no en la part de la UCP. En un moment donat, la UCP activa
 *  interrupcions sese sincronitzar-se amb el VBlank, i la rutina de
 *  VINT no desactiva les interrupcions. Suposse que si quan passa açò
 *  ja ens trovem en el VBlank no hi ha cap problema, però en el meu
 *  cas no és així (sospite per una no perfecta emulació del timing),
 *  consequentment és produeïx un error fatal.
 *
 *  Per aquest motiu he decidit desactivar el VInt al final de cada
 *  frame. Crec que no pot afectar a ningun joc, en tot cas a ROMs que
 *  es dediquen a medir el comportament de la VDP.
 *
 *  NOTA!!! gràcies a la implementació de MAME he pogut descobrir un
 *  problema totalment desconegut per a mi. Aparentment en el SVP i
 *  alguns dispositius com el MegaCD s'introdueïx un "lag" en l'adreça
 *  de la DMA, i compensar aquest lag és fonamental per fer que
 *  funcione correctament aquests dispositius.
 *
 *     src/devices/video/315_5313.cpp
 *     src/devices/video/315_5313.h
 *
 *
 *  NOTES:
 *
 * - NOTA!!!!!!!!!!!!!!!!!!! EL TIMING ESTÀ MAL !!!!!!!!!
 *   VEURE MACROS DE MEM2VRAM
 *
 * - VITAL!!!! (50*NPOINTS)/CYCLES_PER_SEC == points/cicle
 * 
 * - El document més important per al timing del VDP és
 *   vdp_HV_nemesis.txt seguit de
 *   http://gendev.spritesmind.net/forum/viewtopic.php?p=11681
 *
 * - De moment no implemente la interrupció dels dispositius externs.
 *
 * - Vaig a moure el display al cantó superior esquerre de la
 *   pantalla.
 *
 * - Assumisc que en interlace mode, la MegaDrive està controlant la
 *   generació d'una única imatge final a base de juntar dos
 *   fields. En interlace mode 1, seria equivalent al non-interlace
 *   però controlat (el programa s'esperaria a canviar el frame cada
 *   2), en interlace mode 2 se treballa amb una imatge de doble
 *   resolució de la que es generen 2 camps. Per tant quan estiga en
 *   mode interlace generaré directament un frame del doble de
 *   resolució a 30Hz (25 Hz).
 *
 * - EL FIFO no el vaig a emular, però vaig a fer una fulla per a que
 *   parega que està fent algo. Mai estarà ple, però el flag d'empty
 *   cada vegada que s'escriga algo l'activaré, i cada vegada que es
 *   consulte o al final de cada frame el desactivaré.
 *
 * - Assumiré que el enable display pinta la pantalla amb el color de
 *   background i es com si estaguera en un continu VBlank.
 *
 */


#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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

#define MAX(a,b) (((a)>(b)) ? (a) : (b))
#define MIN(a,b) (((a)<(b)) ? (a) : (b))

#define _64K 65536

#define PAL_CC2PP 1 /* 50 -> 1 */
#define PAL_FRAC (MD_CYCLES_PER_SEC_PAL/50)
#define PAL_NLINES 313

#define NTSC_CC2PP 3 /* 60 -> 3*/
#define NTSC_FRAC (MD_CYCLES_PER_SEC_NTSC/20)
#define NTSC_NLINES 262

#define POINTSPERLINE_H32 342
#define POINTSPERLINE_H40 422

/* Vore 'vdp_HV_nemesis.txt'. No se si el HInt es fa a la vegada que
   el final del display o que el principi del HBlank. Aniré
   provant. */
/* Vore http://gendev.spritesmind.net/forum/viewtopic.php?p=11681 per
   als canvis. It is decremented on each at HCounter = $A6 (if VDP
   width is set to 320 pixels) or $86 (if VDP width is set to 256
   pixels).*/
#define LINEPP_BEFORE_END_DISPLAY_H32 256
#define LINEPP_BEFORE_BEGIN_HBLANK_H32 268
/*
#define LINEPP_BEFORE_BEGIN_HBLANK_H32 294
*/
#define LINEPP_BEFORE_END_HBLANK_H32 10
#define LINEPP_BEFORE_END_DISPLAY_H40 320
#define LINEPP_BEFORE_BEGIN_HBLANK_H40 330
/*
#define LINEPP_BEFORE_BEGIN_HBLANK_H40 358
*/
#define LINEPP_BEFORE_END_HBLANK_H40 12

/* Cicles de CPU approximats que empra el DMA MEM->VRAM en cada mode.
 NOTA!!! Estic assumint que el redondeig que estic fent no és molt
 important ja que la CPU mentres tant està parada, ningun xip
 necessita en aquest periode estar sincronitzat amb el VDP, i si bé es
 cert que l'execució de la resta de xips es podria
 accelerar/desaccelerar un poc, com es en un periode molt curt no
 crec que tinga molta importància.
 NOTA!!! Al redondejar PAL i NTSC donen
 el mateix. */
/* PROVISIONAL !!!!!!!!
 *
 * Tinc problemes de timing, en particular en Streets of Rage 2 !!!!
 * (veure streetsofrage2_dump4.txt). El problema és que està massa
 * cicles fent el DMA, he descobert que si fique scale=1 sempre aplega,
 * però clarament això és trampa, el timing està mal en algun punt.
 *
 * De totes maneres! La meua impressió és que emular el timing del DMA
 * no afecta a ningun joc, i m'estalvie problemes de timing. Així que de
 * moment fique 1 cicle en mem2vram i menys problemes, el scale l'he deixat
 * com crec que deuria.
 *
 */
#define DMA_MEM2VRAM_CC_H32_DISPLAY /*30*/1
#define DMA_MEM2VRAM_CC_H32_VBLANK /*3*/1
#define DMA_MEM2VRAM_CC_H40_DISPLAY /*27*/1
#define DMA_MEM2VRAM_CC_H40_VBLANK /*2*/1

/* Bytes per line per a cada mode de DMA. */
#define DMA_FILL_BYTES_PER_LINE_H32_DISPLAY 15
#define DMA_FILL_BYTES_PER_LINE_H32_VBLANK 166
#define DMA_FILL_BYTES_PER_LINE_H40_DISPLAY 17
#define DMA_FILL_BYTES_PER_LINE_H40_VBLANK 204
#define DMA_COPY_BYTES_PER_LINE_H32_DISPLAY 8
#define DMA_COPY_BYTES_PER_LINE_H32_VBLANK 83
#define DMA_COPY_BYTES_PER_LINE_H40_DISPLAY 9
#define DMA_COPY_BYTES_PER_LINE_H40_VBLANK 102

#define MAXWIDTH 640
#define MAXHEIGHT 480

/* Macros per a renderitzar. */
#define GET_NEXT_NT        			\
  addr= addr_row|addr_col;        		\
  NT= (((MDu16) _vram[addr])<<8)|_vram[addr|1]; \
  addr_col= (addr_col+2)&col_mask

#define CALC_ADDR_PAT        						\
  addr_pat= (NT&0x07FF)<<pat_size;        				\
  if ( NT&0x1000 /*vf*/ ) addr_pat|= (maxrowcell-(row&maxrowcell))<<2;        \
  else                    addr_pat|= (row&maxrowcell)<<2

#define INIT_BITS_AND_VALS        			\
  byte0= _vram[addr_pat++]; byte1= _vram[addr_pat++];        \
  byte2= _vram[addr_pat++]; byte3= _vram[addr_pat];        \
  if ( NT&0x0800 /*hf*/)        			\
    bits=        					\
      (((MDu64) (((byte0&0xF)<<4)|(byte0>>4)))<<32) |        \
      (((MDu64) (((byte1&0xF)<<4)|(byte1>>4)))<<40) |        \
      (((MDu64) (((byte2&0xF)<<4)|(byte2>>4)))<<48) |        \
      (((MDu64) (((byte3&0xF)<<4)|(byte3>>4)))<<56);        \
  else        						\
    bits=        					\
      (((MDu64) byte3)<<32) |        			\
      (((MDu64) byte2)<<40) |        			\
      (((MDu64) byte1)<<48) |        			\
      (((MDu64) byte0)<<56);        			\
  pal_vals[0]= (MDu8) (((NT>>13)&0x3)<<4);        	\
  prior_vals[0]= (MDu8) (NT>>15)

#define LOAD_BITS_AND_VALS        			\
  byte0= _vram[addr_pat++]; byte1= _vram[addr_pat++];        \
  byte2= _vram[addr_pat++]; byte3= _vram[addr_pat];        \
  if ( NT&0x0800 /*hf*/)        			\
    bits|=        					\
      ((MDu64) (((byte0&0xF)<<4)|(byte0>>4))) |        	\
      (((MDu64) (((byte1&0xF)<<4)|(byte1>>4)))<<8) |        \
      (((MDu64) (((byte2&0xF)<<4)|(byte2>>4)))<<16) |        \
      (((MDu64) (((byte3&0xF)<<4)|(byte3>>4)))<<24);        \
  else        						\
    bits|=        					\
      ((MDu64) byte3) |        				\
      (((MDu64) byte2)<<8) |        			\
      (((MDu64) byte1)<<16) |        			\
      (((MDu64) byte0)<<24);        			\
  pal_vals[1]= (MDu8) (((NT>>13)&0x3)<<4);        	\
  prior_vals[1]= (MDu8) (NT>>15)

#define NSPRITES 80

#define SHA_COLOR -1
#define HIG_COLOR -2




/*********/
/* TIPUS */
/*********/

typedef struct
{
  
  int     off;
  int     off_2;
  MDu16   NT_addr;
  MDu8    line[MAXWIDTH];
  MD_Bool isp0[MAXWIDTH];
  int     prio0[MAXWIDTH]; /* prio0 i prio1 són piles de N0,N1. */
  int     prio1[MAXWIDTH];
  int     N0,N1;
  
} scroll_t;


typedef struct sprite sprite_t;

struct sprite
{
  
  MD_Bool vflip, hflip, prio;
  MDu8 width, height, pal;
  MDu16 y,x,pat;
  
};

typedef struct
{
  
  sprite_t v[NSPRITES];
  int      N;
  
} sprites_cache_t;


typedef struct
{
  
  int     row;
  int     ind;
  int     width;
  MD_Bool isp0;
  
} sprite_eval_t;


typedef struct
{
  
  sprite_eval_t v[80];
  int           N;
  
} sprite_buff_t;


typedef struct
{
  
  int color;
  int type; // -1 - None, 0 - LowPrio, 1 - HighPrio
  int coll_counter;
  
} sprite_pixel_t;




/*********/
/* ESTAT */
/*********/

/* Callbacks. */
static MD_SResChanged *_sres_changed;
static MD_UpdateScreen *_update_screen;
static MD_Warning *_warning;
static void *_udata;

/* Per a accedir a la memòria. */
static struct
{
  
  MD_Bool second_pass;
  MDu16   addr;
  MDu8    code;
  
} _access;

/* Memòria. */
static MDu8 _vram[_64K];
static MDu16 _cram[64]; /* 9 bit words!. B*3G*3R*3 */
static MDu16 _vsram[40]; /* 10 bit words!. */

/* Registres. */
static struct
{
  
  /* Reg 0. */
  MD_Bool HInt_enabled;     /* Per defecte no ho està. */
  MD_Bool HV_counter_stop;
  
  /* Reg 1. */
  MD_Bool enabled;          /* Per defecte no ho està. */
  MD_Bool VInt_enabled;     /* Per defecte no ho està. */
  MD_Bool DMA_enabled;      /* Per defecte no ho està. */
  MD_Bool V30_cell_mode_tmp;
  MD_Bool V30_cell_mode;    /* Per defecte mode V28. */
  
  /* Reg 2. */
  MDu16 scrollA_name_table_addr;
  
  /* Reg 3. */
  MDu16 window_name_table_addr;
  
  /* Reg 4. */
  MDu16 scrollB_name_table_addr;
  
  /* Reg 5. */
  MDu16 sprite_attribute_table_addr;
  
  /* Reg 7. */
  MDu8 bgcolor;     /* Entrada en la CRAM del color. */
  
  /* Reg 10. */
  MDu8 H_interrupt_register;
  
  /* Reg 11. */
  MD_Bool vsc_mode_is_cell;
  enum { FULL, CELL, LINE } hsc_mode;
  
  /* Reg 12. */
  MD_Bool H40_cell_mode_tmp;
  MD_Bool H40_cell_mode;    /* Per defecte mode 32. */
  char    interlace_mode_tmp;
  char    interlace_mode;    /* 0 (None), 1 o 3. */
  MD_Bool S_TE;
  
  /* Reg 13. */
  MDu16 H_scroll_table_addr;
  
  /* Reg 15. */
  MDu8 auto_increment_data;
  
  /* Reg 16. */
  MDu8 VSZ;
  MDu8 HSZ;
  
  /* Reg 17. */
  MD_Bool isRIGT;
  MDu8    WHP;
  
  /* Reg 18. */
  MD_Bool isDOWN;
  MDu8    WVP;
  
  /* Reg 19 i 20. */
  MDu16 dma_length_counter;
  MDu16 dma_length_counter_tmp;
  
  /* Reg 21, 22 i 23. */
  MDu32 dma_source_address;
  MDu32 dma_source_address_tmp;
  enum {
    DMA_MEM2VRAM,
    DMA_FILL,
    DMA_COPY
  } dma_mode;
  
} _regs;

/* Resolució actual. */
static struct
{
  
  int width;
  int ntiles;
  int height;
  int resw;    /* Resolució real. */
  int resh;    /* Resolució real. */
  
} _csize;

/* Per al timing. */
static struct
{
  
  /* NOTA!! Tots els cc* estan medits en fraccions. */
  int64_t cc2frac; /* Multiplica cicles per a obtindre les
        	      fraccions. Cada vegada que es completa una
        	      fracció tenim un punt. */
  int64_t frac;
  int64_t pointsperline;
  int64_t ciclesperline;
  int64_t cc;                      /* Cicles CPU (en frac) acumulats. */
  int  H,V;                     /* Posicions actuals. */
  int  lines;                   /* Número de línies (PAL/NTSC). */
  int  lines_before_end_display;
  int  linecc_before_hint;
  int  linepp_before_hint;
  int  linepp_before_begin_hblank;
  int  linepp_before_end_hblank;
  int  linepp_before_end_display;
  int64_t cctoVInt;                /* Cicles*7 fins a la següent
        			      interrupció vertical. Tambés es
        			      gasta per a generar frames. */
  int64_t cctoHInt;                /* Cicles*7 fins a la següent
        			      interrupció horitzontal. */
  int64_t cctoendframe;            /* Cicles que falten per al final del
        			      següent frame. */
  int  dma_mem2vram_cc_display;
  int  dma_mem2vram_cc_vblank;
  int64_t cctonextline;            /* Cicles*7 fins a la següent línia. */
  
} _timing;

/* DMA */
static struct
{
  
  int     fill_bytes_per_line_display;
  int     fill_bytes_per_line_vblank;
  int     copy_bytes_per_line_display;
  int     copy_bytes_per_line_vblank;
  MD_Word fill_data;
  MD_Bool fill_started;
  MD_Bool tovram;
  
} _dma;

/* Flags auxiliars per al registre STATUS. */
static struct
{
  
  MD_Bool VInt,HInt;
  MD_Bool ispal;
  MD_Bool odd_frame;
  MD_Bool dma_busy;
  MD_Bool fifo_empty; /* Açò és una fulla!!!. */
  MD_Bool too_many_sprites; /* No se on resetejar-lo. Ho fare cada
        		       vegada que es llisca. */
  MD_Bool spr_collision;
  MD_Word HVC;
  MD_Bool VBlank;
  
} _status_aux;

/* Hint counter. */
static int _hint_counter;

/* Estat renderitzat. */
static struct
{
  
  int  fb[MAXWIDTH*MAXHEIGHT];    /* Frame buffer. */
  int *p;                         /* Apunta al següent píxel. */
  MDu8 bgcolor;                   /* Color de background. */
  int  width;                     /* Amplaria de la línia en píxels
        			     reals. */
  int      tmp[MAXWIDTH];
  sprite_pixel_t spr_line[MAXWIDTH];
  enum {
    NOR= 0x000,
    SHA= 0x200,
    HIG= 0x400
  }        s_te[MAXWIDTH];
  MDu16    lines;                      /* linies que porte. */
  scroll_t sc[2];   /* 0->A, 1->B */
  MDu16    Htable;
  MDu8     HSZ,VSZ;
  int      hsc_mode;
  MD_Bool  vsc_mode_is_cell;
  MDu16    win_NT_addr;
  MD_Bool  isRIGT,isDOWN;
  MDu8     WHP,WVP;
  MD_Bool  dot_overflow; /* Sprites dot overflow en l'anterior línia. */
  MD_Bool  S_TE;
  
} _render;


static sprites_cache_t _sprites;
static sprite_buff_t _sprites_buff;

/* Interrupció Z80. Indica que encara no s'ha de desactivar. */
static MD_Bool _z80_int_enabled;

// DMA lag. No cal desar-ho en l'estat!!!!! Quan es carrega la ROM es
// configura, és una constant que depen de la ROM.
static int _dma_lag= 0;




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static void
res_changed (
             const int width,
             const int height
             )
{

  _csize.resw= width;
  _csize.resh= height;
  _sres_changed ( width, height, _udata );
  
} /* end res_changed */


static void
update_HVC (void)
{
  
  int V;
  
  
  /* H */
  if ( _regs.H40_cell_mode )
    {
      if ( _timing.H < 0xB7*2 ) _status_aux.HVC.b.v0= _timing.H>>1;
      else _status_aux.HVC.b.v0= (_timing.H>>1) + (0xE4-0xB7);
    }
  else /* H32 cell mode */
    {
      if ( _timing.H < 0x94*2 ) _status_aux.HVC.b.v0= _timing.H>>1;
      else _status_aux.HVC.b.v0= (_timing.H>>1) + (0xE9-0x94);
    }
  
  /* V */
  V= _timing.V;
  if ( _timing.H < _timing.linepp_before_hint )
    if ( --V == -1 ) V= _timing.lines-1;
  if ( _status_aux.ispal )
    {
      if ( _regs.V30_cell_mode )
        {
          switch ( _regs.interlace_mode )
            {
            case 0: /* No interlace mode. */
              if ( V < 0x100 ) _status_aux.HVC.b.v1= V;
              else if ( V < 0x10B )
        	_status_aux.HVC.b.v1= V-0x100;
              else _status_aux.HVC.b.v1= (V-0x100) + (0xD2-0x0B);
              break;
            case 1: /* Interlace mode. */
              if ( V < 0x100 ) _status_aux.HVC.b.v1= V;
              else if ( V < 0x10B )
        	_status_aux.HVC.b.v1= (V-0x100)|0x01;
              else
        	{
        	  if ( _status_aux.odd_frame )
        	    {
        	      if ( V == 0x10B ) _status_aux.HVC.b.v1= 0xD1;
        	      else _status_aux.HVC.b.v1=
        		     ((V-0x100) + (0xD2-0x0C))|0x01;
        	    }
        	  else _status_aux.HVC.b.v1=
        		 ((V-0x100) + (0xD2-0x0B))|0x01;
        	}
              break;
            case 3: /* Double interlace mode. */
              if ( V < 0x80 ) _status_aux.HVC.b.v1= V<<1;
              else if ( V < 0x100 )
        	_status_aux.HVC.b.v1= ((V-0x80)<<1)|0x01;
              else if ( V < 0x10A )
        	_status_aux.HVC.b.v1= (V-0x100)<<1;
              else
        	{
        	  if ( _status_aux.odd_frame )
        	    {
        	      if ( V == 0x10A ) _status_aux.HVC.b.v1= 0xA3;
        	      else _status_aux.HVC.b.v1=
        		     (((V-0x100) + (0x52-0x0B))<<1)|0x01;
        	    }
        	  else _status_aux.HVC.b.v1=
        		 (((V-0x100) + (0x52-0x0A))<<1)|0x01;
        	}
              break;
            default: _status_aux.HVC.b.v1= 0; /* ¿? */
            }
        }
      else /* V28 cell mode */
        {
          switch ( _regs.interlace_mode )
            {
            case 0: /* No interlace mode. */
              if ( V < 0x100 ) _status_aux.HVC.b.v1= V;
              else if ( V < 0x103 )
        	_status_aux.HVC.b.v1= V-0x100;
              else _status_aux.HVC.b.v1= (V-0x100) + (0xCA-0x03);
              break;
            case 1: /* Interlace mode. */
              if ( V < 0x100 ) _status_aux.HVC.b.v1= V&0xFE;
              else if ( V < 0x103 )
        	_status_aux.HVC.b.v1= (V-0x100)|0x01;
              else
        	{
        	  if ( _status_aux.odd_frame )
        	    {
        	      if ( V == 0x103 ) _status_aux.HVC.b.v1= 0xC9;
        	      else _status_aux.HVC.b.v1=
        		     ((V-0x100) + (0xCA-0x04))|0x01;
        	    }
        	  else _status_aux.HVC.b.v1=
        		 ((V-0x100) + (0xCA-0x03))|0x01;
        	}
              break;
            case 3: /* Double interlace mode. */
              if ( V < 0x80 ) _status_aux.HVC.b.v1= V<<1;
              else if ( V < 0x100 )
        	_status_aux.HVC.b.v1= ((V-0x80)<<1)|0x01;
              else if ( V < 0x102 )
        	_status_aux.HVC.b.v1= (V-0x100)<<1;
              else
        	{
        	  if ( _status_aux.odd_frame )
        	    {
        	      if ( V == 0x102 ) _status_aux.HVC.b.v1= 0x93;
        	      else _status_aux.HVC.b.v1=
        		     (((V-0x100) + (0x4A-0x03))<<1)|0x01;
        	    }
        	  else _status_aux.HVC.b.v1=
        		 (((V-0x100) + (0x4A-0x02))<<1)|0x01;
        	}
              break;
            default: _status_aux.HVC.b.v1= 0; /* ¿? */
            }
        }
    }
  else /* NTSC */
    {
      if ( _regs.V30_cell_mode )
        {
          switch ( _regs.interlace_mode )
            {
            case 0: /* No interlace mode. */
              if ( V < 0x100 ) _status_aux.HVC.b.v1= V;
              else _status_aux.HVC.b.v1= V-0x100;
              break;
            case 1: /* Interlace mode. */
              if ( V < 0x100 ) _status_aux.HVC.b.v1= V&0xFE;
              else _status_aux.HVC.b.v1= (V-0x100)|0x01;
              break;
            case 3: /* Double interlace mode. */
              if ( V < 0x80 ) _status_aux.HVC.b.v1= V<<1;
              else if ( V < 0x100 )
        	_status_aux.HVC.b.v1= ((V-0x80)<<1)|0x01;
              else if ( V < 0x180 )
        	_status_aux.HVC.b.v1= (V-0x100)<<1;
              else _status_aux.HVC.b.v1= ((V-0x180)<<1)|0x01;
              break;
            default: _status_aux.HVC.b.v1= 0; /* ¿? */
            }
        }
      else /* V28 cell mode */
        {
          switch ( _regs.interlace_mode )
            {
            case 0: /* No interlace mode. */ 
              if ( V < 0xEB ) _status_aux.HVC.b.v1= V;
              else _status_aux.HVC.b.v1= (V-0xEB)+0xE5;
              break;
            case 1: /* Interlace mode. */
              if ( V < 0xEA ) _status_aux.HVC.b.v1= V&0xFE;
              else if ( V == 0xEA ) _status_aux.HVC.b.v1= 0xEA;
              else _status_aux.HVC.b.v1= ((V-0xEB)+0xE5)|0x01;
              break;
            case 3: /* Double interlace mode. */
              if ( V < 0x80 ) _status_aux.HVC.b.v1= V<<1;
              else if ( V < 0xEB )
        	_status_aux.HVC.b.v1= ((V-0x80)<<1)|0x01;
              else
        	{
        	  if ( _status_aux.odd_frame )
        	    {
        	      if ( V == 0xEB ) _status_aux.HVC.b.v1= 0xE4;
        	      else _status_aux.HVC.b.v1=
        		     (((V-0xEC) + 0x65)<<1)|0x01;
        	    }
        	  else _status_aux.HVC.b.v1=
        		 (((V-0xEB) + 0x65)<<1)|0x01;
        	}
              break;
            default: _status_aux.HVC.b.v1= 0; /* ¿? */
            }
        }
    }
  
} /* end update_HVC */


static void
recalc_cctoendframe (
        	     const int V,
        	     const int H
        	     )
{
  
  _timing.cctoendframe=
    (_timing.lines-V)*_timing.ciclesperline -
    H*_timing.frac;
  
} /* end recalc_cctoendframe */


static void
recalc_cctoVInt (
        	 const int V,
        	 const int H
        	 )
{
  
  /* FROM http://gendev.spritesmind.net/forum/viewtopic.php?p=11681 :
   *   VINT pending flag is set on line $E0 (if VDP height is set to
   *   224 lines) or line $F0 (if VDP height is set to 240 lines),
   *   exactly at HCounter = $02 (regardless of the VDP width). The
   *   state of this flag can be seen in real-time by reading VDP
   *   status bit 7.
   *  Cada 2 punts de H és un valor de HCounter.
   */
  /* Recordatori important. La línia 0 comença relament en el H del
   * HInt de V=0. Per tant el VInt es produeix en el HCounter=2 de la
   * línia E0 (F0) la cual comença en HInt, es a dir, en H=4 i
   * V=_timing.lines_before_end_display+1
   */
  
  if ( V <= _timing.lines_before_end_display ||
       (V == (_timing.lines_before_end_display+1) && H < 4))
    _timing.cctoVInt=
      (_timing.lines_before_end_display+1-V)*_timing.ciclesperline;
  else
    _timing.cctoVInt=
      (_timing.lines_before_end_display+1
       +_timing.lines-V)*_timing.ciclesperline;
  _timing.cctoVInt+= (4-H)*_timing.frac; /* Reste el que ja porte fet
        				    de línia i sume els 2
        				    cicles de rigor. */
  
} /* end recalc_cctoVInt */


static void
recalc_cctoHInt (
        	 const int V,
        	 const int H
        	 )
{
  
  /* Estic en el display o l'última línia. */
  if ( V <= _timing.lines_before_end_display )
    {
      if ( H < _timing.linepp_before_hint )
        _timing.cctoHInt= (_timing.linepp_before_hint-H)*_timing.frac;
      else if ( V == _timing.lines_before_end_display )
        _timing.cctoHInt= 
          /* Línies que queden de VBlank contant l'actual */
          ((_timing.lines-V)*_timing.ciclesperline) +
          /* Cicles a la Hint - el que ja portem */
          (_timing.linepp_before_hint-H)*_timing.frac;
      else
        _timing.cctoHInt=
          ((_timing.pointsperline-H)+_timing.linepp_before_hint)*_timing.frac;
    }
  
  /* Estic després del display. */
  else
    _timing.cctoHInt= 
      /* Línies que queden de VBlank contant l'actual */
      ((_timing.lines-V)*_timing.ciclesperline) +
      /* Cicles a la Hint - el que ja portem */
      (_timing.linepp_before_hint-H)*_timing.frac;
  
} /* end recalc_cctoHInt */


/* Aquesta funció afecta a cc2frac i per tant recalcula tots els
   valors de timing que depenen de frac i cc2frac, bàsicament tots els
   cc algo. */
static void
recalc_pointsperline (void)
{

  /* NOTA!! Com frac és constant per a PAL o NTSC, no cal modificar
     _timing.cc. */
  
  _timing.pointsperline=
    _regs.H40_cell_mode ? POINTSPERLINE_H40 : POINTSPERLINE_H32;
  
  /* Recalcula cc2frac */
  _timing.cc2frac= _timing.pointsperline * _timing.lines *
    (_status_aux.ispal ? PAL_CC2PP : NTSC_CC2PP);

  /* Recalcula valors depenents. */
  _timing.ciclesperline= _timing.pointsperline*_timing.frac;
  _timing.linecc_before_hint= _timing.linepp_before_hint*_timing.frac;
  _timing.cctonextline= (_timing.pointsperline - _timing.H)*_timing.frac;
  recalc_cctoVInt ( _timing.V, _timing.H );
  recalc_cctoHInt ( _timing.V, _timing.H );
  recalc_cctoendframe ( _timing.V, _timing.H );
  
} /* end recalc_pointsperline */


static void
set_H40_cell_mode (
        	   const MD_Bool mode
        	   )
{
  
  int scale;
  
  
  if ( mode == _regs.H40_cell_mode ) return;
  
  _regs.H40_cell_mode= mode;
  if ( _regs.H40_cell_mode )
    {
      _csize.width= 320;
      _csize.ntiles= 40;
      _timing.linepp_before_hint= LINEPP_BEFORE_END_DISPLAY_H40;
      _timing.linepp_before_begin_hblank= LINEPP_BEFORE_BEGIN_HBLANK_H40;
      _timing.linepp_before_end_hblank= LINEPP_BEFORE_END_HBLANK_H40;
      _timing.linepp_before_end_display= LINEPP_BEFORE_END_DISPLAY_H40;
      _timing.dma_mem2vram_cc_display= DMA_MEM2VRAM_CC_H40_DISPLAY;
      _timing.dma_mem2vram_cc_vblank= DMA_MEM2VRAM_CC_H40_VBLANK;
      _dma.fill_bytes_per_line_display= DMA_FILL_BYTES_PER_LINE_H40_DISPLAY;
      _dma.fill_bytes_per_line_vblank= DMA_FILL_BYTES_PER_LINE_H40_VBLANK;
      _dma.copy_bytes_per_line_display= DMA_COPY_BYTES_PER_LINE_H40_DISPLAY;
      _dma.copy_bytes_per_line_vblank= DMA_COPY_BYTES_PER_LINE_H40_VBLANK;
    }
  else
    {
      _csize.width= 256;
      _csize.ntiles= 32;
      _timing.linepp_before_hint= LINEPP_BEFORE_END_DISPLAY_H32;
      _timing.linepp_before_begin_hblank= LINEPP_BEFORE_BEGIN_HBLANK_H32;
      _timing.linepp_before_end_hblank= LINEPP_BEFORE_END_HBLANK_H32;
      _timing.linepp_before_end_display= LINEPP_BEFORE_END_DISPLAY_H32;
      _timing.dma_mem2vram_cc_display= DMA_MEM2VRAM_CC_H32_DISPLAY;
      _timing.dma_mem2vram_cc_vblank= DMA_MEM2VRAM_CC_H32_VBLANK;
      _dma.fill_bytes_per_line_display= DMA_FILL_BYTES_PER_LINE_H32_DISPLAY;
      _dma.fill_bytes_per_line_vblank= DMA_FILL_BYTES_PER_LINE_H32_VBLANK;
      _dma.copy_bytes_per_line_display= DMA_COPY_BYTES_PER_LINE_H32_DISPLAY;
      _dma.copy_bytes_per_line_vblank= DMA_COPY_BYTES_PER_LINE_H32_VBLANK;
    }
  recalc_pointsperline ();
  /* NO CAL!! ho fa recalc_pointsperline
  _timing.linecc_before_hint= _timing.linepp_before_hint*_timing.pp2cc;
  recalc_cctoVInt ( _timing.V, _timing.H );
  recalc_cctoHInt ( _timing.V, _timing.H );
  recalc_cctoendframe ( _timing.V, _timing.H );
  */
  scale= _regs.interlace_mode==3 ? 2 : 1;
  res_changed ( _csize.width*scale, _csize.height*scale );
  
} /* end set_H40_cell_mode */


static void
set_V30_cell_mode (
        	   const MD_Bool mode
        	   )
{
  
  int scale;
  
  
  if ( mode == _regs.V30_cell_mode ) return;
  
  _regs.V30_cell_mode= mode;
  _csize.height= _regs.V30_cell_mode ? 240 : 224;
  _timing.lines_before_end_display= _csize.height;
  /* No m'importa que al canviar de mode ens botem una interrupció. */
  recalc_cctoVInt ( _timing.V, _timing.H );
  recalc_cctoHInt ( _timing.V, _timing.H );
  recalc_cctoendframe ( _timing.V, _timing.H );
  scale= _regs.interlace_mode==3 ? 2 : 1;
  res_changed ( _csize.width*scale, _csize.height*scale );
  
} /* end set_V30_cell_mode */


static void
data_write (
            const MD_Word data
            )
{
  
  int aux;
  
  
  _status_aux.fifo_empty= MD_FALSE;
  switch ( _access.code&0xF )
    {
      
    case 0x01: /* VRAM write. */
      if ( _access.addr&0x1 )
        {
          _vram[_access.addr&0xFFFE]= data.b.v0;
          _vram[_access.addr]= data.b.v1;
        }
      else
        {
          _vram[_access.addr]= data.b.v1;
          _vram[_access.addr|0x0001]= data.b.v0;
        }
      break;
      
    case 0x03: /* CRAM write. */
      aux= (_access.addr&0x007F)>>1;
      _cram[aux]=
        ((data.v>>3)&0x01C0) | /* B2 B1 B0 */
        ((data.v>>2)&0x0038) | /* G2 G1 G0 */
        ((data.v>>1)&0x0007);  /* R2 R1 R0 */
      break;
      
    case 0x05: /* VSRAM write. */
      aux= (_access.addr%80)>>1;
      _vsram[aux]= data.v&0x07FF; /* VS10 ~ VS0 */
      break;

    default: _warning ( _udata,
        		"VDP: codi de control desconegut (write): 0x%02X",
        		_access.code );
      
    }
  _access.addr+= _regs.auto_increment_data;
  
} /* end data_write */


static void
data_write8 (
             const MDu8    data,
             const MD_Bool isH
             )
{
  
  int aux;
  
  
  _status_aux.fifo_empty= MD_FALSE;
  switch ( _access.code&0xF )
    {
      
    case 0x01: /* VRAM write. */
      if ( _access.addr&0x1 ) _vram[_access.addr&0xFFFE]= data;
      else                    _vram[_access.addr|0x0001]= data;
      break;
      
    case 0x03: /* CRAM write. */
      aux= (_access.addr&0x007F)>>1;
      if ( isH )
        {
          _cram[aux]&= 0x003F;
          _cram[aux]|= (((MDu16) (data&0x0E))<<5); /* B2 B1 B0 */
        }
      else
        {
          _cram[aux]&= 0x1C0;
          _cram[aux]|=
            ((data>>2)&0x38) | /* G2 G1 G0 */
            ((data>>1)&0x07);  /* R2 R1 R0 */
        }
      break;
      
    case 0x05: /* VSRAM write. */
      aux= (_access.addr&0x004F)>>1;
      if ( isH )
        {
          _vsram[aux]&= 0x00FF;
          _vsram[aux]|= (((MDu16) (data&0x07))<<8); /* VS10 ~ VS8 */
        }
      else
        {
          _vsram[aux]&= 0x0700;
          _vsram[aux]|= data; /* VS7 ~ VS0 */
        }
      break;

    default: _warning ( _udata,
        		"VDP: codi de control desconegut (write): 0x%02X",
        		_access.code );
      
    }
  _access.addr+= _regs.auto_increment_data;
  
} /* end data_write8 */


/* 1 - Acaba. De moment sols suporte VRAM. */
static int
dma_fill (
          int nbytes
          )
{
  
  int n;
  
  
  if ( nbytes == 0 ) return 0;
  /* -- APROXIMACÓ VELLA QUE PAREIX MÉS D'ACORD AMB 'genvdp.txt'
  if ( !_dma.fill_started )
    {
      _vram[_access.addr]= _dma.fill_data.b.v0;
      _dma.fill_started= MD_TRUE;
    }
  */
  if ( !_dma.fill_started )
    {
      _vram[_access.addr]= _dma.fill_data.b.v0;
      _vram[_access.addr^0x1]= _dma.fill_data.b.v1;
      _access.addr+= _regs.auto_increment_data;
      _dma.fill_started= MD_TRUE;
    }
  for ( n= 0; n < nbytes; ++n )
    {
      _vram[_access.addr^0x1]= _dma.fill_data.b.v1;
      _access.addr+= _regs.auto_increment_data;
      if ( --_regs.dma_length_counter_tmp == 0 ) return 1;
    }
  
  return 0;
  
} /* end dma_fill */


/* 1 - Acaba. */
static int
dma_copy (
          const int nbytes
          )
{
  
  int n;
  
  
  for ( n= 0; n < nbytes; ++n )
    {
      _vram[_access.addr]= _vram[_regs.dma_source_address_tmp&0xFFFF];
      ++_regs.dma_source_address_tmp;
      _access.addr+= _regs.auto_increment_data;
      if ( --_regs.dma_length_counter_tmp == 0 ) return 1;
    }
  
  return 0;
  
} /* end dma_copy */


static void
finish_dma (void)
{
  
  _status_aux.dma_busy= MD_FALSE;
  _access.code|= 0x20; /* Restaure el bit. */
  /*_regs.DMA_enabled= MD_FALSE ¿¿????? */
  
} /* end finish_dma */


/* Executa accesos de DMA fill o copy a nivell de línia. Els accessos
   de Ve mai s'executen. */
static void
run_dma (
         const int Vb,
         const int Ve
         )
{
  
  /*
   * NOTA: El VBLANK comença a mitjan V=0xE0 i acaba a mitjan de
   * l'última línia. Per a no comlicar-ho farem que ací
   * considerarem diplay 0<=V<=0xE0.
   */
  int lines, ret, V, min;
  
  
  /* Prepara. */
  V= Vb;
  if ( V >= Ve ) return;
  
  /* Display. */
  if ( V <= _timing.lines_before_end_display )
    {
      min= MIN ( Ve, _timing.lines_before_end_display );
      lines= min - V;
      V= min;
      if ( _regs.dma_mode == DMA_FILL )
        ret= dma_fill ( lines*(_regs.enabled ?
        		       _dma.fill_bytes_per_line_display :
        		       _dma.fill_bytes_per_line_vblank) );
      else
        ret= dma_copy ( lines*(_regs.enabled ?
        		       _dma.copy_bytes_per_line_display :
        		       _dma.copy_bytes_per_line_vblank) );
      if ( ret ) goto finish;
    }
  if ( V >= Ve ) return;
  
  /* VBlank inferior. */
  /*if ( V < _timing.lines )*/
  lines= Ve - V;
  if ( _regs.dma_mode == DMA_FILL )
    ret= dma_fill ( lines*_dma.fill_bytes_per_line_vblank );
  else
    ret= dma_copy ( lines*_dma.copy_bytes_per_line_vblank );
  if ( ret ) goto finish;
  
  return;
  
 finish:
  finish_dma ();
  
} /* end run_dma */


static void
eval_line_spr (void)
{
  
  int n, N, row, brow, max_row, maxN, pat_height, width, NDots;
  MD_Bool s1_mask, masked, dot_overflow;
  MDu16 y_mask;
  
  
  /* Preliminars. */
  if ( _regs.interlace_mode == 3 ) { brow= 256; pat_height= 16; y_mask= 0x3FF; }
  else                             { brow= 128; pat_height= 8; y_mask= 0x1FF; }
  if ( _regs.H40_cell_mode ) { NDots= 320; maxN= 20; }
  else                       { NDots= 256; maxN= 16; }
  
  /* AVALUACIÓ. En realitat açò es deuria de fer una línia avanç, però
     els atributs estan en cache i no crec que els patterns canvien
     molt, si no hi ha ningun problema amb el timing ho deixaré
     ací. */
  dot_overflow= MD_FALSE;
  _sprites_buff.N= N= 0;
  s1_mask= masked= MD_FALSE;
  for ( n= 0; n < _sprites.N && !dot_overflow; ++n )
    {
      max_row= pat_height*_sprites.v[n].height;
      row= (_render.lines + brow) - ((_sprites.v[n].y)&y_mask);
      if ( row >= 0 && row < max_row )
        {
          if ( N == maxN )
            {
              _status_aux.too_many_sprites= MD_TRUE;
              break;
            }
          if ( !masked && _sprites.v[n].x == 0 )
            {
              if ( N == 0 && !_render.dot_overflow ) s1_mask= MD_TRUE;
              else if ( !s1_mask ) masked= MD_TRUE;
            }
          width= _sprites.v[n].width*8;
          if ( width > NDots ) { dot_overflow= MD_TRUE; width= NDots; }
          else NDots-= width;
          if ( !masked && width )
            {
              _sprites_buff.v[_sprites_buff.N].ind= n;
              _sprites_buff.v[_sprites_buff.N].row= row;
              _sprites_buff.v[_sprites_buff.N].width= width;
              _sprites_buff.v[_sprites_buff.N++].isp0= !_sprites.v[n].prio;
            }
          ++N;
        }
    }
  _render.dot_overflow= dot_overflow;
  
} /* end eval_line_spr */


static void
render_spr_line_set_val (
        		 MDu8 const    color,
        		 MDu8 const    pal,
        		 int const     x,
        		 MD_Bool const isp0
        		 )
{

  int new_color;
  

  // Calcula el color.
  if ( _render.S_TE )
    {
      if ( pal == 0x30 )
        {
          if ( color == 0xF ) // Shadow.
            new_color= SHA_COLOR;
          else if ( color == 0xE ) // Highlight.
            new_color= HIG_COLOR;
          else if ( color )
            new_color= color|pal;
          else return; // Transparent
        }
      else if ( color ) new_color= color|pal;
      else return; // Transparent
    }
  else if ( color ) new_color= color|pal;
  else return; // Transparent

  // Inserta color
  if ( _render.spr_line[x].type == -1 )
    _render.spr_line[x].coll_counter= 0;
  _render.spr_line[x].color= new_color;
  _render.spr_line[x].type= isp0 ? 0 : 1;
  if ( new_color != SHA_COLOR && new_color != HIG_COLOR )
    {
      if ( ++_render.spr_line[x].coll_counter > 1 )
        _status_aux.spr_collision= MD_TRUE;
    }
  
} // end render_spr_line_set_val


static void
render_line_spr (
        	 sprite_buff_t const * const buffer
        	 )
{
  
  int n, row, pat_height, pat_size, w, i, x, begin, end, width;
  const sprite_t *p;
  MDu16 addr_pat, addr, inc_pat;
  MDu8 byte, color;
  MD_Bool isp0;
  
  
  /* Preliminars. */
  for ( i= 0; i < _csize.width; ++i )
    _render.spr_line[i].type= -1;
  if ( _regs.interlace_mode == 3 )
    {
      pat_height= 16;
      pat_size= 64;
    }
  else
    {
      pat_height= 8;
      pat_size= 32;
    }
  
  /* Pinta. */
  for ( n= buffer->N-1; n >= 0; --n )
    {
      
      /* Obté informació. */
      p= &(_sprites.v[buffer->v[n].ind]);
      row= buffer->v[n].row;
      width= buffer->v[n].width;
      isp0= buffer->v[n].isp0;
      
      /* Recalcula la fila. */
      if ( p->vflip ) row= p->height*pat_height - row - 1;
      
      /* Renderitza. Depen de hflip. */
      inc_pat= pat_size*p->height;
      begin= p->x - 128; end= begin + width;
      if ( end > _csize.width ) end= _csize.width;
      if ( p->hflip )
        {
          
          addr_pat= (p->pat*pat_size); /* Adreçá pat 0. */
          addr_pat+= (row/pat_height)*pat_size; /* Adreça pat on està row. */
          addr_pat+= (row%pat_height)*4; /* Adreça inicial si no fora flip. */
          addr_pat+= inc_pat*(p->width-1) + 3; /* Adreça inicial. */
          for ( w= 0, x= begin; w < p->width && x < end; ++w )
            {
              addr= addr_pat;
              for ( i= 0; i < 4 && x < end; ++i, --addr )
        	{
        	  byte= _vram[addr];
                  if ( x >= 0 )
                    {
                      color= byte&0xF;
        	      render_spr_line_set_val ( color, p->pal, x, isp0 );
                    }
                  if ( ++x == end ) break;
                  if ( x >= 0 )
                    {
                      color= byte>>4;
        	      render_spr_line_set_val ( color, p->pal, x, isp0 );
                    }
                  ++x;
        	}
              addr_pat-= inc_pat;
            }
        }
      else /* Normal. */
        {
          addr_pat= (p->pat*pat_size); /* Adreçá pat 0. */
          addr_pat+= (row/pat_height)*pat_size; /* Adreça pat on està row. */
          addr_pat+= (row%pat_height)*4; /* Adreça inicial. */
          for ( w= 0, x= begin; w < p->width && x < end; ++w )
            {
              addr= addr_pat;
              for ( i= 0; i < 4 && x < end; ++i, ++addr )
        	{
        	  byte= _vram[addr];
        	  if ( x >= 0 )
        	    {
        	      color= byte>>4;
        	      render_spr_line_set_val ( color, p->pal, x, isp0 );
        	    }
        	  if ( ++x == end ) break;
        	  if ( x >= 0 )
        	    {
        	      color= byte&0xF;
        	      render_spr_line_set_val ( color, p->pal, x, isp0 );
        	    }
        	  ++x;
        	}
              addr_pat+= inc_pat;
            }
        }
      
    }
  
} /* end render_line_spr */


static void
render_line_sc (
        	scroll_t * const sc
        	)
{
  
  MDu16 aux, addr_row, row, init_col, col_mask, addr_col, addr, NT,
    addr_pat, pat_size, sel_tile, row_mask, maxrowcell, rowbits;
  MDu64 bits;
  MDu8 byte0,byte1,byte2,byte3, pal_vals[2], prior_vals[2], color, selected;
  int desp_bits, desp_tile, x, i, j, niters, n, ntiles, addr_row_desp, cols;
  MD_Bool isp1;
  
  
  /* Preparació. */
  switch ( _render.VSZ )
    {
    case 32:  row_mask= 0xFF;  break;
    case 64:  row_mask= 0x1FF; break;
    case 128: row_mask= 0x3FF; break;
    default: row_mask= 0x000;
    }
  switch ( _render.HSZ )
    {
    case 32:  addr_row_desp= 6; cols= 256; col_mask= 0x003F; break;
    case 64:  addr_row_desp= 7; cols= 512; col_mask= 0x007F; break;
    case 128: addr_row_desp= 8; cols= 1024; col_mask= 0x00FF; break;
    default: addr_row_desp= 0; cols= col_mask= 0x0000;
    }
  if ( _regs.interlace_mode == 3 )
    {
      pat_size= 6;
      maxrowcell= 0xF;
      rowbits= 4;
      row_mask<<= 1;
    }
  else
    {
      pat_size= 5;
      maxrowcell= 0x7;
      rowbits= 3;
    }
  sc->N0= sc->N1= 0;
  if ( _render.vsc_mode_is_cell )
    {
      niters= _csize.ntiles/2;
      ntiles= 2;
    }
  else
    {
      niters= 1;
      ntiles= _csize.ntiles;
    }
  
  /* Itera cada 2 columnes, o sobre tota la línia. */
  for ( x= n= 0; n < niters; ++n )
    {

      /* Adreça NT 13 bits. El bit 0 no conta. */
      /* H32 -> 5+1 bits -> row en D12-D6
       * H64 -> 6+1 bits -> row en D12-D7
       * H128 -> 7+1 bits -> row en D12-D8
       */
      /* Calcula fila (i adreça base) de la NT. */
      aux= _regs.interlace_mode==3 ?
        (_vsram[sc->off_2+n*2]&0x07FF) : (_vsram[sc->off_2+n*2]&0x03FF);
      row= (_render.lines + aux)&row_mask;
      addr_row= sc->NT_addr | ((row>>rowbits)<<addr_row_desp);
      
      /* Columna inicial (Realment em quede amb l'anterior). */
      aux= _render.Htable|sc->off;
      switch ( _render.hsc_mode )
        {
        case CELL: aux+= ((_render.lines>>rowbits)<<5); break;
        case LINE:
          if ( _regs.interlace_mode == 3 ) aux+= ((_render.lines>>1)<<2);
          else                             aux+= (_render.lines<<2);
          break;
        }
      aux= ((((MDu16) _vram[aux])<<8)|_vram[aux|1])&0x03FF;
      init_col= (16*n+cols-(aux%cols))%cols;
      addr_col= ((init_col>>3)<<1);
      
      /* Desplaçaments. */
      desp_bits= 60-((init_col&0x7)*4);
      desp_tile= 15-(init_col&0x7);
      
      /* Renderitza. Inicialització. */
      GET_NEXT_NT;
      CALC_ADDR_PAT;
      INIT_BITS_AND_VALS;
      /* Renderitza. Cos. */
      for ( i= 0; i < ntiles; ++i )
        {
          GET_NEXT_NT;
          CALC_ADDR_PAT;
          LOAD_BITS_AND_VALS;
          sel_tile= 0x00FF;
          for ( j= 0; j < 8; ++j, ++x )
            {
              selected= (sel_tile>>desp_tile)&0x1;
              isp1=  (prior_vals[selected]!=0);
              color= (bits>>desp_bits)&0xF;
              if ( color )
        	{
        	  sc->line[x]= color|pal_vals[selected];
        	  if ( isp1 ) sc->prio1[sc->N1++]= x;
        	  else        sc->prio0[sc->N0++]= x;
        	}
              else sc->line[x]= 0;
              sc->isp0[x]= !isp1;
              bits<<= 4; sel_tile<<= 1;
            }
          pal_vals[0]= pal_vals[1];
          prior_vals[0]= prior_vals[1];
        }
      
    }
  
} /* end render_line_sc */


static void
render_line_win (
        	 const int        begin, /* Primera celda a dibuixar.*/
        	 const int        end,   /* Última més 1. */
        	 scroll_t * const sc
        	 )
{
  
  int i, x, addr_row_desp, j, *prio, *Np;
  MDu32 addr, addr_pat, addr_nt;
  MDu16 NT, pat_size;
  MDu8 byte, pal, color;
  MD_Bool isp0;
  
  
  pat_size= _regs.interlace_mode==3 ? 6 : 5;
  if ( _regs.H40_cell_mode )
    {
      addr_nt= _render.win_NT_addr&0xF800;
      addr_row_desp= 7;
    }
  else
    {
      addr_nt= _render.win_NT_addr;
      addr_row_desp= 6;
    }
  addr= addr_nt | ((_render.lines>>3)<<addr_row_desp) | (begin<<1);
  for ( x= begin*8, i= begin; i != end; ++i )
    {
      
      /* Obté NT. */
      NT= (((MDu16) _vram[addr])<<8)|_vram[addr|1];
      addr+= 2;
      
      /* Calcula addr_pat. */
      addr_pat= (NT&0x07FF)<<pat_size;
      if ( NT&0x1000 /*vf*/ ) addr_pat|= (7-(_render.lines&0x7))<<2;
      else                    addr_pat|= (_render.lines&0x7)<<2;
      
      /* Dibuixa. */
      pal= (MDu8) (((NT>>13)&0x3)<<4);
      if ( (MDu8) (NT>>15) )
        { Np= &(sc->N1); prio= &(sc->prio1[0]); isp0= MD_TRUE; }
      else
        { Np= &(sc->N0); prio= &(sc->prio0[0]); isp0= MD_FALSE; }
      if ( NT&0x0800 /*hf*/)
        {
          addr_pat+= 4;
          for ( j= 0; j < 4; ++j )
            {
              byte= _vram[--addr_pat];
              sc->isp0[x]= isp0;
              color= byte&0xF;
              if ( color ) { prio[(*Np)++]= x; sc->line[x++]= color|pal; }
              else         sc->line[x++]= 0;
              sc->isp0[x]= isp0;
              color= byte>>4;
              if ( color ) { prio[(*Np)++]= x; sc->line[x++]= color|pal; }
              else         sc->line[x++]= 0;
            }
        }
      else
        for ( j= 0; j < 4; ++j )
          {
            byte= _vram[addr_pat++];
            sc->isp0[x]= isp0;
            color= byte>>4;
            if ( color ) { prio[(*Np)++]= x; sc->line[x++]= color|pal; }
            else         sc->line[x++]= 0;
            sc->isp0[x]= isp0;
            color= byte&0xF;
            if ( color ) { prio[(*Np)++]= x; sc->line[x++]= color|pal; }
            else         sc->line[x++]= 0;
          }
      
    }
  
} /* end render_line_win */


static void
render_line_scA_win (void)
{
  
  MD_Bool all_win;
  int aux, aux2, begin, end, n;
  scroll_t *scA;
  
  
  /* Mira si verticalment és una línia completa per a la finestra. */
  aux= _render.WVP*8;
  if ( _render.isDOWN ) all_win= (_render.lines>=aux);
  else all_win= (_render.lines<aux);
  
  /* Agarra punter i si tota la línia és finestra aleshores dibuixa-la. */
  scA= &(_render.sc[0]);
  if ( all_win )
    {
      scA->N0= scA->N1= 0;
      render_line_win ( 0, _regs.H40_cell_mode ? 40 : 32, scA );
      return;
    }
  
  /* Mira la posició horizontal de la finestra. */
  if ( _render.isRIGT ) { begin= _render.WHP*2; end= _csize.ntiles; }
  else                  { begin= 0; end= _render.WHP*2; }
  
  /* Dibuixa. */
  render_line_sc ( scA );
  if ( begin < end )
    {
      aux= begin*8; aux2= end*8;
      for ( n= 0; n < scA->N0; )
        if ( scA->prio0[n]>=aux && scA->prio0[n]<aux2 )
          scA->prio0[n]= scA->prio0[--(scA->N0)];
        else ++n;
      for ( n= 0; n < scA->N1; )
        if ( scA->prio1[n]>=aux && scA->prio1[n]<aux2 )
          scA->prio1[n]= scA->prio1[--(scA->N1)];
        else ++n;
      render_line_win ( begin, end, scA );
    }
  
} /* end render_line_scA_win */


static void
render_line (void)
{
  
  /* NOTA: Gaste l'algoritme del pintor. */
  /* NOTA!!! El millor document per explicar el STE és genvdp.txt. */
  
  int n, i, color;
  scroll_t *scB, *scA;
  
  
  /* Background */
  for ( i= 0; i < _csize.width; ++i )
    _render.tmp[i]= _render.bgcolor;
  if ( _regs.enabled )
    {
      scB= &(_render.sc[1]); scA= &(_render.sc[0]);
      render_line_sc ( scB );
      render_line_scA_win ();/*render_line_sc ( scA );*/
      eval_line_spr ();
      render_line_spr ( &_sprites_buff );
      /* Scroll B - Prioritat 0. */
      for ( n= 0; n < scB->N0; ++n )
        { i= scB->prio0[n]; _render.tmp[i]= scB->line[i]; }
      /* Scroll A - Prioritat 0. */
      for ( n= 0; n < scA->N0; ++n )
        { i= scA->prio0[n]; _render.tmp[i]= scA->line[i]; }
      /* Inicialitza buffer S_TE. */
      if ( _render.S_TE )
        for ( i= 0; i < _csize.width; ++i )
          _render.s_te[i]= (scA->isp0[i] && scB->isp0[i]) ? SHA : NOR;
      /* Sprites - Prioritat 0. */
      for ( i= 0; i < _csize.width; ++i )
        if ( _render.spr_line[i].type == 0 )
          {
            color= _render.spr_line[i].color;
            if ( color == SHA_COLOR )
              _render.s_te[i]= (_render.s_te[i]==HIG) ? NOR : SHA;
            else if ( color == HIG_COLOR )
              _render.s_te[i]= (_render.s_te[i]==SHA) ? NOR : HIG;
            else _render.tmp[i]= color;
          }
      /* Scroll B - Prioritat 1. */
      for ( n= 0; n < scB->N1; ++n )
        { i= scB->prio1[n]; _render.tmp[i]= scB->line[i]; }
      if ( _render.S_TE )
        for ( n= 0; n < scB->N1; ++n )
          { i= scB->prio1[n]; _render.s_te[i]= NOR; }
      /* Scroll A - Prioritat 1. */
      for ( n= 0; n < scA->N1; ++n )
        { i= scA->prio1[n]; _render.tmp[i]= scA->line[i]; }
      if ( _render.S_TE )
        for ( n= 0; n < scA->N1; ++n )
          { i= scA->prio1[n]; _render.s_te[i]= NOR; }
      /* Sprites - Prioritat 1. */
      for ( i= 0; i < _csize.width; ++i )
        if ( _render.spr_line[i].type == 1 )
          {
            color= _render.spr_line[i].color;
            if ( color == SHA_COLOR )
              _render.s_te[i]= (_render.s_te[i]==HIG) ? NOR : SHA;
            else if ( color == HIG_COLOR )
              _render.s_te[i]= (_render.s_te[i]==SHA) ? NOR : HIG;
            else { _render.tmp[i]= color; _render.s_te[i]= NOR; }
          }
    }
  if ( _regs.interlace_mode == 3 )
    {
      if ( _render.S_TE )
        for ( i= 0; i < _csize.width; ++i )
          {
            color= _cram[_render.tmp[i]] | _render.s_te[i];
            *(_render.p++)= color;
            *(_render.p++)= color;
          }
      else
        for ( i= 0; i < _csize.width; ++i )
          {
            color= _cram[_render.tmp[i]];
            *(_render.p++)= color;
            *(_render.p++)= color;
          }
      _render.p+= _render.width;
      _render.lines+= 2;
    }
  else
    {
      if ( _render.S_TE )
        for ( i= 0; i < _csize.width; ++i )
          *(_render.p++)= _cram[_render.tmp[i]] | _render.s_te[i];
      else
        for ( i= 0; i < _csize.width; ++i )
          *(_render.p++)= _cram[_render.tmp[i]];
      ++_render.lines;
    }
  
} /* end render_line */


static void
update_sprites (void)
{
  
  MDu16 addr;
  int i, nsprites;
  sprite_t *p;
  MDu8 next;
  const MDu8 *q;
  
  
  addr= _regs.sprite_attribute_table_addr;
  next= 0; i= 0;
  if ( _regs.H40_cell_mode )
    {
      nsprites= 80;
    }
  else
    {
      nsprites= 64;
    }
  _sprites.N= 0;
  do {
    q= &(_vram[addr+next*8]);
    p= &_sprites.v[_sprites.N++];
    p->y= ((((MDu16) q[0])<<8)|q[1])&0x3FF;
    p->width= ((q[2]>>2)&0x3) + 1;
    p->height= (q[2]&0x3) + 1;
    next= q[3]&0x7F;
    p->prio= ((q[4]&0x80)!=0);
    p->pal= (q[4]&0x60)>>1;
    p->vflip= ((q[4]&0x10)!=0);
    p->hflip= ((q[4]&0x08)!=0);
    p->pat= ((((MDu16) q[4])<<8)|q[5])&0x7FF;
    p->x= ((((MDu16) q[6])<<8)|q[7])&0x1FF;
    ++i;
  } while ( next > 0 && next < nsprites && i < nsprites );
  
} /* end update_sprites */


static void
update_render_values (void)
{
  
  _render.bgcolor= _regs.bgcolor;
  _render.sc[0].NT_addr= _regs.scrollA_name_table_addr;
  _render.sc[1].NT_addr= _regs.scrollB_name_table_addr;
  _render.Htable= _regs.H_scroll_table_addr;
  _render.HSZ= _regs.HSZ;
  _render.VSZ= _regs.VSZ;
  _render.hsc_mode= _regs.hsc_mode;
  _render.vsc_mode_is_cell= _regs.vsc_mode_is_cell;
  _render.win_NT_addr= _regs.window_name_table_addr;
  _render.isRIGT= _regs.isRIGT;
  _render.isDOWN= _regs.isDOWN;
  _render.WHP= _regs.WHP;
  _render.WVP= _regs.WVP;
  _render.S_TE= _regs.S_TE;
  update_sprites ();
  
} /* end update_render_values */


/* Pot ser 0. */
static void
render_lines (
              const int lines
              )
{
  
  int i;
  
  
  if ( lines == 0 ) return;
  render_line ();
  update_render_values ();
  for ( i= 1; i < lines; ++i )
    render_line ();
  
} /* end render_lines */


static void
run_end_frame (void)
{
  
  _hint_counter= _regs.H_interrupt_register;
  /* En realitat açò es fa dos punts després, però crec que no passa
     res si ho faig ací. */
  _status_aux.spr_collision= MD_FALSE;
  _status_aux.fifo_empty= MD_TRUE;
  
  /* Prepara el render. */
  if ( _regs.interlace_mode == 3 )
    {
      _render.width= _csize.width*2;
      _render.p= _status_aux.odd_frame ?
        &(_render.fb[_render.width]) : &(_render.fb[0]);
      _render.lines=
        (_status_aux.odd_frame ) ? 1 : 0;
    }
  else
    {
      _render.width= _csize.width;
      _render.p= &(_render.fb[0]);
      _render.lines= 0;
    }
  update_render_values ();
  _render.dot_overflow= MD_FALSE;
  
  /* Desactiva VInt. */
  if ( _status_aux.VInt )
    {
      _status_aux.VInt= MD_FALSE;
      MD_cpu_clear_auto_vector_int ( 6 );
    }
  
} /* end run_end_frame */


static void
run (
     const int Vb,
     const int Hb,
     const int Ve,
     const int He
     )
{
  
  int lines, linesH, aux;
  
  
  if ( _status_aux.dma_busy && _regs.dma_mode != DMA_MEM2VRAM )
    run_dma ( Vb, Ve );
  
  /* En el display */
  if ( Vb < _timing.lines_before_end_display )
    {
      if ( Ve < _timing.lines_before_end_display )
        {
          lines= linesH= Ve-Vb;
          if ( He >= _timing.linepp_before_hint ) ++linesH;
          if ( Hb >= _timing.linepp_before_hint ) --linesH;
          if ( He >= _timing.linepp_before_end_display ) ++lines;
          if ( Hb >= _timing.linepp_before_end_display ) --lines;
          /* ATENCIÓ!!: lines pot ser 0. */
          _hint_counter-= linesH;
          render_lines ( lines );
        }
      else if ( Ve < _timing.lines )
        {
          
          /* HInt i Acaba de dibuixar. */
          lines= linesH= _timing.lines_before_end_display-Vb;
          if ( Hb >= _timing.linepp_before_hint ) --linesH;
          if ( Hb >= _timing.linepp_before_end_display ) --lines;
          /* ATENCIÓ!!: lines pot ser 0. */
          _hint_counter-= linesH;
          render_lines ( lines );
          
          /* Inici de VInt. */
          MD_io_end_frame_1 ();
          MD_io_end_frame_2 ();
          if ( _regs.interlace_mode != 3 || !_status_aux.odd_frame )
            _update_screen ( _render.fb, _udata );
          if ( _regs.V30_cell_mode_tmp != _regs.V30_cell_mode )
            set_V30_cell_mode ( _regs.V30_cell_mode_tmp );
          if ( _regs.H40_cell_mode_tmp != _regs.H40_cell_mode )
            set_H40_cell_mode ( _regs.H40_cell_mode_tmp );
          if ( _regs.interlace_mode_tmp != _regs.interlace_mode )
            {
              if ( (_regs.interlace_mode_tmp == 3 && _regs.interlace_mode != 3)
        	   ||
        	   (_regs.interlace_mode_tmp != 3 && _regs.interlace_mode == 3)
        	   )
        	{
        	  aux= (_regs.interlace_mode_tmp==3) ? 2 : 1;
        	  res_changed ( _csize.width*aux, _csize.height*aux );
        	}
              _status_aux.odd_frame= MD_FALSE;
              _regs.interlace_mode= _regs.interlace_mode_tmp;
            }
          if ( _regs.interlace_mode )
            _status_aux.odd_frame= !_status_aux.odd_frame;
          
        }
      else run_end_frame ();
    }
  
  /* VBlank. EL if es tonto però no se si acabré ficant algo ací. */
  else if ( Vb < _timing.lines )
    {
      if ( Ve == _timing.lines ) run_end_frame ();
    }
  
  /* Final del frames. */
  else run_end_frame ();
  
} /* end run */


static void
clock (void)
{
  
  int newV, newH;
  int64_t npoints, ccused;
  
  
  /* ENABLED ???? */
  
  /* Points a processar. */
  npoints= _timing.cc/_timing.frac;
  ccused= npoints*_timing.frac;
  _timing.cc%= _timing.frac;
  
  /* Actualitza comptadors. Alguns en cicles (es comproven en el
     clock), altres en punts (es comproven ací). */
  _timing.cctoVInt-= ccused;
  _timing.cctoHInt-= ccused;
  _timing.cctonextline-= ccused;
  _timing.cctoendframe-= ccused;
  
  /* Calcula nous valors i executa. */
  newV= _timing.V + npoints/_timing.pointsperline;
  newH= _timing.H + npoints%_timing.pointsperline;
  if ( newH >= _timing.pointsperline ) { ++newV; newH-= _timing.pointsperline; }
  while ( newV >= _timing.lines )
    {
      run ( _timing.V, _timing.H, _timing.lines, 0 );
      newV-= _timing.lines;
      _timing.V= _timing.H= 0;
    }
  run ( _timing.V, _timing.H, newV, newH );
  _timing.V= newV;
  _timing.H= newH;
  _status_aux.VBlank=
    (newV==_timing.lines_before_end_display &&
     newH>=_timing.linepp_before_hint) ||
    (newV>_timing.lines_before_end_display && newV<(_timing.lines-1)) ||
    (newV==_timing.lines-1 && newH<_timing.linepp_before_hint);
  
  /* Recalcula els comptadors que tenen que ser recalculats i si és el
     cas demana interrupció. */
  if ( _timing.cctonextline <= 0 )
    {
      if ( _z80_int_enabled )
        {
          Z80_IRQ ( Z80_FALSE, 0xFF );
          _z80_int_enabled= MD_FALSE;
        }
      _timing.cctonextline= (_timing.pointsperline - newH)*_timing.frac;
    }
  if ( _timing.cctoVInt <= 0 )
    {
      Z80_IRQ ( Z80_TRUE, 0xFF );
      _z80_int_enabled= MD_TRUE;
      _status_aux.VInt= MD_TRUE;
      if ( _regs.VInt_enabled ) MD_cpu_set_auto_vector_int ( 6 );
      recalc_cctoVInt ( newV, newH );
    }
  if ( _timing.cctoHInt <= 0 )
    {
      if ( _hint_counter < 0 )
        {
          _hint_counter= _regs.H_interrupt_register;
          _status_aux.HInt= MD_TRUE;
          if ( _regs.HInt_enabled ) MD_cpu_set_auto_vector_int ( 4 );
        }
      recalc_cctoHInt ( newV, newH );
    }
  if ( _timing.cctoendframe <= 0 )
    recalc_cctoendframe ( newV, newH );
  
} /* end clock */


static void
set_register (
              const MDu8 reg,
              const MDu8 data
              )
{
  
  char caux;
  
  
  switch ( reg )
    {
      
      /* MODE SET REGISTER No. 1 */
    case  0:
      /* IE1 1: Enable H interrupt ; 0: Disable H interrupt */
      _regs.HInt_enabled= ((data&0x10)!=0);
      if ( _regs.HInt_enabled && _status_aux.HInt )
        MD_cpu_set_auto_vector_int ( 4 );
      else MD_cpu_clear_auto_vector_int ( 4 );
      /* M3 1: HV. Counter stop ; 0: Enable read HV. counter */
      _regs.HV_counter_stop= ((data&0x02)!=0);
      if ( _regs.HV_counter_stop ) update_HVC ();
      break;
      
      /* MODE SET REGISTER No. 2 */
    case  1:
      /* DISP 1: Enable Display ; 0: Disable Display */
      _regs.enabled= ((data&0x40)!=0);
      /* 1: Enable V interrupt ; 0: Disable V interrupt */
      _regs.VInt_enabled= ((data&0x20)!=0);
      if ( _regs.VInt_enabled && _status_aux.VInt )
        MD_cpu_set_auto_vector_int ( 6 );
      else MD_cpu_clear_auto_vector_int ( 6 );
      /* M1 1: DMA Enable ; 0: DMA Disable */
      _regs.DMA_enabled= ((data&0x10)!=0);
      /* 1: V 30 cell mode (PAL mode) ; 0: V 28 cell mode */
      if ( data&0x08 )
        {
          if ( !_status_aux.ispal )
            _warning ( _udata, "VDP: V 30 cell mode sols"
        	       " està disponible en sistemes PAL" );
          else _regs.V30_cell_mode_tmp= MD_TRUE;
        }
      else _regs.V30_cell_mode_tmp= MD_FALSE;
      break;
      
      /* SCROLL "A" PATTERN NAME TABLE BASE ADDRESS */
    case 2:
      _regs.scrollA_name_table_addr= ((MDu16) (data&0x38))<<10;
      break;
      
      /* WINDOW PATTERN NAME TABLE AND BASE ADDRESS */
    case 3:
      _regs.window_name_table_addr= ((MDu16) (data&0x3E))<<10;
      break;
      
      /* SCROLL "B" PATTERN NAME TABLE BASE ADDRESS */
    case 4:
      _regs.scrollB_name_table_addr= ((MDu16) (data&0x07))<<13;
      break;
      
      /* SPRITE ATTRIBUTE TABLE BASE ADDRESS */
    case 5:
      _regs.sprite_attribute_table_addr= ((MDu16) data)<<9;
      update_sprites ();
      break;
      
      /* BACKGROUND COLOR */
    case 7:
      _regs.bgcolor= data&0x3F;
      break;
      
      /* H INTERRUPT REGISTER */
    case 10:
      _regs.H_interrupt_register= data;
      if ( _status_aux.VBlank )
        _hint_counter= _regs.H_interrupt_register;
      break;
      
      /* MODE SET REGISTER No. 3 */
    case 11:
      /*  IE2 1: Enable external interrupt ; 0: Disable external interrupt */
      if ( data&0x80 )
        _warning ( _udata, "VDP: no s'ha implementat la interrupció externa" );
      /* V scroll mode */
      _regs.vsc_mode_is_cell= ((data&0x04)!=0);
      /* H scroll mode */
      switch ( data&0x03 )
        {
        case 0: _regs.hsc_mode= FULL; break;
        case 2: _regs.hsc_mode= CELL; break;
        case 3: _regs.hsc_mode= LINE; break;
        default: _warning ( _udata, "VDP: H scroll mode no vàlid" );
        }
      break;
      
      /* MODE SET REGISTER No. 4 */
    case 12:
      /* 0: Horizontal 32 cell mode ; 1: Horizontal 40 cell mode */
      switch ( data&0x81 )
        {
        case 0x00: _regs.H40_cell_mode_tmp= MD_FALSE; break;
        case 0x81: _regs.H40_cell_mode_tmp= MD_TRUE; break;
        default:
          _warning ( _udata, "VDP: en el Reg #12, RS0 and RS1 deuen"
        	     " ser iguals" );
        }
      /* S/TE 1: Enable SHADOW and HIGHLIGHT; 0: Disable ... */
      _regs.S_TE= ((data&0x08)!=0);
      /* LSM1, LSM0 : Interlace mode setting */
      caux= (data&0x06)>>1;
      if ( caux == 0x02 ) _regs.interlace_mode_tmp= 0;
      else _regs.interlace_mode_tmp= caux;
      break;
      
      /* H SCROLL DATA TABLE BASE ADDRESS */
    case 13:
      _regs.H_scroll_table_addr= ((MDu16) (data&0x3F))<<10;
      break;
      
      /* AUTO INCREMENT DATA */
    case 15: _regs.auto_increment_data= data; break;
      
      /* SCROLL SIZE */
    case 16:
      switch ( (data&0x30)>>4 )
        {
        case 0: _regs.VSZ= 32; break;
        case 1: _regs.VSZ= 64; break;
        case 3: _regs.VSZ= 128; break;
        default: _warning ( _udata, "VDP: VSZ prohibit" );
        }
      switch ( data&0x03 )
        {
        case 0: _regs.HSZ= 32; break;
        case 1: _regs.HSZ= 64; break;
        case 3: _regs.HSZ= 128; break;
        default: _warning ( _udata, "VDP: HSZ prohibit" );
        }
      break;
      
      /* WINDOW H POSITION */
    case 17:
      _regs.isRIGT= ((data&0x80)!=0);
      _regs.WHP= data&0x1F;
      break;
      
      /* WINDOW V POSITION */
    case 18:
      _regs.isDOWN= ((data&0x80)!=0);
      _regs.WVP= data&0x1F;
      break;
      
      /* DMA LENGTH COUNTER LOW */
    case 19:
      _regs.dma_length_counter&= 0xFF00;
      _regs.dma_length_counter|= data;
      break;
      
      /* DMA LENGTH COUNTER HIGH */
    case 20:
      _regs.dma_length_counter&= 0x00FF;
      _regs.dma_length_counter|= (((MDu16) data)<<8);
      break;
      
      /* DMA SOURCE ADDRESS LOW */
    case 21:
      _regs.dma_source_address&= 0x00FFFE00;
      _regs.dma_source_address|= (((MDu32) data)<<1);
      break;
      
      /* DMA SOURCE ADDRESS MID */
    case 22:
      _regs.dma_source_address&= 0x00FE01FF;
      _regs.dma_source_address|= (((MDu32) data)<<9);
      break;
      
      /* DMA SOURCE ADDRESS HIGH */
    case 23:
      _regs.dma_source_address&= 0x0001FFFF;
      if ( data&0x80 ) /* FILL o COPY */
        {
          _regs.dma_mode= (data&0x40) ? DMA_COPY : DMA_FILL;
          _regs.dma_source_address|= (((MDu32) (data&0x3F))<<17);
        }
      else /* MEMORY TO VRAM */
        {
          _regs.dma_mode= DMA_MEM2VRAM;
          _regs.dma_source_address|= (((MDu32) data)<<17);
        }
      break;
      
    default: break;
      
    }
  
} /* end set_register */




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
MD_vdp_clear_interrupt (
        		const int priority 
        		)
{
  
  clock ();
  
  switch ( priority )
    {
    case 4: _status_aux.HInt= MD_FALSE; break;
    case 6: _status_aux.VInt= MD_FALSE; break;
    }
  
} /* end MD_vdp_clear_interrupt */


MD_Bool
MD_vdp_clock (
              const int cc
              )
{
  
  _timing.cc+= cc*_timing.cc2frac;
  /* El VInt comprove inclús quan està desactivat, per a dibuixar frames. */
  if ( _timing.cc >= _timing.cctoVInt ||
       _timing.cc >= _timing.cctoendframe ||
       (_regs.HInt_enabled && _timing.cc >= _timing.cctoHInt) ||
       ((_status_aux.dma_busy || _z80_int_enabled)
        && _timing.cc >= _timing.cctonextline) )
    clock ();
  
  return _status_aux.dma_busy && (_regs.dma_mode==DMA_MEM2VRAM);
  
} /* end MD_vdp_clock */


MD_Word
MD_vdp_data_read (void)
{
  
  MD_Word ret;
  MDu16 aux;
  static const MD_Word _zero= {0};
  
  
  if ( _status_aux.dma_busy ) return _zero;
  
  clock (); /* Per si de cas, però pot ser que no siga necessari. Però
               tampoc molesta molt. */
  
  switch ( _access.code&0x0F )
    {
     
    case 0x00: /* VRAM READ */
      aux= _access.addr&0xFFFE;
      ret.b.v1= _vram[aux];
      ret.b.v0= _vram[aux|0x0001];
      break;
      
    case 0x04: /* VSRAM READ */
      ret.v= _vsram[(_access.addr&0x004F)>>1];
      break;
      
    case 0x08: /* CRAM READ */
      aux= _cram[(_access.addr&0x007F)>>1];
      ret.v=
        ((aux<<3)&0x0E00) | /* B2 B1 B0 */
        ((aux<<2)&0x00E0) | /* G2 G1 G0 */
        ((aux<<1)&0x000E);  /* R2 R1 R0 */
      break;
      
    default:
      ret.v= 0;
      _warning ( _udata, "VDP: codi de control desconegut (read): 0x%02X",
        	 _access.code );
      
    }
  _access.addr+= _regs.auto_increment_data;
  
  return ret;
  
} /* end MD_vdp_data_read */


void
MD_vdp_data_write (
        	   const MD_Word data
        	   )
{
  
  MDu8 dma_code;
  
  
  if ( _status_aux.dma_busy ) return;
  clock ();
  
  _access.second_pass= MD_FALSE; /* Reseteja al escriure */
  
  /* DMA. */
  dma_code= _access.code&0x30;
  if ( dma_code && _regs.DMA_enabled ) /* START DMA */
    {
      if ( _regs.dma_mode == DMA_FILL )
        {
          if ( dma_code != 0x20 )
            {
              _warning ( _udata, "VDP: CD5 CD4 no vàlids per a DMA FILL" );
              return;
            }
          if ( (_access.code&0x0F) != 0x01 /* VRAM */ )
            {
              _warning ( _udata, "VDP: No és pot fer un DMA fill sobre"
        		 " la CRAM o la VSRAM" );
              return;
            }
          _access.code&= 0x0F; /* Quan acabe el DMA el torne a ficar. */
          _status_aux.dma_busy= MD_TRUE;
          _dma.fill_data= data;
          _dma.fill_started= MD_FALSE;
          return;
        }
    }
  
  data_write ( data );
  
} /* end MD_vdp_data_write */


void
MD_vdp_data_write8 (
        	    const MDu8    data,
        	    const MD_Bool isH
        	    )
{
  
  MDu8 dma_code;
  
  
  if ( _status_aux.dma_busy ) return;
  clock ();
  
  _access.second_pass= MD_FALSE; /* Reseteja al escriure */
  
  /* DMA fill. */
  dma_code= _access.code&0x30;
  if ( dma_code && _regs.DMA_enabled ) /* START DMA */
    {
      if ( _regs.dma_mode == DMA_FILL )
        {
          if ( dma_code != 0x20 )
            {
              _warning ( _udata, "VDP: CD5 CD4 no vàlids per a DMA FILL" );
              return;
            }
          if ( (_access.code&0x0F) != 0x01 /* VRAM */ )
            {
              _warning ( _udata, "VDP: No és pot fer un DMA fill sobre"
        		 " la CRAM o la VSRAM" );
              return;
            }
          _access.code&= 0x0F; /* Quan acabe el DMA el torne a ficar. */
          _status_aux.dma_busy= MD_TRUE;
          _dma.fill_data.b.v0= _dma.fill_data.b.v1= data; /* CERT??? */
          _dma.fill_started= MD_FALSE;
          return;
        }
    }
  
  data_write8 ( data, isH );
  
} /* end MD_vdp_data_write8 */


int
MD_vdp_dma_mem2vram_step (void)
{
  
  int scale;
  
  
  // Pas. Atenció a la locura del DMA lag!!!! Vital per al VR.
  if ( _dma_lag && _regs.dma_source_address_tmp <= 0x3FFFFF )
    data_write ( MD_mem_read ( _regs.dma_source_address_tmp-_dma_lag ) );
  else
    data_write ( MD_mem_read ( _regs.dma_source_address_tmp ) );
  //_regs.dma_source_address_tmp= (_regs.dma_source_address_tmp+2)&0x00FFFFFF;
  // Ara ho faig com MAME no sé quina importància té, però pinta que
  // podria ser important.
  _regs.dma_source_address_tmp= (_regs.dma_source_address_tmp+2);
  if ( _regs.dma_source_address_tmp > 0x00FFFFFF )
    _regs.dma_source_address_tmp= 0x00FE0000;
  if ( --_regs.dma_length_counter_tmp == 0 ) finish_dma ();
  
  /* Cicles emprats. */
  /* En VRAM els accessos es fan a nivell de byte, per tant cada
     access són dos bytes. */
  scale= _dma.tovram ? 2 : 1;
  /*scale= ((_access.code&0xF)==1) ? 2 : 1;*/
  /*scale= 1;*/
  if ( !_regs.enabled || _status_aux.VBlank )
    return _timing.dma_mem2vram_cc_vblank*scale;
  else return _timing.dma_mem2vram_cc_display*scale;
  
} /* end MD_vdp_dma_mem2vram_step */


void
MD_vdp_control (
        	const MD_Word data
        	)
{
  
  MDu8 dma_code;
  
  
  if ( _status_aux.dma_busy ) return;
  
  clock ();
  
  /* Accés (2on pas). */
  if ( _access.second_pass )
    {
      _access.code&= 0x03;
      _access.code|= (data.b.v0&0xF0)>>2;
      _access.addr&= 0x3FFF;
      _access.addr|= (data.v<<14);
      _access.second_pass= MD_FALSE;
      dma_code= _access.code&0x30;
      if ( dma_code && _regs.DMA_enabled ) /* START DMA */
        {
          if ( _regs.dma_mode == DMA_MEM2VRAM )
            {
              if ( dma_code != 0x20 )
        	{
        	  _warning ( _udata, "VDP: CD5 CD4 no vàlids"
        		     " per a DMA MEM -> VRAM" );
        	  return;
        	}
              _access.code&= 0x0F; /* Quan acabe el DMA el torne a ficar. */
              _status_aux.dma_busy= MD_TRUE;
              _dma.tovram= (_access.code==0x01);
              _regs.dma_length_counter_tmp= _regs.dma_length_counter;
              _regs.dma_source_address_tmp= _regs.dma_source_address;
            }
          else if ( _regs.dma_mode == DMA_COPY )
            {
              if ( dma_code != 0x30 )
        	{
        	  _warning ( _udata, "VDP: CD5 CD4 no vàlids per a DMA COPY" );
        	  return;
        	}
              if ( (_access.code&0x0F) != 0x01 /* VRAM */ )
        	{
        	  _warning ( _udata, "VDP: No és pot fer un DMA copy sobre"
        		     " la CRAM o la VSRAM" );
        	  return;
        	}
              _access.code&= 0x0F; /* Quan acabe el DMA el torne a ficar. */
              _status_aux.dma_busy= MD_TRUE;
              _regs.dma_length_counter_tmp= _regs.dma_length_counter;
              _regs.dma_source_address_tmp= _regs.dma_source_address;
            }
          else
            {
              if ( dma_code != 0x20 )
        	{
        	  _warning ( _udata, "VDP: CD5 CD4 no vàlids per a DMA FILL" );
        	  return;
        	}
              /* FILL no fa res. Faltaria ficar les dades en el port. */
              _regs.dma_length_counter_tmp= _regs.dma_length_counter;
            }
        }
      return;
    }
  
  /* Registre. */
  if ( (data.v&0xE000) == 0x8000 )
    {
      set_register ( data.b.v1&0x1F, data.b.v0 );
      return;
    }
  
  /* Accés (1er pas). */
  _access.code&= 0xFC;
  _access.code|= data.b.v1>>6;
  _access.addr&= 0xC000;
  _access.addr|= data.v&0x3FFF;
  _access.second_pass= MD_TRUE;
  
} /* end MD_vdp_control */


const MDu16 *
MD_vdp_get_cram (void)
{
  return &(_cram[0]);
} /* end MD_vdp_get_cram */


const MDu8 *
MD_vdp_get_vram (void)
{
  return &(_vram[0]);
} /* end MD_vdp_get_vram */


MD_Word
MD_vdp_HV (void)
{
  
  clock ();
  
  if ( !_regs.HV_counter_stop ) update_HVC ();
  
  return _status_aux.HVC;
  
} /* end MD_vdp_HV */


void
MD_vdp_init (
             const MD_Bool    ispal,
             MD_SResChanged  *sres_changed,
             MD_UpdateScreen *update_screen,
             MD_Warning      *warning,
             void            *udata
             )
{
  
  /* Callbacks. */
  _sres_changed= sres_changed;
  _update_screen= update_screen;
  _warning= warning;
  _udata= udata;
  
  /* Status auxiliar. */
  _status_aux.ispal= ispal;
  
  /* Timing. */
  if ( ispal )
    {
      _timing.lines= PAL_NLINES;
      _timing.frac= PAL_FRAC;
    }
  else
    {
      _timing.lines= NTSC_NLINES;
      _timing.frac= NTSC_FRAC;
    }
  
  MD_vdp_init_state ();
  
} /* end MD_vdp_init */


void
MD_vdp_init_state (void)
{
  
  /* Resolució inicial. */
  _csize.width= 256;
  _csize.ntiles= 32;
  _csize.height= 224;
  res_changed ( _csize.width, _csize.height );
  
  /* Memòria. */
  memset ( _vram, 0, _64K );
  memset ( _cram, 0, 64*sizeof(MDu16) );
  memset ( _vsram, 0, 40*sizeof(MDu16) );
  
  /* Accés. */
  _access.second_pass= MD_FALSE;
  _access.addr= 0x0000;
  _access.code= 0;
  
  /* Registres. */
  _regs.HInt_enabled= MD_FALSE;
  _regs.HV_counter_stop= MD_FALSE;
  _regs.enabled= MD_FALSE;
  _regs.VInt_enabled= MD_FALSE;
  _regs.DMA_enabled= MD_FALSE;
  _regs.V30_cell_mode_tmp= MD_FALSE;
  _regs.V30_cell_mode= MD_FALSE;
  _regs.scrollA_name_table_addr= 0x0000;
  _regs.window_name_table_addr= 0x0000;
  _regs.scrollB_name_table_addr= 0x0000;
  _regs.sprite_attribute_table_addr= 0x0000;
  _regs.bgcolor= 0;
  _regs.H_interrupt_register= 0x00;
  _regs.vsc_mode_is_cell= MD_FALSE;
  _regs.hsc_mode= FULL;
  _regs.H40_cell_mode_tmp= MD_FALSE;
  _regs.H40_cell_mode= MD_FALSE;
  _regs.interlace_mode_tmp= 0;
  _regs.interlace_mode= 0;
  _regs.S_TE= MD_FALSE;
  _regs.H_scroll_table_addr= 0x0000;
  _regs.auto_increment_data= 0;
  _regs.HSZ= _regs.VSZ= 32;
  _regs.isRIGT= MD_FALSE;
  _regs.WHP= 0;
  _regs.isDOWN= MD_FALSE;
  _regs.WVP= 0;
  _regs.dma_length_counter= 0x0000;
  _regs.dma_length_counter_tmp= 0x0000;
  _regs.dma_source_address= 0x00000000;
  _regs.dma_source_address_tmp= 0x00000000;
  _regs.dma_mode= DMA_MEM2VRAM;
  
  /* Timing. */

  _timing.cc= 0;
  _timing.H= _timing.V= 0;

  _timing.lines_before_end_display= _csize.height;
  _timing.linepp_before_hint= LINEPP_BEFORE_END_DISPLAY_H32;
  _timing.linepp_before_end_hblank= LINEPP_BEFORE_END_HBLANK_H32;
  _timing.linepp_before_begin_hblank= LINEPP_BEFORE_BEGIN_HBLANK_H32;
  _timing.linepp_before_end_display= LINEPP_BEFORE_END_DISPLAY_H32;
  recalc_pointsperline ();
  /* NO CAL !!! Ho fa recalc_pointsperline
  _timing.linecc_before_hint= _timing.linepp_before_hint*_timing.pp2cc;
  recalc_cctoendframe ( _timing.V, _timing.H );
  recalc_cctoVInt ( _timing.V, _timing.H );
  recalc_cctoHInt ( _timing.V, _timing.H );
  recalc_cctoendframe ( _timing.V, _timing.H );
  _timing.cctonextline= _timing.ciclesperline;
  */
  _timing.dma_mem2vram_cc_display= DMA_MEM2VRAM_CC_H32_DISPLAY;
  _timing.dma_mem2vram_cc_vblank= DMA_MEM2VRAM_CC_H32_VBLANK;
  
  /* DMA. */
  _dma.fill_bytes_per_line_display= DMA_FILL_BYTES_PER_LINE_H32_DISPLAY;
  _dma.fill_bytes_per_line_vblank= DMA_FILL_BYTES_PER_LINE_H32_VBLANK;
  _dma.copy_bytes_per_line_display= DMA_COPY_BYTES_PER_LINE_H32_DISPLAY;
  _dma.copy_bytes_per_line_vblank= DMA_COPY_BYTES_PER_LINE_H32_VBLANK;
  _dma.fill_data.v= 0x0000;
  _dma.fill_started= MD_FALSE;
  _dma.tovram= MD_TRUE;
  
  /* Status auxiliar. */
  _status_aux.VInt= _status_aux.HInt= MD_FALSE;
  _status_aux.odd_frame= MD_FALSE;
  _status_aux.dma_busy= MD_FALSE;
  _status_aux.fifo_empty= MD_TRUE;
  _status_aux.too_many_sprites= MD_FALSE;
  _status_aux.spr_collision= MD_FALSE;
  _status_aux.HVC.v= 0;
  _status_aux.VBlank= MD_TRUE;
  
  /* HInt counter. */
  _hint_counter= 0x00;
  
  /* Renderitzat. */
  memset ( _render.fb, 0, MAXWIDTH*MAXHEIGHT*sizeof(int) );
  _render.p= &(_render.fb[0]);
  _render.width= _csize.width;
  _render.sc[0].NT_addr= 0x0000;
  _render.sc[0].off= 0;
  _render.sc[0].off_2= 0;
  _render.sc[1].NT_addr= 0x0000;
  _render.sc[1].off= 2;
  _render.sc[1].off_2= 1;
  _render.lines= 0;
  _render.HSZ= _regs.HSZ;
  _render.VSZ= _regs.VSZ;
  _render.hsc_mode= _regs.hsc_mode;
  _render.vsc_mode_is_cell= _regs.vsc_mode_is_cell;
  _render.win_NT_addr= 0x0000;
  _render.isRIGT= _render.isDOWN= MD_FALSE;
  _render.WHP= _render.WVP= 0;
  _render.dot_overflow= MD_FALSE;
  memset ( _render.spr_line, 0, sizeof(_render.spr_line) );
  
  /* Sprites. */
  _sprites.N= 0;
  
  /* Z80 int. */
  _z80_int_enabled= MD_FALSE;
  
} /* end MD_vdp_init_state */


MD_Word
MD_vdp_status (void)
{
  
  MD_Word ret;
  
  
  clock ();
  
  /* Reseteja el control. */
  _access.second_pass= MD_FALSE;
  
  /* Status. */
  ret.v= 0x0000;
  if ( _status_aux.fifo_empty ) ret.v|= 0x0200;
  _status_aux.fifo_empty= MD_TRUE;
  /*if ( FULL ) ret.v|= 0x0100; NO FAIG RES */
  if ( _status_aux.VInt ) ret.v|= 0x0080;
  if ( _status_aux.too_many_sprites )
    {
      _status_aux.too_many_sprites= MD_FALSE;
      ret.v|= 0x0040;
    }
  if ( _status_aux.spr_collision ) ret.v|= 0x0020;
  if ( _status_aux.odd_frame ) ret.v|= 0x0010;
  if ( _status_aux.VBlank ) ret.v|= 0x0008;
  if ( _timing.H < _timing.linepp_before_end_hblank ||
       _timing.H >= _timing.linepp_before_begin_hblank )
    ret.v|= 0x0004;
  if ( _status_aux.dma_busy ) ret.v|= 0x0002;
  if ( _status_aux.ispal ) ret.v|= 0x0001;
  
  return ret;
  
} /* end MD_vdp_status */


int
MD_vdp_save_state (
        	   FILE *f
        	   )
{
  
  int *aux;
  size_t ret;

  
  SAVE ( _access );
  SAVE ( _vram );
  SAVE ( _cram );
  SAVE ( _vsram );
  SAVE ( _regs );
  SAVE ( _csize );
  SAVE ( _timing );
  SAVE ( _dma );
  SAVE ( _status_aux );
  SAVE ( _hint_counter );
  aux= _render.p;
  _render.p= (void *) (_render.p-&(_render.fb[0]));
  ret= fwrite ( &_render, sizeof(_render), 1, f );
  _render.p= aux;
  if ( ret != 1 ) return -1;
  SAVE ( _sprites );
  SAVE ( _sprites_buff );
  SAVE ( _z80_int_enabled );
  
  return 0;
  
} /* end MD_vdp_save_state */


int
MD_vdp_load_state (
        	   FILE *f
        	   )
{

  int i;
  ptrdiff_t diff;
  
  
  LOAD ( _access );
  LOAD ( _vram );
  LOAD ( _cram );
  for ( i= 0; i < 64; ++i )
    {
      CHECK ( (_cram[i]&0x1FF) == _cram[i] );
    }
  LOAD ( _vsram );
  for ( i= 0; i < 40; ++i )
    {
      CHECK ( (_vsram[i]&0x7FF) == _vsram[i] );
    }
  LOAD ( _regs );
  CHECK ( (_regs.scrollA_name_table_addr&0xE000) ==
          _regs.scrollA_name_table_addr );
  CHECK ( (_regs.window_name_table_addr&0xF800) ==
          _regs.window_name_table_addr );
  CHECK ( (_regs.scrollB_name_table_addr&0xE000) ==
          _regs.scrollB_name_table_addr );
  CHECK ( (_regs.sprite_attribute_table_addr&0x1FE00) ==
          _regs.sprite_attribute_table_addr );
  CHECK ( (_regs.bgcolor&0x3F) == _regs.bgcolor );
  CHECK ( (_regs.H_scroll_table_addr&0xFC00) == _regs.H_scroll_table_addr );
  CHECK ( (_regs.WHP&0x1F) == _regs.WHP );
  CHECK ( (_regs.WVP&0x1F) == _regs.WVP );
  LOAD ( _csize );
  CHECK ( _csize.width == 320 || _csize.width == 256 );
  CHECK ( _csize.ntiles == 40 || _csize.ntiles == 32 );
  CHECK ( _csize.height == 240 || _csize.height == 224 );
    CHECK ( (_csize.resw == _csize.width && _csize.resh == _csize.height) ||
          (_csize.resw == (_csize.width*2) &&
           _csize.resh == (_csize.height*2)) );
  res_changed ( _csize.resw, _csize.resh );
  LOAD ( _timing );
  CHECK ( _timing.cc >= 0 );
  CHECK ( _timing.dma_mem2vram_cc_display == DMA_MEM2VRAM_CC_H40_DISPLAY ||
          _timing.dma_mem2vram_cc_display == DMA_MEM2VRAM_CC_H32_DISPLAY );
  CHECK ( _timing.dma_mem2vram_cc_vblank == DMA_MEM2VRAM_CC_H40_VBLANK ||
          _timing.dma_mem2vram_cc_vblank == DMA_MEM2VRAM_CC_H32_VBLANK );
  LOAD ( _dma );
  CHECK ( _dma.fill_bytes_per_line_display ==
          DMA_FILL_BYTES_PER_LINE_H40_DISPLAY ||
          _dma.fill_bytes_per_line_display ==
          DMA_FILL_BYTES_PER_LINE_H32_DISPLAY );
  CHECK ( _dma.fill_bytes_per_line_vblank ==
          DMA_FILL_BYTES_PER_LINE_H40_VBLANK ||
          _dma.fill_bytes_per_line_vblank ==
          DMA_FILL_BYTES_PER_LINE_H32_VBLANK );
  LOAD ( _status_aux );
  LOAD ( _hint_counter );
  LOAD ( _render );
  _render.p= &(_render.fb[0]) + (ptrdiff_t) _render.p;
  diff= _render.p - (&(_render.fb[0]));
  CHECK ( diff <= MAXWIDTH*MAXHEIGHT && diff >= 0 );
  CHECK ( (_render.bgcolor&0x3F) == _render.bgcolor );
  CHECK ( _render.width == _csize.width*2 || _render.width == _csize.width );
  CHECK ( (&(_render.fb[0]) + _render.lines*_render.width) == _render.p );
  for ( i= 0; i < MAXWIDTH; ++i )
    if ( _render.tmp[i] < 0 || _render.tmp[i] > 0x7FF )
      return -1;
  for ( i= 0; i < MAXWIDTH*MAXHEIGHT; ++i )
    if ( _render.fb[i] < 0 || _render.fb[i] > 0x7FF )
      return -1;
  for ( i= 0; i < MAXWIDTH; ++i )
    if ( _render.s_te[i] != NOR &&
         _render.s_te[i] != SHA &&
         _render.s_te[i] != HIG )
      return -1;
  for ( i= 0; i < MAXWIDTH; ++i )
    {
      if ( _render.sc[0].line[i] < 0 || _render.sc[0].line[i] > 0x7FF )
        return -1;
      if ( _render.sc[1].line[i] < 0 || _render.sc[1].line[i] > 0x7FF )
        return -1;
      if ( (_render.spr_line[i].color < 0 ||
            _render.spr_line[i].color > 0x7FF) &&
           _render.spr_line[i].color != SHA_COLOR &&
           _render.spr_line[i].color != HIG_COLOR )
        return -1;
    }
  CHECK ( (_render.sc[0].NT_addr&0xE000) == _render.sc[0].NT_addr );
  CHECK ( (_render.sc[1].NT_addr&0xE000) == _render.sc[1].NT_addr );
  CHECK ( _render.sc[0].off == 0 );
  CHECK ( _render.sc[0].off_2 == 0 );
  CHECK ( _render.sc[1].off == 2 );
  CHECK ( _render.sc[1].off_2 == 1 );
  CHECK ( (_render.Htable&0xFC00) == _render.Htable );
  CHECK ( (_render.win_NT_addr&0xF800) == _render.win_NT_addr );
  CHECK ( (_render.WHP&0x1F) == _render.WHP );
  CHECK ( (_render.WVP&0x1F) == _render.WVP );
  LOAD ( _sprites );
  CHECK ( _sprites.N <= NSPRITES );
  for ( i= 0; i < _sprites.N; ++i )
    {
      if ( (_sprites.v[i].y&0x3FF) != _sprites.v[i].y ) return -1;
      if ( _sprites.v[i].width < 1 || _sprites.v[i].width > 4 ) return -1;
      if ( _sprites.v[i].height < 1 || _sprites.v[i].height > 4 ) return -1;
      if ( (_sprites.v[i].pal&0x30) != _sprites.v[i].pal ) return -1;
      if ( (_sprites.v[i].pat&0x7FF) != _sprites.v[i].pat ) return -1;
      if ( (_sprites.v[i].x&0x1FF) != _sprites.v[i].x ) return -1;
    }
  LOAD ( _sprites_buff );
  LOAD ( _z80_int_enabled );
  
  return 0;
  
} /* end MD_vdp_load_state */


void
MD_vdp_set_dma_lag (
                    const int lag
                    )
{
  _dma_lag= lag;
} // end MD_vdp_set_dma_lag
