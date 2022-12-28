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
 *  MD.h - Simulador de la 'Mega Drive' escrit en ANSI C.
 *
 */

#ifndef __MD_H__
#define __MD_H__

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "Z80.h"


/*********/
/* TIPUS */
/*********/

#ifdef __LITTLE_ENDIAN__
#define MD_LE
#elif defined __BIG_ENDIAN__
#define MD_BE
#else
#error Per favor defineix __LITTLE_ENDIAN__ o __BIG_ENDIAN__
#endif

/* Tipus booleà. */
typedef enum
  {
    MD_FALSE= 0,
    MD_TRUE
  } MD_Bool;

/* Tipus sencers. */
typedef uint8_t MDu8;
typedef int8_t MDs8;
typedef uint16_t MDu16;
typedef int16_t MDs16;
typedef uint32_t MDu32;
typedef int32_t MDs32;
typedef uint64_t MDu64;

/* Paraules i registres de 32 bits. */
#ifdef MD_LE
typedef union
{
  MDu16                  v;
  struct { MDu8 v0,v1; } b;
} MD_Word;

typedef union
{
  MDu32                        v;
  struct { MD_Word v0,v1; }    w;
  struct { MDu8 v0,v1,v2,v3; } b;
} MD_Reg32;
#else /* MD_BE */
typedef union
{
  MDu16                  v;
  struct { MDu8 v1,v0; } b;
} MD_Word;

typedef union
{
  MDu32                        v;
  struct { MD_Word v1,v0; }    w;
  struct { MDu8 v3,v2,v1,v0; } b;
} MD_Reg32;
#endif

/* Funció per a emetre avísos. */
typedef void 
(MD_Warning) (
              void       *udata,
              const char *format,
              ...
              );

typedef enum
  {
    MD_NOERROR= 0,    /* No hi ha cap error. */
    MD_EMEM           /* No hi ha prou memòria. */
  } MD_Error;


/*******/
/* ROM */
/*******/
/* Definició de tipus i funcions relacionades amb la ROM. */

/* Estructura per a guardar una ROM. */
typedef struct
{
  
  int      nwords;    /* Número de paraules. */
  MDu8    *bytes;     /* nwords*2. Dades originals de la ROM. */
  
  /* No tocar esta part directament. */
  MD_Word *words;     /* Paraules. */
  MDu32    nbytes;    /* Número de bytes. */
  
} MD_Rom;

typedef enum
  {
    MD_SRAMINFO_AVAILABLE= 0x1,
    MD_SRAMINFO_ISFORBACKUP= 0x2,
    MD_SRAMINFO_ODDBYTES= 0x4,
    MD_SRAMINFO_EVENBYTES= 0x8
  } MD_SramInfo;

/* Estructura per a emmagatzemar la informació de la capçalera d'una
 * ROM de manera accessible.
 */
typedef struct
{
  
  char        console[17];         /* The name of the console. */
  char        firm_build[17];      /* Firm name and build date. */
  char        dom_name[49];        /* Domestic name. */
  char        int_name[49];        /* International name. */
  char        type_snumber[15];    /* Program type and serial number. */
  MDu16       checksum;            /* Checksum. */
  char        io[17];              /* I/O device support. */
  MDu32       start;               /* Start of the ROM. */
  MDu32       end;                 /* End of the ROM. */
  MDu32       start_ram;           /* Start of RAM. */
  MDu32       end_ram;             /* End of RAM. */
  char        sramid[5];           /* Backup RAM ID. */
  MD_SramInfo sram_flags;          /* Propietats de la SRAM. */
  MDu32       start_sram;          /* Start address of backup RAM. */
  MDu32       end_sram;            /* End address of backup RAM. */
  char        modem[13];           /* Modem support. */
  char        notes[41];           /* Notes. */
  char        ccodes[17];          /* Country codes. */
  
} MD_RomHeader;

/* Reserva memòria per als bytes d'una variable 'MD_Rom'. Si la
 * variable continua a NULL s'ha produït un error.
 */
#define MD_rom_alloc(ROM)        			\
  do {        						\
    ((ROM).bytes= (MDu8 *) malloc ( (ROM).nwords*2 ));        \
    (ROM).words= NULL;        				\
  } while(0)

/* Comprova que el checksum de la capçalera és correcte. */
MD_Bool
MD_rom_check_checksum (
        	       const MD_Rom *rom
        	       );

/* Si s'ha reservat memòria en la ROM l'allibera. */
void
MD_rom_free (
             MD_Rom *rom
             );

/* Obté la capçalera d'una ROM. */
void
MD_rom_get_header (
        	   const MD_Rom *rom,
        	   MD_RomHeader *header
        	   );

/* Aquest funció prepara la ROM per a ser usada. Necessita que s'haja
 * carregat la ROM en bytes. Pot tornar MD_EMEM.
 */
MD_Error
MD_rom_prepare (
        	MD_Rom *rom
        	);


/**********/
/* EEPROM */
/**********/
/* Mòdul que simula les EEPROM. */

/* Tipus de la funció utilitzada per a demanar la memòria de la
 * EEPROM. En cas de demanar-se es demana quan s'inicialitza
 * l'emulador. Inicialment la memòria està incialitzada a init_val.
 */
