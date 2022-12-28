/*
 * Copyright 2019-2022 Adrià Giménez Pastor.
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
 *  svp.c - Implementació del mòdul 'svp'.
 *
 */
/*
 * NOTA!! M'estic basant en aquesta documentació:
 *
 *  https://notaz.gp2x.de/docs/svpdoc.txt
 *
 * NOTA2!!! La documentació anterior no està malament, però té molts
 * forats, la que està realment bé és la implementació de MAME, ahí he
 * pogut entendre moltes coses que no s'expliquen i són fonamentals
 * perquè funcione correctament.
 *
 *    src/devices/bus/megadrive/svp.cpp 
 *    src/devices/cpu/ssp1601/ssp1601.cpp
 *
 * L'objectiu no és fer una implementació perfecta, l'objectiu es
 * aconsseguir que funcione el Virtua Racing. Per exemple, el xip
 * genera interrupcions, però no és necessari implementar-les per fer
 * funcionar Virtua Racing, per tant no ho faré.
 */


#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
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

#define ST_ST5 0x0020
#define ST_ST6 0x0040
#define ST_L   0x1000
#define ST_Z   0x2000
#define ST_OV  0x4000
#define ST_N   0x8000

#define DRAM_SIZE (64*1024) /* En words */

#define IRAM_SIZE (1024) /* En words */

#define RAM_SIZE (256) /* En words */

#define EXT_ADDR_MASK 0x1FFFFF

// NOTA!!!!!!!! Lo de 0 -> FF me ho he inventat.
#define RPL_MASK                                                        \
  ((uint8_t) ((_regs.ST&0x7) ? ((1<<((int) (_regs.ST&0x7)))-1) : 0xFF))

#define CHECKC(CHECKSUM,SERIAL)                                 \
  (header.checksum == CHECKSUM && !strcmp ( SERIAL, header.type_snumber ))

#define SET_ZN_FLAGS(VAL)                          \
  {                                                \
    _regs.ST&= ~(ST_Z|ST_N);                       \
    if ( (VAL)&0x80000000 ) _regs.ST|= ST_N;       \
    if ( (VAL) == 0 ) _regs.ST|= ST_Z;             \
  }

// NOTA!! Sols faig clear però no actualize mai OV i L
#define SET_ZNOVL_FLAGS(VAL)                       \
  {                                                \
    _regs.ST&= ~(ST_Z|ST_N|ST_OV|ST_L);            \
    if ( (VAL)&0x80000000 ) _regs.ST|= ST_N;       \
    if ( (VAL) == 0 ) _regs.ST|= ST_Z;             \
  }

#define RAMj(INST) (_ram[((INST)>>8)&0x1])

#define RIJ(INST) (_regs.rX[((INST)>>8)&0x1][(INST)&0x3])




/*********/
/* TIPUS */
/*********/

typedef struct
{

  // For read.
  uint16_t r_mode;
  uint32_t r_addr;
  int      r_auto_inc;
  bool     r_neg_inc;
  bool     r_special_inc;
  //bool     r_over_write; // unknown when reading (not used).

  // For writing.
  uint16_t w_mode;
  uint32_t w_addr;
  int      w_auto_inc;
  bool     w_neg_inc;
  bool     w_special_inc;
  bool     w_over_write; // unknown when reading (not used).
  
} pgm_t;




/*********/
/* ESTAT */
/*********/

// Callbacks.
static MD_Warning *_warning;
static MD_CPUStepSVP *_cpu_step_svp;
static void *_udata;

// Rom.
static const MD_Rom *_rom;

// Indica que la ROM té SVP.
static MD_Bool _enabled;

// DRAM.
static uint16_t _dram[DRAM_SIZE];

// IRAM
static uint16_t _iram[IRAM_SIZE];

// RAM (fast ram)
static uint16_t _ram[2][RAM_SIZE];

// Registres
static struct
{

  // Registres status.
  uint16_t XST;
  uint16_t XST_Status;

  // Registres generals.
  uint16_t X;
  uint16_t Y;
  uint32_t A;
  uint16_t ST;
  struct
  {
    uint16_t v[6];
    int      N;
  }        STACK;
  uint16_t PC;
  uint32_t P;

  // Registres externs.
  pgm_t PM0;
  pgm_t PM1;
  pgm_t PM2;
  pgm_t PM3;
  pgm_t PM4;
  struct
  {
    enum
      {
       WAIT_ADDR,
       WAIT_MODE,
       SET
    }        state;
    uint16_t addr_word;
    uint16_t mode_word;
  }     PMC;
  // AL -> No cal estat.

  // Registers punter.
  uint8_t rX[2][4];
  
} _regs;




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

// Si és una ROM amb SVP activa el mòdul.
static void
check_rom (
           const MD_Rom *rom
           )
{

  MD_RomHeader header;


  MD_rom_get_header ( rom, &header );

  _enabled=
    // Virtua Racing (E) [!].gen
    CHECKC ( 0x345d, "GM MK-1229 -00" ) ||
    // Virtua Racing (J) [!].gen
    CHECKC ( 0x3b99, "GM G-7001  -00" ) ||
    // Virtua Racing (U) [!].gen
    CHECKC ( 0x95ab, "GM MK-1229 -00" ) ||
    // Virtua Racing (E) [a1].gen
    CHECKC ( 0x74fb, "GM MK-1229 -00" )
    ;
  
} // end check_rom


static void
update_P (void)
{

  int32_t x,y;

  
  x= (int16_t) _regs.X;
  y= (int16_t) _regs.Y;
  _regs.P= (uint32_t) (x*y*2);
  
} // end update_P


// Llig de la memòria de programa.
static uint16_t
read (
      const uint16_t addr
      )
{
  
  if ( addr < 0x400 ) return _iram[addr];
  else
    {
      if ( (int) ((uint32_t) addr) <  _rom->nwords )
        return (uint16_t) _rom->words[addr].v;
      _warning ( _udata, "[SVP] read --> addr >= _rom->nwords" );
      return 0;
    }
  
} // end read


static uint16_t
read_ext (
          const uint32_t addr // Està en words!!!!
          )
{

  // ROM
  if ( addr < 0x100000 )
    {
      
      if ( ((int) addr) <  _rom->nwords ) return (uint16_t) _rom->words[addr].v;
      _warning ( _udata, "SVP: read_ext -> llegint fora de la ROM" );
      return 0;
    }

  // unused (1): reads seem to return data from internal bus (last
  // word read by SSP160x). Writes probably have no effect.
  // Passe de fer res.
  else if ( addr < 0x180000 )
    return 0xFFFF;

  // DRAM
  else if ( addr < 0x190000 )
    return _dram[addr&0xFFFF];

  // ¿¿3 mirrors i unused(1)???
  else if ( addr < 0x1C8000 )
    return 0xFFFF;

  // IRAM
  else if ( addr < 0x1C8400 )
    return _iram[addr&0x3FF];

  // cell arrange, unused (2), ¿¿status/control???
  else return 0x0000;
  
} // end read_ext


static void
write_ext (
           const uint32_t addr, // Està en words!!!!
           const uint16_t data
           )
{
  
  // ROM i unused (1)
  if ( addr < 0x180000 ) return;
  
  // DRAM
  else if ( addr < 0x190000 )
    _dram[addr&0xFFFF]= data;

  // ¿¿3 mirrors i unused(1)???
  else if ( addr < 0x1C8000 ) return;
  
  // IRAM
  else if ( addr < 0x1C8400 )
    _iram[addr&0x3FF]= data;
  
  // cell arrange, unused (2), ¿¿status/control???
  else return;
  
} // end write_ext


static uint16_t
read_pgm (
          pgm_t *reg
          )
{

  uint16_t ret;

  
  ret= read_ext ( reg->r_addr );
  if ( reg->r_auto_inc != 0 )
    {
      assert ( !reg->r_special_inc );
      reg->r_addr+= reg->r_neg_inc ? -reg->r_auto_inc : reg->r_auto_inc;
      reg->r_addr&= EXT_ADDR_MASK;
    }
  else if ( reg->r_special_inc )
    {
      assert ( !reg->r_neg_inc );
      reg->r_addr+= (reg->r_addr&0x1) ? 31 : 1;
      reg->r_addr&= EXT_ADDR_MASK;
    }
  _regs.PMC.addr_word= (uint16_t) (reg->r_addr&0xFFFF);
  _regs.PMC.mode_word=
    (reg->r_mode&0xFFE0) |
    ((uint16_t) ((reg->r_addr>>16)&0x1F));
  
  return ret;
  
} // end read_pgm


