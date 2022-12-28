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
 *  mem.c - Implementació del mòdul de memòria.
 *
 */


#include <assert.h>
#include <stdbool.h>
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

#define SSF2_BANK_SIZE (512*1024)




/*************/
/* CONSTANTS */
/*************/

static MD_Word _zero= {0};
static MD_Word _nzero= {0xFFFF};




/*********/
/* TIPUS */
/*********/

typedef struct
{
  MD_Word *mem;
  MDu32    start_w;
  MDu32    end_w;
  MDu32    start_b;
  MDu32    end_b;
  MD_Bool  overlapped;
  MD_Bool  overlapped_enabled;
} sram_t;




/*********/
/* ESTAT */
/*********/

static const MD_Rom *_rom;

// No és necessari guardar açò en l'estat. Indica si s'ha de mapejar
// el xip SVP.
static MD_Bool _map_svp;

static MD_Word _ram[32768]; /* 64K */

/* Callbacks. */
static MD_Warning *_warning;
static MD_MemAccess *_mem_access;
static MD_MemAccess8 *_mem_access8;
static void *_udata;

/* Funcions. */
static MD_Word (*_mem_read) (const MDu32 addr);
static void (*_mem_write) (const MDu32 addr,const MD_Word data);
static MDu8 (*_mem_read8) (const MDu32 addr);
static void (*_mem_write8) (const MDu32 addr,const MDu8 data);

/* SRAM. */
static sram_t _sram;

// SSF2 mapper (sols s'activa per a ROM de més de 4M). Si no està
// activitat no cal desar l'estat.
static struct
{

  bool enabled;
  int  nbanks; // Cada bank es de SSF2_BANK_SIZE (512KB)
  struct
  {
    int            ind;
    const MD_Word *words;
    const MDu8    *bytes;
  } banks[8];
  
} _ssf2_mapper;




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static void
ssf2_mapper_init (
                  const MD_Rom *rom
                  )
{

  int i;

  
  // NOTA!! Assumisc que sols es gasta en ROMs de més de
  // 4MB. Aparentment sols ho gasta el SSF2, i demos que hi han per
  // ahí.
  // NOTA!! Hi han demos que gasten el mapper que el binari no és
  // múltiple de 512K.
  _ssf2_mapper.enabled=
    rom->nbytes>=(4*1024*1024) /*&& rom->nbytes%SSF2_BANK_SIZE==0*/;
  if ( !_ssf2_mapper.enabled ) return;
  
  // Inicialització. M'ho invente.
  _ssf2_mapper.nbanks= rom->nbytes/SSF2_BANK_SIZE;
  assert ( _ssf2_mapper.nbanks >= 8 );
  for ( i= 0; i < 8; ++i )
    {
      _ssf2_mapper.banks[i].ind= i;
      _ssf2_mapper.banks[i].words= &(rom->words[(i*SSF2_BANK_SIZE)/2]);
      _ssf2_mapper.banks[i].bytes= &(rom->bytes[i*SSF2_BANK_SIZE]);
    }
  
} // end ssf2_mapper_init


static MD_Word
ssf2_mapper_read (
                  const MDu32 addr
                  )
{
  return _ssf2_mapper.banks[(addr>>19)&0x7].words[(addr&0x7FFFF)>>1];
} // end ssf2_mapper_read


static MDu8
ssf2_mapper_read8 (
                  const MDu32 addr
                  )
{
  return _ssf2_mapper.banks[(addr>>19)&0x7].bytes[addr&0x7FFFF];
} // end ssf2_mapper_read8


static void
ssf2_mapper_configure (
                       const int  bank,
                       const MDu8 val
                       )
{

  int ind;


  if ( !_ssf2_mapper.enabled ) return;
  
  ind= (int) val;
  if ( ind >= _ssf2_mapper.nbanks )
    {
      _warning ( _udata, "SSF2_MAPPER: s'ha intentat fixar un"
                 " bank (%d) que no està", ind );
      return;
    }
  _ssf2_mapper.banks[bank].ind= ind;
  _ssf2_mapper.banks[bank].words= &(_rom->words[(ind*SSF2_BANK_SIZE)/2]);
  _ssf2_mapper.banks[bank].bytes= &(_rom->bytes[ind*SSF2_BANK_SIZE]);
  
} // end ssf2_mapper_configure