typedef MDu8 * (MD_GetEEPROM) (
        		       const size_t  nbytes,
        		       const MDu8    init_val,
        		       void         *udata
        		       );

/* Allibera la memòria del submòdul. */
void
MD_eeprom_close (void);

/* Inicialitza el mòdul EEPROM. */
void
MD_eeprom_init (
        	const MD_Rom *rom,
        	MD_GetEEPROM *get_eeprom,
        	void         *udata
        	);

void
MD_eeprom_init_state (void);

/* Per a llegir de la EEPROM. addr és l'adreça amb la que s'intenta
 * llegir la EEPROM. Si no hi ha cap EEPROM o no és l'adreça apropiada
 * torna 0.
 */
MDu8
MD_eeprom_read (
        	const MDu32 addr
        	);

/* Per a escriure en la EEPROM. addr és l'adreça amb la que s'intenta
 * escriure en la EEPROM.
 */
void
MD_eeprom_write (
        	 const MDu32 addr,
        	 const MDu8  data
        	 );

int
MD_eeprom_save_state (
        	      FILE *f
        	      );

int
MD_eeprom_load_state (
        	      FILE *f
        	      );


/*******/
/* MEM */
/*******/
/* Mòdul que simula el mapa de memòria. */

/* Tipus d'accessos a memòria. */
typedef enum
  {
    MD_READ,
    MD_WRITE
  } MD_MemAccessType;

/* Tipus de la funció per a fer una traça dels accessos a
 * memòria. Cada vegada que es produeix un accés a memòria es crida.
 */
typedef void (MD_MemAccess) (
        		     const MD_MemAccessType  type, /* Tipus */
        		     const MDu32             addr, /* Adreça */
        		     const MD_Word           data, /* Dades */
        		     void                   *udata
                             );

/* Tipus de la funció per a fer una traça dels accessos a memòria a
 * nivell de byte. Cada vegada que es produeix un accés a memòria es
 * crida.
 */
typedef void (MD_MemAccess8) (
        		      const MD_MemAccessType  type, /* Tipus */
        		      const MDu32             addr, /* Adreça */
        		      const MDu8              data, /* Dades */
        		      void                   *udata
        		      );

/* Tipus de la funció utilitzada per a demanar la memòria RAM
 * estàtica. En cas de demanar-se es demana quan s'inicialitza
 * l'emulador.
 */
typedef MD_Word * (MD_GetStaticRAM) (
        			     const int  num_words,
        			     void      *udata
        			     );

/* Inicialitza el mapa de memòria. */
void
MD_mem_init (
             const MD_Rom    *rom,
             const MD_Bool    map_svp, // S'ha de mapejar el SVP.
             MD_GetStaticRAM *get_static_ram,
             MD_MemAccess    *mem_access,      /* Pot ser NULL. */
             MD_MemAccess8   *mem_access8,     /* Pot ser NULL. */
             MD_Warning      *warning,
             void            *udata
             );

void
MD_mem_init_state (void);

/* Llig una paraula de l'adreça especificada. */
MD_Word
MD_mem_read (
             const MDu32 addr    /* Adreça. */
             );

/* Llig un byte de l'adreça especificada. */
MDu8
MD_mem_read8 (
              const MDu32 addr    /* Adreça. */
              );

/* Activa/Desactiva el mode traça en el mòdul de memòria. */
void
MD_mem_set_mode_trace (
        	       const MD_Bool val
        	       );

/* Escriu una paraula en l'adreça especificada. */
void
MD_mem_write (
              const MDu32   addr,    /* Adreça. */
              const MD_Word data     /* Dades. */
              );

/* Escriu un byte en l'adreça especificada. */
void
MD_mem_write8 (
               const MDu32 addr,    /* Adreça. */
               const MDu8  data     /* Dades. */
               );

int
MD_mem_save_state (
        	   FILE *f
        	   );

int
MD_mem_load_state (
        	   FILE *f
        	   );

/* Torna un punter a la memòria de vídeo. La grandària és 32768. */
const MD_Word *
MD_mem_get_ram (void);


/*******/
/* I/O */
/*******/
/* Mòdul que simula l'entrada/eixida. */

/* Tipus per a indicar amb l'operador lògic OR els botons actualment
 * actius en un pad normal o de 6 botons.
 */
typedef enum
  {
    MD_UP= 0x01,
    MD_DOWN= 0x02,
    MD_LEFT= 0x04,
    MD_RIGHT= 0x08,
    MD_A= 0x10,
    MD_B= 0x20,
    MD_C= 0x40,
    MD_START= 0x80,
    MD_X= 0x100,
    MD_Y= 0x200,
    MD_Z= 0x400
  } MD_Button;

/* Tipus d'una funció que obté l'estat dels botons. Serveix tant per a
 * un pad de normal o un de 6 botons. Un botó està apretat si el seu
 * bit corresponent (MD_Button) està actiu. La mateixa funció es
 * gastar per al pad 1 (pad==0) i el pad2 (pad==1).
 */
typedef int (MD_CheckButtons) (
        		       const int  pad,
        		       void      *udata
        		       );