static uint16_t
read_reg (
          const int reg
          )
{

  uint16_t ret;

  
  switch ( reg )
    {
    case 0: // -
      return 0xFFFF;
    case 1: // X
      return _regs.X;
    case 2: // Y
      return _regs.Y;
    case 3: // A
      return (uint16_t) (_regs.A>>16);
    case 4: // ST
      return _regs.ST;
    case 5: // STACK
      if ( _regs.STACK.N == 0 )
        {
          _warning ( _udata,
                     "SVP: s'ha intentat accedir a STACK quan estava buit" );
          return 0;
        }
      else return _regs.STACK.v[--_regs.STACK.N];
    case 6: // PC
      return _regs.PC;
    case 7: // P, NOTA!!!! No queda clar que fer amb aquest registre,
            // faré el mateix que amb A.
      update_P ();
      return (uint16_t) (_regs.P>>16);
    case 8: // PM0
      if ( _regs.PMC.state == WAIT_MODE ) _regs.PMC.state= WAIT_ADDR;
      if ( _regs.ST&(ST_ST5|ST_ST6) ) // Programable
        ret= read_pgm ( &_regs.PM0 );
      else
        {
          ret= _regs.XST_Status;
          _regs.XST_Status&= ~0x2; // cleared on PM0 read by SSP160x)
        }
      return ret;
    case 9: // PM1
      if ( _regs.PMC.state == WAIT_MODE ) _regs.PMC.state= WAIT_ADDR;
      if ( _regs.ST&(ST_ST5|ST_ST6) ) // Programable
        ret= read_pgm ( &_regs.PM1 );
      else
        {
          ret= 0;
          _warning ( _udata,
                     "SVP: S'ha intentat accedir a PM1 sense programar-lo" );
        }
      return ret;
    case 10: // PM2
      if ( _regs.PMC.state == WAIT_MODE ) _regs.PMC.state= WAIT_ADDR;
      if ( _regs.ST&(ST_ST5|ST_ST6) ) // Programable
        ret= read_pgm ( &_regs.PM2 );
      else
        {
          ret= 0;
          _warning ( _udata,
                     "SVP: S'ha intentat accedir a PM2 sense programar-lo" );
        }
      return ret;
    case 11: // PM3
      if ( _regs.PMC.state == WAIT_MODE ) _regs.PMC.state= WAIT_ADDR;
      if ( _regs.ST&(ST_ST5|ST_ST6) ) // Programable
        ret= read_pgm ( &_regs.PM3 );
      else ret= _regs.XST;
      return ret;
    case 12: // PM4
      if ( _regs.PMC.state == WAIT_MODE ) _regs.PMC.state= WAIT_ADDR;
      return read_pgm ( &_regs.PM4 );
    case 13: // PM5
      if ( _regs.PMC.state == WAIT_MODE ) _regs.PMC.state= WAIT_ADDR;
      printf ( "SVP, EXT5 not implemented\n" );
      return 0;
    case 14: // PMC
      ret= _regs.PMC.addr_word;
      if ( _regs.PMC.state == WAIT_MODE )
        {
          //ret= ((ret<<4)&0xFFF0) | ((ret&0xF000)>>12);
          ret= ((ret<<4)&0xFFF0) | ((ret>>4)&0xF); //<-- mame????
          _regs.PMC.state= SET;
        }
      else _regs.PMC.state= WAIT_MODE; // WAIT_ADDR or SET
      return ret;
    case 15: // AL
      _regs.PMC.state= WAIT_ADDR;
      return (uint16_t) (_regs.A&0xFFFF);
    default:
      _warning ( _udata, "SVP: S'ha intentat llegir del registre %d", reg );
      return 0;
    }
  
} // end read_reg


// Per a operacions amb la ALU
static uint32_t
read_reg_alu (
              const int reg
              )
{

  switch ( reg )
    {
    case 3: // A
      return _regs.A;
    case 7: // P
      update_P ();
      return _regs.P;
    default:
      return ((uint32_t) read_reg ( reg ))<<16;
    }
  
} // end read_reg_alu


static void
write_pgm (
           pgm_t          *reg,
           const uint16_t  data
           )
{

  uint16_t tmp,tmp2;

  
  // Escriu.
  if ( reg->w_over_write )
    {
      tmp= read_ext ( reg->w_addr );
      tmp2=  (data&0x000F) ? (data&0x000F) : (tmp&0x000F);
      tmp2|= (data&0x00F0) ? (data&0x00F0) : (tmp&0x00F0);
      tmp2|= (data&0x0F00) ? (data&0x0F00) : (tmp&0x0F00);
      tmp2|= (data&0xF000) ? (data&0xF000) : (tmp&0xF000);
      write_ext ( reg->w_addr, tmp2 );
    }
  else write_ext ( reg->w_addr, data );

  // Actualitza adreça.
  if ( reg->w_auto_inc != 0 )
    {
      assert ( !reg->w_special_inc );
      reg->w_addr+= reg->w_neg_inc ? -reg->w_auto_inc : reg->w_auto_inc;
      reg->w_addr&= EXT_ADDR_MASK;
    }
  else if ( reg->w_special_inc )
    {
      assert ( !reg->r_neg_inc );
      reg->w_addr+= (reg->w_addr&0x1) ? 31 : 1;
      reg->w_addr&= EXT_ADDR_MASK;
    }
  _regs.PMC.addr_word= (uint16_t) (reg->w_addr&0xFFFF);
  _regs.PMC.mode_word=
    (reg->w_mode&0xFFE0) |
    ((uint16_t) ((reg->w_addr>>16)&0x1F));
  
} // end write_pgm


static void
write_reg (
           const int      reg,
           const uint16_t data
           )
{

  switch ( reg )
    {
    case 0: // -
      break;
    case 1: // X
      _regs.X= data;
      break;
    case 2: // Y
      _regs.Y= data;
      break;
    case 3: // A
      _regs.A= (_regs.A&0xFFFF) | (((uint32_t) data)<<16);
      break;
    case 4: // ST
      _regs.ST= data;
      break;
    case 5: // STACK
      if ( _regs.STACK.N == 6 )
        _warning ( _udata, "SVP: no es poden escriure més valors en STACK" );
      else
        _regs.STACK.v[_regs.STACK.N++]= data;
      break;
    case 6: // PC
      _regs.PC= data;
      break;
    case 7: // P, NOTA!!!! No queda clar que fer amb aquest registre
      _warning ( _udata, "SVP:  s'ha intentat escriure directament P" );
      //update_P ();
      //_regs.P= (_regs.P&0xFFFF) | (((uint32_t) data)<<16);
      break;
    case 8: // PM0
      if ( _regs.PMC.state == WAIT_MODE ) _regs.PMC.state= WAIT_ADDR;
      if ( _regs.ST&(ST_ST5|ST_ST6) ) // Programable
        write_pgm ( &_regs.PM0, data );
      else _regs.XST_Status= data;
      break;
    case 9: // PM1
      if ( _regs.PMC.state == WAIT_MODE ) _regs.PMC.state= WAIT_ADDR;
      if ( _regs.ST&(ST_ST5|ST_ST6) ) // Programable
        write_pgm ( &_regs.PM1, data );
      else
        _warning ( _udata,
                   "SVP: S'ha intentat escriure en PM1 sense programar-lo" );
      break;
    case 10: // PM2
      if ( _regs.PMC.state == WAIT_MODE ) _regs.PMC.state= WAIT_ADDR;
      if ( _regs.ST&(ST_ST5|ST_ST6) ) // Programable
        write_pgm ( &_regs.PM2, data );
      else
        _warning ( _udata,
                   "SVP: S'ha intentat escriure en PM2 sense programar-lo" );
      break;
    case 11: // PM3
      if ( _regs.PMC.state == WAIT_MODE ) _regs.PMC.state= WAIT_ADDR;
      if ( _regs.ST&(ST_ST5|ST_ST6) ) // Programable
        write_pgm ( &_regs.PM3, data );
      else
        {
          _regs.XST= data;
          _regs.XST_Status|= 0x1; // set, when SSP160x has written
                                  // something to XST
        }
      break;
    case 12: // PM4
      if ( _regs.PMC.state == WAIT_MODE ) _regs.PMC.state= WAIT_ADDR;
      write_pgm ( &_regs.PM4, data );
      break;
    case 13: // PM5
      if ( _regs.PMC.state == WAIT_MODE ) _regs.PMC.state= WAIT_ADDR;
      printf ( "SVP, EXT5 not implemented for W\n" );
      break;
    case 14: // PMC
      if ( _regs.PMC.state == WAIT_MODE )
        {
          _regs.PMC.mode_word= data;
          _regs.PMC.state= SET;
        }
      else // SET or WAIT_ADDR
        {
          _regs.PMC.addr_word= data;
          _regs.PMC.state= WAIT_MODE;
        }
      break;
    case 15: // AL
      _regs.A= (_regs.A&0xFFFF0000) | ((uint32_t) data);
      break;
    default:
      _warning ( _udata, "SVP: S'ha intentat escriure en el registre %d", reg );
    }
  
} // end write_reg