static MD_Word
mem_read (
          const MDu32 addr
          )
{
  
  MDu32 aux;
  Z80u16 a16;
  MD_Word word;
  
  
  aux= (addr&0xFFFFFF)>>1;
  
  /* ROM/SRAM */
  if ( aux < 0x200000 )
    {
      if ( _sram.mem != NULL &&
           (!_sram.overlapped || _sram.overlapped_enabled) &&
           aux < _sram.end_w && aux >= _sram.start_w )
        return _sram.mem[aux-_sram.start_w];
      else if ( _ssf2_mapper.enabled )
        return ssf2_mapper_read ( addr );
      else if ( aux < (MDu32) _rom->nwords )
        return _rom->words[aux];
      else if ( _map_svp )
        return MD_svp_mem_read ( addr );
      else /* Assumisc que de hi haure EEPROM estarà en aquesta àrea. */
        {
          aux= addr&0xFFFFFE;
          word.b.v1= MD_eeprom_read ( aux );
          word.b.v0= MD_eeprom_read ( aux|0x1 );
          return word;
        }
    }
  
  /* Reserved */
  else if ( aux < 0x500000 ) return _zero;
  
  /* Z80 */
  else if ( aux < 0x508000 )
    {
      a16= addr&0xFFFE;
      word.b.v1= Z80_read ( a16 );
      word.b.v0= Z80_read ( a16|0x1 );
      return word;
    }
  
  /* I/O area. */
  else if ( aux < 0x508800 )
    {
      /* NOTA MENTAL! Amb els registres fique word.b.v1= 0, i v0 amb
         el valor del registre. */
      switch ( aux )
        {
        case 0x508000:
          word.b.v1= 0x00;
          word.b.v0= MD_version_no ();
          return word;
        case 0x508001:
          word.b.v1= 0x00;
          word.b.v0= MD_io_data_read_1 ();
          return word;
        case 0x508002:
          word.b.v1= 0x00;
          word.b.v0= MD_io_data_read_2 ();
          return word;
        case 0x508003:
          word.b.v1= 0x00;
          word.b.v0= MD_io_data_read_exp ();
          return word;
        case 0x508004:
          word.b.v1= 0x00;
          word.b.v0= MD_io_control_read_1 ();
          return word;
        case 0x508005:
          word.b.v1= 0x00;
          word.b.v0= MD_io_control_read_2 ();
          return word;
        case 0x508006:
          word.b.v1= 0x00;
          word.b.v0= MD_io_control_read_exp ();
          return word;
        case 0x508007:
          printf("Rw Controller 1 serial transmit\n"); return _nzero;
        case 0x508008:
          printf("Rw Controller 1 serial receive\n"); return _nzero;
        case 0x508009:
          printf("Rw Controller 1 serial control\n"); return _nzero;
        case 0x50800A:
          printf("Rw Controller 2 serial transmit\n"); return _nzero;
        case 0x50800B:
          printf("Rw Controller 2 serial receive\n"); return _nzero;
        case 0x50800C:
          printf("Rw Controller 2 serial control\n"); return _nzero;
        case 0x50800D:
          printf("Rw Expansion port serial transmit\n"); return _nzero;
        case 0x50800E:
          printf("Rw Expansion port serial receive\n"); return _nzero;
        case 0x50800F:
          printf("Rw Expansion port serial control\n"); return _nzero;
        default: return _nzero; /* ACCESS PROHIBITED o desaliniat. */
        }
    }
  
  /* Control area. */
  else if ( aux < 0x600000 )
    {
      switch ( aux )
        {
        case 0x508880:
          word.b.v1= MD_z80_busreq_read ();
          word.b.v0= 0x00;
          return word;
        case 0x50A800 ... 0x50A807: // SVP
          return _map_svp ? MD_svp_port_read ( addr ) : _zero;
        default: return _zero; /* Memory mode, TMMS, Z89 reset o
        			  reserved. */
        }
    }
  
  /* VDP area. */
  else if ( aux < 0x700000 )
    {
      switch ( aux )
        {
        case 0x600000:
        case 0x600001: return MD_vdp_data_read ();
        case 0x600002:
        case 0x600003: return MD_vdp_status ();
        case 0x600004: return MD_vdp_HV ();
        default: return _zero; /* Reserved or PSG. */
        }
    }
  
  /* RAM. WORK RAM mapejada */
  else return _ram[aux&0x7FFF];
  
} /* end mem_read */