typedef enum
  {
    MD_IODEV_PAD= 0,    /* Hi ha un pad normal (Port1 i Port2).*/
    MD_IODEV_PAD6B,     /* Hi ha un pad de 6 botons (Port1 i Port2). */
    MD_IODEV_NONE       /* No hi ha cap dispositiu (Tots els ports). */
  } MD_IODevice;

typedef struct
{
  
  MD_IODevice dev1;
  MD_IODevice dev2;
  MD_IODevice dev_exp;
  
} MD_IOPluggedDevices;

void
MD_io_close (void);

MDu8
MD_io_data_read_1 (void);

MDu8
MD_io_data_read_2 (void);

MDu8
MD_io_data_read_exp (void);

void
MD_io_data_write_1 (
        	    const MDu8 data
        	    );

void
MD_io_data_write_2 (
        	    const MDu8 data
        	    );

void
MD_io_data_write_exp (
        	      const MDu8 data
        	      );

void
MD_io_end_frame_1 (void);

void
MD_io_end_frame_2 (void);

MDu8
MD_io_control_read_1 (void);

MDu8
MD_io_control_read_2 (void);

MDu8
MD_io_control_read_exp (void);

void
MD_io_control_write_1 (
        	       const MDu8 data
        	       );

void
MD_io_control_write_2 (
        	       const MDu8 data
        	       );

void
MD_io_control_write_exp (
        		 const MDu8 data
        		 );

void
MD_io_init (
            const MD_IOPluggedDevices  devices,
            MD_CheckButtons           *check_buttons,
            void                      *udata
            );

void
MD_io_init_state (void);

int
MD_io_save_state (
        	  FILE *f
        	  );

int
MD_io_load_state (
        	  FILE *f
        	  );


/*******/
/* CPU */
/*******/
/* Mòdul que simula el processador. */

/* Mnemonics. */
typedef enum
  {
    
    MD_UNK= 0,
    MD_ABCD,
    MD_ADDb,
    MD_ADDw,
    MD_ADDl,
    MD_ADDIb,
    MD_ADDIw,
    MD_ADDIl,
    MD_ADDQb,
    MD_ADDQl,
    MD_ADDQw,
    MD_ADDXb,
    MD_ADDXw,
    MD_ADDXl,
    MD_ANDb,
    MD_ANDw,
    MD_ANDl,
    MD_ANDIb,
    MD_ANDIw,
    MD_ANDIl,
    MD_ASLb,
    MD_ASLw,
    MD_ASLl,
    MD_ASRb,
    MD_ASRw,
    MD_ASRl,
    MD_BCC,
    MD_BCS,
    MD_BEQ,
    MD_BGE,
    MD_BGT,
    MD_BHI,
    MD_BLE,
    MD_BLS,
    MD_BLT,
    MD_BMI,
    MD_BNE,
    MD_BPL,
    MD_BVC,
    MD_BVS,
    MD_BCHG,
    MD_BCLR,
    MD_BRA,
    MD_BSET,
    MD_BSR,
    MD_BTST,
    MD_CHK,
    MD_CLRb,
    MD_CLRw,
    MD_CLRl,
    MD_CMPb,
    MD_CMPw,
    MD_CMPl,
    MD_CMPIb,
    MD_CMPIw,
    MD_CMPIl,
    MD_CMPMb,
    MD_CMPMw,
    MD_CMPMl,
    MD_DBCC,
    MD_DBCS,
    MD_DBEQ,
    MD_DBF,
    MD_DBGE,
    MD_DBGT,
    MD_DBHI,
    MD_DBLE,
    MD_DBLS,
    MD_DBLT,
    MD_DBMI,
    MD_DBNE,
    MD_DBPL,
    MD_DBT,
    MD_DBVC,
    MD_DBVS,
    MD_DIVS,
    MD_DIVU,
    MD_EORb,
    MD_EORw,
    MD_EORl,
    MD_EORIb,
    MD_EORIw,
    MD_EORIl,
    MD_EXG,
    MD_EXTl,
    MD_EXTw,
    MD_ILLEGAL,
    MD_JMP,
    MD_JSR,
    MD_LEA,
    MD_LINK,
    MD_LSLb,
    MD_LSLw,
    MD_LSLl,
    MD_LSRb,
    MD_LSRw,
    MD_LSRl,
    MD_MOVEb,
    MD_MOVEw,
    MD_MOVEl,
    MD_MOVEMl,
    MD_MOVEMw,
    MD_MOVEPw,
    MD_MOVEPl,
    MD_MOVEQ,
    MD_MULS,
    MD_MULU,
    MD_NBCD,
    MD_NEGb,
    MD_NEGl,
    MD_NEGw,
    MD_NEGXb,
    MD_NEGXl,
    MD_NEGXw,
    MD_NOP,
    MD_NOTb,
    MD_NOTl,
    MD_NOTw,
    MD_ORb,
    MD_ORl,
    MD_ORw,
    MD_ORIb,
    MD_ORIl,
    MD_ORIw,
    MD_PEA,
    MD_RESET,
    MD_ROLb,
    MD_ROLw,
    MD_ROLl,
    MD_RORb,
    MD_RORw,
    MD_RORl,
    MD_ROXLb,
    MD_ROXLw,
    MD_ROXLl,
    MD_ROXRb,
    MD_ROXRw,
    MD_ROXRl,
    MD_RTE,
    MD_RTR,
    MD_RTS,
    MD_SBCD,
    MD_SCC,
    MD_SCS,
    MD_SEQ,
    MD_SF,
    MD_SGE,
    MD_SGT,
    MD_SHI,
    MD_SLE,
    MD_SLS,
    MD_SLT,
    MD_SMI,
    MD_SNE,
    MD_SPL,
    MD_ST,
    MD_SVC,
    MD_SVS,
    MD_STOP,
    MD_SUBb,
    MD_SUBw,
    MD_SUBl,
    MD_SUBIb,
    MD_SUBIw,
    MD_SUBIl,
    MD_SUBQb,
    MD_SUBQl,
    MD_SUBQw,
    MD_SUBXb,
    MD_SUBXl,
    MD_SUBXw,
    MD_SWAP,
    MD_TRAP,
    MD_TSTb,
    MD_TSTw,
    MD_TSTl,
    MD_UNLK
    
  } MD_Mnemonic;