static void
conf_pgm_r (
            const int reg
            )
{

  pgm_t *pgm;

  
  assert ( reg >= 8 );
  
  _regs.PMC.state= WAIT_ADDR;
  
  // Selecciona registre
  pgm= NULL; // CALLA!!!!
  switch ( reg )
    {
    case 8:  pgm= &_regs.PM0; break;
    case 9:  pgm= &_regs.PM1; break;
    case 10: pgm= &_regs.PM2; break;
    case 11: pgm= &_regs.PM3; break;
    case 12: pgm= &_regs.PM4; break;
    case 13: printf ( "SVP, EXT5 not implemented (conf_pmg_r)\n" ); return;
    case 14:
      //if ( _regs.PMC.state == WAIT_MODE ) _regs.PMC.state= SET;
      //else                                _regs.PMC.state= WAIT_ADDR;
      printf ( "SVP, conf_pgm_r:14, no es deuria veure açò\n" );
      return;
    case 15:
      return;
    }
  assert ( pgm != NULL );
  
  // Programa
  pgm->r_mode= _regs.PMC.mode_word;
  pgm->r_addr=
    (((uint32_t) _regs.PMC.mode_word&0x1F)<<16) |
    ((uint32_t) _regs.PMC.addr_word);
  switch ( (_regs.PMC.mode_word>>11)&0x7 )
    {
    case 1: pgm->r_auto_inc= 1; break;
    case 2: pgm->r_auto_inc= 2; break;
    case 3: pgm->r_auto_inc= 4; break;
    case 4: pgm->r_auto_inc= 8; break;
    case 5: pgm->r_auto_inc= 16; break;
    case 6: pgm->r_auto_inc= 32; break;
    case 7: pgm->r_auto_inc= 128; break;
    default:
    case 0: pgm->r_auto_inc= 0; break;
    }
  pgm->r_neg_inc= (_regs.PMC.mode_word&0x8000)!=0;
  pgm->r_special_inc= (_regs.PMC.mode_word&0x4000)!=0;
  
} // end conf_pmg_r


static void
conf_pgm_w (
            const int reg
            )
{

  pgm_t *pgm;

  
  assert ( reg >= 8 );
  
  _regs.PMC.state= WAIT_ADDR;
  
  // Selecciona registre
  pgm= NULL; // CALLA !!!
  switch ( reg )
    {
    case 8:  pgm= &_regs.PM0; break;
    case 9:  pgm= &_regs.PM1; break;
    case 10: pgm= &_regs.PM2; break;
    case 11: pgm= &_regs.PM3; break;
    case 12: pgm= &_regs.PM4; break;
    case 13: printf ( "SVP, EXT5 not implemented (conf_pmg_r)\n" ); return;
    case 14:
      printf ( "SVP, conf_pgm_w:14, no es deuria veure açò\n" );
      //_warning ( _udata, "SVP: s'ha intentat programar PMC per a lectura" );
      return;
    case 15:
      return;
    }

  // Programa
  pgm->w_mode= _regs.PMC.mode_word;
  pgm->w_addr=
    (((uint32_t) _regs.PMC.mode_word&0x1F)<<16) |
    ((uint32_t) _regs.PMC.addr_word);
  switch ( (_regs.PMC.mode_word>>11)&0x7 )
    {
    case 1: pgm->w_auto_inc= 1; break;
    case 2: pgm->w_auto_inc= 2; break;
    case 3: pgm->w_auto_inc= 4; break;
    case 4: pgm->w_auto_inc= 8; break;
    case 5: pgm->w_auto_inc= 16; break;
    case 6: pgm->w_auto_inc= 32; break;
    case 7: pgm->w_auto_inc= 128; break;
    default:
    case 0: pgm->w_auto_inc= 0; break;
    }
  pgm->w_neg_inc= (_regs.PMC.mode_word&0x8000)!=0;
  pgm->w_special_inc= (_regs.PMC.mode_word&0x4000)!=0;
  pgm->w_over_write= (_regs.PMC.mode_word&0x0400)!=0;
  
} // end conf_pmg_w


static uint8_t
pr_modif_read (
               const uint16_t inst
               )
{

  // NOTA!!! El funcionament concret del
  // modulo_increment/modulo_decrement me l'he inventat jo!!!!
  
  uint8_t pp,mm,ret;
  uint8_t *reg;
  
  
  // Si pp == 0x3 torna mm
  pp= inst&0x3;
  mm= (inst>>2)&0x3;
  if ( pp == 0x3 ) return mm;

  // Accedeix al registre
  reg= &(_regs.rX[(inst>>8)&0x1][pp]);
  switch ( mm )
    {
    case 0: return *reg;
    case 1: return (*reg)++;
    case 2:
      ret= *reg;
      *reg= (uint8_t) (((ret)&(~RPL_MASK)) | ((ret-1)&RPL_MASK));
      return ret;
    case 3:
      ret= *reg;
      *reg= (uint8_t) (((ret)&(~RPL_MASK)) | ((ret+1)&RPL_MASK));
      return ret;
    default: return 0; // CALLA!!!!
    }
  
} // end pr_modif_read


static bool
check_cond (
            const uint16_t inst
            )
{

  switch ( (inst>>4)&0xF )
    {
    case 0: return true;
    case 5: return (((_regs.ST&ST_Z)!=0) == ((inst&0x0100)!=0));
    case 7: return (((_regs.ST&ST_N)!=0) == ((inst&0x0100)!=0));
    default:
      _warning ( _udata,
                 "SVP: condició no suportada en instrucció %04X",
                 inst );
      return false;
    }
  
} // end check_cond


static void
sub_s (
       const uint16_t inst
       )
{

  uint32_t s;

  
  s= read_reg_alu ( inst&0xF );
  _regs.A-= s;
  SET_ZNOVL_FLAGS ( _regs.A );
  
} // end sub_s


static void
sub_Pri (
         const uint16_t inst
         )
{

  uint32_t v;


  v= ((uint32_t) RAMj(inst)[pr_modif_read ( inst )])<<16;
  _regs.A-= v;
  SET_ZNOVL_FLAGS ( _regs.A );
  
} // end sub_Pri


static void
sub_adr (
         const uint16_t inst
         )
{

  uint32_t v;

  
  v= ((uint32_t) RAMj(inst)[inst&0xFF])<<16;
  _regs.A-= v;
  SET_ZNOVL_FLAGS ( _regs.A );
  
} // end sub_adr


static void
sub_imm (void)
{

  uint32_t v;


  v= ((uint32_t) read ( _regs.PC++ ) )<<16;
  _regs.A-= v;
  SET_ZNOVL_FLAGS ( _regs.A );
  
} // end sub_imm


static void
sub_PPri (
          const uint16_t inst
          )
{

  uint32_t v;

  
  v= ((uint32_t) read ( (RAMj(inst)[pr_modif_read ( inst )])++ ))<<16;
  _regs.A-= v;
  SET_ZNOVL_FLAGS ( _regs.A );
  
} // end sub_PPri


static void
sub_ri (
        const uint16_t inst
        )
{

  uint32_t v;


  v= ((uint32_t) RIJ(inst))<<16;
  _regs.A-= v;
  SET_ZNOVL_FLAGS ( _regs.A );
  
} // end sub_ri


static void
sub_simm (
          const uint16_t inst
          )
{

  uint32_t v;


  v= ((uint32_t) (inst&0xFF))<<16;
  _regs.A-= v;
  SET_ZNOVL_FLAGS ( _regs.A );
  
} // end sub_simm


static void
cmp_s (
       const uint16_t inst
       )
{

  uint32_t s,tmp;

  
  s= read_reg_alu ( inst&0xF );
  tmp= _regs.A-s;
  SET_ZNOVL_FLAGS ( tmp );
  
} // end cmp_s


static void
cmp_Pri (
         const uint16_t inst
         )
{

  uint32_t v,tmp;


  v= ((uint32_t) RAMj(inst)[pr_modif_read ( inst )])<<16;
  tmp= _regs.A-v;
  SET_ZNOVL_FLAGS ( tmp );
  
} // end cmp_Pri


static void
cmp_adr (
         const uint16_t inst
         )
{

  uint32_t v,tmp;


  v= ((uint32_t) RAMj(inst)[inst&0xFF])<<16;
  tmp= _regs.A-v;
  SET_ZNOVL_FLAGS ( tmp );
  
} // end cmp_adr