static MD_Word
mem_read_trace (
        	const MDu32 addr
        	)
{
  
  MD_Word data;
  
  
  data= mem_read ( addr );
  _mem_access ( MD_READ, addr, data, _udata );
  
  return data;
  
} /* end mem_read_trace */


static MDu8
mem_read8 (
           const MDu32 addr
           )
{
  
  MDu32 aux;
  
  
  aux= addr&0xFFFFFF;
  
  /* ROM/SRAM */
  if ( aux < 0x400000 )
    {
      if ( _sram.mem != NULL &&
           (!_sram.overlapped || _sram.overlapped_enabled) &&
           aux < _sram.end_b && aux >= _sram.start_b )
#ifdef MD_LE
        return ((MDu8 *) _sram.mem)[(aux-_sram.start_b)^0x1];
#else
        return ((MDu8 *) _sram.mem)[(aux-_sram.start_b)];
#endif
      else if ( _ssf2_mapper.enabled )
        return ssf2_mapper_read8 ( addr );
      else if ( aux < _rom->nbytes )
        return _rom->bytes[aux];
      else if ( _map_svp )
        {
          printf ( "Rb MD_svp_mem_read\n" );
          return 0x00;
          /*
          tmpw= MD_svp_mem_read ( addr&0xFFFFFE );
          return addr&0x1 ? tmpw.b.v0 : tmpw.b.v1;
          */
        }
      else /* Assumisc que si hi ha EEPROM estarà ací. */
        return MD_eeprom_read ( aux );
    }
  
  /* Reserved */
  else if ( aux < 0xA00000 ) return 0x00;
  
  /* Z80. */
  else if ( aux < 0xA10000 ) return (MDu8) Z80_read ( (Z80u16) (aux&0xFFFF) );
  
  /* I/O area. */
  else if ( aux < 0xA11000 )
    {
      switch ( aux )
        {
        case 0xA10000: return 0x00;
        case 0xA10001: return MD_version_no ();
        case 0xA10003: return MD_io_data_read_1 ();
        case 0xA10005: return MD_io_data_read_2 ();
        case 0xA10007: return MD_io_data_read_exp ();
        case 0xA10009: return MD_io_control_read_1 ();
        case 0xA1000B: return MD_io_control_read_2 ();
        case 0xA1000D: return MD_io_control_read_exp ();
        case 0xA1000F:
          printf("Rb Controller 1 serial transmit\n"); return 0x00;
        case 0xA10011:
          printf("Rb Controller 1 serial receive\n"); return 0x00;
        case 0xA10013:
          printf("Rb Controller 1 serial control\n"); return 0x00;
        case 0xA10015:
          printf("Rb Controller 2 serial transmit\n"); return 0x00;
        case 0xA10017:
          printf("Rb Controller 2 serial receive\n"); return 0x00;
        case 0xA10019:
          printf("Rb Controller 2 serial control\n"); return 0x00;
        case 0xA1001B:
          printf("Rb Expansion port serial transmit\n"); return 0x00;
        case 0xA1001D:
          printf("Rb Expansion port serial receive\n"); return 0x00;
        case 0xA1001F:
          printf("Rb Expansion port serial control\n"); return 0x00;
        default: return 0x00; /* ACCESS PROHIBITED o pars. */
        }
    }
  
  /* Control area. */
  else if ( aux < 0xC00000 )
    {
      switch ( aux )
        {
        case 0xA11100: return MD_z80_busreq_read ();
        case 0xA15000 ... 0xA1500F:
          if ( _map_svp ) printf ( "Rb MD_svp_port_read\n" );
          return 0x00;
        default: return 0x00; /* Memory mode, TMMS, Z89 reset o
        			 reserved. */
        }
    }
  
  /* VDP area. */
  else if ( aux < 0xE00000 )
    {
      switch ( aux )
        {
        case 0xC00000:
        case 0xC00002: return MD_vdp_data_read ().b.v1;
        case 0xC00001:
        case 0xC00003: return MD_vdp_data_read ().b.v0;
        case 0xC00004:
        case 0xC00006: return MD_vdp_status ().b.v1;
        case 0xC00005:
        case 0xC00007: return MD_vdp_status ().b.v0;
        case 0xC00008: return MD_vdp_HV ().b.v1;
        case 0xC00009: return MD_vdp_HV ().b.v0;
        case 0xC00011: printf("Rb PSG output\n"); return 0x00;
        default: return 0x00; /* Reserved. */
        }
    }
  
  /* RAM. WORK RAM mapejada. */
  else
#ifdef MD_LE
    return ((MDu8 *) _ram)[(aux&0xFFFF)^0x1];
#else
    return ((MDu8 *) _ram)[(aux&0xFFFF)];
#endif
  
} /* end mem_read8 */