/* Tipus d'operador. */
typedef enum
  {
    
    MD_NONE= 0,
    MD_INMb,     /* byte */
    MD_INMw,     /* word */
    MD_INMl,     /* longval */
    MD_DN,       /* reg */
    MD_AN,       /* reg */
    MD_pAN,      /* reg */
    MD_pANi,     /* reg */
    MD_pANd,     /* reg */
    MD_pD16AN,   /* d16an */
    MD_pD8ANXN,  /* d8anxn */
    MD_pW,       /* word */
    MD_pL,       /* longval */
    MD_pD16PC,   /* d16an (reg=FF) */
    MD_pD8PCXN,  /* d8anxn (ret=FF) */
    MD_SR,
    MD_VECTOR,   /* vector */
    MD_DIS16,    /* dis16 */
    MD_USP,
    MD_LIST,     /* list */
    MD_CCR,
    MD_LABEL,    /* label */
    MD_COUNT     /* count */
    
  } MD_OpType;

/* Identifica a una instrucció. */
typedef struct
{
  
  MD_Mnemonic name;    /* Nom. */
  MD_OpType   op1;     /* Primer operant. */
  MD_OpType   op2;     /* Segon operant. */
  
} MD_InstId;

/* Dades relacionades amb els operadors. */
typedef union
{
  
  MDu8  byte;           /* Byte. */
  MDu8  reg;            /* Número de registre. */
  struct
  {
    MDu8  reg;
    MDs16 dis;
  }     d16an;          /* Número de registre i desplaçament. */
  struct
  {
    MDu8    areg;
    MDu8    xreg;
    MDu8    dis;
    /*MDu8    scale;  No suportat. */
    MD_Bool isd;
    MD_Bool isw;
  }     d8anxn;
  MDu16 word;
  MDu32 longval;
  int   vector;
  MDs16 dis16;
  MDu16 list;    /* 0x1 --> D0..D7A0..A7 */
  struct
  {
    MDu32 addr;
    MDs16 dis;
  }     label;
  MDu8  count;
  
} MD_InstExtra;

/* Estructura per a desar tota la informació relativa a una
 * instrucció.
 */
typedef struct
{
  
  MD_InstId    id;           /* Identificador d'instrucció. */
  MDu8         bytes[10];    /* Bytes */
  MDu8         nbytes;       /* Número de bytes. */
  MD_InstExtra e1;           /* Dades extra operador1. */
  MD_InstExtra e2;           /* Dades extra operador2. */

} MD_Inst;

/* Tipus de pas d'execució. */
typedef struct
{
  
  enum {
    MD_STEP_INST,
    MD_STEP_STOP,
    MD_STEP_RESET,
    MD_STEP_AUTOVECTOR
  } type;    /* Tipus. */
  union
  {
    MD_Inst      inst;
    unsigned int priority;
  } val;     /* Valor. */
  
} MD_Step;

/* Desactiva la senyal d'interrupció associat al auto-vector indicat. */
void
MD_cpu_clear_auto_vector_int (
        		      const int num   /* 1-7. Número de
        					 vector. */
        		      );

/* Descodifica en INST la instrucció de l'adreça indicada i torna
 * l'adreça de la següent instrucció en memòria.
 */
MDu32
MD_cpu_decode (
               MDu32    addr,
               MD_Inst *inst
               );

/* Descodifica en STEP el següent pas i torna l'adreça de la
 * següent instrucció en memòria.
 */
MDu32
MD_cpu_decode_next_step (
        		 MD_Step *step
        		 );

/* Inicialitza i força un RESET del processador. */
void
MD_cpu_init (
             MD_Warning *warning,     /* Funció per als avisos. */
             void       *udata        /* Dades de l'usuari. */
             );

void
MD_cpu_init_state (void);

/* Aquesta funció (no implementada) es cridada pel processador per a
 * resetejar els dispositus externs.
 */
void
MD_cpu_reset_external_devices_signal (void);

/* Executa la següent instrucció o interrupció. Torna el número de
 * cicles.
 */
int
MD_cpu_run (void);