static void
cmp_imm (void)
{
  
  uint32_t v,tmp;


  v= ((uint32_t) read ( _regs.PC++ ) )<<16;
  tmp= _regs.A-v;
  SET_ZNOVL_FLAGS ( tmp );
  
} // end cmp_imm


static void
cmp_PPri (
          const uint16_t inst
          )
{

  uint32_t v,tmp;


  v= ((uint32_t) read ( (RAMj(inst)[pr_modif_read ( inst )])++ ))<<16;
  tmp= _regs.A-v;
  SET_ZNOVL_FLAGS ( tmp );
  
} // end cmp_PPri


static void
cmp_ri (
        const uint16_t inst
        )
{

  uint32_t v,tmp;


  v= ((uint32_t) RIJ(inst))<<16;
  tmp= _regs.A-v;
  SET_ZNOVL_FLAGS ( tmp );
  
} // end cmp_ri


static void
cmp_simm (
          const uint16_t inst
          )
{

  uint32_t v,tmp;


  v= ((uint32_t) (inst&0xFF))<<16;
  tmp= _regs.A-v;
  SET_ZNOVL_FLAGS ( tmp );
  
} // end cmp_simm


static void
add_s (
       const uint16_t inst
       )
{

  uint32_t s;

  
  s= read_reg_alu ( inst&0xF );
  _regs.A+= s;
  SET_ZNOVL_FLAGS ( _regs.A );
  
} // end add_s


static void
add_Pri (
         const uint16_t inst
         )
{

  uint32_t v;


  v= ((uint32_t) RAMj(inst)[pr_modif_read ( inst )])<<16;
  _regs.A+= v;
  SET_ZNOVL_FLAGS ( _regs.A );
  
} // end add_Pri


static void
add_adr (
         const uint16_t inst
         )
{

  uint32_t v;

  
  v= ((uint32_t) RAMj(inst)[inst&0xFF])<<16;
  _regs.A+= v;
  SET_ZNOVL_FLAGS ( _regs.A );
  
} // end add_adr


static void
add_imm (void)
{

  uint32_t v;


  v= ((uint32_t) read ( _regs.PC++ ) )<<16;
  _regs.A+= v;
  SET_ZNOVL_FLAGS ( _regs.A );
  
} // end add_imm


static void
add_PPri (
          const uint16_t inst
          )
{

  uint32_t v;


  v= ((uint32_t) read ( (RAMj(inst)[pr_modif_read ( inst )])++ ))<<16;
  _regs.A+= v;
  SET_ZNOVL_FLAGS ( _regs.A );
  
} // end add_PPri


static void
add_ri (
        const uint16_t inst
        )
{

  uint32_t v;


  v= ((uint32_t) RIJ(inst))<<16;
  _regs.A+= v;
  SET_ZNOVL_FLAGS ( _regs.A );
  
} // end add_ri


static void
add_simm (
          const uint16_t inst
          )
{

  uint32_t v;


  v= ((uint32_t) (inst&0xFF))<<16;
  _regs.A+= v;
  SET_ZNOVL_FLAGS ( _regs.A );
  
} // end add_simm


static void
and_s (
       const uint16_t inst
       )
{

  uint32_t s;

  
  s= read_reg_alu ( inst&0xF );
  _regs.A&= s;
  SET_ZN_FLAGS ( _regs.A );
  
} // end and_s


static void
and_Pri (
         const uint16_t inst
         )
{

  uint32_t v;


  v= ((uint32_t) RAMj(inst)[pr_modif_read ( inst )])<<16;
  _regs.A&= v;
  SET_ZN_FLAGS ( _regs.A );
  
} // end and_Pri


static void
and_adr (
         const uint16_t inst
         )
{

  uint32_t v;


  v= ((uint32_t) RAMj(inst)[inst&0xFF])<<16;
  _regs.A&= v;
  SET_ZN_FLAGS ( _regs.A );
  
} // end and_adr


static void
and_imm (void)
{

  uint32_t v;


  v= ((uint32_t) read ( _regs.PC++ ) )<<16;
  _regs.A&= v;
  SET_ZN_FLAGS ( _regs.A );
  
} // end and_imm


static void
and_PPri (
          const uint16_t inst
          )
{

  uint32_t v;


  v= ((uint32_t) read ( (RAMj(inst)[pr_modif_read ( inst )])++ ))<<16;
  _regs.A&= v;
  SET_ZN_FLAGS ( _regs.A );
  
} // end and_PPri


static void
and_ri (
        const uint16_t inst
        )
{

  uint32_t v;


  v= ((uint32_t) RIJ(inst))<<16;
  _regs.A&= v;
  SET_ZN_FLAGS ( _regs.A );
  
} // end and_ri


static void
and_simm (
          const uint16_t inst
          )
{

  uint32_t v;


  v= ((uint32_t) (inst&0xFF))<<16;
  _regs.A&= v;
  SET_ZN_FLAGS ( _regs.A );
  
} // end and_simm


static void
or_s (
      const uint16_t inst
      )
{

  uint32_t s;

  
  s= read_reg_alu ( inst&0xF );
  _regs.A|= s;
  SET_ZN_FLAGS ( _regs.A );
  
} // end or_s


static void
or_Pri (
        const uint16_t inst
        )
{

  uint32_t v;


  v= ((uint32_t) RAMj(inst)[pr_modif_read ( inst )])<<16;
  _regs.A|= v;
  SET_ZN_FLAGS ( _regs.A );
  
} // end or_Pri


static void
or_adr (
        const uint16_t inst
        )
{

  uint32_t v;


  v= ((uint32_t) RAMj(inst)[inst&0xFF])<<16;
  _regs.A|= v;
  SET_ZN_FLAGS ( _regs.A );
  
} // end or_adr


static void
or_imm (void)
{

  uint32_t v;


  v= ((uint32_t) read ( _regs.PC++ ) )<<16;
  _regs.A|= v;
  SET_ZN_FLAGS ( _regs.A );
  
} // end or_imm


static void
or_PPri (
         const uint16_t inst
         )
{

  uint32_t v;


  v= ((uint32_t) read ( (RAMj(inst)[pr_modif_read ( inst )])++ ))<<16;
  _regs.A|= v;
  SET_ZN_FLAGS ( _regs.A );
  
} // end or_PPri


static void
or_ri (
       const uint16_t inst
       )
{

  uint32_t v;


  v= ((uint32_t) RIJ(inst))<<16;
  _regs.A|= v;
  SET_ZN_FLAGS ( _regs.A );
  
} // end or_ri


static void
or_simm (
         const uint16_t inst
         )
{

  uint32_t v;


  v= ((uint32_t) (inst&0xFF))<<16;
  _regs.A|= v;
  SET_ZN_FLAGS ( _regs.A );
  
} // end or_simm


static void
eor_s (
       const uint16_t inst
       )
{

  uint32_t s;

  
  s= read_reg_alu ( inst&0xF );
  _regs.A^= s;
  SET_ZN_FLAGS ( _regs.A );
  
} // end eor_s


static void
eor_Pri (
         const uint16_t inst
         )
{

  uint32_t v;


  v= ((uint32_t) RAMj(inst)[pr_modif_read ( inst )])<<16;
  _regs.A^= v;
  SET_ZN_FLAGS ( _regs.A );
  
} // end eor_Pri


static void
eor_adr (
         const uint16_t inst
         )
{

  uint32_t v;


  v= ((uint32_t) RAMj(inst)[inst&0xFF])<<16;
  _regs.A^= v;
  SET_ZN_FLAGS ( _regs.A );
  
} // end eor_adr


static void
eor_imm (void)
{

  uint32_t v;


  v= ((uint32_t) read ( _regs.PC++ ) )<<16;
  _regs.A^= v;
  SET_ZN_FLAGS ( _regs.A );
  
} // end eor_imm


static void
eor_PPri (
          const uint16_t inst
          )
{

  uint32_t v;


  v= ((uint32_t) read ( (RAMj(inst)[pr_modif_read ( inst )])++ ))<<16;
  _regs.A^= v;
  SET_ZN_FLAGS ( _regs.A );
  
} // end eor_PPri


static void
eor_ri (
        const uint16_t inst
        )
{

  uint32_t v;


  v= ((uint32_t) RIJ(inst))<<16;
  _regs.A^= v;
  SET_ZN_FLAGS ( _regs.A );
  
} // end eor_ri


static void
eor_simm (
          const uint16_t inst
          )
{

  uint32_t v;


  v= ((uint32_t) (inst&0xFF))<<16;
  _regs.A^= v;
  SET_ZN_FLAGS ( _regs.A );
  
} // end eor_simm


