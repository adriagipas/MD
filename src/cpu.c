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
 *  cpu.c - Implementació del processador 68000.
 *
 */
/*
 * NOTA: Sols implemente les interrupcions RESET i auto-vectored.
 */


#include <string.h>

#include "MD.h"
#include "unpack.h"




/**********/
/* MACROS */
/**********/

#define SAVE(VAR)                                               \
  if ( fwrite ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1

#define LOAD(VAR)                                               \
  if ( fread ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1


/* General. */
#define UTIME 4
#define CLEAR_FLAGS(FLAGS) _regs.CCR.v&= ((FLAGS)^0xFFFF)

/* General. */
#define C1B(VAL) ((MDu8) (~(VAL)))
#define C1W(VAL) ((MDu16) (~(VAL)))
#define C1L(VAL) ((MDu32) (~(VAL)))

/* Flags. */
#define T1FLAG 0x8000
#define SFLAG  0x2000
#define I2FLAG 0x0400
#define I1FLAG 0x0200
#define I0FLAG 0x0100
#define XFLAG  0x0010
#define NFLAG  0x0008
#define ZFLAG  0x0004
#define VFLAG  0x0002
#define CFLAG  0x0001

#define IBITS (T1FLAG|SFLAG|I2FLAG|I1FLAG|I0FLAG|XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG)
#define IBITS_CCR (XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG)




/*********/
/* ESTAT */
/*********/

/* Informació de l'usuari. */
static MD_Warning *_warning;
static void *_udata;

/* Registres. */
static struct
{
  
  MD_Reg32 D[8];
  MD_Reg32 A[8];
  MD_Reg32 _SP;    /* En mode ususari: A7->USP i _SP->SSP; En mode
        	      supervisor: A7->SSP i _SP->USP. */
  MDu32    PC;
  MD_Word  CCR;
  
} _regs;

/* Interrupcions. */
static MDu32 _ints;    /* Bits: 1->RESET, 2-8->AUTO_VEC[1..7]. */

/* Per a la instrucció STOP. */
static MD_Bool _stop;




/*********/
/* TIPUS */
/*********/

typedef MDu8 (opb_t) (MDu8 const a,MDu8 const b);
typedef MD_Word (opw_t) (MD_Word const a,MD_Word const b);
typedef MD_Reg32 (opl_t) (MD_Reg32 const a,MD_Reg32 const b);
typedef MDu32 (op_t) (const MDu32 a,const MDu32 b);
typedef MDu8 (sopb_t) (MDu8 const val,unsigned int count);
typedef MD_Word (sopw_t) (MD_Word const val,unsigned int count);
typedef MD_Reg32 (sopl_t) (MD_Reg32 const val,unsigned int count);
typedef int (bop_mem_t) (MDu32 const addr,int const bit);
typedef int (bop_reg_t) (MDu8 const reg,int const bit);




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static MDu32
calc_8bit_displacement (void)
{
  
  MD_Word ew;
  MDu32 aux;
  
  
  ew= MD_mem_read ( _regs.PC );
  _regs.PC+= 2;
  switch ( ew.b.v1>>3 ) /* D/A Reg.Num. W/L */
    {
    case 0x00: aux= (MDs16) _regs.D[0].w.v0.v; break;
    case 0x01: aux= _regs.D[0].v; break;
    case 0x02: aux= (MDs16) _regs.D[1].w.v0.v; break;
    case 0x03: aux= _regs.D[1].v; break;
    case 0x04: aux= (MDs16) _regs.D[2].w.v0.v; break;
    case 0x05: aux= _regs.D[2].v; break;
    case 0x06: aux= (MDs16) _regs.D[3].w.v0.v; break;
    case 0x07: aux= _regs.D[3].v; break;
    case 0x08: aux= (MDs16) _regs.D[4].w.v0.v; break;
    case 0x09: aux= _regs.D[4].v; break;
    case 0x0A: aux= (MDs16) _regs.D[5].w.v0.v; break;
    case 0x0B: aux= _regs.D[5].v; break;    
    case 0x0C: aux= (MDs16) _regs.D[6].w.v0.v; break;
    case 0x0D: aux= _regs.D[6].v; break;  
    case 0x0E: aux= (MDs16) _regs.D[7].w.v0.v; break;
    case 0x0F: aux= _regs.D[7].v; break;
    case 0x10: aux= (MDs16) _regs.A[0].w.v0.v; break;
    case 0x11: aux= _regs.A[0].v; break;
    case 0x12: aux= (MDs16) _regs.A[1].w.v0.v; break;
    case 0x13: aux= _regs.A[1].v; break;
    case 0x14: aux= (MDs16) _regs.A[2].w.v0.v; break;
    case 0x15: aux= _regs.A[2].v; break;
    case 0x16: aux= (MDs16) _regs.A[3].w.v0.v; break;
    case 0x17: aux= _regs.A[3].v; break;
    case 0x18: aux= (MDs16) _regs.A[4].w.v0.v; break;
    case 0x19: aux= _regs.A[4].v; break;
    case 0x1A: aux= (MDs16) _regs.A[5].w.v0.v; break;
    case 0x1B: aux= _regs.A[5].v; break;
    case 0x1C: aux= (MDs16) _regs.A[6].w.v0.v; break;
    case 0x1D: aux= _regs.A[6].v; break;  
    case 0x1E: aux= (MDs16) _regs.A[7].w.v0.v; break;
    case 0x1F: aux= _regs.A[7].v; break;
    default: aux= 0; break;
    }
  /*aux<<= ((ew.b.v1>>1)&0x3);*/
  
  return aux + (MDs8) ew.b.v0;
  
} /* end calc_8bit_displacement */


/* ADRECES ********************************************************************/
static MD_Reg32 pd16an (const MD_Reg32 reg)
{
  MD_Reg32 addr;
  addr.v= reg.v + (MDs16) MD_mem_read ( _regs.PC ).v;
  _regs.PC+= 2;
  return addr;
}
static MD_Reg32 pw (void)
{
  MD_Reg32 addr;
  addr.v= (MDs16) MD_mem_read ( _regs.PC ).v;
  _regs.PC+= 2;
  return addr;
}
static MD_Reg32 pl (void)
{
  MD_Reg32 addr;
  addr.w.v1= MD_mem_read ( _regs.PC );
  addr.w.v0= MD_mem_read ( _regs.PC+2 );
  _regs.PC+= 4;
  return addr;
}
static MD_Reg32 pd16pc (void)
{
  MD_Reg32 addr;
  addr.v= _regs.PC + (MDs16) MD_mem_read ( _regs.PC ).v;
  _regs.PC+= 2;
  return addr;
}


/* I/O ************************************************************************/
static MDu8 inm_byte (void)
{
  MDu8 byte;
  byte= MD_mem_read ( _regs.PC ).b.v0;
  _regs.PC+= 2;
  return byte;
}
static MD_Reg32 inm_long (void)
{
  MD_Reg32 long_;
  long_.w.v1= MD_mem_read ( _regs.PC );
  long_.w.v0= MD_mem_read ( _regs.PC+2 );
  _regs.PC+= 4;
  return long_;
}
static MD_Word inm_word (void)
{
  MD_Word word;
  word= MD_mem_read ( _regs.PC );
  _regs.PC+= 2;
  return word;
}
static MD_Reg32 read_long (const MDu32 addr)
{
  MD_Reg32 long_;
  long_.w.v1= MD_mem_read ( addr );
  long_.w.v0= MD_mem_read ( addr+2 );
  return long_;
}
static void write_long (const MD_Reg32 long_,const MDu32 addr)
{
  MD_mem_write ( addr, long_.w.v1 );
  MD_mem_write ( addr+2, long_.w.v0 );
}


/* Excepcions *****************************************************************/
static int
trap (
      MDu32 const vector
      )
{
  
  MD_Reg32 tmp;
  
  
  /* Si estava en mode usuari. */
  if ( !(_regs.CCR.v&SFLAG) )
    {
      tmp= _regs.A[7];
      _regs.A[7]= _regs._SP;
      _regs._SP= tmp;
    }
  _regs.A[7].v-= 4;
  write_long ( (MD_Reg32) _regs.PC, _regs.A[7].v );
  _regs.A[7].v-= 2;
  MD_mem_write ( _regs.A[7].v, _regs.CCR );
  _regs.PC= read_long ( vector ).v;
  _regs.CCR.v|= SFLAG;
  
  return 34;
  
} /* end trap */


static int
reset_int (void)
{
  
  _regs.CCR.v= 0x2700;
  _regs.A[7]= read_long ( 0x0000 );
  _regs.PC= read_long ( 0x0004 ).v;
  
  return 44; /* ¿No estic segur?*/
  
} /* end reset_int */


static int
auto_vector_int (
        	 unsigned int const priority
        	 )
{
  
  if ( priority <= ((_regs.CCR.v&0x700)>>8) ) return 0;
  trap ( (priority + 24)*4 );
  
  return 44;
  
} /* end auto_vector_int */


static int
interrupts (void)
{
  
  MDu32 mask;
  unsigned int i;
  int ret;
  
  
  _stop= MD_FALSE;
  
  /* Reset */
  if ( _ints&0x1 )
    {
      _ints^= 0x1;
      return reset_int ();
    }
  
  /* Auto-vectors */
  for ( i= 7, mask= 0x80 ; i >= 1; --i, mask>>=1 )
    if ( mask&_ints )
      {
        ret= auto_vector_int ( i );
        if ( ret != 0 )
          {
            MD_vdp_clear_interrupt ( i );
            _ints^= mask;
          }
        return ret;
      }
  
  return 0; /* No s'ha servit la interrupció. */
  
} /* end interrupts */


/* Data Movement Instructions *************************************************/
static int
lea (
     MDu8 const reg,
     MDu8 const eamode,
     MDu8 const eareg
     )
{
  
  MD_Reg32 tmp;
  
  
  switch ( eamode )
    {
    case 2: _regs.A[reg]= _regs.A[eareg]; return 4;
    case 5: _regs.A[reg]= pd16an ( _regs.A[eareg] ); return 8;
    case 6:
      _regs.A[reg].v= _regs.A[eareg].v + calc_8bit_displacement ();
      return 12;
    case 7:
      switch ( eareg )
        {
        case 0: _regs.A[reg]= pw (); return 8;
        case 1: _regs.A[reg]= pl (); return 12;
        case 2: _regs.A[reg]= pd16pc (); return 8;
        case 3:
          tmp.v= _regs.PC;
          _regs.A[reg].v= tmp.v + calc_8bit_displacement ();
          return 12;
        default: break;
        }
      break;
    default: break;
    }
  
  _warning ( _udata, "LEA no accepta la <ea> %d:%d", eamode, eareg );
  
  return UTIME;
  
} /* end lea */


static int
link (
      MDu8 const reg
      )
{
  
  _regs.A[7].v-= 4;
  write_long ( _regs.A[reg], _regs.A[7].v );
  _regs.A[reg]= _regs.A[7];
  _regs.A[7].v+= (MDs16) pw ().v;
  
  return 18;
  
} /* end link */


static int
move_from_sr (
              MDu8 const mode,
              MDu8 const reg
              )
{
  
  switch ( mode )
    {
    case 0: _regs.D[reg].w.v0= _regs.CCR; return 6;
    case 2: MD_mem_write ( _regs.A[reg].v, _regs.CCR ); return 12;
    case 3:
      MD_mem_write ( _regs.A[reg].v, _regs.CCR );
      _regs.A[reg].v+= 2;
      return 12;
    case 4: MD_mem_write ( _regs.A[reg].v-= 2, _regs.CCR ); return 14;
    case 5: MD_mem_write ( pd16an ( _regs.A[reg] ).v, _regs.CCR ); return 16;
    case 6:
      MD_mem_write ( _regs.A[reg].v + calc_8bit_displacement (), _regs.CCR );
      return 18;
    case 7:
      switch ( reg )
        {
        case 0: MD_mem_write ( pw ().v, _regs.CCR ); return 16;
        case 1: MD_mem_write ( pl ().v, _regs.CCR ); return 20;
        default:
          _warning ( _udata,
        	     "MOVE from SR no accepta mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "MOVE from SR no accepta el mode %d", mode );
      return UTIME;
    }
  
} /* end move_from_sr */


static int
move_to_ccr (
             MDu8 const mode,
             MDu8 const reg
             )
{
  
  MDu8 byte;
  int ret;
  MDu32 addr;
  
  
  switch ( mode )
    {
    case 0: byte= _regs.D[reg].b.v0; ret= 12; break;
    case 1:
      _warning ( _udata, "MOVE to CCR no accepta el mode %d", mode );
      return UTIME;
    case 2: byte= MD_mem_read ( _regs.A[reg].v ).b.v0; ret= 16; break;
    case 3:
      byte= MD_mem_read ( _regs.A[reg].v ).b.v0;
      _regs.A[reg].v+= 2;
      ret= 16;
      break;
    case 4: byte= MD_mem_read ( _regs.A[reg].v-= 2 ).b.v0; ret= 18; break;
    case 5:
      byte= MD_mem_read ( pd16an ( _regs.A[reg] ).v ).b.v0;
      ret= 20;
      break;
    case 6:
      addr= _regs.A[reg].v + calc_8bit_displacement ();
      byte= MD_mem_read ( addr ).b.v0;
      ret= 22;
      break;
    case 7:
      switch ( reg )
        {
        case 0: byte= MD_mem_read ( pw ().v ).b.v0; ret= 20; break;
        case 1: byte= MD_mem_read ( pl ().v ).b.v0; ret= 24; break;
        case 2: byte= MD_mem_read ( pd16pc ().v ).b.v0; ret= 20; break;
        case 3:
          addr= _regs.PC;
          addr+= calc_8bit_displacement ();
          byte= MD_mem_read ( addr ).b.v0;
          ret= 22;
          break;
        case 4: byte= inm_word ().b.v0; ret= 16; break;
        default:
          _warning ( _udata,
        	     "MOVE to CCR no accepta mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default: byte= ret= 0; /* CALLA*/ break;
    }
  _regs.CCR.b.v0= byte&0x1F;
  
  return ret;
  
} /* end move_to_ccr */


static int
moveb (
       MDu8 const dreg,
       MDu8 const dmode,
       MDu8 const smode,
       MDu8 const sreg
       )
{
  
  int ret;
  MDu8 tmp;
  MDu32 addr;
  
  
  tmp= 0; /* CALLA! */
  
  /* Temps inicial. */
  ret= 4;
  
  /* Source. */
  switch ( smode )
    {
    case 0: tmp= _regs.D[sreg].b.v0; break;
    case 1:
      _warning ( _udata, "MOVE.b no accepta en 'source' el mode %d", smode );
      return UTIME;
    case 2: tmp= MD_mem_read8 ( _regs.A[sreg].v ); ret+= 4; break;
    case 3:
      tmp= MD_mem_read8 ( _regs.A[sreg].v );
      if ( sreg == 7 ) _regs.A[sreg].v+= 2;
      else             ++(_regs.A[sreg].v);
      ret+= 4;
      break;
    case 4:
      addr= (sreg==7) ? (_regs.A[sreg].v-= 2) : --(_regs.A[sreg].v);
      tmp= MD_mem_read8 ( addr );
      ret+= 6;
      break;
    case 5: tmp= MD_mem_read8 ( pd16an ( _regs.A[sreg] ).v ); ret+= 8; break;
    case 6:
      addr= _regs.A[sreg].v + calc_8bit_displacement ();
      tmp= MD_mem_read8 ( addr );
      ret+= 10;
      break;
    case 7:
      switch ( sreg )
        {
        case 0: tmp= MD_mem_read8 ( pw ().v ); ret+= 8; break;
        case 1: tmp= MD_mem_read8 ( pl ().v ); ret+= 12; break;
        case 2: tmp= MD_mem_read8 ( pd16pc ().v ); ret+= 8; break;
        case 3:
          addr= _regs.PC;
          addr+= calc_8bit_displacement ();
          tmp= MD_mem_read8 ( addr );
          ret+= 10;
          break;
        case 4: tmp= inm_byte (); ret+= 4; break;
        default:
          _warning ( _udata,
        	     "MOVE.b no accepta en 'source' mode:reg 7:%d", sreg );
          return UTIME;
        }
      break;
    default: break;
    }
  
  /* Destination. */
  switch ( dmode )
    {
    case 0: _regs.D[dreg].b.v0= tmp; break;
    case 1:
      _warning ( _udata,
        	 "MOVE.b no accepta en 'destination' el mode %d", smode );
      return UTIME;
    case 2: MD_mem_write8 ( _regs.A[dreg].v, tmp ); ret+= 4; break;
    case 3:
      MD_mem_write8 ( _regs.A[dreg].v, tmp );
      if ( dreg == 7 ) _regs.A[dreg].v+= 2;
      else             ++(_regs.A[dreg].v);
      ret+= 4;
      break;
    case 4:
      addr= (dreg==7) ? (_regs.A[dreg].v-= 2) : --(_regs.A[dreg].v);
      MD_mem_write8 ( addr, tmp );
      ret+= 4;
      break;
    case 5: MD_mem_write8 ( pd16an ( _regs.A[dreg] ).v, tmp ); ret+= 8; break;
    case 6:
      addr= _regs.A[dreg].v + calc_8bit_displacement ();
      MD_mem_write8 ( addr, tmp );
      ret+= 10;
      break;
    case 7:
      switch ( dreg )
        {
        case 0: MD_mem_write8 ( pw ().v, tmp ); ret+= 8; break;
        case 1: MD_mem_write8 ( pl ().v, tmp ); ret+= 12; break;
        default:
          _warning ( _udata,
        	     "MOVE.b no accepta en 'destination' mode:reg 7:%d", dreg );
          return UTIME;
        }
      break;
    default: break;
    }
  
  /* Flags. */
  CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
  _regs.CCR.v|= ((tmp==0x00)?ZFLAG:0) | ((tmp&0x80)?NFLAG:0);
  
  return ret;
  
} /* end moveb */


static void
movel_setflags (
        	MD_Reg32 const reg
        	)
{
  
  CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
  _regs.CCR.v|=
    ((reg.v==0x00000000)?ZFLAG:0) | ((reg.v&0x80000000)?NFLAG:0);
  
} /* end movel_setflags */


static int
movel (
       MDu8 const dreg,
       MDu8 const dmode,
       MDu8 const smode,
       MDu8 const sreg
       )
{
  
  int ret;
  MD_Reg32 tmp;
  MDu32 addr;
  MD_Bool movea;
  
  
  /* Temps inicial. */
  ret= 4;
  movea= MD_FALSE;
  
  /* Source. */
  switch ( smode )
    {
    case 0: tmp= _regs.D[sreg]; break;
    case 1: tmp= _regs.A[sreg]; break;
    case 2: tmp= read_long ( _regs.A[sreg].v ); ret+= 8; break;
    case 3:
      tmp= read_long ( _regs.A[sreg].v );
      _regs.A[sreg].v+= 4;
      ret+= 8;
      break;
    case 4:
      tmp= read_long ( _regs.A[sreg].v-= 4 );
      ret+= 10;
      break;
    case 5: tmp= read_long ( pd16an ( _regs.A[sreg] ).v ); ret+= 12; break;
    case 6:
      addr= _regs.A[sreg].v + calc_8bit_displacement ();
      tmp= read_long ( addr );
      ret+= 14;
      break;
    case 7:
      switch ( sreg )
        {
        case 0: tmp= read_long ( pw ().v ); ret+= 12; break;
        case 1: tmp= read_long ( pl ().v ); ret+= 16; break;
        case 2: tmp= read_long ( pd16pc ().v ); ret+= 12; break;
        case 3:
          addr= _regs.PC;
          addr+= calc_8bit_displacement ();
          tmp= read_long ( addr );
          ret+= 14;
          break;
        case 4: tmp= inm_long (); ret+= 8; break;
        default:
          _warning ( _udata,
        	     "MOVE.l no accepta en 'source' mode:reg 7:%d", sreg );
          return UTIME;
        }
      break;
    default: tmp.v= 0; break;
    }
  
  /* Destination. */
  switch ( dmode )
    {
    case 0: _regs.D[dreg]= tmp; break;
    case 1: _regs.A[dreg]= tmp; movea= MD_TRUE; break;
    case 2: write_long ( tmp, _regs.A[dreg].v ); ret+= 8; break;
    case 3:
      write_long ( tmp, _regs.A[dreg].v );
      _regs.A[dreg].v+= 4;
      ret+= 8;
      break;
    case 4:
      write_long ( tmp, _regs.A[dreg].v-= 4 );
      ret+= 10;
      break;
    case 5: write_long ( tmp, pd16an ( _regs.A[dreg] ).v ); ret+= 12; break;
    case 6:
      addr= _regs.A[dreg].v + calc_8bit_displacement ();
      write_long ( tmp, addr );
      ret+= 14;
      break;
    case 7:
      switch ( dreg )
        {
        case 0: write_long ( tmp, pw ().v ); ret+= 12; break;
        case 1: write_long ( tmp, pl ().v ); ret+= 16; break;
        default:
          _warning ( _udata,
        	     "MOVE.l no accepta en 'destination' mode:reg 7:%d", dreg );
          return UTIME;
        }
      break;
    default: break;
    }
  
  /* Flags. */
  if ( !movea ) movel_setflags ( tmp );
  
  return ret;
  
} /* end movel */


static int moveml_addr_l (MDu32 addr,const MD_Word mask)
{
  int ret;
  ret= 0;
  if ( mask.v&0x0001 ) {_regs.D[0]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x0002 ) {_regs.D[1]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x0004 ) {_regs.D[2]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x0008 ) {_regs.D[3]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x0010 ) {_regs.D[4]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x0020 ) {_regs.D[5]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x0040 ) {_regs.D[6]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x0080 ) {_regs.D[7]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x0100 ) {_regs.A[0]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x0200 ) {_regs.A[1]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x0400 ) {_regs.A[2]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x0800 ) {_regs.A[3]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x1000 ) {_regs.A[4]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x2000 ) {_regs.A[5]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x4000 ) {_regs.A[6]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x8000 ) {_regs.A[7]=read_long(addr); addr+=4; ret+=8;}
  return ret;
}


static int moveml_pani_l (MD_Reg32 * const reg,const MD_Word mask)
{
  MDu32 addr; int ret;
  addr= reg->v;
  ret= 0;
  if ( mask.v&0x0001 ) {_regs.D[0]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x0002 ) {_regs.D[1]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x0004 ) {_regs.D[2]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x0008 ) {_regs.D[3]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x0010 ) {_regs.D[4]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x0020 ) {_regs.D[5]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x0040 ) {_regs.D[6]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x0080 ) {_regs.D[7]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x0100 ) {_regs.A[0]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x0200 ) {_regs.A[1]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x0400 ) {_regs.A[2]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x0800 ) {_regs.A[3]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x1000 ) {_regs.A[4]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x2000 ) {_regs.A[5]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x4000 ) {_regs.A[6]=read_long(addr); addr+=4; ret+=8;}
  if ( mask.v&0x8000 ) {_regs.A[7]=read_long(addr); addr+=4; ret+=8;}
  reg->v= addr;
  return ret;
}


static int
moveml_mem_reg (
        	MDu8 const mode,
        	MDu8 const reg
        	)
{
  
  int ret;
  MD_Word mask;
  MDu32 addr;
  
  
  mask= inm_word ();
  switch ( mode )
    {
    case 2: addr= _regs.A[reg].v; ret= 12; break;
    case 3: return 12 + moveml_pani_l ( &(_regs.A[reg]), mask );
    case 5: addr= pd16an ( _regs.A[reg] ).v; ret= 16; break;
    case 6: addr= _regs.A[reg].v + calc_8bit_displacement (); ret= 18; break;
    case 7:
      switch ( reg )
        {
        case 0: addr= pw ().v; ret= 16; break;
        case 1: addr= pl ().v; ret= 20; break;
        case 2: addr= pd16pc ().v; ret= 16; break;
        case 3:
          addr= _regs.PC;
          addr+= calc_8bit_displacement ();
          ret= 18;
          break;
        default:
          _warning ( _udata,
        	     "MOVEM.l <ea>,<list> no accepta el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "MOVEM.l <ea>,<list> no accepta el mode %d", mode );
      return UTIME;
    }
  
  return ret + moveml_addr_l ( addr, mask );
  
} /* end moveml_mem_reg */


static int moveml_l_addr (MDu32 addr,const MD_Word mask)
{
  int ret;
  ret= 0;
  if ( mask.v&0x0001 ) {write_long(_regs.D[0],addr); addr+=4; ret+=10;}
  if ( mask.v&0x0002 ) {write_long(_regs.D[1],addr); addr+=4; ret+=10;}
  if ( mask.v&0x0004 ) {write_long(_regs.D[2],addr); addr+=4; ret+=10;}
  if ( mask.v&0x0008 ) {write_long(_regs.D[3],addr); addr+=4; ret+=10;}
  if ( mask.v&0x0010 ) {write_long(_regs.D[4],addr); addr+=4; ret+=10;}
  if ( mask.v&0x0020 ) {write_long(_regs.D[5],addr); addr+=4; ret+=10;}
  if ( mask.v&0x0040 ) {write_long(_regs.D[6],addr); addr+=4; ret+=10;}
  if ( mask.v&0x0080 ) {write_long(_regs.D[7],addr); addr+=4; ret+=10;}
  if ( mask.v&0x0100 ) {write_long(_regs.A[0],addr); addr+=4; ret+=10;}
  if ( mask.v&0x0200 ) {write_long(_regs.A[1],addr); addr+=4; ret+=10;}
  if ( mask.v&0x0400 ) {write_long(_regs.A[2],addr); addr+=4; ret+=10;}
  if ( mask.v&0x0800 ) {write_long(_regs.A[3],addr); addr+=4; ret+=10;}
  if ( mask.v&0x1000 ) {write_long(_regs.A[4],addr); addr+=4; ret+=10;}
  if ( mask.v&0x2000 ) {write_long(_regs.A[5],addr); addr+=4; ret+=10;}
  if ( mask.v&0x4000 ) {write_long(_regs.A[6],addr); addr+=4; ret+=10;}
  if ( mask.v&0x8000 ) {write_long(_regs.A[7],addr); addr+=4; ret+=10;}
  return ret;
}


static int moveml_l_pand (MD_Reg32 * const reg,const MD_Word mask)
{
  MDu32 addr; int ret;
  addr= reg->v-4;
  ret= 0;
  if ( mask.v&0x0001 ) {write_long(_regs.A[7],addr); addr-=4; ret+=10;}
  if ( mask.v&0x0002 ) {write_long(_regs.A[6],addr); addr-=4; ret+=10;}
  if ( mask.v&0x0004 ) {write_long(_regs.A[5],addr); addr-=4; ret+=10;}
  if ( mask.v&0x0008 ) {write_long(_regs.A[4],addr); addr-=4; ret+=10;}
  if ( mask.v&0x0010 ) {write_long(_regs.A[3],addr); addr-=4; ret+=10;}
  if ( mask.v&0x0020 ) {write_long(_regs.A[2],addr); addr-=4; ret+=10;}
  if ( mask.v&0x0040 ) {write_long(_regs.A[1],addr); addr-=4; ret+=10;}
  if ( mask.v&0x0080 ) {write_long(_regs.A[0],addr); addr-=4; ret+=10;}
  if ( mask.v&0x0100 ) {write_long(_regs.D[7],addr); addr-=4; ret+=10;}
  if ( mask.v&0x0200 ) {write_long(_regs.D[6],addr); addr-=4; ret+=10;}
  if ( mask.v&0x0400 ) {write_long(_regs.D[5],addr); addr-=4; ret+=10;}
  if ( mask.v&0x0800 ) {write_long(_regs.D[4],addr); addr-=4; ret+=10;}
  if ( mask.v&0x1000 ) {write_long(_regs.D[3],addr); addr-=4; ret+=10;}
  if ( mask.v&0x2000 ) {write_long(_regs.D[2],addr); addr-=4; ret+=10;}
  if ( mask.v&0x4000 ) {write_long(_regs.D[1],addr); addr-=4; ret+=10;}
  if ( mask.v&0x8000 ) {write_long(_regs.D[0],addr); addr-=4; ret+=10;}
  reg->v= addr+4; /* No estic segur d'acò. */
  return ret;
}


static int
moveml_reg_mem (
        	MDu8 const mode,
        	MDu8 const reg
        	)
{
  
  int ret;
  MD_Word mask;
  MDu32 addr;
  
  
  mask= inm_word ();
  switch ( mode )
    {
    case 2: addr= _regs.A[reg].v; ret= 8; break;
    case 4: return 8 + moveml_l_pand ( &(_regs.A[reg]), mask );
    case 5: addr= pd16an ( _regs.A[reg] ).v; ret= 12; break;
    case 6: addr= _regs.A[reg].v + calc_8bit_displacement (); ret= 14; break;
    case 7:
      switch ( reg )
        {
        case 0: addr= pw ().v; ret= 12; break;
        case 1: addr= pl ().v; ret= 16; break;
        default:
          _warning ( _udata,
        	     "MOVEM.l <list>,<ea> no accepta el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "MOVEM.l <list>,<ea> no accepta el mode %d", mode );
      return UTIME;
    }
  
  return ret + moveml_l_addr ( addr, mask );
  
} /* end moveml_reg_mem */


static int movemw_addr_l (MDu32 addr,const MD_Word mask)
{
  int ret;
  ret= 0;
  if (mask.v&0x0001) {_regs.D[0].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x0002) {_regs.D[1].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x0004) {_regs.D[2].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x0008) {_regs.D[3].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x0010) {_regs.D[4].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x0020) {_regs.D[5].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x0040) {_regs.D[6].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x0080) {_regs.D[7].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x0100) {_regs.A[0].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x0200) {_regs.A[1].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x0400) {_regs.A[2].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x0800) {_regs.A[3].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x1000) {_regs.A[4].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x2000) {_regs.A[5].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x4000) {_regs.A[6].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x8000) {_regs.A[7].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  return ret;
}


static int movemw_pani_l (MD_Reg32 * const reg,const MD_Word mask)
{
  MDu32 addr; int ret;
  addr= reg->v;
  ret= 0;
  if (mask.v&0x0001) {_regs.D[0].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x0002) {_regs.D[1].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x0004) {_regs.D[2].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x0008) {_regs.D[3].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x0010) {_regs.D[4].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x0020) {_regs.D[5].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x0040) {_regs.D[6].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x0080) {_regs.D[7].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x0100) {_regs.A[0].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x0200) {_regs.A[1].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x0400) {_regs.A[2].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x0800) {_regs.A[3].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x1000) {_regs.A[4].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x2000) {_regs.A[5].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x4000) {_regs.A[6].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  if (mask.v&0x8000) {_regs.A[7].v=(MDs16)MD_mem_read(addr).v; addr+=2; ret+=4;}
  reg->v= addr;
  return ret;
}


static int
movemw_mem_reg (
        	MDu8 const mode,
        	MDu8 const reg
        	)
{
  
  int ret;
  MD_Word mask;
  MDu32 addr;
  
  
  mask= inm_word ();
  switch ( mode )
    {
    case 2: addr= _regs.A[reg].v; ret= 12; break;
    case 3: return 12 + movemw_pani_l ( &(_regs.A[reg]), mask );
    case 5: addr= pd16an ( _regs.A[reg] ).v; ret= 16; break;
    case 6: addr= _regs.A[reg].v + calc_8bit_displacement (); ret= 18; break;
    case 7:
      switch ( reg )
        {
        case 0: addr= pw ().v; ret= 16; break;
        case 1: addr= pl ().v; ret= 20; break;
        case 2: addr= pd16pc ().v; ret= 16; break;
        case 3:
          addr= _regs.PC;
          addr+= calc_8bit_displacement ();
          ret= 18;
          break;
        default:
          _warning ( _udata,
        	     "MOVEM.w <ea>,<list> no accepta el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "MOVEM.w <ea>,<list> no accepta el mode %d", mode );
      return UTIME;
    }
  
  return ret + movemw_addr_l ( addr, mask );
  
} /* end movemw_mem_reg */


static int movemw_l_addr (MDu32 addr,MD_Word const mask)
{
  int ret;
  ret= 0;
  if ( mask.v&0x0001 ) {MD_mem_write(addr,_regs.D[0].w.v0); addr+=2; ret+=5;}
  if ( mask.v&0x0002 ) {MD_mem_write(addr,_regs.D[1].w.v0); addr+=2; ret+=5;}
  if ( mask.v&0x0004 ) {MD_mem_write(addr,_regs.D[2].w.v0); addr+=2; ret+=5;}
  if ( mask.v&0x0008 ) {MD_mem_write(addr,_regs.D[3].w.v0); addr+=2; ret+=5;}
  if ( mask.v&0x0010 ) {MD_mem_write(addr,_regs.D[4].w.v0); addr+=2; ret+=5;}
  if ( mask.v&0x0020 ) {MD_mem_write(addr,_regs.D[5].w.v0); addr+=2; ret+=5;}
  if ( mask.v&0x0040 ) {MD_mem_write(addr,_regs.D[6].w.v0); addr+=2; ret+=5;}
  if ( mask.v&0x0080 ) {MD_mem_write(addr,_regs.D[7].w.v0); addr+=2; ret+=5;}
  if ( mask.v&0x0100 ) {MD_mem_write(addr,_regs.A[0].w.v0); addr+=2; ret+=5;}
  if ( mask.v&0x0200 ) {MD_mem_write(addr,_regs.A[1].w.v0); addr+=2; ret+=5;}
  if ( mask.v&0x0400 ) {MD_mem_write(addr,_regs.A[2].w.v0); addr+=2; ret+=5;}
  if ( mask.v&0x0800 ) {MD_mem_write(addr,_regs.A[3].w.v0); addr+=2; ret+=5;}
  if ( mask.v&0x1000 ) {MD_mem_write(addr,_regs.A[4].w.v0); addr+=2; ret+=5;}
  if ( mask.v&0x2000 ) {MD_mem_write(addr,_regs.A[5].w.v0); addr+=2; ret+=5;}
  if ( mask.v&0x4000 ) {MD_mem_write(addr,_regs.A[6].w.v0); addr+=2; ret+=5;}
  if ( mask.v&0x8000 ) {MD_mem_write(addr,_regs.A[7].w.v0); addr+=2; ret+=5;}
  return ret;
}


static int movemw_l_pand (MD_Reg32 * const reg,const MD_Word mask)
{
  MDu32 addr; int ret;
  addr= reg->v-2;
  ret= 0;
  if ( mask.v&0x0001 ) {MD_mem_write(addr,_regs.A[7].w.v0); addr-=2; ret+=5;}
  if ( mask.v&0x0002 ) {MD_mem_write(addr,_regs.A[6].w.v0); addr-=2; ret+=5;}
  if ( mask.v&0x0004 ) {MD_mem_write(addr,_regs.A[5].w.v0); addr-=2; ret+=5;}
  if ( mask.v&0x0008 ) {MD_mem_write(addr,_regs.A[4].w.v0); addr-=2; ret+=5;}
  if ( mask.v&0x0010 ) {MD_mem_write(addr,_regs.A[3].w.v0); addr-=2; ret+=5;}
  if ( mask.v&0x0020 ) {MD_mem_write(addr,_regs.A[2].w.v0); addr-=2; ret+=5;}
  if ( mask.v&0x0040 ) {MD_mem_write(addr,_regs.A[1].w.v0); addr-=2; ret+=5;}
  if ( mask.v&0x0080 ) {MD_mem_write(addr,_regs.A[0].w.v0); addr-=2; ret+=5;}
  if ( mask.v&0x0100 ) {MD_mem_write(addr,_regs.D[7].w.v0); addr-=2; ret+=5;}
  if ( mask.v&0x0200 ) {MD_mem_write(addr,_regs.D[6].w.v0); addr-=2; ret+=5;}
  if ( mask.v&0x0400 ) {MD_mem_write(addr,_regs.D[5].w.v0); addr-=2; ret+=5;}
  if ( mask.v&0x0800 ) {MD_mem_write(addr,_regs.D[4].w.v0); addr-=2; ret+=5;}
  if ( mask.v&0x1000 ) {MD_mem_write(addr,_regs.D[3].w.v0); addr-=2; ret+=5;}
  if ( mask.v&0x2000 ) {MD_mem_write(addr,_regs.D[2].w.v0); addr-=2; ret+=5;}
  if ( mask.v&0x4000 ) {MD_mem_write(addr,_regs.D[1].w.v0); addr-=2; ret+=5;}
  if ( mask.v&0x8000 ) {MD_mem_write(addr,_regs.D[0].w.v0); addr-=2; ret+=5;}
  reg->v= addr+2; /* No estic segur d'acò. */
  return ret;
}


static int
movemw_reg_mem (
        	MDu8 const mode,
        	MDu8 const reg
        	)
{
  
  int ret;
  MD_Word mask;
  MDu32 addr;
  
  
  mask= inm_word ();
  switch ( mode )
    {
    case 2: addr= _regs.A[reg].v; ret= 8; break;
    case 4: return 8 + movemw_l_pand ( &(_regs.A[reg]), mask );
    case 5: addr= pd16an ( _regs.A[reg] ).v; ret= 12; break;
    case 6: addr= _regs.A[reg].v + calc_8bit_displacement (); ret= 14; break;
    case 7:
      switch ( reg )
        {
        case 0: addr= pw ().v; ret= 12; break;
        case 1: addr= pl ().v; ret= 16; break;
        default:
          _warning ( _udata,
        	     "MOVEM.w <list>,<ea> no accepta el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "MOVEM.w <list>,<ea> no accepta el mode %d", mode );
      return UTIME;
    }
  
  return ret + movemw_l_addr ( addr, mask );
  
} /* end movemw_reg_mem */


static int
movepl_mem_reg (
        	MDu8 const dreg,
        	MDu8 const areg
        	)
{
  
  MDu32 addr;
  
  
  addr= pd16an ( _regs.A[areg] ).v;
  _regs.D[dreg].w.v1.b.v1= MD_mem_read8 ( addr );
  _regs.D[dreg].w.v1.b.v0= MD_mem_read8 ( addr+2 );
  _regs.D[dreg].w.v0.b.v1= MD_mem_read8 ( addr+4 );
  _regs.D[dreg].w.v0.b.v0= MD_mem_read8 ( addr+6 );
  
  return 24;
  
} /* end movepl_mem_reg */


static int
movepl_reg_mem (
        	MDu8 const dreg,
        	MDu8 const areg
        	)
{
  
  MDu32 addr;
  
  
  addr= pd16an ( _regs.A[areg] ).v;
  MD_mem_write8 ( addr, _regs.D[dreg].w.v1.b.v1 );
  MD_mem_write8 ( addr+2, _regs.D[dreg].w.v1.b.v0 );
  MD_mem_write8 ( addr+4, _regs.D[dreg].w.v0.b.v1 );
  MD_mem_write8 ( addr+6, _regs.D[dreg].w.v0.b.v0 );
  
  return 24;
  
} /* end movepl_reg_mem */


static int
movepw_mem_reg (
        	MDu8 const dreg,
        	MDu8 const areg
        	)
{
  
  MDu32 addr;
  
  
  addr= pd16an ( _regs.A[areg] ).v;
  _regs.D[dreg].w.v0.b.v1= MD_mem_read8 ( addr );
  _regs.D[dreg].w.v0.b.v0= MD_mem_read8 ( addr+2 );
  
  return 16;
  
} /* end movepw_mem_reg */


static int
movepw_reg_mem (
        	MDu8 const dreg,
        	MDu8 const areg
        	)
{
  
  MDu32 addr;
  
  
  addr= pd16an ( _regs.A[areg] ).v;
  MD_mem_write8 ( addr, _regs.D[dreg].w.v0.b.v1 );
  MD_mem_write8 ( addr+2, _regs.D[dreg].w.v0.b.v0 );
  
  return 16;
  
} /* end movepw_reg_mem */


static int
movew (
       MDu8 const dreg,
       MDu8 const dmode,
       MDu8 const smode,
       MDu8 const sreg
       )
{
  
  int ret;
  MD_Word tmp;
  MDu32 addr;
  MD_Bool movea;
  
  
  /* Temps inicial. */
  ret= 4;
  movea= MD_FALSE;
  
  /* Source. */
  switch ( smode )
    {
    case 0: tmp= _regs.D[sreg].w.v0; break;
    case 1: tmp= _regs.A[sreg].w.v0; break;
    case 2: tmp= MD_mem_read ( _regs.A[sreg].v ); ret+= 4; break;
    case 3:
      tmp= MD_mem_read ( _regs.A[sreg].v );
      _regs.A[sreg].v+= 2;
      ret+= 4;
      break;
    case 4:
      tmp= MD_mem_read ( _regs.A[sreg].v-= 2 );
      ret+= 6;
      break;
    case 5: tmp= MD_mem_read ( pd16an ( _regs.A[sreg] ).v ); ret+= 8; break;
    case 6:
      addr= _regs.A[sreg].v + calc_8bit_displacement ();
      tmp= MD_mem_read ( addr );
      ret+= 10;
      break;
    case 7:
      switch ( sreg )
        {
        case 0: tmp= MD_mem_read ( pw ().v ); ret+= 8; break;
        case 1: tmp= MD_mem_read ( pl ().v ); ret+= 12; break;
        case 2: tmp= MD_mem_read ( pd16pc ().v ); ret+= 8; break;
        case 3:
          addr= _regs.PC;
          addr+= calc_8bit_displacement ();
          tmp= MD_mem_read ( addr );
          ret+= 10;
          break;
        case 4: tmp= inm_word (); ret+= 4; break;
        default:
          _warning ( _udata,
        	     "MOVE.w no accepta en 'source' mode:reg 7:%d", sreg );
          return UTIME;
        }
      break;
    default: tmp.v= 0; break;
    }
  
  /* Destination. */
  switch ( dmode )
    {
    case 0: _regs.D[dreg].w.v0= tmp; break;
    case 1: _regs.A[dreg].v= (MDs16) tmp.v; movea= MD_TRUE; break;
    case 2: MD_mem_write ( _regs.A[dreg].v, tmp ); ret+= 4; break;
    case 3:
      MD_mem_write ( _regs.A[dreg].v, tmp );
      _regs.A[dreg].v+= 2;
      ret+= 4;
      break;
    case 4:
      MD_mem_write ( _regs.A[dreg].v-= 2, tmp );
      ret+= 4;
      break;
    case 5: MD_mem_write ( pd16an ( _regs.A[dreg] ).v, tmp ); ret+= 8; break;
    case 6:
      addr= _regs.A[dreg].v + calc_8bit_displacement ();
      MD_mem_write ( addr, tmp );
      ret+= 10;
      break;
    case 7:
      switch ( dreg )
        {
        case 0: MD_mem_write ( pw ().v, tmp ); ret+= 8; break;
        case 1: MD_mem_write ( pl ().v, tmp ); ret+= 12; break;
        default:
          _warning ( _udata,
        	     "MOVE.w no accepta en 'destination' mode:reg 7:%d", dreg );
          return UTIME;
        }
      break;
    default: break;
    }
  
  /* Flags. */
  if ( !movea )
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|= ((tmp.v==0x0000)?ZFLAG:0) | ((tmp.v&0x8000)?NFLAG:0);
    }
  
  return ret;
  
} /* end movew */


static int
pea (
     MDu8 const mode,
     MDu8 const reg
     )
{
  
  MD_Reg32 addr;
  int ret;
  
  
  switch ( mode )
    {
    case 2: addr.v= _regs.A[reg].v; ret= 12; break;
    case 5: addr.v= pd16an ( _regs.A[reg] ).v; ret= 16; break;
    case 6: addr.v= _regs.A[reg].v + calc_8bit_displacement (); ret= 20; break;
    case 7:
      switch ( reg )
        {
        case 0: addr.v= pw ().v; ret= 16; break;
        case 1: addr.v= pl ().v; ret= 20; break;
        case 2: addr.v= pd16pc ().v; ret= 16; break;
        case 3:
          addr.v= _regs.PC;
          addr.v+= calc_8bit_displacement ();
          ret= 20;
          break;
        default:
          _warning ( _udata,
        	     "PEA no accepta el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "PEA no accepta el mode %d", mode );
      return UTIME;
    }
  
  _regs.A[7].v-= 4;
  write_long ( addr, _regs.A[7].v );
  
  return ret;
  
} /* end pea */


static int
unlk (
      MDu8 const reg
      )
{
  
  _regs.A[7]= _regs.A[reg];
  _regs.A[reg]= read_long ( _regs.A[7].v );
  _regs.A[7].v+= 4;
  
  return 12;
  
} /* end unlk */


/* Integer Arithmetic Instructions ********************************************/
static MDu32
add (
     const MDu32 a,
     const MDu32 b
     )
{
  return a+b;
} /* end add */


static MDu8
addb (
      const MDu8 a,
      const MDu8 b
      )
{
  
  MDu32 aux;
  
  
  aux= a + b;
  CLEAR_FLAGS ( XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG );
  _regs.CCR.v|=
    ((aux&0x100)?(CFLAG|XFLAG):0) |
    (((~(a^b))&(aux^a)&0x80)?VFLAG:0) |
    ((aux&0xFF)?0:ZFLAG) |
    ((aux&0x80)?NFLAG:0);
  
  return (MDu8) aux; /* No cal fer &0xFF */
  
} /* end addb */


static MDu8
addb_x (
        const MDu8 a,
        const MDu8 b
        )
{
  
  MDu32 aux;
  
  
  aux= a + b + ((_regs.CCR.v&XFLAG)?1:0);
  CLEAR_FLAGS ( XFLAG|NFLAG|VFLAG|CFLAG );
  _regs.CCR.v|=
    ((aux&0x100)?(CFLAG|XFLAG):0) |
    (((~(a^b))&(aux^a)&0x80)?VFLAG:0) |
    ((aux&0x80)?NFLAG:0);
  if ( aux&0xFF ) CLEAR_FLAGS ( ZFLAG );
  
  return (MDu8) aux; /* No cal fer &0xFF */
  
} /* end addb_x */


static MD_Reg32
addl (
      const MD_Reg32 a,
      const MD_Reg32 b
      )
{
  
  MD_Reg32 aux;
  
  
  aux.v= a.v + b.v;
  CLEAR_FLAGS ( XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG );
  _regs.CCR.v|=
    ((((a.v&b.v)|((a.v^b.v)&(~aux.v)))&0x80000000)?(CFLAG|XFLAG):0) |
    (((~(a.v^b.v))&(aux.v^a.v)&0x80000000)?VFLAG:0) |
    (aux.v?0:ZFLAG) |
    ((aux.v&0x80000000)?NFLAG:0);
  
  return aux;
  
} /* end addl */


static MD_Reg32
addl_x (
        const MD_Reg32 a,
        const MD_Reg32 b
        )
{
  
  MD_Reg32 aux;
  
  
  aux.v= a.v + b.v + ((_regs.CCR.v&XFLAG)?1:0);
  CLEAR_FLAGS ( XFLAG|NFLAG|VFLAG|CFLAG );
  _regs.CCR.v|=
    ((((a.v&b.v)|((a.v^b.v)&(~aux.v)))&0x80000000)?(CFLAG|XFLAG):0) |
    (((~(a.v^b.v))&(aux.v^a.v)&0x80000000)?VFLAG:0) |
    ((aux.v&0x80000000)?NFLAG:0);
  if ( aux.v ) CLEAR_FLAGS ( ZFLAG );
  
  return aux;
  
} /* end addl_x */


static MD_Word
addw (
      const MD_Word a,
      const MD_Word b
      )
{
  
  MD_Reg32 aux;
  
  
  aux.v= a.v + b.v;
  CLEAR_FLAGS ( XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG );
  _regs.CCR.v|=
    ((aux.v&0x10000)?(CFLAG|XFLAG):0) |
    (((~(a.v^b.v))&(aux.v^a.v)&0x8000)?VFLAG:0) |
    (aux.w.v0.v?0:ZFLAG) |
    ((aux.v&0x8000)?NFLAG:0);
  
  return aux.w.v0;
  
} /* addw */


static MD_Word
addw_x (
        const MD_Word a,
        const MD_Word b
        )
{
  
  MD_Reg32 aux;
  
  
  aux.v= a.v + b.v + ((_regs.CCR.v&XFLAG)?1:0);
  CLEAR_FLAGS ( XFLAG|NFLAG|VFLAG|CFLAG );
  _regs.CCR.v|=
    ((aux.v&0x10000)?(CFLAG|XFLAG):0) |
    (((~(a.v^b.v))&(aux.v^a.v)&0x8000)?VFLAG:0) |
    ((aux.v&0x8000)?NFLAG:0);
  if ( aux.w.v0.v ) CLEAR_FLAGS ( ZFLAG );
  
  return aux.w.v0;
  
} /* addw_x */


static MDu32
sub (
     const MDu32 src,
     const MDu32 dst
     )
{
  return dst+C1L(src)+1;
} /* end sub */


/* dst-src*/
static MDu8
subb (
      const MDu8 src,
      const MDu8 dst
      )
{
  
  MDu8 neg;
  MDu32 aux;
  
  
  neg= C1B(src);
  aux= (dst + neg) + 1;
  CLEAR_FLAGS ( XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG );
  _regs.CCR.v|=
    (((~aux)&0x100)?(CFLAG|XFLAG):0) |
    (((~(neg^dst))&(aux^neg)&0x80)?VFLAG:0) |
    ((aux&0xFF)?0:ZFLAG) |
    ((aux&0x80)?NFLAG:0);
  
  return (MDu8) aux; /* No cal fer &0xFF */
  
} /* end subb */


static MDu8
subb_x (
        const MDu8 src,
        const MDu8 dst
        )
{
  
  MDu8 neg;
  MDu32 aux;
  
  
  neg= C1B(src);
  aux= (dst + neg) + ((_regs.CCR.v&XFLAG)?0:1);
  CLEAR_FLAGS ( XFLAG|NFLAG|VFLAG|CFLAG );
  _regs.CCR.v|=
    (((~aux)&0x100)?(CFLAG|XFLAG):0) |
    (((~(neg^dst))&(aux^neg)&0x80)?VFLAG:0) |
    ((aux&0x80)?NFLAG:0);
  if ( aux&0xFF ) CLEAR_FLAGS ( ZFLAG );
  
  return (MDu8) aux; /* No cal fer &0xFF */
  
} /* end subb_x */


/* dst-src */
static MD_Reg32
subl (
      const MD_Reg32 src,
      const MD_Reg32 dst
      )
{
  
  MDu32 neg;
  MD_Reg32 aux;
  
  
  neg= C1L(src.v);
  aux.v= (dst.v + neg) + 1;
  CLEAR_FLAGS ( XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG );
  _regs.CCR.v|=
    ((~(((neg&dst.v)|((neg^dst.v)&(~aux.v))))&0x80000000)?(CFLAG|XFLAG):0) |
    (((~(neg^dst.v))&(aux.v^neg)&0x80000000)?VFLAG:0) |
    (aux.v?0:ZFLAG) |
    ((aux.v&0x80000000)?NFLAG:0);
  
  return aux;
  
} /* end subl */


static MD_Reg32
subl_x (
        const MD_Reg32 src,
        const MD_Reg32 dst
        )
{
  
  MDu32 neg;
  MD_Reg32 aux;
  
  
  neg= C1L(src.v);
  aux.v= (dst.v + neg) + ((_regs.CCR.v&XFLAG)?0:1);
  CLEAR_FLAGS ( XFLAG|NFLAG|VFLAG|CFLAG );
  _regs.CCR.v|=
    ((~(((neg&dst.v)|((neg^dst.v)&(~aux.v))))&0x80000000)?(CFLAG|XFLAG):0) |
    (((~(neg^dst.v))&(aux.v^neg)&0x80000000)?VFLAG:0) |
    ((aux.v&0x80000000)?NFLAG:0);
  if ( aux.v ) CLEAR_FLAGS ( ZFLAG );
  
  return aux;
  
} /* end subl_x */


/* dst-src */
static MD_Word
subw (
      const MD_Word src,
      const MD_Word dst
      )
{
  
  MD_Word neg;
  MD_Reg32 aux;
  
  
  neg.v= C1W(src.v);
  aux.v= (dst.v + neg.v) + 1;
  CLEAR_FLAGS ( XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG );
  _regs.CCR.v|=
    (((~aux.v)&0x10000)?(CFLAG|XFLAG):0) |
    (((~(neg.v^dst.v))&(aux.v^neg.v)&0x8000)?VFLAG:0) |
    (aux.w.v0.v?0:ZFLAG) |
    ((aux.v&0x8000)?NFLAG:0);
  
  return aux.w.v0;
  
} /* subw */


static MD_Word
subw_x (
        const MD_Word src,
        const MD_Word dst
        )
{
  
  MD_Word neg;
  MD_Reg32 aux;
  
  
  neg.v= C1W(src.v);
  aux.v= (dst.v + neg.v) + ((_regs.CCR.v&XFLAG)?0:1);
  CLEAR_FLAGS ( XFLAG|NFLAG|VFLAG|CFLAG );
  _regs.CCR.v|=
    (((~aux.v)&0x10000)?(CFLAG|XFLAG):0) |
    (((~(neg.v^dst.v))&(aux.v^neg.v)&0x8000)?VFLAG:0) |
    ((aux.v&0x8000)?NFLAG:0);
  if ( aux.w.v0.v ) CLEAR_FLAGS ( ZFLAG );
  
  return aux.w.v0;
  
} /* subw_x */


static int
extl (
      MDu8 const reg
      )
{
  
  MDu32 aux;

  
  _regs.D[reg].v= aux= (MDs16) _regs.D[reg].w.v0.v;
  CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
  _regs.CCR.v|= ((aux&0x80000000)?NFLAG:0) | ((aux==0)?ZFLAG:0);
  
  return 4;
  
} /* end extl */


static int
extw (
      MDu8 const reg
      )
{
  
  MDu16 aux;

  
  _regs.D[reg].w.v0.v= aux= (MDs8) _regs.D[reg].b.v0;
  CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
  _regs.CCR.v|= ((aux&0x8000)?NFLAG:0) | ((aux==0)?ZFLAG:0);
  
  return 4;
  
} /* end extw */


static int
clrb (
      MDu8 const mode,
      MDu8 const reg
      )
{
  
  MDu32 addr;
  
  
  CLEAR_FLAGS ( NFLAG|VFLAG|CFLAG );
  _regs.CCR.v|= ZFLAG;
  switch ( mode )
    {
    case 0: _regs.D[reg].b.v0= 0; return 4;
    case 2: MD_mem_write8 ( _regs.A[reg].v, 0 ); return 12;
    case 3:
      MD_mem_write8 ( _regs.A[reg].v, 0 );
      if ( reg == 7 ) _regs.A[reg].v+= 2;
      else            ++_regs.A[reg].v;
      return 12;
    case 4:
      addr= (reg==7) ? (_regs.A[reg].v-= 2) : --_regs.A[reg].v;
      MD_mem_write8 ( addr, 0 );
      return 14;
    case 5: MD_mem_write8 ( pd16an ( _regs.A[reg] ).v, 0 ); return 16;
    case 6:
      MD_mem_write8 ( _regs.A[reg].v + calc_8bit_displacement (), 0 );
      return 18;
    case 7:
      switch ( reg )
        {
        case 0: MD_mem_write8 ( pw ().v, 0 ); return 16;
        case 1: MD_mem_write8 ( pl ().v, 0 ); return 20;
        default: _warning ( _udata, "CLR.b no accepta el mode:reg 7:%d", reg );
        }
      break;
    default: _warning ( _udata, "CLR.b no accepta el mode %d", mode );
    }
  
  return UTIME;
  
} /* end clrb */


static int
clrl (
      MDu8 const mode,
      MDu8 const reg
      )
{
  
  MD_Reg32 const zero= { 0 };
  
  CLEAR_FLAGS ( NFLAG|VFLAG|CFLAG );
  _regs.CCR.v|= ZFLAG;
  switch ( mode )
    {
    case 0: _regs.D[reg]= zero; return 6;
    case 2: write_long ( zero, _regs.A[reg].v ); return 20;
    case 3:
      write_long ( zero, _regs.A[reg].v );
      _regs.A[reg].v+= 4;
      return 20;
    case 4: write_long ( zero, _regs.A[reg].v-= 4 ); return 22;
    case 5: write_long ( zero, pd16an ( _regs.A[reg] ).v ); return 24;
    case 6:
      write_long ( zero, _regs.A[reg].v + calc_8bit_displacement () );
      return 26;
    case 7:
      switch ( reg )
        {
        case 0: write_long ( zero, pw ().v ); return 24;
        case 1: write_long ( zero, pl ().v ); return 28;
        default: _warning ( _udata, "CLR.l no accepta el mode:reg 7:%d", reg );
        }
      break;
    default: _warning ( _udata, "CLR.l no accepta el mode %d", mode );
    }
  
  return UTIME;
  
} /* end clrl */


static int
clrw (
      MDu8 const mode,
      MDu8 const reg
      )
{
  
  MD_Word const zero= { 0 };
  
  
  CLEAR_FLAGS ( NFLAG|VFLAG|CFLAG );
  _regs.CCR.v|= ZFLAG;
  switch ( mode )
    {
    case 0: _regs.D[reg].w.v0= zero; return 4;
    case 2: MD_mem_write ( _regs.A[reg].v, zero ); return 12;
    case 3:
      MD_mem_write ( _regs.A[reg].v, zero );
      _regs.A[reg].v+= 2;
      return 12;
    case 4: MD_mem_write ( _regs.A[reg].v-= 2, zero ); return 14;
    case 5: MD_mem_write ( pd16an ( _regs.A[reg] ).v, zero ); return 16;
    case 6:
      MD_mem_write ( _regs.A[reg].v + calc_8bit_displacement (), zero );
      return 18;
    case 7:
      switch ( reg )
        {
        case 0: MD_mem_write ( pw ().v, zero ); return 16;
        case 1: MD_mem_write ( pl ().v, zero ); return 20;
        default: _warning ( _udata, "CLR.w no accepta el mode:reg 7:%d", reg );
        }
      break;
    default: _warning ( _udata, "CLR.w no accepta el mode %d", mode );
    }
  
  return UTIME;
  
} /* end clrw */


static void
cmp (
     MDu32 const src,
     MDu32 const dst
     )
{

  MDu32 neg, aux;
  
  
  neg= C1L(src);
  aux= (dst + neg) + 1;
  CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
  _regs.CCR.v|=
    ((~(((neg&dst)|((neg^dst)&(~aux))))&0x80000000)?(CFLAG):0) |
    (((~(neg^dst))&(aux^neg)&0x80000000)?VFLAG:0) |
    (aux?0:ZFLAG) |
    ((aux&0x80000000)?NFLAG:0);

} /* end cmp */


static void
cmp_byte (
          MDu8 const src,
          MDu8 const dst
          )
{

  MDu8 neg;
  MDu32 aux;
  
  
  neg= C1B(src);
  aux= (dst + neg) + 1;
  CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
  _regs.CCR.v|=
    (((~aux)&0x100)?(CFLAG):0) |
    (((~(neg^dst))&(aux^neg)&0x80)?VFLAG:0) |
    ((aux&0xFF)?0:ZFLAG) |
    ((aux&0x80)?NFLAG:0);

} /* end cmp_byte */


static void
cmp_word (
          MD_Word const src,
          MD_Word const dst
          )
{
  
  MD_Word neg;
  MD_Reg32 aux;
  
  
  neg.v= C1W(src.v);
  aux.v= (dst.v + neg.v) + 1;
  CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
  _regs.CCR.v|=
    (((~aux.v)&0x10000)?(CFLAG):0) |
    (((~(neg.v^dst.v))&(aux.v^neg.v)&0x8000)?VFLAG:0) |
    (aux.w.v0.v?0:ZFLAG) |
    ((aux.v&0x8000)?NFLAG:0);
  
} /* end cmp_word */


static int
cmpb (
      MDu8 const reg,
      MDu8 const eamode,
      MDu8 const eareg
      )
{
  
  int ret;
  MDu8 byte;
  MDu32 addr;
  
  
  switch ( eamode )
    {
    case 0: byte= _regs.D[eareg].b.v0; ret= 4; break;
    case 2: byte= MD_mem_read8 ( _regs.A[eareg].v ); ret= 8; break;
    case 3:
      byte= MD_mem_read8 ( _regs.A[eareg].v );
      if ( eareg == 7 ) _regs.A[eareg].v+= 2;
      else              ++_regs.A[eareg].v;
      ret= 8;
      break;
    case 4:
      if ( eareg == 7 ) _regs.A[eareg].v-= 2;
      else              --_regs.A[eareg].v;
      byte= MD_mem_read8 ( _regs.A[eareg].v );
      ret= 10;
      break;
    case 5: byte= MD_mem_read8 ( pd16an ( _regs.A[eareg] ).v ); ret= 12; break;
    case 6:
      byte= MD_mem_read8 ( _regs.A[eareg].v + calc_8bit_displacement () );
      ret= 14;
      break;
    case 7:
      switch ( eareg )
        {
        case 0: byte= MD_mem_read8 ( pw ().v ); ret= 12; break;
        case 1: byte= MD_mem_read8 ( pl ().v ); ret= 16; break;
        case 2: byte= MD_mem_read8 ( pd16pc ().v ); ret= 12; break;
        case 3:
          addr= _regs.PC;
          addr+= calc_8bit_displacement ();
          byte= MD_mem_read8 ( addr );
          ret= 14;
          break;
        case 4: byte= inm_byte (); ret= 8; break;
        default:
          _warning ( _udata,
        	     "CMP.b no accepta el mode:reg 7:%d", eareg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata,
        	 "CMP.b no accepta el mode %d", eamode );
      return UTIME;
    }
  
  cmp_byte ( byte, _regs.D[reg].b.v0 );
  
  return ret;
  
} /* end cmpb */


static int
cmpbi (
       MDu8 const mode,
       MDu8 const reg
       )
{
  
  int ret;
  MDu32 addr;
  MDu8 byte;
  
  
  byte= inm_byte ();
  switch ( mode )
    {
    case 0: cmp_byte ( byte, _regs.D[reg].b.v0 ); return 8;
    case 2: addr= _regs.A[reg].v; ret= 12; break;
    case 3:
      addr= _regs.A[reg].v;
      if ( reg == 7 ) _regs.A[reg].v+= 2;
      else            ++_regs.A[reg].v;
      ret= 12;
      break;
    case 4:
      addr= (reg==7) ? (_regs.A[reg].v-= 2) : (--_regs.A[reg].v);
      ret= 14;
      break;
    case 5: addr= pd16an ( _regs.A[reg] ).v; ret= 16; break;
    case 6: addr= _regs.A[reg].v + calc_8bit_displacement (); ret= 18; break;
    case 7:
      switch ( reg )
        {
        case 0: addr= pw ().v; ret= 16; break;
        case 1: addr= pl ().v; ret= 20; break;
        default:
          _warning ( _udata,
        	     "CMPI.b no accepta el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata,
        	 "CMPI.b no accepta el mode %d", mode );
      return UTIME;
    }
  
  cmp_byte ( byte, MD_mem_read8 ( addr ) );
  
  return ret;
  
} /* end cmpbi */


static int
cmpmb (
       MDu8 const rx,
       MDu8 const ry
       )
{
  
  
  cmp_byte ( MD_mem_read8 ( _regs.A[ry].v ), MD_mem_read8 ( _regs.A[rx].v ) );
  if ( rx == 7 ) _regs.A[rx].v+= 2;
  else           ++(_regs.A[rx].v);
  if ( ry == 7 ) _regs.A[ry].v+= 2;
  else           ++(_regs.A[ry].v);
  
  return 12;
  
} /* end cmpmb */


static int
cmpml (
       MDu8 const rx,
       MDu8 const ry
       )
{
  
  
  cmp ( read_long ( _regs.A[ry].v ).v, read_long ( _regs.A[rx].v ).v );
  _regs.A[rx].v+= 4;
  _regs.A[ry].v+= 4;
  
  return 20;
  
} /* end cmpml */


static int
cmpmw (
       MDu8 const rx,
       MDu8 const ry
       )
{
  
  
  cmp_word ( MD_mem_read ( _regs.A[ry].v ), MD_mem_read ( _regs.A[rx].v ) );
  _regs.A[rx].v+= 2;
  _regs.A[ry].v+= 2;
  
  return 12;
  
} /* end cmpmw */


static int
cmpl (
      MDu8 const reg,
      MDu8 const eamode,
      MDu8 const eareg
      )
{
  
  int ret;
  MD_Reg32 val;
  MDu32 addr;
  
  
  switch ( eamode )
    {
    case 0: val= _regs.D[eareg]; ret= 6; break;
    case 1: val= _regs.A[eareg]; ret= 6; break;
    case 2: val= read_long ( _regs.A[eareg].v ); ret= 14; break;
    case 3:
      val= read_long ( _regs.A[eareg].v );
      _regs.A[eareg].v+= 4;
      ret= 14;
      break;
    case 4: val= read_long ( (_regs.A[eareg].v-= 4) ); ret= 16; break;
    case 5: val= read_long ( pd16an ( _regs.A[eareg] ).v ); ret= 18; break;
    case 6:
      val= read_long ( _regs.A[eareg].v + calc_8bit_displacement () );
      ret= 20;
      break;
    case 7:
      switch ( eareg )
        {
        case 0: val= read_long ( pw ().v ); ret= 18; break;
        case 1: val= read_long ( pl ().v ); ret= 22; break;
        case 2: val= read_long ( pd16pc ().v ); ret= 18; break;
        case 3:
          addr= _regs.PC;
          addr+= calc_8bit_displacement ();
          val= read_long ( addr );
          ret= 20;
          break;
        case 4: val= inm_long (); ret= 14; break;
        default:
          _warning ( _udata,
        	     "CMP.l no accepta el mode:reg 7:%d", eareg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata,
        	 "CMP.l no accepta el mode %d", eamode );
      return UTIME;
    }
  
  cmp ( val.v, _regs.D[reg].v );
  
  return ret;
  
} /* end cmpl */


static int
cmpli (
       MDu8 const mode,
       MDu8 const reg
       )
{
  
  int ret;
  MDu32 addr;
  MD_Reg32 val;
  
  
  val= inm_long ();
  switch ( mode )
    {
    case 0: cmp ( val.v, _regs.D[reg].v ); return 14;
    case 2: addr= _regs.A[reg].v; ret= 20; break;
    case 3: addr= _regs.A[reg].v; _regs.A[reg].v+= 4; ret= 20; break;
    case 4: addr= (_regs.A[reg].v-= 4); ret= 22; break;
    case 5: addr= pd16an ( _regs.A[reg] ).v; ret= 24; break;
    case 6: addr= _regs.A[reg].v + calc_8bit_displacement (); ret= 26; break;
    case 7:
      switch ( reg )
        {
        case 0: addr= pw ().v; ret= 24; break;
        case 1: addr= pl ().v; ret= 28; break;
        default:
          _warning ( _udata, "CMPI.l no accepta el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "CMPI.l no accepta el mode %d", mode );
      return UTIME;
    }
  
  cmp ( val.v, read_long ( addr ).v );
  
  return ret;
  
} /* end cmpli */


static int
cmpl_A (
        MDu8 const reg,
        MDu8 const eamode,
        MDu8 const eareg
        )
{
  
  int ret;
  MD_Reg32 val;
  MDu32 addr;
  
  
  switch ( eamode )
    {
    case 0: val= _regs.D[eareg]; ret= 6; break;
    case 1: val= _regs.A[eareg]; ret= 6; break;
    case 2: val= read_long ( _regs.A[eareg].v ); ret= 14; break;
    case 3:
      val= read_long ( _regs.A[eareg].v );
      _regs.A[eareg].v+= 4;
      ret= 14;
      break;
    case 4: val= read_long ( (_regs.A[eareg].v-= 4) ); ret= 16; break;
    case 5: val= read_long ( pd16an ( _regs.A[eareg] ).v ); ret= 18; break;
    case 6:
      val= read_long ( _regs.A[eareg].v + calc_8bit_displacement () );
      ret= 20;
      break;
    case 7:
      switch ( eareg )
        {
        case 0: val= read_long ( pw ().v ); ret= 18; break;
        case 1: val= read_long ( pl ().v ); ret= 22; break;
        case 2: val= read_long ( pd16pc ().v ); ret= 18; break;
        case 3:
          addr= _regs.PC;
          addr+= calc_8bit_displacement ();
          val= read_long ( addr );
          ret= 20;
          break;
        case 4: val= inm_long (); ret= 14; break;
        default:
          _warning ( _udata,
        	     "CMPA.l no accepta el mode:reg 7:%d", eareg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata,
        	 "CMPA.l no accepta el mode %d", eamode );
      return UTIME;
    }
  
  cmp ( val.v, _regs.A[reg].v );
  
  return ret;
  
} /* end cmpl_A */


static int
cmpw (
      MDu8 const reg,
      MDu8 const eamode,
      MDu8 const eareg
      )
{
  
  int ret;
  MD_Word word;
  MDu32 addr;
  
  
  switch ( eamode )
    {
    case 0: word= _regs.D[eareg].w.v0; ret= 4; break;
    case 1: word= _regs.A[eareg].w.v0; ret= 4; break;
    case 2: word= MD_mem_read ( _regs.A[eareg].v ); ret= 8; break;
    case 3:
      word= MD_mem_read ( _regs.A[eareg].v );
      _regs.A[eareg].v+= 2;
      ret= 8;
      break;
    case 4: word= MD_mem_read ( (_regs.A[eareg].v-= 2) ); ret= 10; break;
    case 5: word= MD_mem_read ( pd16an ( _regs.A[eareg] ).v ); ret= 12; break;
    case 6:
      word= MD_mem_read ( _regs.A[eareg].v + calc_8bit_displacement () );
      ret= 14;
      break;
    case 7:
      switch ( eareg )
        {
        case 0: word= MD_mem_read ( pw ().v ); ret= 12; break;
        case 1: word= MD_mem_read ( pl ().v ); ret= 16; break;
        case 2: word= MD_mem_read ( pd16pc ().v ); ret= 12; break;
        case 3:
          addr= _regs.PC;
          addr+= calc_8bit_displacement ();
          word= MD_mem_read ( addr );
          ret= 14;
          break;
        case 4: word= inm_word (); ret= 8; break;
        default:
          _warning ( _udata,
        	     "CMP.w no accepta el mode:reg 7:%d", eareg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata,
        	 "CMP.w no accepta el mode %d", eamode );
      return UTIME;
    }
  
  cmp_word ( word, _regs.D[reg].w.v0 );
  
  return ret;
  
} /* end cmpw */


static int
cmpw_A (
        MDu8 const reg,
        MDu8 const eamode,
        MDu8 const eareg
        )
{
  
  int ret;
  MD_Word word;
  MDu32 addr;
  
  
  switch ( eamode )
    {
    case 0: word= _regs.D[eareg].w.v0; ret= 6; break;
    case 1: word= _regs.A[eareg].w.v0; ret= 6; break;
    case 2: word= MD_mem_read ( _regs.A[eareg].v ); ret= 10; break;
    case 3:
      word= MD_mem_read ( _regs.A[eareg].v );
      _regs.A[eareg].v+= 2;
      ret= 10;
      break;
    case 4: word= MD_mem_read ( (_regs.A[eareg].v-= 2) ); ret= 12; break;
    case 5: word= MD_mem_read ( pd16an ( _regs.A[eareg] ).v ); ret= 14; break;
    case 6:
      word= MD_mem_read ( _regs.A[eareg].v + calc_8bit_displacement () );
      ret= 16;
      break;
    case 7:
      switch ( eareg )
        {
        case 0: word= MD_mem_read ( pw ().v ); ret= 14; break;
        case 1: word= MD_mem_read ( pl ().v ); ret= 18; break;
        case 2: word= MD_mem_read ( pd16pc ().v ); ret= 14; break;
        case 3:
          addr= _regs.PC;
          addr+= calc_8bit_displacement ();
          word= MD_mem_read ( addr );
          ret= 16;
          break;
        case 4: word= inm_word (); ret= 10; break;
        default:
          _warning ( _udata,
        	     "CMPA.w no accepta el mode:reg 7:%d", eareg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata,
        	 "CMPA.w no accepta el mode %d", eamode );
      return UTIME;
    }
  
  cmp ( (MDs16) word.v, _regs.A[reg].v );
  
  return ret;
  
} /* end cmpw_A */


static int
cmpwi (
       MDu8 const mode,
       MDu8 const reg
       )
{
  
  int ret;
  MDu32 addr;
  MD_Word word;
  
  
  word= inm_word ();
  switch ( mode )
    {
    case 0: cmp_word ( word, _regs.D[reg].w.v0 ); return 8;
    case 2: addr= _regs.A[reg].v; ret= 12; break;
    case 3: addr= _regs.A[reg].v; _regs.A[reg].v+= 2; ret= 12; break;
    case 4: addr= (_regs.A[reg].v-= 2); ret= 14; break;
    case 5: addr= pd16an ( _regs.A[reg] ).v; ret= 16; break;
    case 6: addr= _regs.A[reg].v + calc_8bit_displacement (); ret= 18; break;
    case 7:
      switch ( reg )
        {
        case 0: addr= pw ().v; ret= 16; break;
        case 1: addr= pl ().v; ret= 20; break;
        default:
          _warning ( _udata, "CMPI.w no accepta el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "CMPI.w no accepta el mode %d", mode );
      return UTIME;
    }
  
  cmp_word ( word, MD_mem_read ( addr ) );
  
  return ret;
  
} /* end cmpwi */


static int
divs (
      MDu8 const reg,
      MDu8 const eamode,
      MDu8 const eareg
      )
{
  
  int ret;
  MD_Word word;
  MDu32 addr;
  MDs32 rem, quo;
  
  
  switch ( eamode )
    {
    case 0: word= _regs.D[eareg].w.v0; ret= 0; break;
    case 2: word= MD_mem_read ( _regs.A[eareg].v ); ret= 4; break;
    case 3:
      word= MD_mem_read ( _regs.A[eareg].v );
      _regs.A[eareg].v+= 2;
      ret= 4;
      break;
    case 4: word= MD_mem_read ( _regs.A[eareg].v-= 2 ); ret= 6; break;
    case 5: word= MD_mem_read ( pd16an ( _regs.A[eareg] ).v ); ret= 8; break;
    case 6:
      word= MD_mem_read ( _regs.A[eareg].v + calc_8bit_displacement () );
      ret= 10;
      break;
    case 7:
      switch ( eareg )
        {
        case 0: word= MD_mem_read ( pw ().v ); ret= 8; break;
        case 1: word= MD_mem_read ( pl ().v ); ret= 12; break;
        case 2: word= MD_mem_read ( pd16pc ().v ); ret= 8; break;
        case 3:
          addr= _regs.PC;
          addr+= calc_8bit_displacement ();
          word= MD_mem_read ( addr );
          ret= 10;
          break;
        case 4: word= inm_word (); ret= 4; break;
        default:
          _warning ( _udata, "DIVS no accepta el mode:reg 7:%d", eareg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "DIVS no accepta el mode %d", eamode );
      return UTIME;
    }
  
  CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
  if ( word.v == 0 ) return trap ( 0x014 ) + ret;
  quo= ((MDs32) _regs.D[reg].v)/((MDs16) word.v);
  if ( quo >= -32768 && quo <= 32767 )
    {
      if ( quo < 0 ) _regs.CCR.v|= NFLAG;
      if ( quo == 0 ) _regs.CCR.v|= ZFLAG;
      rem= ((MDs32) _regs.D[reg].v)%((MDs16) word.v);
      _regs.D[reg].w.v0.v= (MDu16) ((MDs16) quo);
      _regs.D[reg].w.v1.v= (MDu16) ((MDs16) rem);
    }
  else _regs.CCR.v|= VFLAG;
  
  return 158+ret;
  
} /* end divs */


static int
divu (
      MDu8 const reg,
      MDu8 const eamode,
      MDu8 const eareg
      )
{
  
  int ret;
  MD_Word word;
  MDu32 addr;
  MDu32 rem, quo;
  
  
  switch ( eamode )
    {
    case 0: word= _regs.D[eareg].w.v0; ret= 0; break;
    case 2: word= MD_mem_read ( _regs.A[eareg].v ); ret= 4; break;
    case 3:
      word= MD_mem_read ( _regs.A[eareg].v );
      _regs.A[eareg].v+= 2;
      ret= 4;
      break;
    case 4: word= MD_mem_read ( _regs.A[eareg].v-= 2 ); ret= 6; break;
    case 5: word= MD_mem_read ( pd16an ( _regs.A[eareg] ).v ); ret= 8; break;
    case 6:
      word= MD_mem_read ( _regs.A[eareg].v + calc_8bit_displacement () );
      ret= 10;
      break;
    case 7:
      switch ( eareg )
        {
        case 0: word= MD_mem_read ( pw ().v ); ret= 8; break;
        case 1: word= MD_mem_read ( pl ().v ); ret= 12; break;
        case 2: word= MD_mem_read ( pd16pc ().v ); ret= 8; break;
        case 3:
          addr= _regs.PC;
          addr+= calc_8bit_displacement ();
          word= MD_mem_read ( addr );
          ret= 10;
          break;
        case 4: word= inm_word (); ret= 4; break;
        default:
          _warning ( _udata, "DIVU no accepta el mode:reg 7:%d", eareg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "DIVU no accepta el mode %d", eamode );
      return UTIME;
    }
  
  CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
  if ( word.v == 0 ) return trap ( 0x014 ) + ret;
  quo= _regs.D[reg].v/word.v;
  if ( quo <= 65535 )
    {
      if ( quo&0x8000 /*¿¿??*/) _regs.CCR.v|= NFLAG;
      if ( quo == 0 ) _regs.CCR.v|= ZFLAG;
      rem= _regs.D[reg].v%word.v;
      _regs.D[reg].w.v0.v= quo;
      _regs.D[reg].w.v1.v= rem;
    }
  else _regs.CCR.v|= VFLAG;
  
  return 140+ret;
  
} /* end divu */


static int
muls (
      MDu8 const reg,
      MDu8 const eamode,
      MDu8 const eareg
      )
{
  
  int ret;
  MD_Word word;
  MDu32 addr;
  MDs32 res;
  
  
  switch ( eamode )
    {
    case 0: word= _regs.D[eareg].w.v0; ret= 0; break;
    case 2: word= MD_mem_read ( _regs.A[eareg].v ); ret= 4; break;
    case 3:
      word= MD_mem_read ( _regs.A[eareg].v );
      _regs.A[eareg].v+= 2;
      ret= 4;
      break;
    case 4: word= MD_mem_read ( _regs.A[eareg].v-= 2 ); ret= 6; break;
    case 5: word= MD_mem_read ( pd16an ( _regs.A[eareg] ).v ); ret= 8; break;
    case 6:
      word= MD_mem_read ( _regs.A[eareg].v + calc_8bit_displacement () );
      ret= 10;
      break;
    case 7:
      switch ( eareg )
        {
        case 0: word= MD_mem_read ( pw ().v ); ret= 8; break;
        case 1: word= MD_mem_read ( pl ().v ); ret= 12; break;
        case 2: word= MD_mem_read ( pd16pc ().v ); ret= 8; break;
        case 3:
          addr= _regs.PC;
          addr+= calc_8bit_displacement ();
          word= MD_mem_read ( addr );
          ret= 10;
          break;
        case 4: word= inm_word (); ret= 4; break;
        default:
          _warning ( _udata, "MULS no accepta el mode:reg 7:%d", eareg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "MULS no accepta el mode %d", eamode );
      return UTIME;
    }
  
  CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
  _regs.D[reg].v= res=
    ((MDs32) ((MDs16) _regs.D[reg].w.v0.v)) *
    ((MDs32) ((MDs16) word.v));
  if ( res < 0 ) _regs.CCR.v|= NFLAG;
  else if ( res == 0 ) _regs.CCR.v|= ZFLAG;
  
  return 70+ret;
  
} /* end muls */


static int
mulu (
      MDu8 const reg,
      MDu8 const eamode,
      MDu8 const eareg
      )
{
  
  int ret;
  MD_Word word;
  MDu32 addr, res;
  
  
  switch ( eamode )
    {
    case 0: word= _regs.D[eareg].w.v0; ret= 0; break;
    case 2: word= MD_mem_read ( _regs.A[eareg].v ); ret= 4; break;
    case 3:
      word= MD_mem_read ( _regs.A[eareg].v );
      _regs.A[eareg].v+= 2;
      ret= 4;
      break;
    case 4: word= MD_mem_read ( _regs.A[eareg].v-= 2 ); ret= 6; break;
    case 5: word= MD_mem_read ( pd16an ( _regs.A[eareg] ).v ); ret= 8; break;
    case 6:
      word= MD_mem_read ( _regs.A[eareg].v + calc_8bit_displacement () );
      ret= 10;
      break;
    case 7:
      switch ( eareg )
        {
        case 0: word= MD_mem_read ( pw ().v ); ret= 8; break;
        case 1: word= MD_mem_read ( pl ().v ); ret= 12; break;
        case 2: word= MD_mem_read ( pd16pc ().v ); ret= 8; break;
        case 3:
          addr= _regs.PC;
          addr+= calc_8bit_displacement ();
          word= MD_mem_read ( addr );
          ret= 10;
          break;
        case 4: word= inm_word (); ret= 4; break;
        default:
          _warning ( _udata, "MULU no accepta el mode:reg 7:%d", eareg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "MULU no accepta el mode %d", eamode );
      return UTIME;
    }
  
  CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
  _regs.D[reg].v= res= ((MDu32) _regs.D[reg].w.v0.v) * ((MDu32) word.v);
  if ( res&0x80000000 /*¿¿??*/ ) _regs.CCR.v|= NFLAG;
  if ( res == 0 ) _regs.CCR.v|= ZFLAG;
  
  return 70+ret;
  
} /* end mulu */


static int
negb (
      MDu8 const mode,
      MDu8 const reg
      )
{
  
  int ret;
  MDu32 addr;
  
  
  switch ( mode )
    {
    case 0: _regs.D[reg].b.v0= subb ( _regs.D[reg].b.v0, 0 ); return 4;
    case 2: addr= _regs.A[reg].v; ret= 12; break;
    case 3:
      addr= _regs.A[reg].v;
      if ( reg == 7 ) _regs.A[reg].v+= 2;
      else            ++_regs.A[reg].v;
      ret= 12;
      break;
    case 4:
      addr= (reg==7)?(_regs.A[reg].v-= 2):(--_regs.A[reg].v);
      ret= 14;
      break;
    case 5: addr= pd16an ( _regs.A[reg] ).v; ret= 16; break;
    case 6: addr= _regs.A[reg].v + calc_8bit_displacement (); ret= 18; break;
    case 7:
      switch ( reg )
        {
        case 0: addr= pw ().v; ret= 16; break;
        case 1: addr= pl ().v; ret= 20; break;
        default:
          _warning ( _udata, "NEG.b no accepta el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "NEG.b no accepta el mode %d", mode );
      return UTIME;
    }
  
  MD_mem_write8 ( addr, subb ( MD_mem_read8 ( addr ), 0 ) );
  
  return ret;
  
} /* end negb */


static int
negl (
      MDu8 const mode,
      MDu8 const reg
      )
{
  
  MD_Reg32 const zero= { 0 };
  int ret;
  MDu32 addr;
  
  
  switch ( mode )
    {
    case 0: _regs.D[reg]= subl ( _regs.D[reg], zero ); return 6;
    case 2: addr= _regs.A[reg].v; ret= 20; break;
    case 3: addr= _regs.A[reg].v; _regs.A[reg].v+= 4; ret= 20; break;
    case 4: addr= (_regs.A[reg].v-= 4); ret= 22; break;
    case 5: addr= pd16an ( _regs.A[reg] ).v; ret= 24; break;
    case 6: addr= _regs.A[reg].v + calc_8bit_displacement (); ret= 26; break;
    case 7:
      switch ( reg )
        {
        case 0: addr= pw ().v; ret= 24; break;
        case 1: addr= pl ().v; ret= 28; break;
        default:
          _warning ( _udata, "NEG.l no accepta el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "NEG.l no accepta el mode %d", mode );
      return UTIME;
    }
  
  write_long ( subl ( read_long ( addr ), zero ), addr );
  
  return ret;
  
} /* end negl */


static int
negw (
      MDu8 const mode,
      MDu8 const reg
      )
{
  
  MD_Word const zero= { 0 };
  int ret;
  MDu32 addr;
  
  
  switch ( mode )
    {
    case 0: _regs.D[reg].w.v0= subw ( _regs.D[reg].w.v0, zero ); return 4;
    case 2: addr= _regs.A[reg].v; ret= 12; break;
    case 3: addr= _regs.A[reg].v; _regs.A[reg].v+= 2; ret= 12; break;
    case 4: addr= (_regs.A[reg].v-= 2); ret= 14; break;
    case 5: addr= pd16an ( _regs.A[reg] ).v; ret= 16; break;
    case 6: addr= _regs.A[reg].v + calc_8bit_displacement (); ret= 18; break;
    case 7:
      switch ( reg )
        {
        case 0: addr= pw ().v; ret= 16; break;
        case 1: addr= pl ().v; ret= 20; break;
        default:
          _warning ( _udata, "NEG.w no accepta el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "NEG.w no accepta el mode %d", mode );
      return UTIME;
    }
  
  MD_mem_write ( addr, subw ( MD_mem_read ( addr ), zero ) );
  
  return ret;
  
} /* end negw */


static int
negxb (
       MDu8 const mode,
       MDu8 const reg
       )
{
  
  int ret;
  MDu32 addr;
  
  
  switch ( mode )
    {
    case 0: _regs.D[reg].b.v0= subb_x ( _regs.D[reg].b.v0, 0 ); return 4;
    case 2: addr= _regs.A[reg].v; ret= 12; break;
    case 3:
      addr= _regs.A[reg].v;
      if ( reg == 7 ) _regs.A[reg].v+= 2;
      else            ++_regs.A[reg].v;
      ret= 12;
      break;
    case 4:
      addr= (reg==7)?(_regs.A[reg].v-= 2):(--_regs.A[reg].v);
      ret= 14;
      break;
    case 5: addr= pd16an ( _regs.A[reg] ).v; ret= 16; break;
    case 6: addr= _regs.A[reg].v + calc_8bit_displacement (); ret= 18; break;
    case 7:
      switch ( reg )
        {
        case 0: addr= pw ().v; ret= 16; break;
        case 1: addr= pl ().v; ret= 20; break;
        default:
          _warning ( _udata, "NEGX.b no accepta el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "NEGX.b no accepta el mode %d", mode );
      return UTIME;
    }
  
  MD_mem_write8 ( addr, subb_x ( MD_mem_read8 ( addr ), 0 ) );
  
  return ret;
  
} /* end negxb */


static int
negxl (
       MDu8 const mode,
       MDu8 const reg
       )
{
  
  MD_Reg32 const zero= { 0 };
  int ret;
  MDu32 addr;
  
  
  switch ( mode )
    {
    case 0: _regs.D[reg]= subl_x ( _regs.D[reg], zero ); return 6;
    case 2: addr= _regs.A[reg].v; ret= 20; break;
    case 3: addr= _regs.A[reg].v; _regs.A[reg].v+= 4; ret= 20; break;
    case 4: addr= (_regs.A[reg].v-= 4); ret= 22; break;
    case 5: addr= pd16an ( _regs.A[reg] ).v; ret= 24; break;
    case 6: addr= _regs.A[reg].v + calc_8bit_displacement (); ret= 26; break;
    case 7:
      switch ( reg )
        {
        case 0: addr= pw ().v; ret= 24; break;
        case 1: addr= pl ().v; ret= 28; break;
        default:
          _warning ( _udata, "NEG.l no accepta el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "NEG.l no accepta el mode %d", mode );
      return UTIME;
    }
  
  write_long ( subl_x ( read_long ( addr ), zero ), addr );
  
  return ret;
  
} /* end negxl */


static int
negxw (
       MDu8 const mode,
       MDu8 const reg
       )
{
  
  MD_Word const zero= { 0 };
  int ret;
  MDu32 addr;
  
  
  switch ( mode )
    {
    case 0: _regs.D[reg].w.v0= subw_x ( _regs.D[reg].w.v0, zero ); return 4;
    case 2: addr= _regs.A[reg].v; ret= 12; break;
    case 3: addr= _regs.A[reg].v; _regs.A[reg].v+= 2; ret= 12; break;
    case 4: addr= (_regs.A[reg].v-= 2); ret= 14; break;
    case 5: addr= pd16an ( _regs.A[reg] ).v; ret= 16; break;
    case 6: addr= _regs.A[reg].v + calc_8bit_displacement (); ret= 18; break;
    case 7:
      switch ( reg )
        {
        case 0: addr= pw ().v; ret= 16; break;
        case 1: addr= pl ().v; ret= 20; break;
        default:
          _warning ( _udata, "NEGX.w no accepta el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "NEGX.w no accepta el mode %d", mode );
      return UTIME;
    }
  
  MD_mem_write ( addr, subw_x ( MD_mem_read ( addr ), zero ) );
  
  return ret;
  
} /* end negxw */


static int
opb_eadst (
           MDu8 const    reg,
           MDu8 const    eamode,
           MDu8 const    eareg,
           opb_t * const op
           )
{
  
  int ret;
  MDu32 addr;
  
  
  switch ( eamode )
    {
    case 2: addr= _regs.A[eareg].v; ret= 12; break;
    case 3:
      addr= _regs.A[eareg].v;
      if ( eareg == 7 ) _regs.A[eareg].v+= 2;
      else              ++_regs.A[eareg].v;
      ret= 12;
      break;
    case 4:
      addr= (eareg==7) ? (_regs.A[eareg].v-= 2) : (--_regs.A[eareg].v);
      ret= 14;
      break;
    case 5: addr= pd16an ( _regs.A[eareg] ).v; ret= 16; break;
    case 6: addr= _regs.A[eareg].v + calc_8bit_displacement (); ret= 18; break;
    case 7:
      switch ( eareg )
        {
        case 0: addr= pw ().v; ret= 16; break;
        case 1: addr= pl ().v; ret= 20; break;
        default:
          _warning ( _udata,
        	     "Les operacions aritmètiques (OP.b Dn,<ea>)"
        	     " no accepten el mode:reg 7:%d", eareg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata,
        	 "Les operacions aritmètiques (OP.b Dn,<ea>)"
        	 " no accepten el mode %d", eamode );
      return UTIME;
    }
  
  MD_mem_write8 ( addr, op ( _regs.D[reg].b.v0, MD_mem_read8 ( addr ) ) );
  
  return ret;
  
} /* end opb_eadst */


static int
opb_easrc (
           MDu8 const    reg,
           MDu8 const    eamode,
           MDu8 const    eareg,
           opb_t * const op
           )
{
  
  int ret;
  MDu8 byte;
  MDu32 addr;
  
  
  switch ( eamode )
    {
    case 0: byte= _regs.D[eareg].b.v0; ret= 4; break;
    case 2: byte= MD_mem_read8 ( _regs.A[eareg].v ); ret= 8; break;
    case 3:
      byte= MD_mem_read8 ( _regs.A[eareg].v );
      if ( eareg == 7 ) _regs.A[eareg].v+= 2;
      else              ++_regs.A[eareg].v;
      ret= 8;
      break;
    case 4:
      if ( eareg == 7 ) _regs.A[eareg].v-= 2;
      else              --_regs.A[eareg].v;
      byte= MD_mem_read8 ( _regs.A[eareg].v );
      ret= 10;
      break;
    case 5: byte= MD_mem_read8 ( pd16an ( _regs.A[eareg] ).v ); ret= 12; break;
    case 6:
      byte= MD_mem_read8 ( _regs.A[eareg].v + calc_8bit_displacement () );
      ret= 14;
      break;
    case 7:
      switch ( eareg )
        {
        case 0: byte= MD_mem_read8 ( pw ().v ); ret= 12; break;
        case 1: byte= MD_mem_read8 ( pl ().v ); ret= 16; break;
        case 2: byte= MD_mem_read8 ( pd16pc ().v ); ret= 12; break;
        case 3:
          addr= _regs.PC;
          addr+= calc_8bit_displacement ();
          byte= MD_mem_read8 ( addr );
          ret= 14;
          break;
        case 4: byte= inm_byte (); ret= 8; break;
        default:
          _warning ( _udata,
        	     "Les operacions aritmètiques (OP.b <ea>,Dn)"
        	     " no accepten el mode:reg 7:%d", eareg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata,
        	 "Les operacions aritmètiques (OP.b <ea>,Dn)"
        	 " no accepten el mode %d", eamode );
      return UTIME;
    }
  
  _regs.D[reg].b.v0= op ( byte, _regs.D[reg].b.v0 );
  
  return ret;
  
} /* end opb_easrc */


static int
opbi (
      MDu8 const    mode,
      MDu8 const    reg,
      opb_t * const op
      )
{
  
  int ret;
  MDu32 addr;
  MDu8 byte;
  
  
  byte= inm_byte ();
  switch ( mode )
    {
    case 0: _regs.D[reg].b.v0= op ( byte, _regs.D[reg].b.v0 ); return 8;
    case 2: addr= _regs.A[reg].v; ret= 16; break;
    case 3:
      addr= _regs.A[reg].v;
      if ( reg == 7 ) _regs.A[reg].v+= 2;
      else            ++_regs.A[reg].v;
      ret= 16;
      break;
    case 4:
      addr= (reg==7) ? (_regs.A[reg].v-= 2) : (--_regs.A[reg].v);
      ret= 18;
      break;
    case 5: addr= pd16an ( _regs.A[reg] ).v; ret= 20; break;
    case 6: addr= _regs.A[reg].v + calc_8bit_displacement (); ret= 22; break;
    case 7:
      switch ( reg )
        {
        case 0: addr= pw ().v; ret= 20; break;
        case 1: addr= pl ().v; ret= 24; break;
        default:
          _warning ( _udata,
        	     "Les operacions aritmètiques (OPI.b #<data>,<ea>)"
        	     " no accepten el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata,
        	 "Les operacions aritmètiques (OPI.b #<data>,<ea>)"
        	 " no accepten el mode %d", mode );
      return UTIME;
    }
  
  MD_mem_write8 ( addr, op ( byte, MD_mem_read8 ( addr ) ) );
  
  return ret;
  
} /* end opbi */


static int
opbq (
      MDu8          data,
      MDu8 const    mode,
      MDu8 const    reg,
      opb_t * const opb
      )
{
  
  int ret;
  MDu32 addr;
  
  
  if ( data == 0 ) data= 8;
  switch ( mode )
    {
    case 0: _regs.D[reg].b.v0= opb ( data, _regs.D[reg].b.v0 ); return 4;
    case 2: addr= _regs.A[reg].v; ret= 12; break;
    case 3:
      addr= _regs.A[reg].v;
      if ( reg == 7 ) _regs.A[reg].v+= 2;
      else            ++_regs.A[reg].v;
      ret= 12;
      break;
    case 4:
      addr= (reg==7) ? (_regs.A[reg].v-= 2) : (--_regs.A[reg].v);
      ret= 14;
      break;
    case 5: addr= pd16an ( _regs.A[reg] ).v; ret= 16; break;
    case 6: addr= _regs.A[reg].v + calc_8bit_displacement (); ret= 18; break;
    case 7:
      switch ( reg )
        {
        case 0: addr= pw ().v; ret= 16; break;
        case 1: addr= pl ().v; ret= 20; break;
        default:
          _warning ( _udata,
        	     "Les operacions aritmètiques (OPQ.b #<data>,<ea>)"
        	     " no accepten el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata,
        	 "Les operacions aritmètiques (OPQ.b #<data>,<ea>)"
        	 " no accepten el mode %d", mode );
      return UTIME;
    }
  
  MD_mem_write8 ( addr, opb ( data, MD_mem_read8 ( addr ) ) );
  
  return ret;
  
} /* end opbq */


static int
opbx_A (
        MDu8 const    rx,
        MDu8 const    ry,
        opb_t * const op
        )
{
  
  if ( rx == 7 ) _regs.A[rx].v-= 2;
  else           --(_regs.A[rx].v);
  if ( ry == 7 ) _regs.A[ry].v-= 2;
  else           --(_regs.A[ry].v);
  MD_mem_write8 ( _regs.A[rx].v,
        	  op ( MD_mem_read8 ( _regs.A[ry].v ),
        	       MD_mem_read8 ( _regs.A[rx].v ) ) );
  
  return 18;
  
} /* end opbx_A */


static int
opl_eadst (
           MDu8 const    reg,
           MDu8 const    eamode,
           MDu8 const    eareg,
           opl_t * const op
           )
{
  
  int ret;
  MDu32 addr;
  
  
  switch ( eamode )
    {
    case 2: addr= _regs.A[eareg].v; ret= 20; break;
    case 3: addr= _regs.A[eareg].v; _regs.A[eareg].v+= 4; ret= 20; break;
    case 4: addr= (_regs.A[eareg].v-= 4); ret= 22; break;
    case 5: addr= pd16an ( _regs.A[eareg] ).v; ret= 24; break;
    case 6: addr= _regs.A[eareg].v + calc_8bit_displacement (); ret= 26; break;
    case 7:
      switch ( eareg )
        {
        case 0: addr= pw ().v; ret= 24; break;
        case 1: addr= pl ().v; ret= 28; break;
        default:
          _warning ( _udata,
        	     "Les operacions aritmètiques (OP.l Dn,<ea>)"
        	     " no accepten el mode:reg 7:%d", eareg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata,
        	 "Les operacions aritmètiques (OP.l Dn,<ea>)"
        	 " no accepten el mode %d", eamode );
      return UTIME;
    }
  
  write_long ( op ( _regs.D[reg], read_long ( addr ) ), addr );
  
  return ret;
  
} /* end opl_eadst */


static int
opl_easrc (
           MDu8 const    reg,
           MDu8 const    eamode,
           MDu8 const    eareg,
           opl_t * const op
           )
{
  
  int ret;
  MD_Reg32 val;
  MDu32 addr;
  
  
  switch ( eamode )
    {
    case 0: val= _regs.D[eareg]; ret= 8; break;
    case 1: val= _regs.A[eareg]; ret= 8; break;
    case 2: val= read_long ( _regs.A[eareg].v ); ret= 14; break;
    case 3:
      val= read_long ( _regs.A[eareg].v );
      _regs.A[eareg].v+= 4;
      ret= 14;
      break;
    case 4: val= read_long ( (_regs.A[eareg].v-= 4) ); ret= 16; break;
    case 5: val= read_long ( pd16an ( _regs.A[eareg] ).v ); ret= 18; break;
    case 6:
      val= read_long ( _regs.A[eareg].v + calc_8bit_displacement () );
      ret= 20;
      break;
    case 7:
      switch ( eareg )
        {
        case 0: val= read_long ( pw ().v ); ret= 18; break;
        case 1: val= read_long ( pl ().v ); ret= 22; break;
        case 2: val= read_long ( pd16pc ().v ); ret= 18; break;
        case 3:
          addr= _regs.PC;
          addr+= calc_8bit_displacement ();
          val= read_long ( addr );
          ret= 20;
          break;
        case 4: val= inm_long (); ret= 14; break;
        default:
          _warning ( _udata,
        	     "Les operacions aritmètiques (OP.l <ea>,Dn)"
        	     " no accepten el mode:reg 7:%d", eareg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata,
        	 "Les operacions aritmètiques (OP.l <ea>,Dn)"
        	 " no accepten el mode %d", eamode );
      return UTIME;
    }
  
  _regs.D[reg]= op ( val, _regs.D[reg] );
  
  return ret;
  
} /* end opl_easrc */


static int
opl_easrc_A (
             MDu8 const    reg,
             MDu8 const    eamode,
             MDu8 const    eareg,
             op_t * const  op
             )
{
  
  int ret;
  MD_Reg32 val;
  MDu32 addr;
  
  
  switch ( eamode )
    {
    case 0: val= _regs.D[eareg]; ret= 8; break;
    case 1: val= _regs.A[eareg]; ret= 8; break;
    case 2: val= read_long ( _regs.A[eareg].v ); ret= 14; break;
    case 3:
      val= read_long ( _regs.A[eareg].v );
      _regs.A[eareg].v+= 4;
      ret= 14;
      break;
    case 4: val= read_long ( (_regs.A[eareg].v-= 4) ); ret= 16; break;
    case 5: val= read_long ( pd16an ( _regs.A[eareg] ).v ); ret= 18; break;
    case 6:
      val= read_long ( _regs.A[eareg].v + calc_8bit_displacement () );
      ret= 20;
      break;
    case 7:
      switch ( eareg )
        {
        case 0: val= read_long ( pw ().v ); ret= 18; break;
        case 1: val= read_long ( pl ().v ); ret= 22; break;
        case 2: val= read_long ( pd16pc ().v ); ret= 18; break;
        case 3:
          addr= _regs.PC;
          addr+= calc_8bit_displacement ();
          val= read_long ( addr );
          ret= 20;
          break;
        case 4: val= inm_long (); ret= 14; break;
        default:
          _warning ( _udata,
        	     "Les operacions aritmètiques (OP.l <ea>,An)"
        	     " no accepten el mode:reg 7:%d", eareg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata,
        	 "Les operacions aritmètiques (OP.l <ea>,An)"
        	 " no accepten el mode %d", eamode );
      return UTIME;
    }
  
  _regs.A[reg].v= op ( val.v, _regs.A[reg].v );
  
  return ret;
  
} /* end opl_easrc_A */


static int
opli (
      MDu8 const    mode,
      MDu8 const    reg,
      opl_t * const op
      )
{
  
  int ret;
  MDu32 addr;
  MD_Reg32 val;
  
  
  val= inm_long ();
  switch ( mode )
    {
    case 0: _regs.D[reg]= op ( val, _regs.D[reg] ); return 16;
    case 2: addr= _regs.A[reg].v; ret= 28; break;
    case 3: addr= _regs.A[reg].v; _regs.A[reg].v+= 4; ret= 28; break;
    case 4: addr= (_regs.A[reg].v-= 4); ret= 30; break;
    case 5: addr= pd16an ( _regs.A[reg] ).v; ret= 32; break;
    case 6: addr= _regs.A[reg].v + calc_8bit_displacement (); ret= 34; break;
    case 7:
      switch ( reg )
        {
        case 0: addr= pw ().v; ret= 32; break;
        case 1: addr= pl ().v; ret= 36; break;
        default:
          _warning ( _udata,
        	     "Les operacions aritmètiques (OPI.l #<data>,<ea>)"
        	     " no accepten el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata,
        	 "Les operacions aritmètiques (OPI.l #<data>,<ea>)"
        	 " no accepten el mode %d", mode );
      return UTIME;
    }
  
  write_long ( op ( val, read_long ( addr ) ), addr );
  
  return ret;
  
} /* end opli */


static int
oplq (
      MDu8 const    data,
      MDu8 const    mode,
      MDu8 const    reg,
      opl_t * const opl,
      op_t * const  op
      )
{
  
  int ret;
  MDu32 addr;
  MD_Reg32 val;
  
  
  val.v= (data==0) ? 8 : data;
  switch ( mode )
    {
    case 0: _regs.D[reg]= opl ( val, _regs.D[reg] ); return 8;
    case 1: _regs.A[reg].v= op ( val.v, _regs.A[reg].v ); return 8;
    case 2: addr= _regs.A[reg].v; ret= 20; break;
    case 3: addr= _regs.A[reg].v; _regs.A[reg].v+= 4; ret= 20; break;
    case 4: addr= (_regs.A[reg].v-= 4); ret= 22; break;
    case 5: addr= pd16an ( _regs.A[reg] ).v; ret= 24; break;
    case 6: addr= _regs.A[reg].v + calc_8bit_displacement (); ret= 26; break;
    case 7:
      switch ( reg )
        {
        case 0: addr= pw ().v; ret= 24; break;
        case 1: addr= pl ().v; ret= 28; break;
        default:
          _warning ( _udata,
        	     "Les operacions aritmètiques (OPQ.l #<data>,<ea>)"
        	     " no accepten el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default: ret= addr= 0; /*CALLA*/break;
    }
  
  write_long ( opl ( val, read_long ( addr ) ), addr );
  
  return ret;
  
} /* end oplq */


static int
oplx_A (
        MDu8 const    rx,
        MDu8 const    ry,
        opl_t * const op
        )
{
  
  _regs.A[rx].v-= 4;
  _regs.A[ry].v-= 4;
  write_long ( op ( read_long ( _regs.A[ry].v ),
        	    read_long ( _regs.A[rx].v ) ), _regs.A[rx].v );
  
  return 30;
  
} /* end oplx_A */


static int
opw_eadst (
           MDu8 const    reg,
           MDu8 const    eamode,
           MDu8 const    eareg,
           opw_t * const op
           )
{
  
  int ret;
  MDu32 addr;
  
  
  switch ( eamode )
    {
    case 2: addr= _regs.A[eareg].v; ret= 12; break;
    case 3: addr= _regs.A[eareg].v; _regs.A[eareg].v+= 2; ret= 12; break;
    case 4: addr= (_regs.A[eareg].v-= 2); ret= 14; break;
    case 5: addr= pd16an ( _regs.A[eareg] ).v; ret= 16; break;
    case 6: addr= _regs.A[eareg].v + calc_8bit_displacement (); ret= 18; break;
    case 7:
      switch ( eareg )
        {
        case 0: addr= pw ().v; ret= 16; break;
        case 1: addr= pl ().v; ret= 20; break;
        default:
          _warning ( _udata,
        	     "Les operacions aritmètiques (OP.w Dn,<ea>)"
        	     " no accepten el mode:reg 7:%d", eareg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata,
        	 "Les operacions aritmètiques (OP.w Dn,<ea>)"
        	 " no accepten el mode %d", eamode );
      return UTIME;
    }
  
  MD_mem_write ( addr, op ( _regs.D[reg].w.v0, MD_mem_read ( addr ) ) );
  
  return ret;
  
} /* end opw_eadst */


static int
opw_easrc (
           MDu8 const    reg,
           MDu8 const    eamode,
           MDu8 const    eareg,
           opw_t * const op
           )
{
  
  int ret;
  MD_Word word;
  MDu32 addr;
  
  
  switch ( eamode )
    {
    case 0: word= _regs.D[eareg].w.v0; ret= 4; break;
    case 1: word= _regs.A[eareg].w.v0; ret= 4; break;
    case 2: word= MD_mem_read ( _regs.A[eareg].v ); ret= 8; break;
    case 3:
      word= MD_mem_read ( _regs.A[eareg].v );
      _regs.A[eareg].v+= 2;
      ret= 8;
      break;
    case 4: word= MD_mem_read ( (_regs.A[eareg].v-= 2) ); ret= 10; break;
    case 5: word= MD_mem_read ( pd16an ( _regs.A[eareg] ).v ); ret= 12; break;
    case 6:
      word= MD_mem_read ( _regs.A[eareg].v + calc_8bit_displacement () );
      ret= 14;
      break;
    case 7:
      switch ( eareg )
        {
        case 0: word= MD_mem_read ( pw ().v ); ret= 12; break;
        case 1: word= MD_mem_read ( pl ().v ); ret= 16; break;
        case 2: word= MD_mem_read ( pd16pc ().v ); ret= 12; break;
        case 3:
          addr= _regs.PC;
          addr+= calc_8bit_displacement ();
          word= MD_mem_read ( addr );
          ret= 14;
          break;
        case 4: word= inm_word (); ret= 8; break;
        default:
          _warning ( _udata,
        	     "Les operacions aritmètiques (OP.w <ea>,Dn)"
        	     " no accepten el mode:reg 7:%d", eareg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata,
        	 "Les operacions aritmètiques (OP.w <ea>,Dn)"
        	 " no accepten el mode %d", eamode );
      return UTIME;
    }
  
  _regs.D[reg].w.v0= op ( word, _regs.D[reg].w.v0 );
  
  return ret;
  
} /* end opw_easrc */


static int
opw_easrc_A (
             MDu8 const    reg,
             MDu8 const    eamode,
             MDu8 const    eareg,
             op_t  * const op
             )
{
  
  int ret;
  MD_Word word;
  MDu32 addr;
  
  
  switch ( eamode )
    {
    case 0: word= _regs.D[eareg].w.v0; ret= 8; break;
    case 1: word= _regs.A[eareg].w.v0; ret= 8; break;
    case 2: word= MD_mem_read ( _regs.A[eareg].v ); ret= 12; break;
    case 3:
      word= MD_mem_read ( _regs.A[eareg].v );
      _regs.A[eareg].v+= 2;
      ret= 12;
      break;
    case 4: word= MD_mem_read ( (_regs.A[eareg].v-= 2) ); ret= 14; break;
    case 5: word= MD_mem_read ( pd16an ( _regs.A[eareg] ).v ); ret= 16; break;
    case 6:
      word= MD_mem_read ( _regs.A[eareg].v + calc_8bit_displacement () );
      ret= 18;
      break;
    case 7:
      switch ( eareg )
        {
        case 0: word= MD_mem_read ( pw ().v ); ret= 16; break;
        case 1: word= MD_mem_read ( pl ().v ); ret= 20; break;
        case 2: word= MD_mem_read ( pd16pc ().v ); ret= 16; break;
        case 3:
          addr= _regs.PC;
          addr+= calc_8bit_displacement ();
          word= MD_mem_read ( addr );
          ret= 18;
          break;
        case 4: word= inm_word (); ret= 12; break;
        default:
          _warning ( _udata,
        	     "Les operacions aritmètiques (OP.w <ea>,An)"
        	     " no accepten el mode:reg 7:%d", eareg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata,
        	 "Les operacions aritmètiques (OP.w <ea>,An)"
        	 " no accepten el mode %d", eamode );
      return UTIME;
    }
  
  _regs.A[reg].v= op ( (MDs16) word.v, _regs.A[reg].v );
  
  return ret;
  
} /* end opw_easrc_A */


static int
opwi (
      MDu8 const    mode,
      MDu8 const    reg,
      opw_t * const op
      )
{
  
  int ret;
  MDu32 addr;
  MD_Word word;
  
  
  word= inm_word ();
  switch ( mode )
    {
    case 0: _regs.D[reg].w.v0= op ( word, _regs.D[reg].w.v0 ); return 8;
    case 2: addr= _regs.A[reg].v; ret= 16; break;
    case 3: addr= _regs.A[reg].v; _regs.A[reg].v+= 2; ret= 16; break;
    case 4: addr= (_regs.A[reg].v-= 2); ret= 18; break;
    case 5: addr= pd16an ( _regs.A[reg] ).v; ret= 20; break;
    case 6: addr= _regs.A[reg].v + calc_8bit_displacement (); ret= 22; break;
    case 7:
      switch ( reg )
        {
        case 0: addr= pw ().v; ret= 20; break;
        case 1: addr= pl ().v; ret= 24; break;
        default:
          _warning ( _udata,
        	     "Les operacions aritmètiques (OPI.w #<data>,<ea>)"
        	     " no accepten el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata,
        	 "Les operacions aritmètiques (OPI.w #<data>,<ea>)"
        	 " no accepten el mode %d", mode );
      return UTIME;
    }
  
  MD_mem_write ( addr, op ( word, MD_mem_read ( addr ) ) );
  
  return ret;
  
} /* end opwi */


static int
opwq (
      MDu8 const    data,
      MDu8 const    mode,
      MDu8 const    reg,
      opw_t * const opw,
      op_t * const  op
      )
{
  
  int ret;
  MDu32 addr;
  MD_Word word;
  
  
  word.v= (data==0) ? 8 : data;
  switch ( mode )
    {
    case 0: _regs.D[reg].w.v0= opw ( word, _regs.D[reg].w.v0 ); return 4;
    case 1: _regs.A[reg].v= op ( word.v, _regs.A[reg].v ); return 8;
    case 2: addr= _regs.A[reg].v; ret= 12; break;
    case 3: addr= _regs.A[reg].v; _regs.A[reg].v+= 2; ret= 12; break;
    case 4: addr= (_regs.A[reg].v-= 2); ret= 14; break;
    case 5: addr= pd16an ( _regs.A[reg] ).v; ret= 16; break;
    case 6: addr= _regs.A[reg].v + calc_8bit_displacement (); ret= 18; break;
    case 7:
      switch ( reg )
        {
        case 0: addr= pw ().v; ret= 16; break;
        case 1: addr= pl ().v; ret= 20; break;
        default:
          _warning ( _udata,
        	     "Les operacions aritmètiques (OPQ.w #<data>,<ea>)"
        	     " no accepten el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default: ret=addr=0; /*CALLA*/break;
    }
  
  MD_mem_write ( addr, opw ( word, MD_mem_read ( addr ) ) );
  
  return ret;
  
} /* end opwq */


static int
opwx_A (
        MDu8 const    rx,
        MDu8 const    ry,
        opw_t * const op
        )
{
  
  _regs.A[rx].v-= 2;
  _regs.A[ry].v-= 2;
  MD_mem_write ( _regs.A[rx].v,
        	 op ( MD_mem_read ( _regs.A[ry].v ),
        	      MD_mem_read ( _regs.A[rx].v ) ) );
  
  return 18;
  
} /* end opwx_A */


/* Logical Instructions *******************************************************/
static void
logb_flags (
            MDu8 const val
            )
{
  
  CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
  _regs.CCR.v|=
    (val?0:ZFLAG) |
    ((val&0x80)?NFLAG:0);
  
} /* end logb_flags */


static void
logl_flags (
            MD_Reg32 const val
            )
{
  
  CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
  _regs.CCR.v|=
    (val.v?0:ZFLAG) |
    ((val.v&0x80000000)?NFLAG:0);
  
} /* end logl_flags */


static void
logw_flags (
            MD_Word const val
            )
{
  
  CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
  _regs.CCR.v|=
    (val.v?0:ZFLAG) |
    ((val.v&0x8000)?NFLAG:0);
  
} /* end logw_flags */


static MDu8
andb (
      const MDu8 a,
      const MDu8 b
      )
{
  
  MDu8 aux;
  
  
  aux= a & b;
  logb_flags ( aux );
  
  return aux;
  
} /* end andb */


static MD_Reg32
andl (
      const MD_Reg32 a,
      const MD_Reg32 b
      )
{
  
  MD_Reg32 aux;
  
  
  aux.v= a.v & b.v;
  logl_flags ( aux );
  
  return aux;
  
} /* end andl */


static MD_Word
andw (
      const MD_Word a,
      const MD_Word b
      )
{
  
  MD_Word aux;
  
  
  aux.v= a.v & b.v;
  logw_flags ( aux );
  
  return aux;
  
} /* andw */


static MDu8
eor_b (
       const MDu8 a,
       const MDu8 b
       )
{
  
  MDu8 aux;
  
  
  aux= a ^ b;
  logb_flags ( aux );
  
  return aux;
  
} /* end eor_b */


static MD_Reg32
eor_l (
       const MD_Reg32 a,
       const MD_Reg32 b
       )
{
  
  MD_Reg32 aux;
  
  
  aux.v= a.v ^ b.v;
  logl_flags ( aux );
  
  return aux;
  
} /* end eor_l */


static MD_Word
eor_w (
       const MD_Word a,
       const MD_Word b
       )
{
  
  MD_Word aux;
  
  
  aux.v= a.v ^ b.v;
  logw_flags ( aux );
  
  return aux;
  
} /* eor_w */


static int
eorb (
      MDu8 const reg,
      MDu8 const eamode,
      MDu8 const eareg
      )
{
  
  int ret;
  MDu32 addr;
  
  
  switch ( eamode )
    {
    case 0:
      _regs.D[eareg].b.v0= eor_b ( _regs.D[reg].b.v0, _regs.D[eareg].b.v0 );
      return 8;
    case 2: addr= _regs.A[eareg].v; ret= 12; break;
    case 3:
      addr= _regs.A[eareg].v;
      if ( eareg == 7 ) _regs.A[eareg].v+= 2;
      else              ++_regs.A[eareg].v;
      ret= 12;
      break;
    case 4:
      addr= (eareg==7) ? (_regs.A[eareg].v-= 2) : (--_regs.A[eareg].v);
      ret= 14;
      break;
    case 5: addr= pd16an ( _regs.A[eareg] ).v; ret= 16; break;
    case 6: addr= _regs.A[eareg].v + calc_8bit_displacement (); ret= 18; break;
    case 7:
      switch ( eareg )
        {
        case 0: addr= pw ().v; ret= 16; break;
        case 1: addr= pl ().v; ret= 20; break;
        default:
          _warning ( _udata, "EOR.b no accepta el mode:reg 7:%d", eareg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "EOR.b no accepta el mode %d", eamode );
      return UTIME;
    }
  
  MD_mem_write8 ( addr, eor_b ( _regs.D[reg].b.v0, MD_mem_read8 ( addr ) ) );
  
  return ret;
  
} /* end eorb */


static int
eorl (
      MDu8 const reg,
      MDu8 const eamode,
      MDu8 const eareg
      )
{
  
  int ret;
  MDu32 addr;
  
  
  switch ( eamode )
    {
    case 0: _regs.D[eareg]= eor_l ( _regs.D[reg], _regs.D[eareg] ); return 12;
    case 2: addr= _regs.A[eareg].v; ret= 20; break;
    case 3: addr= _regs.A[eareg].v; _regs.A[eareg].v+= 4; ret= 20; break;
    case 4: addr= (_regs.A[eareg].v-= 4); ret= 22; break;
    case 5: addr= pd16an ( _regs.A[eareg] ).v; ret= 24; break;
    case 6: addr= _regs.A[eareg].v + calc_8bit_displacement (); ret= 26; break;
    case 7:
      switch ( eareg )
        {
        case 0: addr= pw ().v; ret= 24; break;
        case 1: addr= pl ().v; ret= 28; break;
        default:
          _warning ( _udata, "EOR.l no accepta el mode:reg 7:%d", eareg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "EOR.l no accepta el mode %d", eamode );
      return UTIME;
    }
  
  write_long ( eor_l ( _regs.D[reg], read_long ( addr ) ), addr );
  
  return ret;
  
} /* end eorl */


static int
eorw (
      MDu8 const reg,
      MDu8 const eamode,
      MDu8 const eareg
      )
{
  
  int ret;
  MDu32 addr;
  
  
  switch ( eamode )
    {
    case 0:
      _regs.D[eareg].w.v0= eor_w ( _regs.D[reg].w.v0, _regs.D[eareg].w.v0 );
      return 8;
    case 2: addr= _regs.A[eareg].v; ret= 12; break;
    case 3: addr= _regs.A[eareg].v; _regs.A[eareg].v+= 2; ret= 12; break;
    case 4: addr= (_regs.A[eareg].v-= 2); ret= 14; break;
    case 5: addr= pd16an ( _regs.A[eareg] ).v; ret= 16; break;
    case 6: addr= _regs.A[eareg].v + calc_8bit_displacement (); ret= 18; break;
    case 7:
      switch ( eareg )
        {
        case 0: addr= pw ().v; ret= 16; break;
        case 1: addr= pl ().v; ret= 20; break;
        default:
          _warning ( _udata, "EOR.w no accepta el mode:reg 7:%d", eareg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "EOR.w no accepta el mode %d", eamode );
      return UTIME;
    }
  
  MD_mem_write ( addr, eor_w ( _regs.D[reg].w.v0, MD_mem_read ( addr ) ) );
  
  return ret;
  
} /* end eorw */


static int
notb (
      MDu8 const mode,
      MDu8 const reg
      )
{
  
  int ret;
  MDu32 addr;
  MDu8 val;
  
  
  switch ( mode )
    {
    case 0:
      _regs.D[reg].b.v0= val= ~_regs.D[reg].b.v0;
      logb_flags ( val );
      return 4;
    case 2: addr= _regs.A[reg].v; ret= 12; break;
    case 3:
      addr= _regs.A[reg].v;
      if ( reg == 7 ) _regs.A[reg].v+= 2;
      else            ++_regs.A[reg].v;
      ret= 12;
      break;
    case 4:
      addr= (reg==7) ? (_regs.A[reg].v-= 2) : (--_regs.A[reg].v);
      ret= 14;
      break;
    case 5: addr= pd16an ( _regs.A[reg] ).v; ret= 16; break;
    case 6: addr= _regs.A[reg].v + calc_8bit_displacement (); ret= 18; break;
    case 7:
      switch ( reg )
        {
        case 0: addr= pw ().v; ret= 16; break;
        case 1: addr= pl ().v; ret= 20; break;
        default:
          _warning ( _udata, "NOT.b no accepta el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "NOT.b no accepta el mode %d", mode );
      return UTIME;
    }
  
  MD_mem_write8 ( addr, val= ~MD_mem_read8 ( addr ) );
  logb_flags ( val );
  
  return ret;
  
} /* end notb */


static int
notl (
      MDu8 const mode,
      MDu8 const reg
      )
{
  
  int ret;
  MDu32 addr;
  MD_Reg32 val;
  
  
  switch ( mode )
    {
    case 0:
      _regs.D[reg].v= val.v= ~_regs.D[reg].v;
      logl_flags ( val );
      return 6;
    case 2: addr= _regs.A[reg].v; ret= 20; break;
    case 3: addr= _regs.A[reg].v; _regs.A[reg].v+= 4; ret= 20; break;
    case 4: addr= (_regs.A[reg].v-= 4); ret= 22; break;
    case 5: addr= pd16an ( _regs.A[reg] ).v; ret= 24; break;
    case 6: addr= _regs.A[reg].v + calc_8bit_displacement (); ret= 26; break;
    case 7:
      switch ( reg )
        {
        case 0: addr= pw ().v; ret= 24; break;
        case 1: addr= pl ().v; ret= 28; break;
        default:
          _warning ( _udata, "NOT.l no accepta el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "NOT.l no accepta el mode %d", mode );
      return UTIME;
    }
  
  val.v= ~read_long ( addr ).v;
  write_long ( val, addr );
  logl_flags ( val );
  
  return ret;
  
} /* end notl */


static int
notw (
      MDu8 const mode,
      MDu8 const reg
      )
{
  
  int ret;
  MDu32 addr;
  MD_Word val;
  
  
  switch ( mode )
    {
    case 0:
      _regs.D[reg].w.v0.v= val.v= ~_regs.D[reg].w.v0.v;
      logw_flags ( val );
      return 4;
    case 2: addr= _regs.A[reg].v; ret= 12; break;
    case 3: addr= _regs.A[reg].v; _regs.A[reg].v+= 2; ret= 12; break;
    case 4: addr= (_regs.A[reg].v-= 2); ret= 14; break;
    case 5: addr= pd16an ( _regs.A[reg] ).v; ret= 16; break;
    case 6: addr= _regs.A[reg].v + calc_8bit_displacement (); ret= 18; break;
    case 7:
      switch ( reg )
        {
        case 0: addr= pw ().v; ret= 16; break;
        case 1: addr= pl ().v; ret= 20; break;
        default:
          _warning ( _udata, "NOT.w no accepta el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "NOT.w no accepta el mode %d", mode );
      return UTIME;
    }
  
  val.v= ~MD_mem_read ( addr ).v;
  logw_flags ( val );
  MD_mem_write ( addr, val );
  
  return ret;
  
} /* end notw */


static MDu8
orb (
     const MDu8 a,
     const MDu8 b
     )
{
  
  MDu8 aux;
  
  
  aux= a | b;
  logb_flags ( aux );
  
  return aux;
  
} /* end orb */


static MD_Reg32
orl (
     const MD_Reg32 a,
     const MD_Reg32 b
     )
{
  
  MD_Reg32 aux;
  
  
  aux.v= a.v | b.v;
  logl_flags ( aux );
  
  return aux;
  
} /* end orl */


static MD_Word
orw (
     const MD_Word a,
     const MD_Word b
     )
{
  
  MD_Word aux;
  
  
  aux.v= a.v | b.v;
  logw_flags ( aux );
  
  return aux;
  
} /* orw */


/* Shift and Rotate Instructions **********************************************/
static MDu8
aslb (
      MDu8 const   val,
      unsigned int count
      )
{
  
  MDu8 ret, prev, next;
  MD_Bool flag;
  unsigned int i;
  
  
  if ( count > 9 ) count= 9;
  flag= MD_FALSE;
  ret= val;
  next= ret&0x80;
  for ( i= 0; i < count; ++i )
    {
      prev= next;
      ret<<= 1;
      next= ret&0x80;
      if ( prev^next ) flag= MD_TRUE;
    }
  if ( count == 0 )
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (next?NFLAG:0) |
        (ret?0:ZFLAG);
    }
  else
    {
      CLEAR_FLAGS ( XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (prev?(XFLAG|CFLAG):0) |
        (next?NFLAG:0) |
        (flag?VFLAG:0) |
        (ret?0:ZFLAG);
    }
  
  return ret;
  
} /* end aslb */


static MD_Reg32
asll (
      MD_Reg32 const val,
      unsigned int   count
      )
{
  
  MDu32 prev, next;
  MD_Reg32 ret;
  MD_Bool flag;
  unsigned int i;
  
  
  if ( count > 33 ) count= 33;
  flag= MD_FALSE;
  ret= val;
  next= ret.v&0x80000000;
  for ( i= 0; i < count; ++i )
    {
      prev= next;
      ret.v<<= 1;
      next= ret.v&0x80000000;
      if ( prev^next ) flag= MD_TRUE;
    }
  if ( count == 0 )
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (next?NFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  else
    {
      CLEAR_FLAGS ( XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (prev?(XFLAG|CFLAG):0) |
        (next?NFLAG:0) |
        (flag?VFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  
  return ret;
  
} /* end asll */


static MD_Word
aslw (
      MD_Word const val,
      unsigned int  count
      )
{
  
  MDu16 prev, next;
  MD_Word ret;
  MD_Bool flag;
  unsigned int i;
  
  
  if ( count > 17 ) count= 17;
  flag= MD_FALSE;
  ret= val;
  next= ret.v&0x8000;
  for ( i= 0; i < count; ++i )
    {
      prev= next;
      ret.v<<= 1;
      next= ret.v&0x8000;
      if ( prev^next ) flag= MD_TRUE;
    }
  if ( count == 0 )
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (next?NFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  else
    {
      CLEAR_FLAGS ( XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (prev?(XFLAG|CFLAG):0) |
        (next?NFLAG:0) |
        (flag?VFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  
  return ret;
  
} /* end aslw */


static MDu8
asrb (
      MDu8 const   val,
      unsigned int count
      )
{
  
  MDu8 ret, prev, next, msb;
  unsigned int i;
  
  
  if ( count > 9 ) count= 9;
  ret= val;
  msb= val&0x80;
  next= ret&0x01;
  for ( i= 0; i < count; ++i )
    {
      prev= next;
      ret= (ret>>1)|msb;
      next= ret&0x01;
      }
  if ( count == 0 )
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (msb?NFLAG:0) |
        (ret?0:ZFLAG);
    }
  else
    {
      CLEAR_FLAGS ( XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (prev?(XFLAG|CFLAG):0) |
        (msb?NFLAG:0) |
        (ret?0:ZFLAG);
    }
  
  return ret;
  
} /* end asrb */


static MD_Reg32
asrl (
      MD_Reg32 const val,
      unsigned int   count
      )
{
  
  MD_Reg32 ret;
  MDu32 prev, next, msb;
  unsigned int i;
  
  
  if ( count > 33 ) count= 33;
  ret= val;
  msb= val.v&0x80000000;
  next= ret.v&0x00000001;
  for ( i= 0; i < count; ++i )
    {
      prev= next;
      ret.v= (ret.v>>1)|msb;
      next= ret.v&0x00000001;
      }
  if ( count == 0 )
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (msb?NFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  else
    {
      CLEAR_FLAGS ( XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (prev?(XFLAG|CFLAG):0) |
        (msb?NFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  
  return ret;
  
} /* end asrl */


static MD_Word
asrw (
      MD_Word const val,
      unsigned int  count
      )
{
  
  MD_Word ret;
  MDu16 prev, next, msb;
  unsigned int i;
  
  
  if ( count > 17 ) count= 17;
  ret= val;
  msb= val.v&0x8000;
  next= ret.v&0x0001;
  for ( i= 0; i < count; ++i )
    {
      prev= next;
      ret.v= (ret.v>>1)|msb;
      next= ret.v&0x0001;
      }
  if ( count == 0 )
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (msb?NFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  else
    {
      CLEAR_FLAGS ( XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (prev?(XFLAG|CFLAG):0) |
        (msb?NFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  
  return ret;
  
} /* end asrw */


static MDu8
lslb (
      MDu8 const   val,
      unsigned int count
      )
{
  
  MDu8 ret, prev, next;
  unsigned int i;
  
  
  if ( count > 9 ) count= 9;
  ret= val;
  next= ret&0x80;
  for ( i= 0; i < count; ++i )
    {
      prev= next;
      ret<<= 1;
      next= ret&0x80;
    }
  if ( count == 0 )
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (next?NFLAG:0) |
        (ret?0:ZFLAG);
    }
  else
    {
      CLEAR_FLAGS ( XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (prev?(XFLAG|CFLAG):0) |
        (next?NFLAG:0) |
        (ret?0:ZFLAG);
    }
  
  return ret;
  
} /* end lslb */


static MD_Reg32
lsll (
      MD_Reg32 const val,
      unsigned int   count
      )
{
  
  MDu32 prev, next;
  MD_Reg32 ret;
  unsigned int i;
  
  
  if ( count > 33 ) count= 33;
  ret= val;
  next= ret.v&0x80000000;
  for ( i= 0; i < count; ++i )
    {
      prev= next;
      ret.v<<= 1;
      next= ret.v&0x80000000;
    }
  if ( count == 0 )
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (next?NFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  else
    {
      CLEAR_FLAGS ( XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (prev?(XFLAG|CFLAG):0) |
        (next?NFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  
  return ret;
  
} /* end lsll */


static MD_Word
lslw (
      MD_Word const val,
      unsigned int  count
      )
{
  
  MDu16 prev, next;
  MD_Word ret;
  unsigned int i;
  
  
  if ( count > 17 ) count= 17;
  ret= val;
  next= ret.v&0x8000;
  for ( i= 0; i < count; ++i )
    {
      prev= next;
      ret.v<<= 1;
      next= ret.v&0x8000;
    }
  if ( count == 0 )
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (next?NFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  else
    {
      CLEAR_FLAGS ( XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (prev?(XFLAG|CFLAG):0) |
        (next?NFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  
  return ret;
  
} /* end lslw */


static MDu8
lsrb (
      MDu8 const   val,
      unsigned int count
      )
{
  
  MDu8 ret, prev, next;
  unsigned int i;
  
  
  if ( count > 9 ) count= 9;
  ret= val;
  next= ret&0x01;
  for ( i= 0; i < count; ++i )
    {
      prev= next;
      ret>>= 1;
      next= ret&0x01;
      }
  if ( count == 0 )
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        ((ret&0x80)?NFLAG:0) |
        (ret?0:ZFLAG);
    }
  else
    {
      CLEAR_FLAGS ( XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (prev?(XFLAG|CFLAG):0) |
        ((ret&0x80)?NFLAG:0) |
        (ret?0:ZFLAG);
    }
  
  return ret;
  
} /* end lsrb */


static MD_Reg32
lsrl (
      MD_Reg32 const val,
      unsigned int   count
      )
{
  
  MD_Reg32 ret;
  MDu32 prev, next;
  unsigned int i;
  
  
  if ( count > 33 ) count= 33;
  ret= val;
  next= ret.v&0x00000001;
  for ( i= 0; i < count; ++i )
    {
      prev= next;
      ret.v>>= 1;
      next= ret.v&0x00000001;
      }
  if ( count == 0 )
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        ((ret.v&0x80000000)?NFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  else
    {
      CLEAR_FLAGS ( XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (prev?(XFLAG|CFLAG):0) |
        ((ret.v&0x80000000)?NFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  
  return ret;
  
} /* end lsrl */


static MD_Word
lsrw (
      MD_Word const val,
      unsigned int  count
      )
{
  
  MD_Word ret;
  MDu16 prev, next;
  unsigned int i;
  
  
  if ( count > 17 ) count= 17;
  ret= val;
  next= ret.v&0x0001;
  for ( i= 0; i < count; ++i )
    {
      prev= next;
      ret.v>>= 1;
      next= ret.v&0x0001;
      }
  if ( count == 0 )
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        ((ret.v&0x8000)?NFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  else
    {
      CLEAR_FLAGS ( XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (prev?(XFLAG|CFLAG):0) |
        ((ret.v&0x8000)?NFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  
  return ret;
  
} /* end lsrw */


static MDu8
rolb (
      MDu8 const   val,
      unsigned int count
      )
{
  
  MDu8 ret, prev, next;
  unsigned int i;
  
  
  ret= val;
  next= ret&0x80;
  for ( i= 0; i < count; ++i )
    {
      prev= next;
      ret= (ret<<1) | (prev?0x1:0x0);
      next= ret&0x80;
    }
  if ( count == 0 )
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (next?NFLAG:0) |
        (ret?0:ZFLAG);
    }
  else
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (prev?(CFLAG):0) |
        (next?NFLAG:0) |
        (ret?0:ZFLAG);
    }
  
  return ret;
  
} /* end rolb */


static MD_Reg32
roll (
      MD_Reg32 const val,
      unsigned int   count
      )
{
  
  MD_Reg32 ret;
  MDu32 prev, next;
  unsigned int i;
  
  
  ret= val;
  next= ret.v&0x80000000;
  for ( i= 0; i < count; ++i )
    {
      prev= next;
      ret.v= (ret.v<<1) | (prev?0x1:0x0);
      next= ret.v&0x80000000;
    }
  if ( count == 0 )
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (next?NFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  else
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (prev?(CFLAG):0) |
        (next?NFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  
  return ret;
  
} /* end roll */


static MD_Word
rolw (
      MD_Word const val,
      unsigned int  count
      )
{
  
  MD_Word ret;
  MDu16 prev, next;
  unsigned int i;
  
  
  ret= val;
  next= ret.v&0x8000;
  for ( i= 0; i < count; ++i )
    {
      prev= next;
      ret.v= (ret.v<<1) | (prev?0x1:0x0);
      next= ret.v&0x8000;
    }
  if ( count == 0 )
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (next?NFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  else
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (prev?(CFLAG):0) |
        (next?NFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  
  return ret;
  
} /* end rolw */


static MDu8
rorb (
      MDu8 const   val,
      unsigned int count
      )
{
  
  MDu8 ret, prev, next;
  unsigned int i;
  
  
  ret= val;
  next= ret&0x01;
  for ( i= 0; i < count; ++i )
    {
      prev= next;
      ret= (ret>>1) | (prev?0x80:0x00);
      next= ret&0x01;
    }
  if ( count == 0 )
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        ((ret&0x80)?NFLAG:0) |
        (ret?0:ZFLAG);
    }
  else
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (prev?(NFLAG|CFLAG):0) |
        (ret?0:ZFLAG);
    }
  
  return ret;
  
} /* end rorb */


static MD_Reg32
rorl (
      MD_Reg32 const val,
      unsigned int   count
      )
{
  
  MD_Reg32 ret;
  MDu32 prev, next;
  unsigned int i;
  
  
  ret= val;
  next= ret.v&0x00000001;
  for ( i= 0; i < count; ++i )
    {
      prev= next;
      ret.v= (ret.v>>1) | (prev?0x80000000:0x00000000);
      next= ret.v&0x00000001;
    }
  if ( count == 0 )
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        ((ret.v&0x80000000)?NFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  else
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (prev?(NFLAG|CFLAG):0) |
        (ret.v?0:ZFLAG);
    }
  
  return ret;
  
} /* end rorl */


static MD_Word
rorw (
      MD_Word const val,
      unsigned int  count
      )
{
  
  MD_Word ret;
  MDu16 prev, next;
  unsigned int i;
  
  
  ret= val;
  next= ret.v&0x0001;
  for ( i= 0; i < count; ++i )
    {
      prev= next;
      ret.v= (ret.v>>1) | (prev?0x8000:0x0000);
      next= ret.v&0x0001;
    }
  if ( count == 0 )
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        ((ret.v&0x8000)?NFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  else
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (prev?(NFLAG|CFLAG):0) |
        (ret.v?0:ZFLAG);
    }
  
  return ret;
  
} /* end rorw */


static MDu8
roxlb (
       MDu8 const   val,
       unsigned int count
       )
{
  
  MDu8 ret, next, xflag;
  unsigned int i;
  
  
  ret= val;
  next= ret&0x80;
  xflag= _regs.CCR.v&XFLAG;
  for ( i= 0; i < count; ++i )
    {
      ret= (ret<<1) | (xflag?0x1:0x0);
      xflag= next;
      next= ret&0x80;
    }
  if ( count == 0 )
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (next?NFLAG:0) |
        (ret?0:ZFLAG) |
        (xflag?CFLAG:0);
    }
  else
    {
      CLEAR_FLAGS ( XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (xflag?(XFLAG|CFLAG):0) |
        (next?NFLAG:0) |
        (ret?0:ZFLAG);
    }
  
  return ret;
  
} /* end roxlb */


static MD_Reg32
roxll (
       MD_Reg32 const val,
       unsigned int   count
       )
{
  
  MD_Reg32 ret;
  MDu32 next, xflag;
  unsigned int i;
  
  
  ret= val;
  next= ret.v&0x80000000;
  xflag= _regs.CCR.v&XFLAG;
  for ( i= 0; i < count; ++i )
    {
      ret.v= (ret.v<<1) | (xflag?0x1:0x0);
      xflag= next;
      next= ret.v&0x80000000;
    }
  if ( count == 0 )
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (next?NFLAG:0) |
        (ret.v?0:ZFLAG) |
        (xflag?CFLAG:0);
    }
  else
    {
      CLEAR_FLAGS ( XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (xflag?(XFLAG|CFLAG):0) |
        (next?NFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  
  return ret;
  
} /* end roxll */


static MD_Word
roxlw (
       MD_Word const val,
       unsigned int  count
       )
{
  
  MD_Word ret;
  MDu16 next, xflag;
  unsigned int i;
  
  
  ret= val;
  next= ret.v&0x8000;
  xflag= _regs.CCR.v&XFLAG;
  for ( i= 0; i < count; ++i )
    {
      ret.v= (ret.v<<1) | (xflag?0x1:0x0);
      xflag= next;
      next= ret.v&0x8000;
    }
  if ( count == 0 )
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (next?NFLAG:0) |
        (ret.v?0:ZFLAG) |
        (xflag?CFLAG:0);
    }
  else
    {
      CLEAR_FLAGS ( XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (xflag?(XFLAG|CFLAG):0) |
        (next?NFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  
  return ret;
  
} /* end roxlw */


static MDu8
roxrb (
       MDu8 const   val,
       unsigned int count
       )
{
  
  MDu8 ret, xflag, next;
  unsigned int i;
  
  
  ret= val;
  next= ret&0x01;
  xflag= _regs.CCR.v&XFLAG;
  for ( i= 0; i < count; ++i )
    {
      ret= (ret>>1) | (xflag?0x80:0x00);
      xflag= next;
      next= ret&0x01;
    }
  if ( count == 0 )
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        ((ret&0x80)?NFLAG:0) |
        (ret?0:ZFLAG) |
        (xflag?CFLAG:0);
    }
  else
    {
      CLEAR_FLAGS ( XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (xflag?(XFLAG|CFLAG):0) |
        ((ret&0x80)?NFLAG:0) |
        (ret?0:ZFLAG);
    }
  
  return ret;
  
} /* end roxrb */


static MD_Reg32
roxrl (
       MD_Reg32 const val,
       unsigned int   count
       )
{
  
  MD_Reg32 ret;
  MDu32 xflag, next;
  unsigned int i;
  
  
  ret= val;
  next= ret.v&0x00000001;
  xflag= _regs.CCR.v&XFLAG;
  for ( i= 0; i < count; ++i )
    {
      ret.v= (ret.v>>1) | (xflag?0x80000000:0x00000000);
      xflag= next;
      next= ret.v&0x00000001;
    }
  if ( count == 0 )
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        ((ret.v&0x80000000)?NFLAG:0) |
        (ret.v?0:ZFLAG) |
        (xflag?CFLAG:0);
    }
  else
    {
      CLEAR_FLAGS ( XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (xflag?(XFLAG|CFLAG):0) |
        ((ret.v&0x80000000)?NFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  
  return ret;
  
} /* end roxrl */


static MD_Word
roxrw (
       MD_Word const val,
       unsigned int  count
       )
{
  
  MD_Word ret;
  MDu16 xflag, next;
  unsigned int i;
  
  
  ret= val;
  next= ret.v&0x0001;
  xflag= _regs.CCR.v&XFLAG;
  for ( i= 0; i < count; ++i )
    {
      ret.v= (ret.v>>1) | (xflag?0x8000:0x0000);
      xflag= next;
      next= ret.v&0x0001;
    }
  if ( count == 0 )
    {
      CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        ((ret.v&0x8000)?NFLAG:0) |
        (ret.v?0:ZFLAG) |
        (xflag?CFLAG:0);
    }
  else
    {
      CLEAR_FLAGS ( XFLAG|NFLAG|ZFLAG|VFLAG|CFLAG );
      _regs.CCR.v|=
        (xflag?(XFLAG|CFLAG):0) |
        ((ret.v&0x8000)?NFLAG:0) |
        (ret.v?0:ZFLAG);
    }
  
  return ret;
  
} /* end roxrw */


static int
sop_mem (
         MDu8 const     mode,
         MDu8 const     reg,
         sopw_t * const op
         )
{
  
  int ret;
  MDu32 addr;
  
  
  switch ( mode )
    {
    case 2: addr= _regs.A[reg].v; ret= 12; break;
    case 3: addr= _regs.A[reg].v; _regs.A[reg].v+= 2; ret= 12; break;
    case 4: addr= (_regs.A[reg].v-= 2); ret= 14; break;
    case 5: addr= pd16an ( _regs.A[reg] ).v; ret= 16; break;
    case 6: addr= _regs.A[reg].v + calc_8bit_displacement (); ret= 18; break;
    case 7:
      switch ( reg )
        {
        case 0: addr= pw ().v; ret= 16; break;
        case 1: addr= pl ().v; ret= 20; break;
        default:
          _warning ( _udata,
        	     "Les operacions shift/rotate (SOP <ea>)"
        	     " no accepten el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata,
        	 "Les operacions shift/rotate (SOP <ea>)"
        	 " no accepten el mode %d", mode );
      return UTIME;
    }
  
  MD_mem_write ( addr, op ( MD_mem_read ( addr ), 1 ) );
  
  return ret + 2;
  
} /* end sop_mem */


static int
sopb_inm (
          MDu8           count,
          MDu8 const     reg,
          sopb_t * const op
          )
{
  
  if ( count == 0 ) count= 8;
  _regs.D[reg].b.v0= op ( _regs.D[reg].b.v0, count );
  
  return 6 + 2*count;
  
} /* end sopb_inm */


static int
sopb_reg (
          MDu8 const     regc,
          MDu8 const     reg,
          sopb_t * const op
          )
{
  
  int count;
  
  
  count= _regs.D[regc].v&0x3F;
  _regs.D[reg].b.v0= op ( _regs.D[reg].b.v0, count );
  
  return 6 + 2*count;
  
} /* end sopb_reg */


static int
sopl_inm (
          MDu8           count,
          MDu8 const     reg,
          sopl_t * const op
          )
{
  
  if ( count == 0 ) count= 8;
  _regs.D[reg]= op ( _regs.D[reg], count );
  
  return 8 + 2*count;
  
} /* end sopl_inm */


static int
sopl_reg (
          MDu8 const     regc,
          MDu8 const     reg,
          sopl_t * const op
          )
{
  
  int count;
  
  
  count= _regs.D[regc].v&0x3F;
  _regs.D[reg]= op ( _regs.D[reg], count );
  
  return 8 + 2*count;
  
} /* end sopl_reg */


static int
sopw_inm (
          MDu8           count,
          MDu8 const     reg,
          sopw_t * const op
          )
{
  
  if ( count == 0 ) count= 8;
  _regs.D[reg].w.v0= op ( _regs.D[reg].w.v0, count );
  
  return 6 + 2*count;
  
} /* end sopw_inm */


static int
sopw_reg (
          MDu8 const     regc,
          MDu8 const     reg,
          sopw_t * const op
          )
{
  
  int count;
  
  
  count= _regs.D[regc].v&0x3F;
  _regs.D[reg].w.v0= op ( _regs.D[reg].w.v0, count );
  
  return 6 + 2*count;
  
} /* end sopw_reg */


static int
swap (
      MDu8 const reg
      )
{
  
  MD_Word tmp;
  
  
  tmp= _regs.D[reg].w.v0;
  _regs.D[reg].w.v0= _regs.D[reg].w.v1;
  _regs.D[reg].w.v1= tmp;
  CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
  _regs.CCR.v|=
    ((_regs.D[reg].v&0x80000000)?NFLAG:0) |
    (_regs.D[reg].v?0:ZFLAG);
  
  return 4;
  
} /* end swap */


/* Bit Manipulation Instructions **********************************************/
static int
bchg_mem (
          MDu32 const addr,
          int const   bit
          )
{
  
  MDu8 mask, byte;
  
  
  mask= 0x1<<bit;
  byte= MD_mem_read8 ( addr );
  if ( byte&mask ) CLEAR_FLAGS ( ZFLAG );
  else _regs.CCR.v|= ZFLAG;
  MD_mem_write8 ( addr, byte^mask );
  
  return 8;
  
} /* end bchg_mem */


static int
bchg_reg (
          MDu8 const reg,
          int const  bit
          )
{
  
  MDu32 mask;
  
  
  mask= 0x1<<bit;
  if ( _regs.D[reg].v&mask ) CLEAR_FLAGS ( ZFLAG );
  else                       _regs.CCR.v|= ZFLAG;
  _regs.D[reg].v^= mask;
  
  return 12;
  
} /* end bchg_reg */


static int
bclr_mem (
          MDu32 const addr,
          int const   bit
          )
{
  
  MDu8 mask, byte;
  
  
  mask= 0x1<<bit;
  byte= MD_mem_read8 ( addr );
  if ( byte&mask ) CLEAR_FLAGS ( ZFLAG );
  else _regs.CCR.v|= ZFLAG;
  MD_mem_write8 ( addr, byte&(~mask) );
  
  return 8; /* ¿¿??? No entenc timing.txt */
  
} /* end bclr_mem */


static int
bclr_reg (
          MDu8 const reg,
          int const  bit
          )
{
  
  MDu32 mask;
  
  
  mask= 0x1<<bit;
  if ( _regs.D[reg].v&mask ) CLEAR_FLAGS ( ZFLAG );
  else                       _regs.CCR.v|= ZFLAG;
  _regs.D[reg].v&= ~mask;
  
  return 12; /* ¿¿¿??? No entenc timing.txt */
  
} /* end bclr_reg */


static int
bop_reg (
         MDu8 const  reg,
         MDu8 const  eamode,
         MDu8 const  eareg,
         bop_mem_t  *op_mem,
         bop_reg_t  *op_reg
         )
{
  
  int ret;
  MDu32 addr;
  
  
  switch ( eamode )
    {
    case 0: return op_reg ( eareg, _regs.D[reg].v&0x1F );
    case 2: addr= _regs.A[eareg].v; ret= 4; break;
    case 3:
      addr= _regs.A[eareg].v;
      if ( eareg == 7 ) _regs.A[eareg].v+= 2;
      else              ++_regs.A[eareg].v;
      ret= 4;
      break;
    case 4:
      addr= (eareg==7) ? (_regs.A[eareg].v-= 2) : (--_regs.A[eareg].v);
      ret= 6;
      break;
    case 5: addr= pd16an ( _regs.A[eareg] ).v; ret= 8; break;
    case 6: addr= _regs.A[eareg].v + calc_8bit_displacement (); ret= 10; break;
    case 7:
      switch ( eareg )
        {
        case 0: addr= pw ().v; ret= 8; break;
        case 1: addr= pl ().v; ret= 12; break;
        default:
          _warning ( _udata,
                     "Les operacions sobre bits (B* Dn,<ea>)"
                     " no accepten el mode:reg 7:%d", eareg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata,
                 "Les operacions sobre bits (B* Dn,<ea>)"
                 " no accepten el mode %d", eamode );
      return UTIME;
    }
  
  return ret + op_mem ( addr, _regs.D[reg].v&0x7 );
  
} /* end bop_reg */


static int
bop_inm (
         MDu8 const  eamode,
         MDu8 const  eareg,
         bop_mem_t  *op_mem,
         bop_reg_t  *op_reg
         )
{
  
  int ret;
  MDu32 addr;
  MDu8 byte;
  
  
  byte= inm_byte ();
  switch ( eamode )
    {
    case 0: return op_reg ( eareg, byte&0x1F );
    case 2: addr= _regs.A[eareg].v; ret= 4; break;
    case 3:
      addr= _regs.A[eareg].v;
      if ( eareg == 7 ) _regs.A[eareg].v+= 2;
      else              ++_regs.A[eareg].v;
      ret= 4;
      break;
    case 4:
      addr= (eareg==7) ? (_regs.A[eareg].v-= 2) : (--_regs.A[eareg].v);
      ret= 6;
      break;
    case 5: addr= pd16an ( _regs.A[eareg] ).v; ret= 8; break;
    case 6: addr= _regs.A[eareg].v + calc_8bit_displacement (); ret= 10; break;
    case 7:
      switch ( eareg )
        {
        case 0: addr= pw ().v; ret= 8; break;
        case 1: addr= pl ().v; ret= 12; break;
        default:
          _warning ( _udata,
                     "Les operacions sobre bits (B* #<data>,<ea>)"
                     " no accepten el mode:reg 7:%d", eareg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata,
                 "Les operacions sobre bits (B* #<data>,<ea>)"
                 " no accepten el mode %d", eamode );
      return UTIME;
    }
  
  return ret + op_mem ( addr, byte&0x7 );
  
} /* end bop_inm */


static int
bset_mem (
          MDu32 const addr,
          int const   bit
          )
{
  
  MDu8 mask, byte;
  
  
  mask= 0x1<<bit;
  byte= MD_mem_read8 ( addr );
  if ( byte&mask ) CLEAR_FLAGS ( ZFLAG );
  else _regs.CCR.v|= ZFLAG;
  MD_mem_write8 ( addr, byte|mask );
  
  return 8;
  
} /* end bset_mem */


static int
bset_reg (
          MDu8 const reg,
          int const  bit
          )
{
  
  MDu32 mask;
  
  
  mask= 0x1<<bit;
  if ( _regs.D[reg].v&mask ) CLEAR_FLAGS ( ZFLAG );
  else                       _regs.CCR.v|= ZFLAG;
  _regs.D[reg].v|= mask;
  
  return 12;
  
} /* end bset_reg */


static int
btst_reg (
          MDu8 const  reg,
          MDu8 const  eamode,
          MDu8 const  eareg
          )
{
  
  int ret;
  MDu32 addr, mask;
  MDu8 byte;
  
  
  switch ( eamode )
    {
    case 0:
      mask= 0x1<<(_regs.D[reg].v&0x1F);
      if ( _regs.D[eareg].v&mask ) CLEAR_FLAGS ( ZFLAG );
      else                         _regs.CCR.v|= ZFLAG;
      return 10; /* ¿¿?? No entenc timing.txt */
    case 2: addr= _regs.A[eareg].v; ret= 4; break;
    case 3:
      addr= _regs.A[eareg].v;
      if ( eareg == 7 ) _regs.A[eareg].v+= 2;
      else              ++_regs.A[eareg].v;
      ret= 4;
      break;
    case 4:
      addr= (eareg==7) ? (_regs.A[eareg].v-= 2) : (--_regs.A[eareg].v);
      ret= 6;
      break;
    case 5: addr= pd16an ( _regs.A[eareg] ).v; ret= 8; break;
    case 6: addr= _regs.A[eareg].v + calc_8bit_displacement (); ret= 10; break;
    case 7:
      switch ( eareg )
        {
        case 0: addr= pw ().v; ret= 8; break;
        case 1: addr= pl ().v; ret= 12; break;
        case 2: addr= pd16pc ().v; ret= 8; break;
        case 3:
          addr= _regs.PC;
          addr+= calc_8bit_displacement ();
          ret= 10;
          break;
        case 4:
          byte= inm_byte ();
          mask= 0x1<<(_regs.D[reg].v&0x7);
          if ( byte&mask ) CLEAR_FLAGS ( ZFLAG );
          else             _regs.CCR.v|= ZFLAG;
          return 4 + 6;
          break;
        default:
          _warning ( _udata,
        	     "BTST Dn,<ea> no accepta el mode:reg 7:%d", eareg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "BTST Dn,<ea> no accepta el mode %d", eamode );
      return UTIME;
    }
  
  mask= 0x1<<(_regs.D[reg].v&0x7);
  if ( MD_mem_read8 ( addr )&mask ) CLEAR_FLAGS ( ZFLAG );
  else                              _regs.CCR.v|= ZFLAG;
  
  return ret + 6; /* ¿¿?? No entenc timing.txt */
  
} /* end btst_reg */


static int
btst_inm (
          MDu8 const  eamode,
          MDu8 const  eareg
          )
{
  
  int ret;
  MDu32 addr, mask;
  MDu8 byte;
  
  
  byte= inm_byte ();
  switch ( eamode )
    {
    case 0:
      mask= 0x1<<(byte&0x1F);
      if ( _regs.D[eareg].v&mask ) CLEAR_FLAGS ( ZFLAG );
      else                         _regs.CCR.v|= ZFLAG;
      return 10; /* ¿¿?? No entenc timing.txt */
    case 2: addr= _regs.A[eareg].v; ret= 4; break;
    case 3:
      addr= _regs.A[eareg].v;
      if ( eareg == 7 ) _regs.A[eareg].v+= 2;
      else              ++_regs.A[eareg].v;
      ret= 4;
      break;
    case 4:
      addr= (eareg==7) ? (_regs.A[eareg].v-= 2) : (--_regs.A[eareg].v);
      ret= 6;
      break;
    case 5: addr= pd16an ( _regs.A[eareg] ).v; ret= 8; break;
    case 6: addr= _regs.A[eareg].v + calc_8bit_displacement (); ret= 10; break;
    case 7:
      switch ( eareg )
        {
        case 0: addr= pw ().v; ret= 8; break;
        case 1: addr= pl ().v; ret= 12; break;
        case 2: addr= pd16pc ().v; ret= 8; break;
        case 3:
          addr= _regs.PC;
          addr+= calc_8bit_displacement ();
          ret= 10;
          break;
        default:
          _warning ( _udata,
                     "BTST #<data>,<ea> no accepta el mode:reg 7:%d", eareg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "BTST #<data>,<ea> no accepta el mode %d", eamode );
      return UTIME;
    }
  
  mask= 0x1<<(byte&0x7);
  if ( MD_mem_read8 ( addr )&mask ) CLEAR_FLAGS ( ZFLAG );
  else                              _regs.CCR.v|= ZFLAG;
  
  return ret + 6; /* ¿¿?? No entenc timing.txt */
  
} /* end btst_inm */


/* Binary-Coded Decimal Instructions ******************************************/
static MDu8
abcd_op (
         const MDu8 src,
         const MDu8 dst
         )
{
  
  MDu32 aux;
  MDu8 ret;
  MD_Bool cflag, hflag;
  
  
  aux= src + dst + ((_regs.CCR.v&XFLAG)?1:0);
  cflag= ((aux&0x100)!=0);
  hflag= ((((src^dst)^aux)&0x10)!=0);
  ret= (MDu8) aux;
  CLEAR_FLAGS ( XFLAG|CFLAG );
  if ( hflag || (ret&0xF) > 9 )
    {
      if ( cflag || ret > 0x99 ) { ret+= 0x66; _regs.CCR.v|= (CFLAG|XFLAG); }
      else                         ret+= 0x06;
    }
  else
    {
      if ( cflag || ret > 0x99 ) { ret+= 0x60; _regs.CCR.v|= (CFLAG|XFLAG); }
    }
  if ( ret ) CLEAR_FLAGS ( ZFLAG );
  
  return ret;
  
} /* end abcd_op */


static MDu8
sbcd_op (
         const MDu8 src,
         const MDu8 dst
         )
{
  
  MDu8 neg, ret;
  MDu32 aux;
  MD_Bool cflag, hflag;
  
  
  neg= C1B(src);
  aux= (dst + neg) + ((_regs.CCR.v&XFLAG)?0:1);
  cflag= ((aux&0x100)==0);
  hflag= ((((neg^dst)^aux)&0x10)==0);
  ret= (MDu8) aux;
  CLEAR_FLAGS ( XFLAG|CFLAG );
  if ( hflag )
    {
      if ( cflag ) { ret+= 0x9A; _regs.CCR.v|= (CFLAG|XFLAG); }
      else           ret+= 0xFA;
    }
  else
    {
      if ( cflag ) { ret+= 0xA0; _regs.CCR.v|= (CFLAG|XFLAG); }
    }
  if ( ret ) CLEAR_FLAGS ( ZFLAG );
  
  return ret;
  
} /* end sbcd_op */


static int
nbcd (
      MDu8 const mode,
      MDu8 const reg
      )
{
  
  int ret;
  MDu32 addr;
  
  
  switch ( mode )
    {
    case 0: _regs.D[reg].b.v0= sbcd_op ( _regs.D[reg].b.v0, 0 ); return 6;
    case 2: addr= _regs.A[reg].v; ret= 12; break;
    case 3:
      addr= _regs.A[reg].v;
      if ( reg == 7 ) _regs.A[reg].v+= 2;
      else            ++_regs.A[reg].v;
      ret= 12;
      break;
    case 4:
      addr= (reg==7)?(_regs.A[reg].v-= 2):(--_regs.A[reg].v);
      ret= 14;
      break;
    case 5: addr= pd16an ( _regs.A[reg] ).v; ret= 16; break;
    case 6: addr= _regs.A[reg].v + calc_8bit_displacement (); ret= 18; break;
    case 7:
      switch ( reg )
        {
        case 0: addr= pw ().v; ret= 16; break;
        case 1: addr= pl ().v; ret= 20; break;
        default:
          _warning ( _udata, "NBCD no accepta el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "NBCD no accepta el mode %d", mode );
      return UTIME;
    }
  
  MD_mem_write8 ( addr, sbcd_op ( MD_mem_read8 ( addr ), 0 ) );
  
  return ret;
  
} /* end nbcd */


/* Program Control Instructions ***********************************************/
/* Totes les condicions. */
static MD_Bool
get_cond (
          MDu8 const cond
          )
{
  
  switch ( cond )
    {
    case 0x0: return MD_TRUE;
    case 0x1: return MD_FALSE;
    case 0x2: return (!(_regs.CCR.v&CFLAG)) && (!(_regs.CCR.v&ZFLAG));
    case 0x3: return (_regs.CCR.v&CFLAG) || (_regs.CCR.v&ZFLAG);
    case 0x4: return !(_regs.CCR.v&CFLAG);
    case 0x5: return ((_regs.CCR.v&CFLAG)!=0);
    case 0x6: return !(_regs.CCR.v&ZFLAG);
    case 0x7: return ((_regs.CCR.v&ZFLAG)!=0);
    case 0x8: return !(_regs.CCR.v&VFLAG);
    case 0x9: return ((_regs.CCR.v&VFLAG)!=0);
    case 0xA: return !(_regs.CCR.v&NFLAG);
    case 0xB: return ((_regs.CCR.v&NFLAG)!=0);
    case 0xC: return
        ((_regs.CCR.v&NFLAG) && (_regs.CCR.v&VFLAG)) ||
        ((!(_regs.CCR.v&NFLAG)) && (!(_regs.CCR.v&VFLAG)));
    case 0xD: return
        ((_regs.CCR.v&NFLAG) && (!(_regs.CCR.v&VFLAG))) ||
        ((!(_regs.CCR.v&NFLAG)) && (_regs.CCR.v&VFLAG));
    case 0xE: return
        ((_regs.CCR.v&NFLAG) && (_regs.CCR.v&VFLAG)
         && (!(_regs.CCR.v&ZFLAG))) ||
        ((!(_regs.CCR.v&NFLAG)) && (!(_regs.CCR.v&VFLAG))
         && (!(_regs.CCR.v&ZFLAG)));
    case 0xF: return
        (_regs.CCR.v&ZFLAG) ||
        ((_regs.CCR.v&NFLAG) && (!(_regs.CCR.v&VFLAG))) ||
        ((!(_regs.CCR.v&NFLAG)) && (_regs.CCR.v&VFLAG));
    default: return MD_FALSE; /* Açò no té que passar. */
    }
  
} /* end get_cond */


static int
bcc (
     MDu8 const cond,
     MDu8 const disp
     )
{
  
  
  MDu32 newaddr;
  MD_Bool isword;
  
  
  /* IRRELEVANT!!! MAI APLEGA ACÍ
  if ( cond == 0 || cond == 1 )
    {
      _warning ( _udata, "Bcc no accepta condició %d", cond );
      return UTIME;
    }
  */
  isword= (disp==0x00);
  if ( isword )
    {
      newaddr= _regs.PC;
      newaddr+= (MDs16) inm_word ().v;
    }
  else newaddr= 0; /* CALLA!!! */
  if ( get_cond ( cond ) )
    {
      if ( isword ) _regs.PC= newaddr;
      else _regs.PC+= (MDs8) disp;
      return 10;
    }
  
  return isword ? 12 : 8;
  
} /* end bcc */


static int
bra (
     MDu8 const disp
     )
{
  
  MDs16 disp16;
  MDu32 addr;
  
  
  if ( disp==0x00 )
    {
      addr= _regs.PC;
      disp16= (MDs16) inm_word ().v;
      _regs.PC= addr + disp16;
    }
  else _regs.PC+= (MDs8) disp;
  
  return 10;
  
} /* end bra */


static int
bsr (
     MDu8 const disp
     )
{
  
  MDu32 newaddr;
  MD_Bool isword;
  
  
  isword= (disp==0x00);
  if ( isword )
    {
      newaddr= _regs.PC;
      newaddr+= (MDs16) inm_word ().v;
    }
  _regs.A[7].v-= 4;
  write_long ( (MD_Reg32) _regs.PC, _regs.A[7].v );
  if ( isword ) _regs.PC= newaddr;
  else _regs.PC+= (MDs8) disp;
  
  return 18;
  
} /* end bsr */


static int
dbcc (
      MDu8 const cond,
      MDu8 const reg
      )
{
  
  /* NOTA: No entenc timing.txt !!!! */
  
  MDu32 addr;
  MDs16 disp;
  
  
  addr= _regs.PC;
  disp= (MDs16) inm_word ().v;
  if ( !get_cond ( cond ) )
    {
      if ( (--_regs.D[reg].w.v0.v) != 0xFFFF )
        {
          _regs.PC= addr + disp;
          return 14;
        }
      else return 12;
    }
  else return 10;
  
} /* end dbcc */


static int
jmp (
     MDu8 const mode,
     MDu8 const reg
     )
{
  
  MDu32 addr;
  
  
  switch ( mode )
    {
    case 2: _regs.PC= _regs.A[reg].v; return 8;
    case 5: _regs.PC= pd16an ( _regs.A[reg] ).v; return 10;
    case 6: _regs.PC= _regs.A[reg].v + calc_8bit_displacement (); return 14;
    case 7:
      switch ( reg )
        {
        case 0: _regs.PC= pw ().v; return 10;
        case 1: _regs.PC= pl ().v; return 12;
        case 2: _regs.PC= pd16pc ().v; return 10;
        case 3:
          addr= _regs.PC;
          _regs.PC= addr + calc_8bit_displacement ();
          return 14;
        default:
          _warning ( _udata, "JMP no accepta el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "JMP no accepta el mode %d", mode );
      return UTIME;
    }
  
} /* end jmp */


static int
jsr (
     MDu8 const mode,
     MDu8 const reg
     )
{
  
  MDu32 addr;
  int ret;
  
  
  switch ( mode )
    {
    case 2: addr= _regs.A[reg].v; ret= 16; break;
    case 5: addr= pd16an ( _regs.A[reg] ).v; ret= 18; break;
    case 6: addr= _regs.A[reg].v + calc_8bit_displacement (); ret= 22; break;
    case 7:
      switch ( reg )
        {
        case 0: addr= pw ().v; ret= 18; break;
        case 1: addr= pl ().v; ret= 20; break;
        case 2: addr= pd16pc ().v; ret= 18; break;
        case 3:
          addr= _regs.PC;
          addr+= calc_8bit_displacement ();
          ret= 22;
          break;
        default:
          _warning ( _udata, "JSR no accepta el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "JSR no accepta el mode %d", mode );
      return UTIME;
    }
  
  _regs.A[7].v-= 4;
  write_long ( (MD_Reg32) _regs.PC, _regs.A[7].v );
  _regs.PC= addr;
  
  return ret;
  
} /* end jsr */


static int
nop (void)
{
  return 4;
} /* end nop */


static int
rtr (void)
{
  
  _regs.CCR.b.v0= MD_mem_read ( _regs.A[7].v ).b.v0&0x1F;
  _regs.A[7].v+= 2;
  _regs.PC= read_long ( _regs.A[7].v ).v;
  _regs.A[7].v+= 4;
  
  return 20;
  
} /* end rtr */


static int
rts (void)
{
  
  _regs.PC= read_long ( _regs.A[7].v ).v;
  _regs.A[7].v+= 4;
  
  return 16;
  
} /* end rts */


static int
scc (
     MDu8 const cond,
     MDu8 const mode,
     MDu8 const reg
     )
{
  
  MDu32 addr;
  MDu8 tmp;
  int ret;
  MD_Bool test;
  
  
  test= get_cond ( cond );
  tmp= test ? 0xFF : 0x00;
  switch ( mode )
    {
    case 0: _regs.D[reg].b.v0= tmp; ret= test ? 6 : 4; break;
    case 2: MD_mem_write8 ( _regs.A[reg].v, tmp ); ret= 12; break;
    case 3:
      MD_mem_write8 ( _regs.A[reg].v, tmp );
      if ( reg == 7 ) _regs.A[reg].v+= 2;
      else            ++(_regs.A[reg].v);
      ret= 12;
      break;
    case 4:
      addr= (reg==7) ? (_regs.A[reg].v-= 2) : --(_regs.A[reg].v);
      MD_mem_write8 ( addr, tmp );
      ret= 14;
      break;
    case 5: MD_mem_write8 ( pd16an ( _regs.A[reg] ).v, tmp ); ret= 16; break;
    case 6:
      addr= _regs.A[reg].v + calc_8bit_displacement ();
      MD_mem_write8 ( addr, tmp );
      ret= 18;
      break;
    case 7:
      switch ( reg )
        {
        case 0: MD_mem_write8 ( pw ().v, tmp ); ret= 16; break;
        case 1: MD_mem_write8 ( pl ().v, tmp ); ret= 20; break;
        default:
          _warning ( _udata, "Scc no accepta el mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "Scc no accepta el mode %d", mode );
      return UTIME;
      break;
    }
  
  return ret;
  
} /* end scc */


static int
tstb (
      MDu8 const mode,
      MDu8 const reg
      )
{
  
  MDu8 byte;
  int ret;
  MDu32 addr;
  
  
  switch ( mode )
    {
    case 0: byte= _regs.D[reg].b.v0; ret= 4; break;
    case 1:
      _warning ( _udata, "TST.b no accepta el mode %d", mode );
      return UTIME;
    case 2: byte= MD_mem_read8 ( _regs.A[reg].v ); ret= 8; break;
    case 3:
      byte= MD_mem_read8 ( _regs.A[reg].v );
      if ( reg == 7 ) _regs.A[7].v+= 2;
      else            ++_regs.A[reg].v;
      ret= 8;
      break;
    case 4:
      addr= (reg==7) ? (_regs.A[7].v-= 2) : --_regs.A[reg].v;
      byte= MD_mem_read8 ( addr );
      ret= 10;
      break;
    case 5: byte= MD_mem_read8 ( pd16an ( _regs.A[reg] ).v ); ret= 12; break;
    case 6:
      addr= _regs.A[reg].v + calc_8bit_displacement ();
      byte= MD_mem_read8 ( addr );
      ret= 14;
      break;
    case 7:
      switch ( reg )
        {
        case 0: byte= MD_mem_read8 ( pw ().v ); ret= 12; break;
        case 1: byte= MD_mem_read8 ( pl ().v ); ret= 16; break;
        default:
          _warning ( _udata, "TST.b no accepta mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default: byte= ret= 0; /* CALLA*/ break;
    }
  CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
  _regs.CCR.v|=
    ((byte&0x80)?NFLAG:0) |
    (byte?0:ZFLAG);
  
  return ret;
  
} /* end tstb */


static int
tstl (
      MDu8 const mode,
      MDu8 const reg
      )
{
  
  MD_Reg32 val;
  int ret;
  MDu32 addr;
  
  
  switch ( mode )
    {
    case 0: val= _regs.D[reg]; ret= 8; break;
    case 1: val= _regs.A[reg]; ret= 8; break;
    case 2: val= read_long ( _regs.A[reg].v ); ret= 12; break;
    case 3:
      val= read_long ( _regs.A[reg].v );
      _regs.A[reg].v+= 4;
      ret= 12;
      break;
    case 4: val= read_long ( _regs.A[reg].v-= 4 ); ret= 14; break;
    case 5: val= read_long ( pd16an ( _regs.A[reg] ).v ); ret= 16; break;
    case 6:
      addr= _regs.A[reg].v + calc_8bit_displacement ();
      val= read_long ( addr );
      ret= 18;
      break;
    case 7:
      switch ( reg )
        {
        case 0: val= read_long ( pw ().v ); ret= 16; break;
        case 1: val= read_long ( pl ().v ); ret= 20; break;
        default:
          _warning ( _udata, "TST.l no accepta mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default: val.v= ret= 0; /* CALLA*/ break;
    }
  CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
  _regs.CCR.v|=
    ((val.v&0x80000000)?NFLAG:0) |
    (val.v?0:ZFLAG);
  
  return ret;
  
} /* end tstl */


static int
tstw (
      MDu8 const mode,
      MDu8 const reg
      )
{
  
  MD_Word word;
  int ret;
  MDu32 addr;
  
  
  switch ( mode )
    {
    case 0: word= _regs.D[reg].w.v0; ret= 4; break;
    case 1: word= _regs.A[reg].w.v0; ret= 4; break;
    case 2: word= MD_mem_read ( _regs.A[reg].v ); ret= 8; break;
    case 3:
      word= MD_mem_read ( _regs.A[reg].v );
      _regs.A[reg].v+= 2;
      ret= 8;
      break;
    case 4: word= MD_mem_read ( _regs.A[reg].v-= 2 ); ret= 10; break;
    case 5: word= MD_mem_read ( pd16an ( _regs.A[reg] ).v ); ret= 12; break;
    case 6:
      addr= _regs.A[reg].v + calc_8bit_displacement ();
      word= MD_mem_read ( addr );
      ret= 14;
      break;
    case 7:
      switch ( reg )
        {
        case 0: word= MD_mem_read ( pw ().v ); ret= 12; break;
        case 1: word= MD_mem_read ( pl ().v ); ret= 16; break;
        default:
          _warning ( _udata, "TST.w no accepta mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default: word.v= ret= 0; /* CALLA*/ break;
    }
  CLEAR_FLAGS ( NFLAG|ZFLAG|VFLAG|CFLAG );
  _regs.CCR.v|=
    ((word.v&0x8000)?NFLAG:0) |
    (word.v?0:ZFLAG);
  
  return ret;
  
} /* end tstw */


/* System Control Instructions ************************************************/
static void
check_supervisor_changed (void)
{
  
  MD_Reg32 tmp;
  
  
  if ( !(_regs.CCR.v&SFLAG) )
    {
      tmp= _regs.A[7];
      _regs.A[7]= _regs._SP;
      _regs._SP= tmp;
    }
  
} /* end check_supervisor_changed */


static int
andi_to_ccr (void)
{
  
  _regs.CCR.b.v0&= inm_word ().b.v0;
  
  return 20;
  
} /* end andi_to_ccr */


static int
andi_to_sr (void)
{
  
  if ( _regs.CCR.v&SFLAG )
    {
      _regs.CCR.v&= inm_word ().v;
      check_supervisor_changed ();
      return 20;
    }
  else return trap ( 0x020 );
  
} /* end andi_to_sr */


static int
chk (
     MDu8 const reg,
     MDu8 const eamode,
     MDu8 const eareg
     )
{
  
  /* NOTA: No entenc timing.txt */
  MD_Word word;
  MDu32 addr;
  int ret;
  
  
  if ( ((MDs16) _regs.D[reg].w.v0.v) < 0 )
    {
      _regs.CCR.v|= NFLAG;
      return trap ( 0x018 );
    }
  switch ( eamode )
    {
    case 0: word= _regs.D[eareg].w.v0; ret= 8; break;
    case 2: word= MD_mem_read ( _regs.A[eareg].v ); ret= 12; break;
    case 3:
      word= MD_mem_read ( _regs.A[eareg].v );
      _regs.A[eareg].v+= 2;
      ret= 12;
      break;
    case 4: word= MD_mem_read ( _regs.A[eareg].v-= 2 ); ret= 14; break;
    case 5: word= MD_mem_read ( pd16an ( _regs.A[eareg] ).v ); ret= 16; break;
    case 6:
      addr= _regs.A[eareg].v + calc_8bit_displacement ();
      word= MD_mem_read ( addr );
      ret= 18;
      break;
    case 7:
      switch ( eareg )
        {
        case 0: word= MD_mem_read ( pw ().v ); ret= 16; break;
        case 1: word= MD_mem_read ( pl ().v ); ret= 20; break;
        case 2: word= MD_mem_read ( pd16pc ().v ); ret= 16; break;
        case 3:
          addr= _regs.PC;
          addr+= calc_8bit_displacement ();
          word= MD_mem_read ( addr );
          ret= 18;
          break;
        case 4: word= inm_word (); ret= 12; break;
        default:
          _warning ( _udata, "CHK no accepta mode:reg 7:%d", eareg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "CHK no accepta mode %d", eamode );
      return UTIME;
    }
  if ( ((MDs16) _regs.D[reg].w.v0.v) > ((MDs16) word.v) )
    {
      trap ( 0x018 );
      return 40;
    }
  else return ret;
  
} /* end chk */


static int
eori_to_ccr (void)
{
  
  _regs.CCR.b.v0= (_regs.CCR.b.v0^inm_word ().b.v0)&IBITS_CCR;
  
  return 20;
  
} /* end eori_to_ccr */


static int
eori_to_sr (void)
{
  
  if ( _regs.CCR.v&SFLAG )
    {
      _regs.CCR.v= (_regs.CCR.v^inm_word ().v)&IBITS;
      check_supervisor_changed ();
      return 20;
    }
  else return trap ( 0x020 );
  
} /* end eori_to_sr */


static int
move_to_sr (
            MDu8 const mode,
            MDu8 const reg
            )
{
  
  MD_Word word;
  int ret;
  MDu32 addr;
  
  
  if ( !(_regs.CCR.v&SFLAG) ) return trap ( 0x020 );
  switch ( mode )
    {
    case 0: word= _regs.D[reg].w.v0; ret= 12; break;
    case 2: word= MD_mem_read ( _regs.A[reg].v ); ret= 16; break;
    case 3:
      word= MD_mem_read ( _regs.A[reg].v );
      _regs.A[7].v+= 2;
      ret= 16;
      break;
    case 4: word= MD_mem_read ( _regs.A[7].v-= 2 ); ret= 18; break;
    case 5: word= MD_mem_read ( pd16an ( _regs.A[reg] ).v ); ret= 20; break;
    case 6:
      addr= _regs.A[reg].v + calc_8bit_displacement ();
      word= MD_mem_read ( addr );
      ret= 22;
      break;
    case 7:
      switch ( reg )
        {
        case 0: word= MD_mem_read ( pw ().v ); ret= 20; break;
        case 1: word= MD_mem_read ( pl ().v ); ret= 24; break;
        case 2: word= MD_mem_read ( pd16pc ().v ); ret= 20; break;
        case 3:
          addr= _regs.PC;
          word= MD_mem_read ( addr + calc_8bit_displacement () );
          ret= 22;
          break;
        case 4: word= inm_word (); ret= 16; break;
        default:
          _warning ( _udata, "MOVE to SR no accepta mode:reg 7:%d", reg );
          return UTIME;
        }
      break;
    default:
      _warning ( _udata, "MOVE to SR no accepta mode %d", mode );
      return UTIME;
    }
  _regs.CCR.v= word.v&IBITS;
  check_supervisor_changed ();
  
  return ret;
  
} /* end move_to_sr */


static int
move_to_usp (
             MDu8 const reg
             )
{
  
  if ( _regs.CCR.v&SFLAG )
    {
      _regs._SP= _regs.A[reg];
      return 4;
    }
  else return trap ( 0x020 );
  
} /* end move_to_usp */


static int
move_from_usp (
               MDu8 const reg
               )
{
  
  if ( _regs.CCR.v&SFLAG )
    {
      _regs.A[reg]= _regs._SP;
      return 4;
    }
  else return trap ( 0x020 );
  
} /* end move_from_usp */


static int
ori_to_ccr (void)
{
  
  _regs.CCR.b.v0= (_regs.CCR.b.v0|inm_word ().b.v0)&IBITS_CCR;
  
  return 20;
  
} /* end ori_to_ccr */


static int
ori_to_sr (void)
{
  
  if ( _regs.CCR.v&SFLAG )
    {
      _regs.CCR.v= (_regs.CCR.v|inm_word ().v)&IBITS;
      check_supervisor_changed ();
      return 20;
    }
  else return trap ( 0x020 );
  
} /* end ori_to_sr */


static int
reset (void)
{
  
  if ( _regs.CCR.v&SFLAG )
    {
      MD_cpu_reset_external_devices_signal ();
      return 132;
    }
  else return trap ( 0x020 );
  
} /* end reset */


static int
rte (void)
{
  
  if ( _regs.CCR.v&SFLAG )
    {
      _regs.CCR.v= MD_mem_read ( _regs.A[7].v ).v&IBITS;
      _regs.A[7].v+= 2;
      _regs.PC= read_long ( _regs.A[7].v ).v;
      _regs.A[7].v+= 4;
      check_supervisor_changed ();
      return 20;
    }
  else return trap ( 0x020 );
  
} /* end rte */


static int
stop (void)
{
  
  if ( _regs.CCR.v&SFLAG )
    {
      _regs.CCR.v= inm_word ().v&IBITS;
      check_supervisor_changed ();
      _stop= MD_TRUE;
      return 4;
    }
  else return trap ( 0x020 );
  
} /* end stop */




/* Taules de bot **************************************************************/
static int
unk (
     MDu8 const op,
     MDu8 const v3,
     MDu8 const v2,
     MDu8 const v1,
     MDu8 const v0
     )
{
  
  _warning ( _udata, "instrucció desconeguda: %04X",
             (op<<12)|(v3<<9)|(v2<<6)|(v1<<3)|v0 );
  return UTIME;
  
} /* end unk */


static int
bit_movep_inm (
               MDu8 const v3,
               MDu8 const v2,
               MDu8 const v1,
               MDu8 const v0
               )
{
  
  switch ( v2 )
    {
    case 0:
      switch ( v3 )
        {
        case 0:
          switch ( v1 )
            {
            case 7:
              switch ( v0 )
        	{
        	case 4: return ori_to_ccr ();
        	default: return opbi ( v1, v0, orb );
        	}
              break;
            default: return opbi ( v1, v0, orb );
            }
          break;
        case 1:
          switch ( v1 )
            {
            case 7:
              switch ( v0 )
        	{
        	case 4: return andi_to_ccr ();
        	default: return opbi ( v1, v0, andb );
        	}
              break;
            default: return opbi ( v1, v0, andb );
            }
          break;
        case 2: return opbi ( v1, v0, subb );
        case 3: return opbi ( v1, v0, addb );
        case 4: return btst_inm ( v1, v0 );
        case 5:
          switch ( v1 )
            {
            case 7:
              switch ( v0 )
        	{
        	case 4: return eori_to_ccr ();
        	default: return opbi ( v1, v0, eor_b );
        	}
              break;
            default: return opbi ( v1, v0, eor_b );
            }
          break;
        case 6: return cmpbi ( v1, v0 );
        default: break;
        }
      break;
    case 1:
      switch ( v3 )
        {
        case 0:
          switch ( v1 )
            {
            case 7:
              switch ( v0 )
        	{
        	case 4: return ori_to_sr ();
        	default: return opwi ( v1, v0, orw );
        	}
              break;
            default: return opwi ( v1, v0, orw );
            }
          break;
        case 1:
          switch ( v1 )
            {
            case 7:
              switch ( v0 )
        	{
        	case 4: return andi_to_sr ();
        	default: return opwi ( v1, v0, andw );
        	}
              break;
            default: return opwi ( v1, v0, andw );
            }
          break;
        case 2: return opwi ( v1, v0, subw );
        case 3: return opwi ( v1, v0, addw );
        case 4: return bop_inm ( v1, v0, bchg_mem, bchg_reg );
        case 5:
          switch ( v1 )
            {
            case 7:
              switch ( v0 )
        	{
        	case 4: return eori_to_sr ();
        	default: return opwi ( v1, v0, eor_w );
        	}
              break;
            default: return opwi ( v1, v0, eor_w );
            }
          break;
        case 6: return cmpwi ( v1, v0 );
        default: break;
        }
      break;
    case 2:
      switch ( v3 )
        {
        case 0: return opli ( v1, v0, orl );
        case 1: return opli ( v1, v0, andl );
        case 2: return opli ( v1, v0, subl );
        case 3: return opli ( v1, v0, addl );
        case 4: return bop_inm ( v1, v0, bclr_mem, bclr_reg );
        case 5: return opli ( v1, v0, eor_l );
        case 6: return cmpli ( v1, v0 );
        default: break;
        }
      break;
    case 3:
      switch ( v3 )
        {
        case 4: return bop_inm ( v1, v0, bset_mem, bset_reg );
        default: break;
        }
      break;
    case 4:
      switch ( v1 )
        {
        case 1: return movepw_mem_reg ( v3, v0 );
        default: return btst_reg ( v3, v1, v0 );
        }
      break;
    case 5:
      switch ( v1 )
        {
        case 1: return movepl_mem_reg ( v3, v0 );
        default: return bop_reg ( v3, v1, v0, bchg_mem, bchg_reg );
        }
      break;
    case 6:
      switch ( v1 )
        {
        case 1: return movepw_reg_mem ( v3, v0 );
        default: return bop_reg ( v3, v1, v0, bclr_mem, bclr_reg );
        }
      break;
    case 7:
      switch ( v1 )
        {
        case 1: return movepl_reg_mem ( v3, v0 );
        default: return bop_reg ( v3, v1, v0, bset_mem, bset_reg );
        }
      break;
    default: break;
    }
  
  return unk ( 0x0, v3, v2, v1, v0 );
  
} /* end bit_movep_inm */


static int
miscellaneous (
               MDu8 const v3,
               MDu8 const v2,
               MDu8 const v1,
               MDu8 const v0
               )
{
  
  switch ( v2 )
    {
    case 0:
      switch ( v3 )
        {
        case 0: return negxb ( v1, v0 );
        case 1: return clrb ( v1, v0 );
        case 2: return negb ( v1, v0 );
        case 3: return notb ( v1, v0 );
        case 4: return nbcd ( v1, v0 );
        case 5: return tstb ( v1, v0 );
        default: break;
        }
      break;
    case 1:
      switch ( v3 )
        {
        case 0: return negxw ( v1, v0 );
        case 1: return clrw ( v1, v0 );
        case 2: return negw ( v1, v0 );
        case 3: return notw ( v1, v0 );
        case 4:
          switch ( v1 )
            {
            case 0: return swap ( v0 );
            default: return pea ( v1, v0 );
            }
          break;
        case 5: return tstw ( v1, v0 );
        case 7:
          switch ( v1 )
            {
            case 0: return trap ( (v0+32)<<2 );
            case 1: return trap ( ((v0|0x8)+32)<<2 );
            case 2: return link ( v0 );
            case 3: return unlk ( v0 );
            case 4: return move_to_usp ( v0 );
            case 5: return move_from_usp ( v0 );
            case 6:
              switch ( v0 )
        	{
        	case 0: return reset ();
        	case 1: return nop ();
        	case 2: return stop ();
        	case 3: return rte ();
        	case 5: return rts ();
        	case 7: return rtr ();
        	default: break;
        	}
              break;
            default: break;
            }
          break;
        default: break;
        }
      break;
    case 2:
      switch ( v3 )
        {
        case 0: return negxl ( v1, v0 );
        case 1: return clrl ( v1, v0 );
        case 2: return negl ( v1, v0 );
        case 3: return notl ( v1, v0 );
        case 4:
          switch ( v1 )
            {
            case 0: return extw ( v0 );
            default: return movemw_reg_mem ( v1, v0 );
            }
          break;
        case 5: return tstl ( v1, v0 );
        case 6: return movemw_mem_reg ( v1, v0 );
        case 7: return jsr ( v1, v0 );
        default: break;
        }
      break;
    case 3:
      switch ( v3 )
        {
        case 0: return move_from_sr ( v1, v0 );
        case 2: return move_to_ccr ( v1, v0 );
        case 3: return move_to_sr ( v1, v0 );
        case 4:
          switch ( v1 )
            {
            case 0: return extl ( v0 );
            default: return moveml_reg_mem ( v1, v0 );
            }
          break;
        case 5:
          switch ( v1 )
            {
            case 7:
              switch ( v0 )
        	{
        	case 4: return trap ( 0x10 ); /* ILLEGAL */
        	default: break;
        	}
              break;
            default: break;
            }
          break;
        case 6: return moveml_mem_reg ( v1, v0 );
        case 7: return jmp ( v1, v0 );
        default: break;
        }
      break;
    case 6: return chk ( v3, v1, v0 );
    case 7: return lea ( v3, v1, v0 );
    default: break;
    }
  
  return unk ( 0x4, v3, v2, v1, v0 );
  
} /* end miscellaneous */


static int
addq_subq__ (
             MDu8 const v3,
             MDu8 const v2,
             MDu8 const v1,
             MDu8 const v0
             )
{
  
  switch ( v2 )
    {
    case 0: return opbq ( v3, v1, v0, addb );
    case 1: return opwq ( v3, v1, v0, addw, add );
    case 2: return oplq ( v3, v1, v0, addl, add );
    case 3:
      switch ( v1 )
        {
        case 1: return dbcc ( v3<<1, v0 );
        default: return scc ( v3<<1, v1, v0 );
        }
      break;
    case 4: return opbq ( v3, v1, v0, subb );
    case 5: return opwq ( v3, v1, v0, subw, sub );
    case 6: return oplq ( v3, v1, v0, subl, sub );
    case 7:
      switch ( v1 )
        {
        case 1: return dbcc ( (v3<<1)|1, v0 );
        default: return scc ( (v3<<1)|1, v1, v0 );
        }
      break;
    default: break;
    }
  
  return unk ( 0x5, v3, v2, v1, v0 );
  
} /* end addq_subq__ */


static int
bcc_bsr_bra (
             MDu8 const cond,
             MDu8 const disp
             )
{
  
  switch ( cond )
    {
    case 0: return bra ( disp );
    case 1: return bsr ( disp );
    default: return bcc ( cond, disp );
    }
  
  _warning ( _udata, "instrucció desconeguda: %04X", (0x6<<12)|(cond<<8)|disp );
  
  return UTIME;
  
  
} /* end bcc_bsr_bra */


static int
or_div_sbcd (
             MDu8 const v3,
             MDu8 const v2,
             MDu8 const v1,
             MDu8 const v0
             )
{
  
  switch ( v2 )
    {
    case 0:
      switch ( v1 )
        {
        case 1: break;
        default: return opb_easrc ( v3, v1, v0, orb );
        }
      break;
    case 1:
      switch ( v1 )
        {
        case 1: break;
        default: return opw_easrc ( v3, v1, v0, orw );
        }
      break;
    case 2:
      switch ( v1 )
        {
        case 1: break;
        default: return opl_easrc ( v3, v1, v0, orl );
        }
      break;
    case 3: return divu ( v3, v1, v0 );
    case 4:
      switch ( v1 )
        {
        case 0:
          _regs.D[v3].b.v0= sbcd_op ( _regs.D[v0].b.v0, _regs.D[v3].b.v0 );
          return 6;
        case 1: return opbx_A ( v3, v0, sbcd_op );
        default: return opb_eadst ( v3, v1, v0, orb );
        }
      break;
    case 5: return opw_eadst ( v3, v1, v0, orw );
    case 6: return opl_eadst ( v3, v1, v0, orl );
    case 7: return divs ( v3, v1, v0 );
    default: break;
    }
  
  return unk ( 0x8, v3, v2, v1, v0 );
  
} /* end or_div_sbcd */


static int
sub_subx (
          MDu8 const v3,
          MDu8 const v2,
          MDu8 const v1,
          MDu8 const v0
          )
{
  
  switch ( v2 )
    {
    case 0: return opb_easrc ( v3, v1, v0, subb );
    case 1: return opw_easrc ( v3, v1, v0, subw );
    case 2: return opl_easrc ( v3, v1, v0, subl );
    case 3: return opw_easrc_A ( v3, v1, v0, sub );
    case 4:
      switch ( v1 )
        {
        case 0:
          _regs.D[v3].b.v0= subb_x ( _regs.D[v0].b.v0, _regs.D[v3].b.v0 );
          return 4;
        case 1: return opbx_A ( v3, v0, subb_x );
        default: return opb_eadst ( v3, v1, v0, subb );
        }
      break;
    case 5:
      switch ( v1 )
        {
        case 0:
          _regs.D[v3].w.v0= subw_x ( _regs.D[v0].w.v0, _regs.D[v3].w.v0 );
          return 4;
        case 1: return opwx_A ( v3, v0, subw_x );
        default: return opw_eadst ( v3, v1, v0, subw );
        }
      break;
    case 6:
      switch ( v1 )
        {
        case 0:
          _regs.D[v3]= subl_x ( _regs.D[v0], _regs.D[v3] );
          return 8;
        case 1: return oplx_A ( v3, v0, subl_x );
        default: return opl_eadst ( v3, v1, v0, subl );
        }
      break;
    case 7: return opl_easrc_A ( v3, v1, v0, sub );
    default: break;
    }
  
  return unk ( 0x9, v3, v2, v1, v0 );
  
} /* end sub_subx */


static int
cmp_eor (
         MDu8 const v3,
         MDu8 const v2,
         MDu8 const v1,
         MDu8 const v0
         )
{
  
  switch ( v2 )
    {
    case 0: return cmpb ( v3, v1, v0 );
    case 1: return cmpw ( v3, v1, v0 );
    case 2: return cmpl ( v3, v1, v0 );
    case 3: return cmpw_A ( v3, v1, v0 );
    case 4:
      switch ( v1 )
        {
        case 1: return cmpmb ( v3, v0 );
        default: return eorb ( v3, v1, v0 );
        }
      break;
    case 5:
      switch ( v1 )
        {
        case 1: return cmpmw ( v3, v0 );
        default: return eorw ( v3, v1, v0 );
        }
      break;
    case 6:
      switch ( v1 )
        {
        case 1: return cmpml ( v3, v0 );
        default: return eorl ( v3, v1, v0 );
        }
      break;
    case 7: return cmpl_A ( v3, v1, v0 );
    default: break;
    }
  
  return unk ( 0xB, v3, v2, v1, v0 );
  
} /* end cmp_eor */


static int
and_mul_abcd_exg (
        	  MDu8 const v3,
        	  MDu8 const v2,
        	  MDu8 const v1,
        	  MDu8 const v0
        	  )
{
  
  MD_Reg32 tmp;
  
  
  switch ( v2 )
    {
    case 0:
      switch ( v1 )
        {
        case 1: break;
        default: return opb_easrc ( v3, v1, v0, andb );
        }
      break;
    case 1:
      switch ( v1 )
        {
        case 1: break;
        default: return opw_easrc ( v3, v1, v0, andw );
        }
      break;
    case 2:
      switch ( v1 )
        {
        case 1: break;
        default: return opl_easrc ( v3, v1, v0, andl );
        }
      break;
    case 3: return mulu ( v3, v1, v0 );
    case 4:
      switch ( v1 )
        {
        case 0:
          _regs.D[v3].b.v0= abcd_op ( _regs.D[v0].b.v0, _regs.D[v3].b.v0 );
          return 6;
        case 1: return opbx_A ( v3, v0, abcd_op );
        default: return opb_eadst ( v3, v1, v0, andb );
        }
      break;
    case 5:
      switch ( v1 )
        {
        case 0: /* EXG Dx,Dy */
          tmp= _regs.D[v3];
          _regs.D[v3]= _regs.D[v0];
          _regs.D[v0]= tmp;
          return 6;
        case 1: /* EXG Ax,Ay */
          tmp= _regs.A[v3];
          _regs.A[v3]= _regs.A[v0];
          _regs.A[v0]= tmp;
          return 6;
        default: return opw_eadst ( v3, v1, v0, andw );
        }
      break;
    case 6:
      switch ( v1 )
        {
        case 1: /* EXG Dx,Ay */
          tmp= _regs.D[v3];
          _regs.D[v3]= _regs.A[v0];
          _regs.A[v0]= tmp;
          return 6;
        default: return opl_eadst ( v3, v1, v0, andl );
        }
      break;
    case 7: return muls ( v3, v1, v0 );
    default: break;
    }
  
  return unk ( 0xC, v3, v2, v1, v0 );
  
} /* end and_mul_abcd_exg */


static int
add_addx (
          MDu8 const v3,
          MDu8 const v2,
          MDu8 const v1,
          MDu8 const v0
          )
{
  
  switch ( v2 )
    {
    case 0: return opb_easrc ( v3, v1, v0, addb );
    case 1: return opw_easrc ( v3, v1, v0, addw );
    case 2: return opl_easrc ( v3, v1, v0, addl );
    case 3: return opw_easrc_A ( v3, v1, v0, add );
    case 4:
      switch ( v1 )
        {
        case 0:
          _regs.D[v3].b.v0= addb_x ( _regs.D[v0].b.v0, _regs.D[v3].b.v0 );
          return 4;
        case 1: return opbx_A ( v3, v0, addb_x );
        default: return opb_eadst ( v3, v1, v0, addb );
        }
      break;
    case 5:
      switch ( v1 )
        {
        case 0:
          _regs.D[v3].w.v0= addw_x ( _regs.D[v0].w.v0, _regs.D[v3].w.v0 );
          return 4;
        case 1: return opwx_A ( v3, v0, addw_x );
        default: return opw_eadst ( v3, v1, v0, addw );
        }
      break;
    case 6:
      switch ( v1 )
        {
        case 0:
          _regs.D[v3]= addl_x ( _regs.D[v0], _regs.D[v3] );
          return 8;
        case 1: return oplx_A ( v3, v0, addl_x );
        default: return opl_eadst ( v3, v1, v0, addl );
        }
      break;
    case 7: return opl_easrc_A ( v3, v1, v0, add );
    default: break;
    }
  
  return unk ( 0xD, v3, v2, v1, v0 );
  
} /* end add_addx */


static int
shift_rot_bit (
               MDu8 const v3,
               MDu8 const v2,
               MDu8 const v1,
               MDu8 const v0
               )
{
  
  switch ( v2 )
    {
    case 0:
      switch ( v1 )
        {
        case 0: return sopb_inm ( v3, v0, asrb );
        case 1: return sopb_inm ( v3, v0, lsrb );
        case 2: return sopb_inm ( v3, v0, roxrb );
        case 3: return sopb_inm ( v3, v0, rorb );
        case 4: return sopb_reg ( v3, v0, asrb );
        case 5: return sopb_reg ( v3, v0, lsrb );
        case 6: return sopb_reg ( v3, v0, roxrb );
        case 7: return sopb_reg ( v3, v0, rorb );
        default: break;
        }
      break;
    case 1:
      switch ( v1 )
        {
        case 0: return sopw_inm ( v3, v0, asrw );
        case 1: return sopw_inm ( v3, v0, lsrw );
        case 2: return sopw_inm ( v3, v0, roxrw );
        case 3: return sopw_inm ( v3, v0, rorw );
        case 4: return sopw_reg ( v3, v0, asrw );
        case 5: return sopw_reg ( v3, v0, lsrw );
        case 6: return sopw_reg ( v3, v0, roxrw );
        case 7: return sopw_reg ( v3, v0, rorw );
        default: break;
        }
      break;
    case 2:
      switch ( v1 )
        {
        case 0: return sopl_inm ( v3, v0, asrl );
        case 1: return sopl_inm ( v3, v0, lsrl );
        case 2: return sopl_inm ( v3, v0, roxrl );
        case 3: return sopl_inm ( v3, v0, rorl );
        case 4: return sopl_reg ( v3, v0, asrl );
        case 5: return sopl_reg ( v3, v0, lsrl );
        case 6: return sopl_reg ( v3, v0, roxrl );
        case 7: return sopl_reg ( v3, v0, rorl );
        default: break;
        }
      break;
    case 3:
      switch ( v3 )
        {
        case 0: return sop_mem ( v1, v0, asrw );
        case 1: return sop_mem ( v1, v0, lsrw );
        case 2: return sop_mem ( v1, v0, roxrw );
        case 3: return sop_mem ( v1, v0, rorw );
        default: break;
        }
      break;
    case 4:
      switch ( v1 )
        {
        case 0: return sopb_inm ( v3, v0, aslb );
        case 1: return sopb_inm ( v3, v0, lslb );
        case 2: return sopb_inm ( v3, v0, roxlb );
        case 3: return sopb_inm ( v3, v0, rolb );
        case 4: return sopb_reg ( v3, v0, aslb );
        case 5: return sopb_reg ( v3, v0, lslb );
        case 6: return sopb_reg ( v3, v0, roxlb );
        case 7: return sopb_reg ( v3, v0, rolb );
        default: break;
        }
      break;
    case 5:
      switch ( v1 )
        {
        case 0: return sopw_inm ( v3, v0, aslw );
        case 1: return sopw_inm ( v3, v0, lslw );
        case 2: return sopw_inm ( v3, v0, roxlw );
        case 3: return sopw_inm ( v3, v0, rolw );
        case 4: return sopw_reg ( v3, v0, aslw );
        case 5: return sopw_reg ( v3, v0, lslw );
        case 6: return sopw_reg ( v3, v0, roxlw );
        case 7: return sopw_reg ( v3, v0, rolw );
        default: break;
        }
      break;
    case 6:
      switch ( v1 )
        {
        case 0: return sopl_inm ( v3, v0, asll );
        case 1: return sopl_inm ( v3, v0, lsll );
        case 2: return sopl_inm ( v3, v0, roxll );
        case 3: return sopl_inm ( v3, v0, roll );
        case 4: return sopl_reg ( v3, v0, asll );
        case 5: return sopl_reg ( v3, v0, lsll );
        case 6: return sopl_reg ( v3, v0, roxll );
        case 7: return sopl_reg ( v3, v0, roll );
        default: break;
        }
      break;
    case 7:
      switch ( v3 )
        {
        case 0: return sop_mem ( v1, v0, aslw );
        case 1: return sop_mem ( v1, v0, lslw );
        case 2: return sop_mem ( v1, v0, roxlw );
        case 3: return sop_mem ( v1, v0, rolw );
        default: break;
        }
      break;
    default: break;
    }
  
  return unk ( 0xE, v3, v2, v1, v0 );
  
} /* end shift_rot_bit */




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
MD_cpu_clear_auto_vector_int (
        		      const int num
        		      )
{
  
  if ( num < 1 || num > 7 )
    _warning ( _udata, "%d no és un número d'auto-vector correcte", num );
  _ints&= ~(0x1<<num); /* Del bit 2 al 8. */
  
} /* end MD_cpu_clear_auto_vector_int */


void
MD_cpu_init (
             MD_Warning *warning,     /* Funció per als avisos. */
             void       *udata        /* Dades de l'usuari. */
             )
{
  
  _warning= warning;
  _udata= udata;
  MD_cpu_init_state ();
  
} /* end MD_cpu_init */


void
MD_cpu_init_state (void)
{
  
  /* Inicialitza els registres. */
  memset ( _regs.D, 0, sizeof(MD_Reg32)*8 );
  memset ( _regs.A, 0, sizeof(MD_Reg32)*8 );
  _regs._SP.v= 0;
  _regs.PC= 0;
  _regs.CCR.v= 0;
  
  /* Inicialitza interrupcions externes. */
  _ints= 0;
  
  /* Parada. */
  _stop= MD_FALSE;
  
  /* Força un RESET. */
  reset_int ();
  
} /* end MD_cpu_init_state */

#include <stdio.h>
int
MD_cpu_run (void)
{
  
  MD_Word opword;
  unpack_t uword;
  int ret;
  
  
  
  if ( _regs.CCR.v&0x8000 ) printf("TRACE\n");
  if ( _ints ) { ret= interrupts (); if ( ret ) return ret; }
  if ( _stop ) return 4;
  opword= MD_mem_read ( _regs.PC );
  _regs.PC+= 2;
  uword= MD_unpack__[opword.v];
  switch ( uword.op )
    {
    case 0x0: return bit_movep_inm ( uword.v3, uword.v2, uword.v1, uword.v0 );
    case 0x1: return moveb ( uword.v3, uword.v2, uword.v1, uword.v0 );
    case 0x2: return movel ( uword.v3, uword.v2, uword.v1, uword.v0 );
    case 0x3: return movew ( uword.v3, uword.v2, uword.v1, uword.v0 );
    case 0x4: return miscellaneous ( uword.v3, uword.v2, uword.v1, uword.v0 );
    case 0x5: return addq_subq__ ( uword.v3, uword.v2, uword.v1, uword.v0 );
    case 0x6: return bcc_bsr_bra ( opword.b.v1&0xF, opword.b.v0 );
    case 0x7: /* MOVEQ */
      if ( opword.v&0x0100 )
        return unk ( 0x7, uword.v3, uword.v2, uword.v1, uword.v0 );
      _regs.D[uword.v3].v= (MDs8) opword.b.v0;
      movel_setflags ( _regs.D[uword.v3] );
      return 4;
    case 0x8: return or_div_sbcd ( uword.v3, uword.v2, uword.v1, uword.v0 );
    case 0x9: return sub_subx ( uword.v3, uword.v2, uword.v1, uword.v0 );
    case 0xB: return cmp_eor ( uword.v3, uword.v2, uword.v1, uword.v0 );
    case 0xC: return and_mul_abcd_exg ( uword.v3, uword.v2,
        				uword.v1, uword.v0 );
    case 0xD: return add_addx ( uword.v3, uword.v2, uword.v1, uword.v0 );
    case 0xE: return shift_rot_bit ( uword.v3, uword.v2, uword.v1, uword.v0 );
    default:
      _warning ( _udata, "l'opcode 0x%X és desconegut", uword.op );
      return UTIME;
    }
  
} /* end MD_cpu_run */


void
MD_cpu_set_auto_vector_int (
        		    const int num
        		    )
{
  
  if ( num < 1 || num > 7 )
    _warning ( _udata, "%d no és un número d'auto-vector correcte", num );
  _ints|= (0x1<<num); /* Del bit 2 al 8. */
  
} /* end MD_cpu_set_auto_vector_int */


void
MD_cpu_set_reset (void)
{
  _ints|= 0x1;
} /* end MD_cpu_set_reset */


MDu32
MD_cpu_decode_next_step (
        		 MD_Step *step
        		 )
{
  
  MDu32 addr;
  MDu32 mask;
  unsigned int i;
  
  
  if ( _ints )
    {
      if ( _ints&0x1 )
        {
          addr= read_long ( 0x0004 ).v;
          step->type= MD_STEP_RESET;
          return addr;
        }
      for ( i= 7, mask= 0x80 ; i >= 1; --i, mask>>=1 )
        if ( mask&_ints )
          {
            if ( i > ((_regs.CCR.v&0x700)>>8) )
              {
        	step->type= MD_STEP_AUTOVECTOR;
        	step->val.priority= i;
        	addr= read_long ( (i + 24)*4 ).v;
        	return addr;
              }
          }
    }
  if ( _stop ) { step->type= MD_STEP_STOP; return _regs.PC; }
  step->type= MD_STEP_INST;
  return MD_cpu_decode ( _regs.PC, &(step->val.inst) );
  
} /* end MD_cpu_decode_next_step */


int
MD_cpu_save_state (
        	   FILE *f
        	   )
{

  SAVE ( _regs );
  SAVE ( _ints );
  SAVE ( _stop );

  return 0;
  
} /* end MD_cpu_save_state */


int
MD_cpu_load_state (
        	   FILE *f
        	   )
{

  LOAD ( _regs );
  LOAD ( _ints );
  LOAD ( _stop );

  return 0;
  
} /* end MD_cpu_load_state */
