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
 *  rom.c - Implementa funcions relacionades amb la ROM.
 *
 */


#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "MD.h"




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static MDu32
bs2u32 (
        const MDu8 *bytes
        )
{
  
  return
    (((MDu32) bytes[0])<<24) |
    (((MDu32) bytes[1])<<16) |
    (((MDu32) bytes[2])<<8) |
    bytes[3];
  
} /* end bsu32 */




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

MD_Bool
MD_rom_check_checksum (
        	       const MD_Rom *rom
        	       )
{
  
  MDu16 val, checksum;
  int i;
  
  
  val= 0;
  for ( i= 0x100; i < rom->nwords; ++i )
    val+= rom->words[i].v;
  checksum= (((MDu16) rom->bytes[0x18E])<<8) | rom->bytes[0x18F];
  
  return (checksum==val);
  
} /* end MD_rom_checksum */


void
MD_rom_free (
             MD_Rom *rom
             )
{

#ifdef MD_LE
  if ( rom->words != NULL ) free ( rom->words );
#endif
  if ( rom->bytes != NULL ) free ( rom->bytes );
  
} /* end MD_rom_free */


void
MD_rom_get_header (
        	   const MD_Rom *rom,
        	   MD_RomHeader *header
        	   )
{
  
  /* The name of the console. */
  memcpy ( header->console, &(rom->bytes[0x100]), 16 );
  header->console[16]= '\0';
  
  /* Firm name and build date. */
  memcpy ( header->firm_build, &(rom->bytes[0x110]), 16 );
  header->firm_build[16]= '\0';
  
  /* Domestic name. */
  memcpy ( header->dom_name, &(rom->bytes[0x120]), 48 );
  header->dom_name[48]= '\0';
  
  /* International name. */
  memcpy ( header->int_name, &(rom->bytes[0x150]), 48 );
  header->int_name[48]= '\0';
  
  /* Program type and serial number. */
  memcpy ( header->type_snumber, &(rom->bytes[0x180]), 14 );
  header->type_snumber[14]= '\0';
  
  /* Checksum. */
  header->checksum= (((MDu16) rom->bytes[0x18E])<<8) | rom->bytes[0x18F];
  
  /* I/O device support. */
  memcpy ( header->io, &(rom->bytes[0x190]), 16 );
  header->io[16]= '\0';
  
  /* Start of the ROM. */
  header->start= bs2u32 ( &(rom->bytes[0x1A0]) );
  
  /* End of the ROM. */
  header->end= bs2u32 ( &(rom->bytes[0x1A4]) );
  
  /* Start of RAM. */
  header->start_ram= bs2u32 ( &(rom->bytes[0x1A8]) );
  
  /* End of RAM. */
  header->end_ram= bs2u32 ( &(rom->bytes[0x1AC]) );
  
  /* Backup RAM ID. */
  memcpy ( header->sramid, &(rom->bytes[0x1B0]), 4 );
  header->sramid[4]= '\0';
  header->sram_flags= 0;
  if ( header->sramid[0]=='R' && header->sramid[1]=='A' &&
       (header->sramid[2]&0xA7)==0xA0 && header->sramid[3]==0x20 )
    {
      header->sram_flags|= MD_SRAMINFO_AVAILABLE;
      if ( header->sramid[2]&0x40 )
        header->sram_flags|= MD_SRAMINFO_ISFORBACKUP;
      switch ( (header->sramid[2]&0x18)>>3 )
        {
        case 0:
          header->sram_flags|= MD_SRAMINFO_ODDBYTES|MD_SRAMINFO_EVENBYTES;
          break;
        case 2:
          header->sram_flags|= MD_SRAMINFO_EVENBYTES;
          break;
        case 3:
          header->sram_flags|= MD_SRAMINFO_ODDBYTES;
          break;
        }
    }
  
  /* Start address of backup RAM. */
  header->start_sram= 
    (header->sram_flags&MD_SRAMINFO_AVAILABLE) ?
    bs2u32 ( &(rom->bytes[0x1B4]) ) : 0;
  
  /* End address of backup RAM. */
  header->end_sram= 
    (header->sram_flags&MD_SRAMINFO_AVAILABLE) ?
    bs2u32 ( &(rom->bytes[0x1B8]) ) : 0;
  
  /* Modem support. */
  memcpy ( header->modem, &(rom->bytes[0x1BC]), 12 );
  header->modem[12]= '\0';
  
  /* Notes. */
  memcpy ( header->notes, &(rom->bytes[0x1C8]), 40 );
  header->notes[40]= '\0';
  
  /* Country codes. */
  memcpy ( header->ccodes, &(rom->bytes[0x1F0]), 16 );
  header->ccodes[16]= '\0';
  
} /* end MD_rom_get_header */


MD_Error
MD_rom_prepare (
        	MD_Rom *rom
        	)
{
  
  int i;
  const MDu8 *b;
  
  
  /* Paraules. */
#ifdef MD_LE
  rom->words= (MD_Word *) malloc ( sizeof(MD_Word)*rom->nwords );
  if ( rom->words == NULL ) return MD_EMEM;
  for ( i= 0, b= rom->bytes; i < rom->nwords; ++i )
    {
      rom->words[i].b.v1= *(b++);
      rom->words[i].b.v0= *(b++);
    }
#else /* MD_BE */
  rom->words= (MD_Word *) rom->bytes;
#endif
  rom->nbytes= (MDu32) (rom->nwords*2);
  
  return MD_NOERROR;
  
} /* end MD_rom_prepare */