static void
mod_cond_op (
             const uint16_t inst
             )
{

  // Check cond
  if ( !check_cond ( inst ) ) return;

  // Operació
  switch ( inst&0x7 )
    {
    case 2: _regs.A= (uint32_t) (((int32_t) _regs.A)>>1); break;
    case 3: _regs.A<<= 1; break;
    case 6: _regs.A= ((uint32_t) -((int32_t) _regs.A)); break;
    case 7:
      if ( _regs.A&0x80000000 )
        _regs.A= ((uint32_t) -((int32_t) _regs.A));
      break;
    default:
      _warning ( _udata, "SVP: mod_op no suportada en instrucció %04X", inst );
    }
  SET_ZN_FLAGS ( _regs.A );
  
} // end mod_cond_op


static void
ld_d_s (
        const uint16_t inst
        )
{

  int s,d;


  s= inst&0xF;
  d= (inst>>4)&0xF;

  if ( d == 0 && s == 0 ) return; // NOP
  else if ( d == 0 && s >= 8 && s != 14 &&
            _regs.PMC.state == SET ) // (ld -, PMx) -> program it for reading
    conf_pgm_r ( s );
  else if ( d >= 8 && s == 0 && d != 14 &&
            _regs.PMC.state == SET ) // (ld PMx, -) -> program it for writing
    conf_pgm_w ( d );
  else if ( d == 3 && s == 7 ) // (ld A, P)
    {
      update_P ();
      _regs.A= _regs.P;
    }
  else // Regular
    write_reg ( d, read_reg ( s ) );
  
} // end ld_d_s


static void
ld_d_Pri (
          const uint16_t inst
          )
{

  int d;

  
  d= (inst>>4)&0xF;
  write_reg ( d, RAMj(inst)[pr_modif_read ( inst )]);
  
} // end ld_d_Pri


static void
ld_Pri_s (
          const uint16_t inst
          )
{

  int s;
  

  s= (inst>>4)&0xF;
  RAMj(inst)[pr_modif_read ( inst )]= read_reg ( s );
  
} // end ld_Pri_s


static void
ldi_d_imm (
           const uint16_t inst
           )
{

  int d;
  uint16_t addr;
  
  
  d= (inst>>4)&0xF;
  addr= _regs.PC++;
  write_reg ( d, read ( addr ) );
  
} // end ldi_d_imm


static void
ld_d_PPri (
           const uint16_t inst
           )
{

  int d;
  uint16_t addr;
  
  
  d= (inst>>4)&0xF;
  addr= (RAMj(inst)[pr_modif_read ( inst )])++;
  write_reg ( d, read ( addr ) );
  
} // end ld_d_PPri


static void
ldi_Pri_imm (
             const uint16_t inst
             )
{

  uint16_t addr;


  addr= _regs.PC++;
  RAMj(inst)[pr_modif_read ( inst )]= read ( addr );
  
} // end ldi_Pri_imm


static void
ld_adr_a (
          const uint16_t inst
          )
{
  RAMj(inst)[inst&0xFF]= (uint16_t) (_regs.A>>16);
} // end ld_adr_a


static void
ld_a_adr (
          const uint16_t inst
          )
{
  
  uint32_t v;
  
  
  v= ((uint32_t) RAMj(inst)[inst&0xFF])<<16;
  _regs.A= (_regs.A&0xFFFF) | v;
  
} // end ld_a_adr


static void
ld_d_ri (
         const uint16_t inst
         )
{

  int d;

  
  d= (inst>>4)&0xF;
  write_reg ( d, (uint16_t) RIJ(inst) );
  
} // end ld_d_ri


static void
ld_ri_s (
         const uint16_t inst
         )
{

  int s;


  s= (inst>>4)&0xF;
  RIJ(inst)= (uint8_t) (read_reg ( s )&0xFF);
  
} // end ld_ri_s


static void
ldi_ri_simm (
             const uint16_t inst
             )
{
  // ATENCIÓ!!! El RIJ es calcula de manera diferent!
  _regs.rX[(inst>>10)&0x1][(inst>>8)&0x3]= (uint8_t) (inst&0xFF);
} // end ldi_ri_simm


static void
ld_d_Pa (
         const uint16_t inst
         )
{

  int d;
  
  
  d= (inst>>4)&0xF;
  write_reg ( d, read ( (uint16_t) (_regs.A>>16 )) );
  
} // end ld_d_Pa


static void
call (
      const uint16_t inst
      )
{

  uint16_t addr;


  addr= read ( _regs.PC++ );
  if ( check_cond ( inst ) )
    {
      if ( _regs.STACK.N == 6 )
        _warning ( _udata, "SVP: no es poden escriure més valors en STACK" );
      else
        _regs.STACK.v[_regs.STACK.N++]= _regs.PC;
      _regs.PC= addr;
    }
  
} // end call


static void
bra (
     const uint16_t inst
     )
{

  uint16_t addr;

  
  addr= read ( _regs.PC++ );
  if ( check_cond ( inst ) )
    _regs.PC= addr;
  
} // end bra


static void
mld (
     const uint16_t inst
     )
{

  // NOTA!!! es gasta el pr_modif_read de manera diferent. Per poder
  // aprofitar la funció modifique el inst que li passe.
  
  _regs.A= 0;
  SET_ZN_FLAGS ( _regs.A );
  _regs.X= _ram[0][pr_modif_read ( inst&0xF )];
  _regs.Y= _ram[1][pr_modif_read ( ((inst>>4)&0xF)|0x0100 )];
  
} // end mld


static void
mpya (
      const uint16_t inst
      )
{

  // NOTA!!! es gasta el pr_modif_read de manera diferent. Per poder
  // aprofitar la funció modifique el inst que li passe.
  update_P ();
  _regs.A+= _regs.P;
  SET_ZN_FLAGS ( _regs.A );
  _regs.X= _ram[0][pr_modif_read ( inst&0xF )];
  _regs.Y= _ram[1][pr_modif_read ( ((inst>>4)&0xF)|0x0100 )];
  
} // end mpya


static void
mpys (
      const uint16_t inst
      )
{

  // NOTA!!! es gasta el pr_modif_read de manera diferent. Per poder
  // aprofitar la funció modifique el inst que li passe.

  update_P ();
  _regs.A-= _regs.P;
  SET_ZN_FLAGS ( _regs.A );
  _regs.X= _ram[0][pr_modif_read ( inst&0xF )];
  _regs.Y= _ram[1][pr_modif_read ( ((inst>>4)&0xF)|0x0100 )];
  
} // end mpys


static void
run_inst (void)
{

  uint16_t inst;

  
  inst= read ( _regs.PC++ );
  switch ( inst>>9 )
    {

    case 0x00: ld_d_s ( inst ); break;
    case 0x01: ld_d_Pri ( inst ); break;
    case 0x02: ld_Pri_s ( inst ); break;
    case 0x03: ld_a_adr ( inst ); break;
    case 0x04: ldi_d_imm ( inst ); break;
    case 0x05: ld_d_PPri ( inst ); break;
    case 0x06: ldi_Pri_imm ( inst ); break;
    case 0x07: ld_adr_a ( inst ); break;

    case 0x09: ld_d_ri ( inst ); break;
    case 0x0A: ld_ri_s ( inst ); break;

    case 0x0C ... 0x0F: ldi_ri_simm ( inst ); break;
    case 0x10: sub_s ( inst ); break;
    case 0x11: sub_Pri ( inst ); break;

    case 0x13: sub_adr ( inst ); break;
    case 0x14: sub_imm (); break;
    case 0x15: sub_PPri ( inst ); break;

    case 0x19: sub_ri ( inst ); break;

    case 0x1B: mpys ( inst ); break;
    case 0x1C: sub_simm ( inst ); break;

    case 0x24: call ( inst ); break;
    case 0x25: ld_d_Pa ( inst ); break;
    case 0x26: bra ( inst ); break;
      
    case 0x30: cmp_s ( inst ); break;
    case 0x31: cmp_Pri ( inst ); break;

    case 0x33: cmp_adr ( inst ); break;
    case 0x34: cmp_imm (); break;
    case 0x35: cmp_PPri ( inst ); break;

    case 0x39: cmp_ri ( inst ); break;
      
    case 0x3C: cmp_simm ( inst ); break;
      
    case 0x40: add_s ( inst ); break;
    case 0x41: add_Pri ( inst ); break;

    case 0x43: add_adr ( inst ); break;
    case 0x44: add_imm (); break;
    case 0x45: add_PPri ( inst ); break;

    case 0x48: mod_cond_op ( inst ); break;
    case 0x49: add_ri ( inst ); break;

    case 0x4B: mpya ( inst ); break;
    case 0x4C: add_simm ( inst ); break;

    case 0x50: and_s ( inst ); break;
    case 0x51: and_Pri ( inst ); break;

    case 0x53: and_adr ( inst ); break;
    case 0x54: and_imm (); break;
    case 0x55: and_PPri ( inst ); break;

    case 0x59: and_ri ( inst ); break;

    case 0x5B: mld ( inst ); break;
    case 0x5C: and_simm ( inst ); break;

    case 0x60: or_s ( inst ); break;
    case 0x61: or_Pri ( inst ); break;

    case 0x63: or_adr ( inst ); break;
    case 0x64: or_imm (); break;
    case 0x65: or_PPri ( inst ); break;

    case 0x69: or_ri ( inst ); break;

    case 0x6C: or_simm ( inst ); break;

    case 0x70: eor_s ( inst ); break;
    case 0x71: eor_Pri ( inst ); break;

    case 0x73: eor_adr ( inst ); break;
    case 0x74: eor_imm (); break;
    case 0x75: eor_PPri ( inst ); break;

    case 0x79: eor_ri ( inst ); break;

    case 0x7C: eor_simm ( inst ); break;
      
    default:
      _warning ( _udata, "SVP: instrucció desconeguda: %04X", inst );
    }
  
} // end run_inst