static MDu8
mem_read8_trace (
        	const MDu32 addr
        	)
{
  
  MDu8 data;
  
  
  data= mem_read8 ( addr );
  _mem_access8 ( MD_READ, addr, data, _udata );
  
  return data;
  
} /* end mem_read8_trace */


static void
mem_write (
           const MDu32   addr,
           const MD_Word data
           )
{
  
  MDu32 aux;
  Z80u16 a16;
  
  
  aux= (addr&0xFFFFFF)>>1;
  
  /* SRAM */
  if ( aux < 0x200000 )
    {
      if ( _sram.mem != NULL &&
           (!_sram.overlapped || _sram.overlapped_enabled) &&
           aux < _sram.end_w )
        {
          if ( aux >= _sram.start_w ) _sram.mem[aux-_sram.start_w]= data;
        }
      else if ( _map_svp ) MD_svp_mem_write ( addr, data );
    }
  
  /* ROM i Reserved (Si hi ha EEPROM estarà ací). */
  else if ( aux < 0x500000 )
    {
      aux= (addr&0xFFFFFE);
      MD_eeprom_write ( aux, data.b.v1 );
      MD_eeprom_write ( aux|0x1, data.b.v0 );
    }
  
  /* Z80. */
  else if ( aux < 0x508000 )
    {
      a16= (Z80u16) (addr&0xFFFE);
      Z80_write ( a16, (Z80u8) data.b.v1 );
      Z80_write ( a16|0x1, (Z80u8) data.b.v0 );
    }
  
  /* I/O area. */
  else if ( aux < 0x508800 )
    {
      /* NOTA MENTAL! Sols data.b.v0 es gasta. */
      switch ( aux )
        {
        case 0x508001: MD_io_data_write_1 ( data.b.v0 ); break;
        case 0x508002: MD_io_data_write_2 ( data.b.v0 ); break;
        case 0x508003: MD_io_data_write_exp ( data.b.v0 ); break;
        case 0x508004: MD_io_control_write_1 ( data.b.v0 ); break;
        case 0x508005: MD_io_control_write_2 ( data.b.v0 ); break;
        case 0x508006: MD_io_control_write_exp ( data.b.v0 ); break;
        case 0x508007:
          printf("Ww Controller 1 serial transmit\n"); break;
        case 0x508009:
          printf("Ww Controller 1 serial control\n"); break;
        case 0x50800A:
          printf("Ww Controller 2 serial transmit\n"); break;
        case 0x50800C:
          printf("Ww Controller 2 serial control\n"); break;
        case 0x50800D:
          printf("Ww Expansion port serial transmit\n"); break;
        case 0x50800F:
          printf("Ww Expansion port serial control\n"); break;
        default: break; /* Version No, ACCESS PROHIBITED, desaliniat
        		   o serial recives. */
        }
    }
  
  /* Control area. */
  else if ( aux < 0x600000 )
    {
      switch ( aux )
        {
        case 0x508800: printf("Ww Memory mode register\n"); break;
        case 0x508880: MD_z80_busreq_write ( data.b.v1 ); break;
        case 0x508900: MD_z80_reset ( data.b.v1 ); break;
        case 0x509879 ... 0x50987F:
          printf("Ww SSF2 Mapper\n");
          break;
        case 0x50A800 ... 0x50A807: // SVP
          if ( _map_svp ) MD_svp_port_write ( addr, data );
          break;
        default: break; /* TMMS o reserved. */
        }
    }
  
  /* VDP area. */
  else if ( aux < 0x700000 )
    {
      switch ( aux )
        {
        case 0x600000:
        case 0x600001: MD_vdp_data_write ( data ); break;
        case 0x600002:
        case 0x600003: MD_vdp_control ( data ); break;
        default: break; /* Reserved, HV or PSG. */
        }
    }
  
  /* RAM. WORK RAM mapejada. */
  else _ram[aux&0x7FFF]= data;
  
} /* end mem_write */