/* Activa la senyal d'interrupció associat al auto-vector indicat. */
void
MD_cpu_set_auto_vector_int (
        		    const int num   /* 1-7. Número de
        				       vector. */
        		    );

/* Activa la senyal de RESET del processador. */
void
MD_cpu_set_reset (void);

int
MD_cpu_save_state (
        	   FILE *f
        	   );

int
MD_cpu_load_state (
        	   FILE *f
        	   );


/*******/
/* Z80 */
/*******/
/* Mòdul que controla el Z80. */

/* Tipus de funció per a saber quin a sigut l'últim pas d'execució de
 * la UCP Z80.
 */
typedef void (MD_CPUStepZ80) (
        		      const Z80_Step *step,        /* Punter a
        						      pas
        						      d'execuió. */
        		      const Z80u16    nextaddr,    /* Següent
        						      adreça de
        						      memòria. */
        		      void           *udata
        		      );

/* Tipus de la funció per a fer una traça dels accessos a memòria del
 * Z80.  Cada vegada que es produeix un accés a memòria es crida.
 */
typedef void (MD_MemAccessZ80) (
        			const MD_MemAccessType  type, /* Tipus */
        			const Z80u16            addr, /* Adreça */
        			const Z80u8             data, /* Dades */
        			void                   *udata
        			);

/* Per a consultar si el Z80 està en execució. */
MDu8
MD_z80_busreq_read (void);

/* Per a controlar l'execució del Z80. */
void
MD_z80_busreq_write (
        	     const MDu8 data
        	     );

/* Procesa cicles de la UCP (rellotge). */
void
MD_z80_clock (
              const int cc
              );

/* Inicialitza el Z80. */
void
MD_z80_init (
             MD_CPUStepZ80   *cpu_step,      /* Per a seguir
        					l'execució del
        					processador. Pot ser
        					NULL. */
             MD_MemAccessZ80 *mem_access,    /* Per a seguir els
        					accessos a memòria del
        					Z80. */
             MD_Warning      *warning,       /* Funció per als
        					avisos. */
             void            *udata          /* Dades de l'usuari. */
             );

void
MD_z80_init_state (void);

/* Per a fer una petició per a resetejar el processador. */
void
MD_z80_reset (
              const MDu8 data
              );

/* Fa el mateix que MD_z80_clock però cridant a les funcions de
 * callback.
 */
void
MD_z80_trace (
              const int cc
              );

int
MD_z80_save_state (
        	   FILE *f
        	   );

int
MD_z80_load_state (
        	   FILE *f
        	   );


/*******/
/* VDP */
/*******/
/* Mòdul que implementa el xip gràfic. */

/* Funció per a indicar un canvi en la resolució. Sempre es crida
 * almenys una vegada al inicialitzar el simulador.
 */
typedef void 
(MD_SResChanged) (
        	  const int  width,
        	  const int  height,
        	  void      *udata
        	  );

/* Tipus de la funció que actualitza la pantalla real. FB és el buffer
 * amb una imatge de grandària variable, on cada valor és un color
 * (vore MD_color2RGB).
 */
typedef void (MD_UpdateScreen) (
        			const int  fb[],
        			void      *udata
        			);

/* Indica a la VDP que una interrupció ha sigut servida. Al cridar a
 * aquesta funció la VDP no li diu res a la UCP.
 */
void
MD_vdp_clear_interrupt (
        		const int priority 
        		);

/* Processa cicles de CPU. Torna si està ocupat fent DMA mem->vram o
   no. */
MD_Bool
MD_vdp_clock (
              const int cc
              );

/* Per a configura el VDP. */
void
MD_vdp_control (
        	const MD_Word data
        	);

/* Llig una paraula del port de dades. */
MD_Word
MD_vdp_data_read (void);

/* Escriu una paraula en el port de dades. */
void
MD_vdp_data_write (
        	   const MD_Word data
        	   );

/* Escriu un byte en el port de dades. */
void
MD_vdp_data_write8 (
        	    const MDu8    data,
        	    const MD_Bool isH    /* Indica si és H/L. */
        	    );

/* Executa un pas del DMA mem->vram i torna els cicles de CPU
 * emprats. Sols s'ha de gastar quan la VDP diu que està ocupada fent
 * el DMA. Després de l'últim pas el VDP deixarà de dir que està
 * ocupat. Aquesta funció no consumix clocks de CPU, per tant cal
 * tornar a cridar a MD_vdp_clock després.
 */
int
MD_vdp_dma_mem2vram_step (void);

/* Torna un punter a la paleta de color. La grandària és 64. */
const MDu16 *
MD_vdp_get_cram (void);

/* Torna un punter a la memòria de vídeo. La grandària és 65536. */
const MDu8 *
MD_vdp_get_vram (void);

/* Torna els valors de H i V. */
MD_Word
MD_vdp_HV (void);

/* Inicialitza el mòdul. */
void
MD_vdp_init (
             const MD_Bool    ispal,            /* Indica PAL/NTSC. */
             MD_SResChanged  *sres_changed,     /* Per a avisar de
        					   canvis de
        					   resolució. */
             MD_UpdateScreen *update_screen,    /* Per a actualitzar
        					   la pantalla. */
             MD_Warning      *warning,          /* Funció per als
        					   avisos. */
             void            *udata             /* Dades de
        					   l'usuari. */
             );