static MD_SVPOpType
decode_reg (
            const int reg
            )
{
  
  switch ( reg )
    {
    case 0: return MD_SVP__;
    case 1: return MD_SVP_X;
    case 2: return MD_SVP_Y;
    case 3: return MD_SVP_A;
    case 4: return MD_SVP_ST;
    case 5: return MD_SVP_STACK;
    case 6: return MD_SVP_PC;
    case 7: return MD_SVP_P;
    case 8: return MD_SVP_PM0;
    case 9: return MD_SVP_PM1;
    case 10: return MD_SVP_PM2;
    case 11: return MD_SVP_XST;
    case 12: return MD_SVP_PM4;
    case 13: return MD_SVP_EXT5;
    case 14: return MD_SVP_PMC;
    case 15: return MD_SVP_AL;
    default: return MD_SVP_NONE;
    }
  
} // end decode_reg


static void
decode_Pri (
            MD_SVPInst *inst,
            const int   pos
            )
{

  inst->ri[pos].m= (inst->inst[0]>>2)&0x3;
  inst->ri[pos].reg= (inst->inst[0]&0x3)|((inst->inst[0]>>6)&0x4);
  if ( inst->ri[pos].reg == 3 || inst->ri[pos].reg == 7 )
    inst->op[pos]= MD_SVP_PRI_MVAL;
  else
    switch ( inst->ri[pos].m )
      {
      default:
      case 0: inst->op[pos]= MD_SVP_PRI; break;
      case 1: inst->op[pos]= MD_SVP_PRI_INC; break;
      case 2: inst->op[pos]= MD_SVP_PRI_MODDEC; break;
      case 3: inst->op[pos]= MD_SVP_PRI_MODINC; break;
      }
  
} // end decode_Pri


static void
decode_imm (
            MD_SVPInst     *inst,
            const uint16_t  addr
            )
{

  inst->op[1]= MD_SVP_IMM;
  inst->inst[1]= read ( addr+1 );
  inst->nwords= 2;
  
} // end decode_imm


static void
decode_PPri (
             MD_SVPInst *inst
             )
{

  inst->ri[1].m= (inst->inst[0]>>2)&0x3;
  inst->ri[1].reg= (inst->inst[0]&0x3)|((inst->inst[0]>>6)&0x4);
  if ( inst->ri[1].reg == 3 || inst->ri[1].reg == 7 )
    inst->op[1]= MD_SVP_PPRI_MVAL;
  else
    switch ( inst->ri[1].m )
      {
      default:
      case 0: inst->op[1]= MD_SVP_PPRI; break;
      case 1: inst->op[1]= MD_SVP_PPRI_INC; break;
      case 2: inst->op[1]= MD_SVP_PPRI_MODDEC; break;
      case 3: inst->op[1]= MD_SVP_PPRI_MODINC; break;
      }
  
} // end decode_PPri


static void
decode_adr (
            MD_SVPInst *inst,
            const int   pos
            )
{

  inst->op[pos]= MD_SVP_ADR;
  inst->adr.ram_ind= ((inst->inst[0])>>8)&0x1;
  inst->adr.pos= (inst->inst[0])&0xFF;
  
} // end decode_adr


static void
decode_ri (
           MD_SVPInst *inst,
           const int   pos
           )
{

  inst->op[pos]= MD_SVP_RI;
  inst->ri[pos].m= (inst->inst[0]>>2)&0x3;
  inst->ri[pos].reg= (inst->inst[0]&0x3)|((inst->inst[0]>>6)&0x4);
  
} // end decode_ri


static void
decode_mul (
            MD_SVPInst *inst
            )
{

  inst->ri[0].m= (inst->inst[0]>>6)&0x3;
  inst->ri[0].reg= ((inst->inst[0]>>4)&0x3)|0x4;
  if ( inst->ri[0].reg == 3 || inst->ri[0].reg == 7 )
    inst->op[0]= MD_SVP_PRI_MVAL;
  else
    switch ( inst->ri[0].m )
      {
      default:
      case 0: inst->op[0]= MD_SVP_PRI; break;
      case 1: inst->op[0]= MD_SVP_PRI_INC; break;
      case 2: inst->op[0]= MD_SVP_PRI_MODDEC; break;
      case 3: inst->op[0]= MD_SVP_PRI_MODINC; break;
      }
  inst->ri[1].m= (inst->inst[0]>>2)&0x3;
  inst->ri[1].reg= inst->inst[0]&0x3;
  if ( inst->ri[0].reg == 3 || inst->ri[0].reg == 7 )
    inst->op[1]= MD_SVP_PRI_MVAL;
  else
    switch ( inst->ri[1].m )
      {
      default:
      case 0: inst->op[1]= MD_SVP_PRI; break;
      case 1: inst->op[1]= MD_SVP_PRI_INC; break;
      case 2: inst->op[1]= MD_SVP_PRI_MODDEC; break;
      case 3: inst->op[1]= MD_SVP_PRI_MODINC; break;
      }
  
} // end decode_mul


static void
decode_cond (
             MD_SVPInst *inst
             )
{

  switch ( (inst->inst[0]>>4)&0xF )
    {
    case 0: inst->op[0]= MD_SVP_COND_TRUE; break;
    case 5: inst->op[0]= MD_SVP_COND_Z; break;
    case 7: inst->op[0]= MD_SVP_COND_N; break;
    default: inst->op[0]= MD_SVP_COND_UNK; break;
    }
  inst->cond_f= ((inst->inst[0]>>8)&0x1)!=0;
  
} // end decode_cond


static void
decode_addr (
             MD_SVPInst     *inst,
             const uint16_t  addr
             )
{

  inst->op[1]= MD_SVP_ADDR;
  inst->inst[1]= read ( addr+1 );
  inst->nwords= 2;
  
} // end decode_addr