static void
mem_write_trace (
        	 const MDu32   addr,
        	 const MD_Word data
        	 )
{
  
  mem_write ( addr, data );
  _mem_access ( MD_WRITE, addr, data, _udata );
  
} /* end mem_write_trace */


static void
mem_write8 (
            const MDu32 addr,
            const MDu8  data
            )
{
  
  MDu32 aux;
  
  
  aux= addr&0xFFFFFF;
  
  /* SRAM */
  if ( aux < 0x400000 )
    {
      if ( _sram.mem != NULL &&
           (!_sram.overlapped || _sram.overlapped_enabled) &&
           aux < _sram.end_b )
        {
          if ( aux >= _sram.start_b )
#ifdef MD_LE
            ((MDu8 *) _sram.mem)[(aux-_sram.start_b)^0x1]= data;
#else
          ((MDu8 *) _sram.mem)[(aux-_sram.start_b)]= data;
#endif
        }
      else if ( _map_svp )
        {
          printf ( "Wb MD_svp_mem_write\n" );
        }
    }
  
  /* ROM i Reserved (Si hi ha EEPROM estarà ací) */
  else if ( aux < 0xA00000 )
    MD_eeprom_write ( aux, data );
  
  /* Z80. */
  else if ( aux < 0xA10000 )
    Z80_write ( (Z80u16) (aux&0xFFFF), (Z80u8) data );
  
  /* I/O area. */
  else if ( aux < 0xA11000 )
    {
      switch ( aux )
        {
        case 0xA10003: MD_io_data_write_1 ( data ); break;
        case 0xA10005: MD_io_data_write_2 ( data ); break;
        case 0xA10007: MD_io_data_write_exp ( data ); break;
        case 0xA10009: MD_io_control_write_1 ( data ); break;
        case 0xA1000B: MD_io_control_write_2 ( data ); break;
        case 0xA1000D: MD_io_control_write_exp ( data ); break;
        case 0xA1000F:
          printf("Wb Controller 1 serial transmit\n"); break;
        case 0xA10013:
          printf("Wb Controller 1 serial control\n"); break;
        case 0xA10015:
          printf("Wb Controller 2 serial transmit\n"); break;
        case 0xA10019:
          printf("Wb Controller 2 serial control\n"); break;
        case 0xA1001B:
          printf("Wb Expansion port serial transmit\n"); break;
        case 0xA1001F:
          printf("Wb Expansion port serial control\n"); break;
        default: break; /* ACCESS PROHIBITED, Version No, serial
        			 receives o pars. */
        }
    }
  
  /* Control area. */
  else if ( aux < 0xC00000 )
    {
      switch ( aux )
        {
        case 0xA11000: printf("Wb Memory mode register H\n"); break;
        case 0xA11001: printf("Wb Memory mode register L\n"); break;
        case 0xA11100: MD_z80_busreq_write ( data ); break;
        case 0xA11200: MD_z80_reset ( data ); break;
        case 0xA130F1: _sram.overlapped_enabled= ((data&0x1)==0x1); break;
        case 0xA15000 ... 0xA1500F:
          if ( _map_svp ) printf ( "Wb MD_svp_port_write\n" );
          break;
        case 0xA130F3: ssf2_mapper_configure ( 1, data ); break;
        case 0xA130F5: ssf2_mapper_configure ( 2, data ); break;
        case 0xA130F7: ssf2_mapper_configure ( 3, data ); break;
        case 0xA130F9: ssf2_mapper_configure ( 4, data ); break;
        case 0xA130FB: ssf2_mapper_configure ( 5, data ); break;
        case 0xA130FD: ssf2_mapper_configure ( 6, data ); break;
        case 0xA130FF: ssf2_mapper_configure ( 7, data ); break;
        default: break; /* TMMS o reserved. */
        }
    }
  
  /* VDP area. */
  else if ( aux < 0xE00000 )
    {
      switch ( aux )
        {
        case 0xC00000:
        case 0xC00002: MD_vdp_data_write8 ( data, MD_TRUE ); break;
        case 0xC00001:
        case 0xC00003: MD_vdp_data_write8 ( data, MD_FALSE ); break;
        case 0xC00011: MD_psg_control ( data ); break;
        default: break; /* Reserved, HV or control. */
        }
    }
  
  /* RAM. WORK RAM mapejada. */
  else
#ifdef MD_LE
    ((MDu8 *) _ram)[(aux&0xFFFF)^0x1]= data;
#else
    ((MDu8 *) _ram)[(aux&0xFFFF)]= data;
#endif

} /* end mem_write8 */