void
MD_vdp_init_state (void);

/* Torna l'estat del VDP. */
MD_Word
MD_vdp_status (void);

int
MD_vdp_save_state (
        	   FILE *f
        	   );

int
MD_vdp_load_state (
        	   FILE *f
        	   );

// Fet molt estrany que he descobert gràcies a MAME (visca
// MAME!). Depenent de la ROM s'ha d'activar o no des de fora. Per
// defecte és 0, però si no es modifica explícitament manté l'últim
// valor.
void
MD_vdp_set_dma_lag (
                    const int lag
                    );


/******/
/* FM */
/******/
/* Mòdul que controla el xip de FM. */

// Nombre de cicles CPU per FM SAMPLE
//
// SAMPLES PER SEC = CYCLES_PER_SEC/MD_CPU_CYCLES_PER_FM_SAMPLE
#define MD_CPU_CYCLES_PER_FM_SAMPLE 144

/* Número de mostres que té cadascun dels buffers que genera el xip de
 * fm. Un poc menys d'una centèsima de segon
 */
#define MD_FM_BUFFER_SIZE 512

/* Processa cicles de la UCP (68K) de rellotge. */
void
MD_fm_clock (
             const int cc
             );

/* Inicialitza el mòdul. */
void
MD_fm_init (
            MD_Warning *warning,    /* Funció per als avisos. */
            void       *udata       /* Dades de l'usuari. */
            );

void
MD_fm_init_state (void);

void
MD_fm_part1_set_addr (
        	      const MDu8 data
        	      );
void
MD_fm_part1_write_data (
        		const MDu8 data
        		);

void
MD_fm_part2_set_addr (
        	      const MDu8 data
        	      );
void
MD_fm_part2_write_data (
        		const MDu8 data
        		);

/* Reseteja el xip. */
void
MD_fm_reset (void);

MDu8
MD_fm_status (void);

int
MD_fm_save_state (
        	  FILE *f
        	  );

int
MD_fm_load_state (
        	  FILE *f
        	  );


/*******/
/* PSG */
/*******/
/* Mòdul que controla el xip de so PSG. */

/* Processa cicles de la UCP. */
void
MD_psg_clock (
              const int cc
              );

/* Controla el PSG. */
void
MD_psg_control (
        	const Z80u8 data
        	);

/* Inicialitza el mòdul. */
void
MD_psg_init (void);

void
MD_psg_init_state (void);

int
MD_psg_save_state (
        	   FILE *f
        	   );

int
MD_psg_load_state (
        	   FILE *f
        	   );


/*********/
/* AUDIO */
/*********/
/* Mòdul que fusiona l'eixida del FM i el PSG. */

/* Tipus de la funció que actualitza es crida per a reproduir so. Es
 * proporcionen dos canals intercalats (esquerra/dreta). Cada mostra
 * està codificada com un valor de 16 bits amb signe.
 */
typedef void (MD_PlaySound) (
        		     const MDs16  samples[MD_FM_BUFFER_SIZE*2],
        		     void        *udata
        		     );

/* Inicialitza el mòdul. */
void
MD_audio_init (
               MD_Warning   *warning,     // Funció per als avisos.
               MD_PlaySound *play_sound,  // Callback.
               void         *udata        // Dades de l'usuari.
               );

void
MD_audio_init_state (void);

// Envia una mostra FM al mesclador.
void
MD_audio_fm_play (
                  const int16_t left,
        	  const int16_t right
        	  );

void
MD_audio_psg_play (
                   const double sample
        	   );

int
MD_audio_save_state (
        	     FILE *f
        	     );

int
MD_audio_load_state (
        	     FILE *f
        	     );


/*******/
/* SVP */
/*******/
/* Mòdul que implementa el SVP per al VirtuaRacing. */

// Mnemonics SVP
typedef enum
  {
   MD_SVP_UNK= 0,
   MD_SVP_SUB,
   MD_SVP_CMP,
   MD_SVP_ADD,
   MD_SVP_AND,
   MD_SVP_OR,
   MD_SVP_EOR,
   MD_SVP_MOD,
   MD_SVP_LD,
   MD_SVP_CALL,
   MD_SVP_BRA,
   MD_SVP_MLD,
   MD_SVP_MPYA,
   MD_SVP_MPYS
  } MD_SVPMnemonic;

// Operadors.
typedef enum
  {
   MD_SVP_NONE= 0,
   MD_SVP__,
   MD_SVP_X,
   MD_SVP_Y,
   MD_SVP_A,
   MD_SVP_ST,
   MD_SVP_STACK,
   MD_SVP_PC,
   MD_SVP_P,
   MD_SVP_PM0,
   MD_SVP_PM1,
   MD_SVP_PM2,
   MD_SVP_XST,
   MD_SVP_PM4,
   MD_SVP_EXT5,
   MD_SVP_PMC,
   MD_SVP_AL,
   MD_SVP_PRI,
   MD_SVP_PRI_MVAL,
   MD_SVP_PRI_INC,
   MD_SVP_PRI_MODINC,
   MD_SVP_PRI_MODDEC,
   MD_SVP_ADR,
   MD_SVP_IMM,
   MD_SVP_PPRI,
   MD_SVP_PPRI_MVAL,
   MD_SVP_PPRI_INC,
   MD_SVP_PPRI_MODINC,
   MD_SVP_PPRI_MODDEC,
   MD_SVP_RI,
   MD_SVP_SIMM,
   MD_SVP_COND_UNK,
   MD_SVP_COND_TRUE,
   MD_SVP_COND_Z,
   MD_SVP_COND_N,
   MD_SVP_ADDR,
   MD_SVP_PA,
   MD_SVP_OP_SR,
   MD_SVP_OP_SL,
   MD_SVP_OP_NEG,
   MD_SVP_OP_ABS,
   MD_SVP_OP_UNK
  } MD_SVPOpType;