static void
decode_inst (
             MD_SVPInst     *inst,
             const uint16_t  addr
             )
{

  inst->inst[0]= read ( addr );
  inst->op[0]= inst->op[1]= MD_SVP_NONE;
  inst->nwords= 1;
  switch ( inst->inst[0]>>9 )
    {

    case 0x00:
      inst->mnemonic= MD_SVP_LD;
      inst->op[0]= decode_reg ( (inst->inst[0]>>4)&0xF );
      inst->op[1]= decode_reg ( (inst->inst[0])&0xF );
      break;
    case 0x01:
      inst->mnemonic= MD_SVP_LD;
      inst->op[0]= decode_reg ( (inst->inst[0]>>4)&0xF );
      decode_Pri ( inst, 1 );
      break;
    case 0x02:
      inst->mnemonic= MD_SVP_LD;
      decode_Pri ( inst, 0 );
      inst->op[1]= decode_reg ( (inst->inst[0]>>4)&0xF );
      break;
    case 0x03:
      inst->mnemonic= MD_SVP_LD;
      inst->op[0]= MD_SVP_A;
      decode_adr ( inst, 1 );
      break;
    case 0x04:
      inst->mnemonic= MD_SVP_LD;
      inst->op[0]= decode_reg ( (inst->inst[0]>>4)&0xF );
      decode_imm ( inst, addr );
      break;
    case 0x05:
      inst->mnemonic= MD_SVP_LD;
      inst->op[0]= decode_reg ( (inst->inst[0]>>4)&0xF );
      decode_PPri ( inst );
      break;
    case 0x06:
      inst->mnemonic= MD_SVP_LD;
      decode_Pri ( inst, 0 );
      decode_imm ( inst, addr );
      break;
    case 0x07:
      inst->mnemonic= MD_SVP_LD;
      decode_adr ( inst, 0 );
      inst->op[1]= MD_SVP_A;
      break;

    case 0x09:
      inst->mnemonic= MD_SVP_LD;
      inst->op[0]= decode_reg ( (inst->inst[0]>>4)&0xF );
      decode_ri ( inst, 1 );
      break;
    case 0x0A:
      inst->mnemonic= MD_SVP_LD;
      decode_ri ( inst, 0 );
      inst->op[1]= decode_reg ( (inst->inst[0]>>4)&0xF );
      break;

    case 0x0C ... 0x0F:
      inst->mnemonic= MD_SVP_LD;
      inst->op[0]= MD_SVP_RI;
      inst->ri[0].reg= ((inst->inst[0])>>8)&0x7;
      inst->ri[0].m= -1;
      inst->op[1]= MD_SVP_SIMM;
      inst->simm= (uint8_t) ((inst->inst[0])&0xFF);
      break;
    case 0x10:
      inst->mnemonic= MD_SVP_SUB;
      inst->op[0]= MD_SVP_A;
      inst->op[1]= decode_reg ( inst->inst[0]&0xF );
      break;
    case 0x11:
      inst->mnemonic= MD_SVP_SUB;
      inst->op[0]= MD_SVP_A;
      decode_Pri ( inst, 1 );
      break;

    case 0x13:
      inst->mnemonic= MD_SVP_SUB;
      inst->op[0]= MD_SVP_A;
      decode_adr ( inst, 1 );
      break;
    case 0x14:
      inst->mnemonic= MD_SVP_SUB;
      inst->op[0]= MD_SVP_A;
      decode_imm ( inst, addr );
      break;
    case 0x15:
      inst->mnemonic= MD_SVP_SUB;
      inst->op[0]= MD_SVP_A;
      decode_PPri ( inst );
      break;

    case 0x19:
      inst->mnemonic= MD_SVP_SUB;
      inst->op[0]= MD_SVP_A;
      decode_ri ( inst, 1 );
      break;

    case 0x1B:
      inst->mnemonic= MD_SVP_MPYS;
      decode_mul ( inst );
      break;
    case 0x1C:
      inst->mnemonic= MD_SVP_SUB;
      inst->op[0]= MD_SVP_A;
      inst->op[1]= MD_SVP_SIMM;
      inst->simm= (uint8_t) (inst->inst[0]&0xFF);
      break;

    case 0x24:
      inst->mnemonic= MD_SVP_CALL;
      decode_cond ( inst );
      decode_addr ( inst, addr );
      break;
    case 0x25:
      inst->mnemonic= MD_SVP_LD;
      inst->op[0]= decode_reg ( (inst->inst[0]>>4)&0xF );
      inst->op[1]= MD_SVP_PA;
      break;
    case 0x26:
      inst->mnemonic= MD_SVP_BRA;
      decode_cond ( inst );
      decode_addr ( inst, addr );
      break;
      
    case 0x30:
      inst->mnemonic= MD_SVP_CMP;
      inst->op[0]= MD_SVP_A;
      inst->op[1]= decode_reg ( inst->inst[0]&0xF );
      break;
    case 0x31:
      inst->mnemonic= MD_SVP_CMP;
      inst->op[0]= MD_SVP_A;
      decode_Pri ( inst, 1 );
      break;

    case 0x33:
      inst->mnemonic= MD_SVP_CMP;
      inst->op[0]= MD_SVP_A;
      decode_adr ( inst, 1 );
      break;
    case 0x34:
      inst->mnemonic= MD_SVP_CMP;
      inst->op[0]= MD_SVP_A;
      decode_imm ( inst, addr );
      break;
    case 0x35:
      inst->mnemonic= MD_SVP_CMP;
      inst->op[0]= MD_SVP_A;
      decode_PPri ( inst );
      break;

    case 0x39:
      inst->mnemonic= MD_SVP_CMP;
      inst->op[0]= MD_SVP_A;
      decode_ri ( inst, 1 );
      break;
      
    case 0x3C:
      inst->mnemonic= MD_SVP_CMP;
      inst->op[0]= MD_SVP_A;
      inst->op[1]= MD_SVP_SIMM;
      inst->simm= (uint8_t) (inst->inst[0]&0xFF);
      break;
      
    case 0x40:
      inst->mnemonic= MD_SVP_ADD;
      inst->op[0]= MD_SVP_A;
      inst->op[1]= decode_reg ( inst->inst[0]&0xF );
      break;
    case 0x41:
      inst->mnemonic= MD_SVP_ADD;
      inst->op[0]= MD_SVP_A;
      decode_Pri ( inst, 1 );
      break;

    case 0x43:
      inst->mnemonic= MD_SVP_ADD;
      inst->op[0]= MD_SVP_A;
      decode_adr ( inst, 1 );
      break;
    case 0x44:
      inst->mnemonic= MD_SVP_ADD;
      inst->op[0]= MD_SVP_A;
      decode_imm ( inst, addr );
      break;
    case 0x45:
      inst->mnemonic= MD_SVP_ADD;
      inst->op[0]= MD_SVP_A;
      decode_PPri ( inst );
      break;

    case 0x48:
      inst->mnemonic= MD_SVP_MOD;
      decode_cond ( inst );
      switch ( inst->inst[0]&0x7 )
        {
        case 2: inst->op[1]= MD_SVP_OP_SR; break;
        case 3: inst->op[1]= MD_SVP_OP_SL; break;
        case 6: inst->op[1]= MD_SVP_OP_NEG; break;
        case 7: inst->op[1]= MD_SVP_OP_ABS; break;
        default: inst->op[1]= MD_SVP_OP_UNK; break;
        }
      break;
    case 0x49:
      inst->mnemonic= MD_SVP_ADD;
      inst->op[0]= MD_SVP_A;
      decode_ri ( inst, 1 );
      break;

    case 0x4B:
      inst->mnemonic= MD_SVP_MPYA;
      decode_mul ( inst );
      break;
    case 0x4C:
      inst->mnemonic= MD_SVP_ADD;
      inst->op[0]= MD_SVP_A;
      inst->op[1]= MD_SVP_SIMM;
      inst->simm= (uint8_t) (inst->inst[0]&0xFF);
      break;

    case 0x50:
      inst->mnemonic= MD_SVP_AND;
      inst->op[0]= MD_SVP_A;
      inst->op[1]= decode_reg ( inst->inst[0]&0xF );
      break;
    case 0x51:
      inst->mnemonic= MD_SVP_AND;
      inst->op[0]= MD_SVP_A;
      decode_Pri ( inst, 1 );
      break;

    case 0x53:
      inst->mnemonic= MD_SVP_AND;
      inst->op[0]= MD_SVP_A;
      decode_adr ( inst, 1 );
      break;
    case 0x54:
      inst->mnemonic= MD_SVP_AND;
      inst->op[0]= MD_SVP_A;
      decode_imm ( inst, addr );
      break;
    case 0x55:
      inst->mnemonic= MD_SVP_AND;
      inst->op[0]= MD_SVP_A;
      decode_PPri ( inst );
      break;

    case 0x59:
      inst->mnemonic= MD_SVP_AND;
      inst->op[0]= MD_SVP_A;
      decode_ri ( inst, 1 );
      break;

    case 0x5B:
      inst->mnemonic= MD_SVP_MLD;
      decode_mul ( inst );
      break;
    case 0x5C:
      inst->mnemonic= MD_SVP_AND;
      inst->op[0]= MD_SVP_A;
      inst->op[1]= MD_SVP_SIMM;
      inst->simm= (uint8_t) (inst->inst[0]&0xFF);
      break;

    case 0x60:
      inst->mnemonic= MD_SVP_OR;
      inst->op[0]= MD_SVP_A;
      inst->op[1]= decode_reg ( inst->inst[0]&0xF );
      break;
    case 0x61:
      inst->mnemonic= MD_SVP_OR;
      inst->op[0]= MD_SVP_A;
      decode_Pri ( inst, 1 );
      break;

    case 0x63:
      inst->mnemonic= MD_SVP_OR;
      inst->op[0]= MD_SVP_A;
      decode_adr ( inst, 1 );
      break;
    case 0x64:
      inst->mnemonic= MD_SVP_OR;
      inst->op[0]= MD_SVP_A;
      decode_imm ( inst, addr );
      break;
    case 0x65:
      inst->mnemonic= MD_SVP_OR;
      inst->op[0]= MD_SVP_A;
      decode_PPri ( inst );
      break;

    case 0x69:
      inst->mnemonic= MD_SVP_OR;
      inst->op[0]= MD_SVP_A;
      decode_ri ( inst, 1 );
      break;

    case 0x6C:
      inst->mnemonic= MD_SVP_OR;
      inst->op[0]= MD_SVP_A;
      inst->op[1]= MD_SVP_SIMM;
      inst->simm= (uint8_t) (inst->inst[0]&0xFF);
      break;

    case 0x70:
      inst->mnemonic= MD_SVP_EOR;
      inst->op[0]= MD_SVP_A;
      inst->op[1]= decode_reg ( inst->inst[0]&0xF );
      break;
    case 0x71:
      inst->mnemonic= MD_SVP_EOR;
      inst->op[0]= MD_SVP_A;
      decode_Pri ( inst, 1 );
      break;

    case 0x73:
      inst->mnemonic= MD_SVP_EOR;
      inst->op[0]= MD_SVP_A;
      decode_adr ( inst, 1 );
      break;
    case 0x74:
      inst->mnemonic= MD_SVP_EOR;
      inst->op[0]= MD_SVP_A;
      decode_imm ( inst, addr );
      break;
    case 0x75:
      inst->mnemonic= MD_SVP_EOR;
      inst->op[0]= MD_SVP_A;
      decode_PPri ( inst );
      break;

    case 0x79:
      inst->mnemonic= MD_SVP_EOR;
      inst->op[0]= MD_SVP_A;
      decode_ri ( inst, 1 );
      break;

    case 0x7C:
      inst->mnemonic= MD_SVP_EOR;
      inst->op[0]= MD_SVP_A;
      inst->op[1]= MD_SVP_SIMM;
      inst->simm= (uint8_t) (inst->inst[0]&0xFF);
      break;
      
    default:
      inst->mnemonic= MD_SVP_UNK;
    }

} // end decode_inst




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