static void
mem_write8_trace (
        	  const MDu32 addr,
        	  const MDu8  data
        	  )
{
  
  mem_write8 ( addr, data );
  _mem_access8 ( MD_WRITE, addr, data, _udata );
  
} /* end mem_write8_trace */




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
MD_mem_init (
             const MD_Rom    *rom,
             const MD_Bool    map_svp,
             MD_GetStaticRAM *get_static_ram,
             MD_MemAccess    *mem_access,
             MD_MemAccess8   *mem_access8,
             MD_Warning      *warning,
             void            *udata
             )
{
  
  MD_RomHeader header;
  

  _warning= warning;
  _mem_access= mem_access;
  _mem_access8= mem_access8;
  _udata= udata;
  
  /* ROM i RAM. */
  _rom= rom;

  // SVP
  _map_svp= map_svp;

  /* Funcions. */
  _mem_read= mem_read;
  _mem_read8= mem_read8;
  _mem_write= mem_write;
  _mem_write8= mem_write8;
  
  /* SRAM */
  MD_rom_get_header ( _rom, &header );
  if ( header.sram_flags&MD_SRAMINFO_AVAILABLE )
    {
      _sram.start_b= header.start_sram&0xFFFFFFFE;
      _sram.end_b= (header.end_sram&0xFFFFFFFE)+2;
      _sram.overlapped= (_sram.start_b < _rom->nbytes);
      if ( _sram.end_b <= _sram.start_b ||
           _sram.end_b > 0x400000 ) _sram.mem= NULL;
      else
        {
          _sram.start_w= _sram.start_b>>1;
          _sram.end_w= _sram.end_b>>1;
          _sram.mem= get_static_ram ( _sram.end_w-_sram.start_w, udata );
        }
    }
  else _sram.mem= NULL;
  
  MD_mem_init_state ();
  
} /* end MD_mem_init */