typedef struct
{
  uint16_t       inst[2];
  int            nwords;
  MD_SVPMnemonic mnemonic;
  MD_SVPOpType   op[2];
  struct
  {
    int m;
    int reg;
  }              ri[2];
  MD_Bool        cond_f;
  uint8_t        simm;
  struct
  {
    uint8_t pos;
    int     ram_ind;
  }              adr;
} MD_SVPInst;

// Tipus de funció per a saber quin a sigut l'últim pas d'execució de
// la UCP SVP.
typedef void (MD_CPUStepSVP) (
                              const MD_SVPInst *inst,
                              const uint16_t    addr,
        		      void             *udata
        		      );

// Inicialitza el mòdul. Torna cert si la ROM és una ROM amb el xip
// SVP.
MD_Bool
MD_svp_init (
             const MD_Rom  *rom,
             MD_Warning    *warning,      // Funció per als avisos.
             MD_CPUStepSVP *cpu_step_svp, // Pot ser NULL
             void          *udata         // Dades de l'usuari. */
             );

void
MD_svp_init_state (void);

// Procesa cicles de la UCP (rellotge).
void
MD_svp_clock (
              const int cc
              );

// Reseteja el SVP.
void
MD_svp_reset (void);

// Llig dels registres de control/status. L'adreça que rep és la del
// mapa de memòria de la MegaDrive.
MD_Word
MD_svp_port_read (
                  const MDu32 addr
                  );

// Escriu en els registres de control/status. L'adreça que rep és la del
// mapa de memòria de la MegaDrive.
void
MD_svp_port_write (
                   const MDu32   addr,
                   const MD_Word data
                   );

// Llig de la memòria mapejada del xip SVP. L'adreça que rep és la del
// mapa de memòria de la MegaDrive.
MD_Word
MD_svp_mem_read (
                 const MDu32 addr
                 );

// Escriu en la memòria mapejada del xip SVP. L'adreça que rep és la
// del mapa de memòria de la MegaDrive.
void
MD_svp_mem_write (
                  const MDu32   addr,
                  const MD_Word data
                 );

void
MD_svp_trace (
              const int cc
              );

// Torna un punter a la DRAM. La grandària és 0x1000. */
const uint16_t *
MD_svp_get_dram (void);

// Si la ROM no és SVP no fa res.
int
MD_svp_save_state (
        	   FILE *f
        	   );

// Si la ROM no és SVP no fa res.
int
MD_svp_load_state (
        	   FILE *f
        	   );


/********/
/* MAIN */
/********/
/* Mòdul que controla el simulador. */

// Número de cicles per segon (MOTOROLA 68000).
//
// ATENCIÓ!!! El Master Clock en realitat és el valor de baix per
// x7. I el clock que gasta el Z80 i el VDP és el MasterClock/15.
#define MD_CYCLES_PER_SEC_PAL 7600489
#define MD_CYCLES_PER_SEC_NTSC 7670454

/* Tipus de funció amb la que el 'frontend' indica a la llibreria si
 * s'ha produït alguna senyal. A més esta funció pot ser emprada per
 * el frontend per a tractar els events pendents.
 */
typedef void (MD_CheckSignals) (
        			MD_Bool *stop,
        			MD_Bool *reset,
        			void    *udata
        			);

/* Tipus de funció per a saber quin a sigut l'últim pas d'execució de
 * la UCP.
 */
typedef void (MD_CPUStep) (
        		   const MD_Step *step,         /* Punter a
        						   pas
        						   d'execuió. */
        		   const MDu32    nextaddr,    /* Següent
        						  adreça de
        						  memòria. */
        		   void           *udata
        		   );

/* Els camps poden ser distint de NULL. */
typedef struct
{
  
  MD_MemAccess    *mem_access;        /* Es crida cada vegada que es
        				 produeïx un accés a
        				 memòria. */
  MD_MemAccess8   *mem_access8;       /* Es crida cada vegada que es
        				 produeïx un accés a memòria a
        				 nivell de byte. */
  MD_MemAccessZ80 *mem_access_z80;    /* Es crida cada vegada que es
        				 produeïx un accés a memòria
        				 per part del Z80. */
  MD_CPUStep      *cpu_step;          /* Es crida en cada pas de la
        				 UCP. */
  MD_CPUStepZ80   *cpu_step_z80;      /* Es crida en cada pas de la
        				 UCP Z80. */
  MD_CPUStepSVP   *cpu_step_svp;      // Es crida en cada pasr de la
                                      // UCP SVP.
  
} MD_TraceCallbacks;