MD_Bool
MD_svp_init (
             const MD_Rom  *rom,
             MD_Warning    *warning,
             MD_CPUStepSVP *cpu_step_svp,
             void          *udata  
             )
{

  // Callbacks.
  _warning= warning;
  _cpu_step_svp= cpu_step_svp;
  _udata= udata;
  
  // Inicialitza.
  check_rom ( rom );
  if ( _enabled ) _rom= rom;
  MD_svp_init_state ();

  return _enabled;
  
} // end MD_svp_init


void
MD_svp_init_state (void)
{

  if ( !_enabled ) return;

  memset ( _dram, 0, sizeof(_dram) );
  memset ( _iram, 0, sizeof(_iram) );
  memset ( _ram, 0, sizeof(_ram) );
  memset ( &_regs, 0, sizeof(_regs) );

  MD_svp_reset ();
  
} // end MD_svp_init_state


void
MD_svp_clock (
              const int cc
              )
{

  int n;

  
  // NOTA!! Segons svpdoc no és molt important el timing, però com a
  // mínim ha de ser 10~12M d'instruccions per segon. El que faré es
  // multiplicar per 2 el rellotge, que vold dir ~15M d'instruccions
  // per segon.
  for ( n= 0; n < cc*2; ++n )
    run_inst ();
  
} // end MD_svp_clock


void
MD_svp_reset (void)
{

  _regs.XST= 0xFFFF;
  _regs.XST_Status= 0x0000;
  _regs.PC= 0x0400;
  _regs.ST= 0x0000;
  _regs.STACK.N= 0;
  _regs.PMC.state= WAIT_ADDR;
  memset ( &_regs.PM0, 0, sizeof(_regs.PM0) );
  memset ( &_regs.PM1, 0, sizeof(_regs.PM1) );
  memset ( &_regs.PM2, 0, sizeof(_regs.PM2) );
  memset ( &_regs.PM3, 0, sizeof(_regs.PM3) );
  memset ( &_regs.PM4, 0, sizeof(_regs.PM4) );
  
} // end MD_svp_reset


MD_Word
MD_svp_port_read (
                  const MDu32 addr
                  )
{
  
  MD_Word ret;
  uint32_t aux;
  
  
  aux= (uint32_t) ((addr&0xFFFFFF)>>1);
  switch ( aux )
    {
    case (0xA15000>>1) ... (0xA15002>>1):
      ret.v= _regs.XST;
      break;
    case (0xA15004>>1):
      ret.v= _regs.XST_Status;
      _regs.XST_Status&= ~0x1; // (cleared when a15004 is read by 68k)
      break;
    default: ret.v= 0xFFFF;
    }
  
  return ret;
  
} // end MD_svp_port_read


void
MD_svp_port_write (
                   const MDu32   addr,
                   const MD_Word data
                   )
{

  uint32_t aux;
  
  
  aux= (uint32_t) ((addr&0xFFFFFF)>>1);
  switch ( aux )
    {
    case (0xA15000>>1) ... (0xA15002>>1):
      _regs.XST= data.v;
      _regs.XST_Status|= 0x2; // set, when 68k has written something
                              // to a15000 or a15002
      break;
    default: break;
    }
  
} // end MD_svp_port_write


MD_Word
MD_svp_mem_read (
                 const MDu32 addr
                 )
{
  
  MD_Word ret;
  uint32_t aux;

  
  aux= (uint32_t) ((addr&0xFFFFFF)>>1);
  
  // Ací també va la ROM, però la MD ja pot accedir des de fora.
  if ( aux < (0x200000>>1) )
    ret.v= 0;

  // unused (1)
  else if ( aux < (0x300000>>1) ) ret.v= 0xFFFF;
  
  // DRAM (+ 3 mirrors)
  else if ( aux < (0x380000>>1) )
    ret.v= (MDu16) _dram[aux&0xFFFF];
  
  // unused (1): reads seem to return data from internal bus (last
  // word read by SSP160x). Writes probably have no effect.
  // Vaig a passar de fer res.
  // També va el IRAM???
  else if ( aux < (0x390000>>1) )
    ret.v= 0xFFFF;

  // cell arrange 1 ¿¿Correcte???? No entenc molt bé el que diu.
  else if ( aux < (0x3A0000>>1) )
    ret.v= (MDu16) _dram[aux&0x7FFF];

  // cell arrange 2 ¿¿Correcte???? No entenc molt bé el que diu.
  else if ( aux < (0x3B0000>>1) )
    ret.v= (MDu16) _dram[aux&0x7FFF];

  // unused (2): reads return 0xffff, writes have no effect.
  else ret.v= 0xFFFF;
  
  return ret;
  
} // end MD_svp_mem_read


void
MD_svp_mem_write (
                  const MDu32   addr,
                  const MD_Word data
                 )
{

  MDu32 aux;
  
  
  aux= (addr&0xFFFFFF)>>1;
  
  // Ací també va la ROM, però la MD ja pot accedir des de fora.
  if ( aux < (0x200000>>1) ) return;

  // unused (1)
  else if ( aux < (0x300000>>1) ) return;
  
  // DRAM (+ 3 mirrors)
  else if ( aux < (0x380000>>1) )
    _dram[aux&0xFFFF]= (uint16_t) data.v;
  
  // unused (1): reads seem to return data from internal bus (last
  // word read by SSP160x). Writes probably have no effect.
  // Vaig a passar de fer res.
  // També va el IRAM???
  else if ( aux < (0x390000>>1) ) return;

  // cell arrange 1 ¿¿Correcte???? No entenc molt bé el que diu.
  else if ( aux < (0x3A0000>>1) )
    _dram[aux&0x7FFF]= (uint16_t) data.v;
  
  // cell arrange 2 ¿¿Correcte???? No entenc molt bé el que diu.
  else if ( aux < (0x3B0000>>1) )
    _dram[aux&0x7FFF]= (uint16_t) data.v;
  
  // unused (2): reads return 0xffff, writes have no effect.
  else return;
  
} // end MD_svp_mem_write


void
MD_svp_trace (
              const int cc
              )
{

  int n;
  MD_SVPInst inst;

  
  for ( n= 0; n < cc*2; ++n )
    {
      if ( _cpu_step_svp != NULL )
        {
          decode_inst ( &inst, _regs.PC );
          _cpu_step_svp ( &inst, _regs.PC, _udata );
        }
      run_inst ();
    }
  
} // end MD_svp_trace


const uint16_t *
MD_svp_get_dram (void)
{
  return &_dram[0];
} // end MD_svp_get_dram


int
MD_svp_save_state (
        	   FILE *f
        	   )
{
  
  if ( !_enabled ) return 0;

  SAVE ( _dram );
  SAVE ( _iram );
  SAVE ( _ram );
  SAVE ( _regs );
  
  return 0;
  
} // end MD_svp_save_state


int
MD_svp_load_state (
        	   FILE *f
        	   )
{
  
  if ( !_enabled ) return 0;

  LOAD ( _dram );
  LOAD ( _iram );
  LOAD ( _ram );
  LOAD ( _regs );
  
  return 0;
  
} // end MD_svp_load_state