void
MD_mem_init_state (void)
{
  
  memset ( _ram, 0, sizeof(MD_Word)*32768 );
  _sram.overlapped_enabled= MD_FALSE;
  
  // SSF2 mapper.
  ssf2_mapper_init ( _rom );
  
} /* end MD_mem_init_state */


MD_Word
MD_mem_read (
             const MDu32 addr
             )
{
  return _mem_read ( addr );
} /* end MD_mem_read */


MDu8
MD_mem_read8 (
              const MDu32 addr
              )
{
  return _mem_read8 ( addr );
} /* end MD_mem_read8 */


void
MD_mem_set_mode_trace (
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
  if ( _mem_access8 != NULL )
    {
      if ( val )
        {
          _mem_read8= mem_read8_trace;
          _mem_write8= mem_write8_trace;
        }
      else
        {
          _mem_read8= mem_read8;
          _mem_write8= mem_write8;
        }
    }
  
} /* end MD_mem_set_mode_trace */


void
MD_mem_write (
              const MDu32   addr,
              const MD_Word data
              )
{
  _mem_write ( addr, data );
} /* end MD_mem_write */


void
MD_mem_write8 (
               const MDu32 addr,
               const MDu8  data
               )
{
  _mem_write8 ( addr, data );
} /* end MD_mem_write8 */


int
MD_mem_save_state (
        	   FILE *f
        	   )
{

  SAVE ( _rom->nwords );
  SAVE ( _ram );
  SAVE ( _sram );
  if ( _sram.mem != NULL )
    {
      if ( fwrite ( _sram.mem, (_sram.end_w-_sram.start_w)*2, 1, f ) != 1 )
        return -1;
    }
  if ( _ssf2_mapper.enabled )
    {
      SAVE ( _ssf2_mapper );
    }
  
  return 0;
  
} /* end MD_mem_save_state */


int
MD_mem_load_state (
        	   FILE *f
        	   )
{

  int romsize,i;
  sram_t sram_tmp;

  
  LOAD ( romsize );
  CHECK ( romsize == _rom->nwords );
  LOAD ( _ram );
  LOAD ( sram_tmp );
  CHECK ( (sram_tmp.mem == NULL && _sram.mem == NULL) ||
          (sram_tmp.mem != NULL && _sram.mem != NULL) );
  if ( _sram.mem != NULL )
    {
      CHECK ( sram_tmp.start_w == _sram.start_w );
      CHECK ( sram_tmp.end_w == _sram.end_w );
      CHECK ( sram_tmp.start_b == _sram.start_b );
      CHECK ( sram_tmp.end_b == _sram.end_b );
      if ( fread ( _sram.mem, (_sram.end_w-_sram.start_w)*2, 1, f ) != 1 )
        return -1;
    }
  if ( _ssf2_mapper.enabled )
    {
      LOAD ( _ssf2_mapper );
      CHECK ( _ssf2_mapper.enabled );
      CHECK ( _ssf2_mapper.nbanks == (int) (_rom->nbytes/SSF2_BANK_SIZE) );
      CHECK ( _ssf2_mapper.banks[0].ind == 0 );
      for ( i= 0; i < 8; ++i )
        {
          CHECK ( _ssf2_mapper.banks[i].ind < _ssf2_mapper.nbanks );
          _ssf2_mapper.banks[i].words=
            &(_rom->words[(_ssf2_mapper.banks[i].ind*SSF2_BANK_SIZE)/2]);
          _ssf2_mapper.banks[i].bytes=
            &(_rom->bytes[_ssf2_mapper.banks[i].ind*SSF2_BANK_SIZE]);
        }
    }

  return 0;
  
} /* end MD_mem_load_state */


const MD_Word *
MD_mem_get_ram (void)
{
  return &_ram[0];
} /* end MD_mem_get_ram */