/* Conté la informació necessària per a comunicar-se amb el
 * 'frontend'.
 */
typedef struct
{
  
  MD_Warning              *warning;          /* Funció per a mostrar
        					avisos. */
  MD_CheckSignals         *check;            /* Comprova si ha de
        					parar i events
        					externs. Pot ser NULL,
        					en eixe cas el
        					simulador s'executarà
        					fins que es cride a
        					'MD_stop'. */
  MD_SResChanged          *sres_changed;     /* Es crida cada vegada
        					que canvia la
        					resolució de la
        					pantalla. Es cridarà
        					almenys una vegada
        					durant
        					l'inicialització del
        					simulador. */
  MD_UpdateScreen         *update_screen;    /* Es crida cada vegada
        					que es genera un
        					frame. */
  MD_PlaySound            *play_sound;       /* Es crida per a generar
        					frames de so del xips
        					de so. */
  MD_GetStaticRAM         *get_static_ram;   /* Es crida per a demanar
        					la SRAM. */
  MD_GetEEPROM            *get_eeprom;       /* Es crida per a demanar
        					la memòria de la
        					EEPROM. */
  const MD_TraceCallbacks *trace;            /* Pot ser NULL si no es
        					van a gastar les
        					funcions per a fer una
        					traça. */
  MD_IOPluggedDevices      plugged_devs;     /* La conexió inicial de
        					dispositius. */
  MD_CheckButtons         *check_buttons;    /* Callback emprat per a
        					comprovar si els
        					botons dels 'pads'
        					(normals o de 6
        					botons) han sigut
        					emprats. NULL
        					inhabilita els pads. */
  
} MD_Frontend;

/* Flags per als models. */
enum
  {
    MD_MODEL_OVERSEAS= 0x1, /* Per defecte és Domestic. */
    MD_MODEL_PAL= 0x2       /* Per defecte és NTSC. */
  };

/* Color en RGB. */
typedef struct
{
  MDu8 r,g,b;
} MD_RGB;

/* Allibera memòria de la llibreria. */
void
MD_close (void);

/* Convertix un color en RGB. El color es defineix en el rang
   [0,0x5ff]. */
MD_RGB
MD_color2rgb (
              const int color
              );

/* Torna en devs els dispositius actualment conectats. Si no està
 * inicialitzat no fa res.
 */
void
MD_get_io_devs (
        	MD_IOPluggedDevices *devs
        	);

/* Inicialitza la llibreria. */
void
MD_init (
         const MD_Rom      *rom,            /* ROM. */
         const MDu8         model_flags,    /* Flags per a indicar el
        				       model. */
         const MD_Frontend *frontend,       /* Frontend. */
         void              *udata           /* Dades proporcionades
        				       per l'usuari que són
        				       pasades al
        				       'frontend'. */
         );

/* Executa un cicle de la Mega Drive. Aquesta funció executa una
 * iteració de 'MD_loop' i torna els cicles de UCP emprats. Si
 * CHECKSIGNALS en el frontend no és NULL aleshores cada cert temps al
 * cridar a MD_iter es fa una comprovació de CHECKSIGNALS.  La funció
 * CHECKSIGNALS del frontend es crida amb una freqüència suficient per
 * a que el frontend tracte els seus events. La senyal stop de
 * CHECKSIGNALS és llegit en STOP si es crida a CHECKSIGNALS.
 */
int
MD_iter (
         MD_Bool *stop
         );

/* Carrega l'estat de 'f'. Torna 0 si tot ha anat bé. S'espera que el
 * fitxer siga un fitxer d'estat vàlid de MegaDrive per a la ROM i
 * pads actuals. Si es produeix un error de lectura o es compromet la
 * integritat del simulador, aleshores es reiniciarà el simulador.
 */
int
MD_load_state (
               FILE *f
               );

/* Executa la MegaDrive. Aquesta funció es bloqueja fins que llig una
 * senyal de parada mitjançant CHECKSIGNALS o mitjançant MD_stop, si
 * es para es por tornar a cridar i continuarà on s'havia quedat. La
 * funció CHECKSIGNALS del frontend es crida amb una freqüència
 * suficient per a que el frontend tracte els seus events.
 */
void
MD_loop (void);

/* Torna el model (VERSION NO.). */
MDu8
MD_version_no (void);

/* Fa un reset quan no hi ha checksignals. */
void
MD_reset (void);

/* Escriu en 'f' l'estat de la màquina. Torna 0 si tot ha anat bé, -1
 * en cas contrari.
 */
int
MD_save_state (
               FILE *f
               );

/* Modifica els dispositius conectats. Si no està inicialitzat no fa
 * res.
 */
void
MD_set_io_devs (
        	const MD_IOPluggedDevices devs
        	);

/* Para a 'MD_loop'. */
void
MD_stop (void);

/* Executa els següent pas de UCP en mode traça. Tots aquelles
 * funcions de 'callback' que no són nul·les es cridaran si és el
 * cas. Torna el clocks de rellotge executats en l'últim pas.
 */
int
MD_trace (void);

#endif /* __MD_H__ */
