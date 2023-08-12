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
 *  fm_new.c - Implementació del mòdul FM (YM2612).
 *
 */
/*
 *  REFERÈNCIES:
 *
 * - M'he basatat en la informació aportada per Nemesis en
 *   http://gendev.spritesmind.net:
 *
 *   - http://gendev.spritesmind.net/forum/viewtopic.php?t=386
 *   - http://gendev.spritesmind.net/forum/viewtopic.php?t=386&start=106
 *   - http://gendev.spritesmind.net/forum/viewtopic.php?f=24&t=386&start=150
 *   - http://gendev.spritesmind.net/forum/viewtopic.php?f=24&t=386&start=165
 *   - http://gendev.spritesmind.net/forum/viewtopic.php?f=24&t=386&start=405
 *   - http://gendev.spritesmind.net/forum/viewtopic.php?f=24&t=386&start=465
 *   - http://gendev.spritesmind.net/forum/viewtopic.php?f=24&t=386&start=480
 */


#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
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

#define EG_MAX_ATTENUATION 0x3FF

// OUT és un valor d'almenys 14bits amb signe i passa a un valor de 10 bits
// ¿amb signe?. En qualsevol cas la transformació consisteix en
// quedar-se amb els bits 10-1 del valor de 14 bits.
#define OUT2PHASEMOD(VAL) ((VAL>>1)&0x3FF)

// Hi ha un poc de cacau amb la nomenclatura dels slots
#define SLOT1 0
#define SLOT2 2
#define SLOT3 1
#define SLOT4 3

// NOTA!!! Estos cicles són aproximacions que he fet jo partint que la
// documentació que hi ha per ahí diu que el TIMERA té una precisió de
// ~0.018ms en NTSC i de la documentació del YM2608. TIMER B sempre és
// TIMERA_CC*16
#define TIMERA_CC 138
#define TIMERB_CC (TIMERA_CC*16)




/*************/
/* CONSTANTS */
/*************/

// Shifts per a cada rate.
static const int EG_COUNTER_SHIFT[64]= {
  11, 11, 11, 11,     // 0-3
  10, 10, 10, 10,     // 4-7
  9,  9,  9,  9,      // 8-11
  8,  8,  8,  8,      // 12-15
  7,  7,  7,  7,      // 16-19
  6,  6,  6,  6,      // 20-23
  5,  5,  5,  5,      // 24-27
  4,  4,  4,  4,      // 28-31
  3,  3,  3,  3,      // 32-35
  2,  2,  2,  2,      // 36-39
  1,  1,  1,  1,      // 40-43
  0,  0,  0,  0,      // 44-47
  0,  0,  0,  0,      // 48-51
  0,  0,  0,  0,      // 52-55
  0,  0,  0,  0,      // 56-59
  0,  0,  0,  0       // 60-63
};


static const int EG_ATTENUATION_INCREMENT[64][8]= {
  
  // 0-3
  {0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0},
  {0,1,0,1,0,1,0,1},
  {0,1,0,1,0,1,0,1},

  // 4-7
  {0,1,0,1,0,1,0,1},
  {0,1,0,1,0,1,0,1},
  {0,1,1,1,0,1,1,1},
  {0,1,1,1,0,1,1,1},

  // 8-11
  {0,1,0,1,0,1,0,1},
  {0,1,0,1,1,1,0,1},
  {0,1,1,1,0,1,1,1},
  {0,1,1,1,1,1,1,1},

  // 12-15
  {0,1,0,1,0,1,0,1},
  {0,1,0,1,1,1,0,1},
  {0,1,1,1,0,1,1,1},
  {0,1,1,1,1,1,1,1},

  // 16-19
  {0,1,0,1,0,1,0,1},
  {0,1,0,1,1,1,0,1},
  {0,1,1,1,0,1,1,1},
  {0,1,1,1,1,1,1,1},

  // 20-23
  {0,1,0,1,0,1,0,1},
  {0,1,0,1,1,1,0,1},
  {0,1,1,1,0,1,1,1},
  {0,1,1,1,1,1,1,1},

  // 24-27
  {0,1,0,1,0,1,0,1},
  {0,1,0,1,1,1,0,1},
  {0,1,1,1,0,1,1,1},
  {0,1,1,1,1,1,1,1},

  // 28-31
  {0,1,0,1,0,1,0,1},
  {0,1,0,1,1,1,0,1},
  {0,1,1,1,0,1,1,1},
  {0,1,1,1,1,1,1,1},

  // 32-35
  {0,1,0,1,0,1,0,1},
  {0,1,0,1,1,1,0,1},
  {0,1,1,1,0,1,1,1},
  {0,1,1,1,1,1,1,1},

  // 36-39
  {0,1,0,1,0,1,0,1},
  {0,1,0,1,1,1,0,1},
  {0,1,1,1,0,1,1,1},
  {0,1,1,1,1,1,1,1},

  // 40-43
  {0,1,0,1,0,1,0,1},
  {0,1,0,1,1,1,0,1},
  {0,1,1,1,0,1,1,1},
  {0,1,1,1,1,1,1,1},

  // 44-47
  {0,1,0,1,0,1,0,1},
  {0,1,0,1,1,1,0,1},
  {0,1,1,1,0,1,1,1},
  {0,1,1,1,1,1,1,1},

  // 48-51
  {1,1,1,1,1,1,1,1},
  {1,1,1,2,1,1,1,2},
  {1,2,1,2,1,2,1,2},
  {1,2,2,2,1,2,2,2},

  // 52-55
  {2,2,2,2,2,2,2,2},
  {2,2,2,4,2,2,2,4},
  {2,4,2,4,2,4,2,4},
  {2,4,4,4,2,4,4,4},

  // 56-59
  {4,4,4,4,4,4,4,4},
  {4,4,4,8,4,4,4,8},
  {4,8,4,8,4,8,4,8},
  {4,8,8,8,4,8,8,8},

  // 60-63
  {8,8,8,8,8,8,8,8},
  {8,8,8,8,8,8,8,8},
  {8,8,8,8,8,8,8,8},
  {8,8,8,8,8,8,8,8}
  
};


static const int32_t DETUNE_TABLE[32][8]= {
  {0, 0, 1, 2, 0, 0, -1, -2},
  {0, 0, 1, 2, 0, 0, -1, -2},
  {0, 0, 1, 2, 0, 0, -1, -2},
  {0, 0, 1, 2, 0, 0, -1, -2},
  {0, 1, 2, 2, 0, -1, -2, -2},
  {0, 1, 2, 3, 0, -1, -2, -3},
  {0, 1, 2, 3, 0, -1, -2, -3},
  {0, 1, 2, 3, 0, -1, -2, -3},
  {0, 1, 2, 4, 0, -1, -2, -4},
  {0, 1, 3, 4, 0, -1, -3, -4},
  {0, 1, 3, 4, 0, -1, -3, -4},
  {0, 1, 3, 5, 0, -1, -3, -5},
  {0, 2, 4, 5, 0, -2, -4, -5},
  {0, 2, 4, 6, 0, -2, -4, -6},
  {0, 2, 4, 6, 0, -2, -4, -6},
  {0, 2, 5, 7, 0, -2, -5, -7},
  {0, 2, 5, 8, 0, -2, -5, -8},
  {0, 3, 6, 8, 0, -3, -6, -8},
  {0, 3, 6, 9, 0, -3, -6, -9},
  {0, 3, 7, 10, 0, -3, -7, -10},
  {0, 4, 8, 11, 0, -4, -8, -11},
  {0, 4, 8, 12, 0, -4, -8, -12},
  {0, 4, 9, 13, 0, -4, -9, -13},
  {0, 5, 10, 14, 0, -5, -10, -14},
  {0, 5, 11, 16, 0, -5, -11, -16},
  {0, 6, 12, 17, 0, -6, -12, -17},
  {0, 6, 13, 19, 0, -6, -13, -19},
  {0, 7, 14, 20, 0, -7, -14, -20},
  {0, 8, 16, 22, 0, -8, -16, -22},
  {0, 8, 16, 22, 0, -8, -16, -22},
  {0, 8, 16, 22, 0, -8, -16, -22},
  {0, 8, 16, 22, 0, -8, -16, -22}
};


// APUNTS
//
// - Cada entrada representa un valor entre 0 i 1 (approx). La idea és
//   que fa reerència a la part fraccional que volem convertir.
//
// - Cada entrada en realitat és 2^-entrada
//
// - Cada entrada es representa amb 11bits
//
static const int16_t POW_TABLE[256]= {
  0x7FA, 0x7F5, 0x7EF, 0x7EA, 0x7E4, 0x7DF, 0x7DA, 0x7D4, 
  0x7CF, 0x7C9, 0x7C4, 0x7BF, 0x7B9, 0x7B4, 0x7AE, 0x7A9, 
  0x7A4, 0x79F, 0x799, 0x794, 0x78F, 0x78A, 0x784, 0x77F, 
  0x77A, 0x775, 0x770, 0x76A, 0x765, 0x760, 0x75B, 0x756, 
  0x751, 0x74C, 0x747, 0x742, 0x73D, 0x738, 0x733, 0x72E, 
  0x729, 0x724, 0x71F, 0x71A, 0x715, 0x710, 0x70B, 0x706, 
  0x702, 0x6FD, 0x6F8, 0x6F3, 0x6EE, 0x6E9, 0x6E5, 0x6E0, 
  0x6DB, 0x6D6, 0x6D2, 0x6CD, 0x6C8, 0x6C4, 0x6BF, 0x6BA, 
  0x6B5, 0x6B1, 0x6AC, 0x6A8, 0x6A3, 0x69E, 0x69A, 0x695, 
  0x691, 0x68C, 0x688, 0x683, 0x67F, 0x67A, 0x676, 0x671, 
  0x66D, 0x668, 0x664, 0x65F, 0x65B, 0x657, 0x652, 0x64E, 
  0x649, 0x645, 0x641, 0x63C, 0x638, 0x634, 0x630, 0x62B, 
  0x627, 0x623, 0x61E, 0x61A, 0x616, 0x612, 0x60E, 0x609, 
  0x605, 0x601, 0x5FD, 0x5F9, 0x5F5, 0x5F0, 0x5EC, 0x5E8, 
  0x5E4, 0x5E0, 0x5DC, 0x5D8, 0x5D4, 0x5D0, 0x5CC, 0x5C8, 
  0x5C4, 0x5C0, 0x5BC, 0x5B8, 0x5B4, 0x5B0, 0x5AC, 0x5A8, 
  0x5A4, 0x5A0, 0x59C, 0x599, 0x595, 0x591, 0x58D, 0x589, 
  0x585, 0x581, 0x57E, 0x57A, 0x576, 0x572, 0x56F, 0x56B, 
  0x567, 0x563, 0x560, 0x55C, 0x558, 0x554, 0x551, 0x54D, 
  0x549, 0x546, 0x542, 0x53E, 0x53B, 0x537, 0x534, 0x530, 
  0x52C, 0x529, 0x525, 0x522, 0x51E, 0x51B, 0x517, 0x514, 
  0x510, 0x50C, 0x509, 0x506, 0x502, 0x4FF, 0x4FB, 0x4F8, 
  0x4F4, 0x4F1, 0x4ED, 0x4EA, 0x4E7, 0x4E3, 0x4E0, 0x4DC, 
  0x4D9, 0x4D6, 0x4D2, 0x4CF, 0x4CC, 0x4C8, 0x4C5, 0x4C2, 
  0x4BE, 0x4BB, 0x4B8, 0x4B5, 0x4B1, 0x4AE, 0x4AB, 0x4A8, 
  0x4A4, 0x4A1, 0x49E, 0x49B, 0x498, 0x494, 0x491, 0x48E, 
  0x48B, 0x488, 0x485, 0x482, 0x47E, 0x47B, 0x478, 0x475, 
  0x472, 0x46F, 0x46C, 0x469, 0x466, 0x463, 0x460, 0x45D, 
  0x45A, 0x457, 0x454, 0x451, 0x44E, 0x44B, 0x448, 0x445, 
  0x442, 0x43F, 0x43C, 0x439, 0x436, 0x433, 0x430, 0x42D, 
  0x42A, 0x428, 0x425, 0x422, 0x41F, 0x41C, 0x419, 0x416, 
  0x414, 0x411, 0x40E, 0x40B, 0x408, 0x406, 0x403, 0x400
};


// APUNTS:
//
// - Representa un quart del cercle
//
// - Com la fase és 10bits un quart és 8bits per tant 256 valors
//   diferents.
//
// - Els valors ja estàn desats en format atenuació per poder sumar-ho
//   a l'atenuació. Bàsicament (-log(VAL)/log(2))/256.
//
// - El valor final utilitza 12 bits en format 4.8.
//
static const int16_t SIN_TABLE[256]= {
  0x859, 0x6C3, 0x607, 0x58B, 0x52E, 0x4E4, 0x4A6, 0x471, 
  0x443, 0x41A, 0x3F5, 0x3D3, 0x3B5, 0x398, 0x37E, 0x365, 
  0x34E, 0x339, 0x324, 0x311, 0x2FF, 0x2ED, 0x2DC, 0x2CD, 
  0x2BD, 0x2AF, 0x2A0, 0x293, 0x286, 0x279, 0x26D, 0x261, 
  0x256, 0x24B, 0x240, 0x236, 0x22C, 0x222, 0x218, 0x20F, 
  0x206, 0x1FD, 0x1F5, 0x1EC, 0x1E4, 0x1DC, 0x1D4, 0x1CD, 
  0x1C5, 0x1BE, 0x1B7, 0x1B0, 0x1A9, 0x1A2, 0x19B, 0x195, 
  0x18F, 0x188, 0x182, 0x17C, 0x177, 0x171, 0x16B, 0x166, 
  0x160, 0x15B, 0x155, 0x150, 0x14B, 0x146, 0x141, 0x13C, 
  0x137, 0x133, 0x12E, 0x129, 0x125, 0x121, 0x11C, 0x118, 
  0x114, 0x10F, 0x10B, 0x107, 0x103, 0x0FF, 0x0FB, 0x0F8, 
  0x0F4, 0x0F0, 0x0EC, 0x0E9, 0x0E5, 0x0E2, 0x0DE, 0x0DB, 
  0x0D7, 0x0D4, 0x0D1, 0x0CD, 0x0CA, 0x0C7, 0x0C4, 0x0C1, 
  0x0BE, 0x0BB, 0x0B8, 0x0B5, 0x0B2, 0x0AF, 0x0AC, 0x0A9, 
  0x0A7, 0x0A4, 0x0A1, 0x09F, 0x09C, 0x099, 0x097, 0x094, 
  0x092, 0x08F, 0x08D, 0x08A, 0x088, 0x086, 0x083, 0x081, 
  0x07F, 0x07D, 0x07A, 0x078, 0x076, 0x074, 0x072, 0x070, 
  0x06E, 0x06C, 0x06A, 0x068, 0x066, 0x064, 0x062, 0x060, 
  0x05E, 0x05C, 0x05B, 0x059, 0x057, 0x055, 0x053, 0x052, 
  0x050, 0x04E, 0x04D, 0x04B, 0x04A, 0x048, 0x046, 0x045, 
  0x043, 0x042, 0x040, 0x03F, 0x03E, 0x03C, 0x03B, 0x039, 
  0x038, 0x037, 0x035, 0x034, 0x033, 0x031, 0x030, 0x02F, 
  0x02E, 0x02D, 0x02B, 0x02A, 0x029, 0x028, 0x027, 0x026, 
  0x025, 0x024, 0x023, 0x022, 0x021, 0x020, 0x01F, 0x01E, 
  0x01D, 0x01C, 0x01B, 0x01A, 0x019, 0x018, 0x017, 0x017, 
  0x016, 0x015, 0x014, 0x014, 0x013, 0x012, 0x011, 0x011, 
  0x010, 0x00F, 0x00F, 0x00E, 0x00D, 0x00D, 0x00C, 0x00C, 
  0x00B, 0x00A, 0x00A, 0x009, 0x009, 0x008, 0x008, 0x007, 
  0x007, 0x007, 0x006, 0x006, 0x005, 0x005, 0x005, 0x004, 
  0x004, 0x004, 0x003, 0x003, 0x003, 0x002, 0x002, 0x002, 
  0x002, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 
  0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000
};


// Són valors que he vist en altres emuladors. Aparentment és
// aproximadament (SampleRate/LFO_FREQ)/128. Cal recordar que LFO está
// simulat com un comptador de 128 pasos.
static const int LFO_FREQ2CC[8]= {
  108, 77, 71, 67, 62, 44, 8, 5
};


// La segona dimensió representa l'amplitut d'un quart d'ona sinuidal
// (0..1) representat en 5 bits (0..0x1F). Aquesta amplitut està
// multiplicada pel percentatge indexat en la primera dimensió (PMS).
static const int16_t LFO_PM_INC[8][8]= {
  {0, 0, 0, 0,  0,  0,  0,  0},  // 0%
  {0, 0, 0, 0,  1,  1,  1,  1},  // 3.4%
  {0, 0, 0, 1,  1,  1,  2,  2},  // 6.7%
  {0, 0, 1, 1,  2,  2,  3,  3},  // 10%
  {0, 0, 1, 2,  2,  2,  3,  4},  // 14%
  {0, 0, 2, 3,  4,  4,  5,  6},  // 20%
  {0, 0, 4, 6,  8,  8,  10, 12}, // 40%
  {0, 0, 8, 12, 16, 16, 20, 24}  // 80%
};


// Valors que he vist en altres emuladors, representen dB.
static const int LFO_AM_SHIFT[4]= {
  8, // 0 db
  3, // 1.4 db
  1, // 5.9 db
  0  // 11.8 db
};



/*********/
/* TIPUS */
/*********/

// Operador
typedef struct
{

  bool    keyon;   // Inidica que ja ha sigut activat.
  int32_t out;     // Eixida. El valor estarà entre [-8192,8191]
  int32_t phase;   // Fase actual (20 bits)
  int32_t pg;      // Increment que genera el Phase Generator
  int     keycode; // Key code (Block|Note)
  bool    amon;    // Modulació amplitut habilitada.
  int16_t tlevel;  // Atenuació per defecte.
  struct
  {

    // Eixida del EG. És un valor de 10 bits que representa
    // l'atenuació mesurada en decibels, des de 0db fins a ~96db. La
    // interpretació de cada bit (9 -- 0) segons Nemesis seria:
    //
    //  48   |   24  |   12   |   6     |   3   |  1.5  |
    //  0.75 | 0.375 | 0.1875 | 0.09375 |
    int16_t out;

    // Té la mateixa interpretació que out.
    int16_t sustain;
    
    // Més estat.
    enum {
      EG_ATTACK,
      EG_DECAY,
      EG_SUSTAIN,
      EG_RELEASE
    }    state;
    
    // Timing. El EG s'executa cada 3 mostres.
    int      cc;
    uint32_t counter;
    
    // Rates
    int ar_rate;
    int dr_rate;
    int sr_rate;
    int rr_rate;

    // SSG
    struct
    {
      bool enabled;
      bool attack;
      bool alternate;
      bool hold;
      bool inverted;
    } ssg;
    
  }       eg;  // Envelope generator
  struct
  {
    uint8_t fnum2_block;
    uint8_t fnum1;
    uint8_t det_mul;
    uint8_t ks_ar;
    uint8_t am_dr;
    uint8_t sl_rr;
    uint8_t sr;
    uint8_t ssg_eg;
    uint8_t tl;
  }       regs; // Registres
  
} op_t;

// Canal
typedef struct
{
  
  op_t    slots[4];  // Operadors
  uint8_t pms;       // Phase Modulation Sensivity (LFO), 0..7
  uint8_t ams;       // Amplitude Modulation Sensivity (LFO), 0..2
  uint8_t alg;       // Algoritme.
  uint8_t feedback;  // Tipus de feedback.
  int32_t fb_buf[2]; // Buffer per a feedback
  int32_t out;       // Eixida en 16bits amb signe
  bool    l,r;       // Indica si el canal es mostra o no per els
                     // altaveus corresponents.
  bool    op_freqs;  // Freqüències d'operacions.
  bool    csm_on;    // El timer A pot fer el keyon.
  struct
  {
    uint8_t lr_ams_pms;
    uint8_t fb_alg;
    uint8_t fnum2_block;
    uint8_t fnum1;
  }       regs;     // Registres
  
} channel_t;




/*********/
/* ESTAT */
/*********/

// Callbacks.
static MD_Warning *_warning;
static void *_udata;

// LFO
//
// Freqüència:
//
//  0                    ...                  7
// 3.98  5.56  6.02  6.37  6.88  9.63  48.1  72.2
//
// S'emula com un comptador de 7bits (128 pasos). Cada 128 pasos és un
// cicle de l'oscil·lador.
static struct
{
  bool    enabled;
  int     freq;
  uint8_t counter;
  int     cc; // Cicles que falten per a incrementar el comptador
} _lfo;

// Canals
static channel_t _chns[6];

// Registres globals
static struct
{
  uint8_t lfo_freq;
  uint8_t timers_ch3mode;
} _regs;

// DAC.
static struct
{
  int32_t out;
  bool    enabled;
  struct
  {
    uint8_t dac;
    uint8_t dac_enabled;
  }       regs;
} _dac;

// Timers
static struct
{
  
  // Timer A
  uint16_t a_counter;
  uint16_t a_val;
  bool     a_enabled;
  bool     a_set_flag_enabled;

  // Timer B
  uint8_t b_counter;
  uint8_t b_val;
  bool    b_enabled;
  bool    b_set_flag_enabled;

  // Status
  uint8_t status;
  
} _timers;

// Timing.
static struct
{
  int fm_cc;
  int timerA_cc;
  int timerB_cc;
} _timing;

// Current address.
static struct
{
  uint8_t addr;
  bool    ispart1;
} _current_addr;




/*********************/
/* FUNCIONS PRIVADES */
/*********************/


// OPERADOR ////////////////////////////////////////////////////////////////////

// A partir del valor de rate inicial (R) i el keycode, torna el rate final.
static int
op_calc_eg_rate (
                 const op_t *op,
                 const int   R
                 )
{
  
  int Rks,ks,ret;


  if ( R == 0 ) ret= 0;
  else
    {
      ks= (int) ((op->regs.ks_ar>>6)&0x3);
      Rks= op->keycode>>(3-ks);
      ret= 2*R + Rks;
      if ( ret > 63 ) ret= 63;
    }
  
  return ret;
  
} // end op_calc_eg_rate


static void
op_update_eg_ar_rate (
                      op_t *op
                      )
{
  op->eg.ar_rate= op_calc_eg_rate ( op, (int) (op->regs.ks_ar&0x1F) );
} // end op_update_eg_ar_rate


static void
op_update_eg_dr_rate (
                      op_t *op
                      )
{
  op->eg.dr_rate= op_calc_eg_rate ( op, (int) (op->regs.am_dr&0x1F) );
} // end op_update_eg_dr_rate


static void
op_update_eg_sr_rate (
                      op_t *op
                      )
{
  op->eg.sr_rate= op_calc_eg_rate ( op, (int) (op->regs.sr&0x1F) );
} // end op_update_eg_sr_rate


static void
op_update_eg_rr_rate (
                      op_t *op
                      )
{

  int R;


  R= (int) (op->regs.sl_rr&0xF);
  op->eg.rr_rate= op_calc_eg_rate ( op, 2*R + 1 );
  
} // end op_update_eg_rr_rate


static void
op_update_eg_all_rates (
                        op_t *op
                        )
{

  op_update_eg_ar_rate ( op );
  op_update_eg_dr_rate ( op );
  op_update_eg_sr_rate ( op );
  op_update_eg_rr_rate ( op );
  
} // end op_update_eg_all_rates


static void
op_update_pg_keycode (
                      const channel_t *chn,
                      op_t            *op,
                      const bool       init
                      )
{

  int block,f11,f10,f9,f8,keycode,detune,mult;
  int16_t pm_inc;
  uint16_t fnum;
  int32_t pg;
  uint8_t pm_counter,pm_quarter;
  
  
  // Extrau fnum i block
  if ( chn->op_freqs )
    {
      fnum=
        ((uint16_t) op->regs.fnum1) |
        ((((uint16_t) op->regs.fnum2_block)&0x7)<<8);
      block= (op->regs.fnum2_block>>3)&0x7;
    }
  else
    {
      fnum=
        ((uint16_t) chn->regs.fnum1) |
        ((((uint16_t) chn->regs.fnum2_block)&0x7)<<8);
      block= (chn->regs.fnum2_block>>3)&0x7;
    }
  
  // Aplica LFO phase modulation a fnum.
  if ( !init && _lfo.enabled )
    {
      // El comptador LFO té 7 bits (128 pasos), però per a modular
      // sols tenen pes els 5 superiors. Recordem que els 128 pasos
      // simulen una volta completa.
      pm_counter= (_lfo.counter&0x7F)>>2;
      if ( pm_counter != 0 && chn->pms != 0 )
        {

          // Obté quart
          pm_quarter= pm_counter&0x7;
          if ( pm_counter&0x8 )
            pm_quarter= (~pm_quarter)&0x7;

          // Calcula increment.
          //
          // Aparentment és (fnum*inc)/512. No tinc clar el 512, però
          // bàsicament és com que hi han 9 bits del resultat que són
          // decimals i es descarten.
          //
          // Els 9 bits crec que són 5 per l'amplitut de l'ona que
          // multiplica (és a dir normalitzat entre 0 i 1), i la resta
          // pareix que es divideix per 16 (Pot ser està relacionat
          // amb el block???)
          pm_inc= ((int16_t) fnum)*LFO_PM_INC[chn->pms][pm_quarter];
          if ( pm_counter&0x10 )
            pm_inc= -pm_inc;
          pm_inc>>= 9;
          
          // Aplica
          fnum= (fnum + (uint16_t) pm_inc)&0x7FF;
          
        }
      
    }
  
  // Calcula keycode
  f11= (fnum>>10)&0x1;
  f10= (fnum>>9)&0x1;
  f9= (fnum>>8)&0x1;
  f8= (fnum>>7)&0x1;
  keycode=
    (block<<2) |
    (f11<<1) |
    ((f11&(f10|f9|f8)) | ((~f11)&f10&f9&f8) );
  op->keycode= keycode;

  // El Keycode afecta als rates del EG.
  op_update_eg_all_rates ( op );
                          
  // Calcula pg base
  pg= (int32_t) ((uint32_t) fnum);
  if ( block == 0 )     pg>>= 1;
  else if ( block > 1 ) pg<<= block-1;

  // Aplica detune
  detune= (op->regs.det_mul>>4)&0x7;
  pg+= DETUNE_TABLE[keycode][detune];
  pg&= 0x1FFFF; // És un número de 17bits en aquest punt

  // Aplica multiple
  mult= op->regs.det_mul&0xF;
  if ( mult == 0 )     pg/= 2;
  else if ( mult > 1 )
    {
      pg*= mult;
      pg&= 0xFFFFF; // Pot desbordar un bit.
    }
  
  // Asigna nou valor
  op->pg= pg;
  
} // end op_update_pg_keycode


static void
op_set_fnum1 (
              const channel_t *chn,
              op_t            *op,
              const uint8_t    val,
              const bool       init
              )
{

  op->regs.fnum1= val;
  if ( chn->op_freqs )
    op_update_pg_keycode ( chn, op, init );
  
} // end op_set_fnum1


static void
op_set_fnum2_block (
                    const channel_t *chn,
                    op_t            *op,
                    const uint8_t    val,
                    const bool       init
                    )
{

  op->regs.fnum2_block= val;
  if ( chn->op_freqs )
    op_update_pg_keycode ( chn, op, init );
  
} // end op_set_fnum2_block


static void
op_set_det_mul (
                const channel_t *chn,
                op_t            *op,
                const uint8_t    val,
                const bool       init
                )
{

  op->regs.det_mul= val;
  op_update_pg_keycode ( chn, op, init );
  
} // end op_set_det_mul


static void
op_set_ks_ar (
              op_t          *op,
              const uint8_t  val
              )
{

  op->regs.ks_ar= val;
  op_update_eg_all_rates ( op );
  
} // end op_set_ks_ar


static void
op_set_am_dr (
              op_t          *op,
              const uint8_t  val
              )
{

  op->regs.am_dr= val;
  op_update_eg_dr_rate ( op );
  op->amon= (val&0x80)!=0;
  
} // end op_set_am_dr


static void
op_set_ssg_eg (
               op_t          *op,
               const uint8_t  val
               )
{

  op->regs.ssg_eg= val;
  op->eg.ssg.enabled= (val&0x08)!=0;
  op->eg.ssg.attack= (val&0x04)!=0;
  op->eg.ssg.alternate= (val&0x02)!=0;
  op->eg.ssg.hold= (val&0x01)!=0;
  
} // end op_set_ssg_eg


static void
op_set_tl (
           op_t          *op,
           const uint8_t  val
           )
{

  op->regs.tl= val;
  op->tlevel= ((int16_t) (val&0x7F))<<3;
  
} // end op_set_tl


static void
op_set_sl_rr (
              op_t          *op,
              const uint8_t  val
              )
{

  op->regs.sl_rr= val;
  op_update_eg_rr_rate ( op );
  op->eg.sustain= (int16_t) (val>>4);
  if ( op->eg.sustain == 0xF )
    op->eg.sustain|= 0x10;
  op->eg.sustain<<= 5;
  
} // end op_set_sl_rr


static void
op_set_sr (
           op_t          *op,
           const uint8_t  val
           )
{

  op->regs.sr= val;
  op_update_eg_sr_rate ( op );
  
} // end op_set_sr


static void
op_eg_clock (
             op_t *op
             )
{

  int rate,counter_shift_value,update_cycle,inc;


  // Clock i comprova que no estiga parat.
  ++op->eg.counter;
  if ( op->eg.state == EG_RELEASE && op->eg.out == EG_MAX_ATTENUATION )
    return;
  
  // Selecciona rate (Vaig a fer-ho com una cadena)
  rate= -1;
  if ( op->eg.state == EG_ATTACK )
    {
      if ( op->eg.out == 0 )
        op->eg.state= EG_DECAY;
      else rate= op->eg.ar_rate;
    }
  if ( op->eg.state == EG_DECAY )
    {
      if ( op->eg.out >= op->eg.sustain )
        op->eg.state= EG_SUSTAIN;
      else rate= op->eg.dr_rate;
    }
  if ( op->eg.state == EG_SUSTAIN ) // O és sustain o release o res
    rate= op->eg.sr_rate;
  else if ( op->eg.state == EG_RELEASE )
    rate= op->eg.rr_rate;
  assert ( rate != -1 );
  
  // Actualitza
  counter_shift_value= EG_COUNTER_SHIFT[rate];
  if ( op->eg.counter%(1<<counter_shift_value) == 0 )
    {

      // Calcula increment
      update_cycle= (op->eg.counter>>counter_shift_value)&0x7;
      inc= EG_ATTENUATION_INCREMENT[rate][update_cycle];

      // Actualitza
      if ( op->eg.state == EG_ATTACK )
        {
          if ( rate < 62 )
            {
              op->eg.out+= ((int16_t) ((~op->eg.out)*inc))>>4;
              if ( op->eg.out < 0 ) op->eg.out= 0;
            }
        }
      else
        {
          if ( op->eg.ssg.enabled )
            {
              if ( op->eg.out < 0x200 )
                op->eg.out+= 4*inc;
            }
          else op->eg.out+= inc;
          if ( op->eg.out > EG_MAX_ATTENUATION )
            op->eg.out= EG_MAX_ATTENUATION;
        }
      
    }
  
} // end op_eg_clock


static void
op_eg_ssg_pre_clock (
                     op_t *op
                     )
{

  if ( op->eg.ssg.enabled && op->eg.out >= 0x200 )
    {

      // Canvia sentit inversió
      if ( op->eg.ssg.alternate && (!op->eg.ssg.hold || !op->eg.ssg.inverted) )
        op->eg.ssg.inverted= !op->eg.ssg.inverted;

      // Cal reiniciar la fase quan alternate i hold estan desactivat.
      if ( !op->eg.ssg.alternate && !op->eg.ssg.hold )
        op->phase= 0;

      // Canvis de fase
      if ( op->eg.state != EG_ATTACK )
        {
          if ( op->eg.state != EG_RELEASE && !op->eg.ssg.hold )
            op->eg.state= EG_ATTACK;
          else if ( op->eg.state == EG_RELEASE ||
                    !(op->eg.ssg.inverted ^ op->eg.ssg.attack) )
            op->eg.out= EG_MAX_ATTENUATION;
        }
      
    }
  
} // end op_eg_ssg_pre_clock


static int16_t
op_eg_ssg_get_attenuation (
                           op_t *op
                           )
{

  int16_t ret;
  
  
  if ( op->eg.ssg.enabled &&
       (op->eg.state != EG_RELEASE) &&
       (op->eg.ssg.inverted ^ op->eg.ssg.attack) )
    {
      ret= 0x200 - op->eg.out;
      ret&= 0x3FF;
    }
  else ret= op->eg.out;

  return ret;
  
} // end op_eg_ssg_get_attenuation


static void
op_eg_keyon (
             op_t *op
             )
{
  
  if ( !op->keyon )
    {
      op->eg.out= op->eg.ar_rate >= 62 ? 0 : EG_MAX_ATTENUATION;
      op->eg.state= EG_ATTACK;
      op->eg.ssg.inverted= false;
      op->phase= 0;
      op->keyon= true;
    }
  
} // end op_eg_keyon


static void
op_eg_keyoff (
              op_t *op
              )
{

  if ( op->keyon )
    {
      op->eg.state= EG_RELEASE;
      // NOTA!!! Açò no ho acabe d'entendre. Ni tan sols sé si he seguit
      // bé les instruccions de Nemesis en aquest punt.
      if ( op->eg.ssg.enabled && (op->eg.ssg.inverted ^ op->eg.ssg.attack) )
        op->eg.out= 0x200 - op->eg.out;
      op->keyon= false;
    }
  
} // end op_eg_keyoff


static void
op_clock (
          const channel_t *chn,
          op_t            *op,
          const int16_t    ph_mod // 10bit
          )
{

  int16_t att,phase,out_att,out,am_att;
  uint8_t sin_ind,w,f;
  
  
  // Incrementa phase i modula
  op->phase= (op->phase + op->pg)&0xFFFFF; // Comptador intern
  phase= (op->phase>>10); // Descarta precisió inferior
  phase= (phase + ph_mod)&0x3FF; // Suma 10bits
  
  // Calcula atenuació EG/SSG.
  op_eg_ssg_pre_clock ( op );
  if ( ++op->eg.cc == 3 )
    {
      op_eg_clock ( op );
      op->eg.cc= 0;
    }
  att= op_eg_ssg_get_attenuation ( op );
  
  // Aplica total level
  att+= op->tlevel;
  
  // Aplica Amplitude Modulation
  if ( _lfo.enabled && op->amon )
    {
      // NOTA!!! El LFO és un contador de 7bits (128 pasos) el bit
      // superior és el signe. Però en l'atenuació interpretem el
      // signe al revés.
      am_att= (int16_t) (_lfo.counter&0x3F);
      if ( (_lfo.counter&0x40) == 0 )
        am_att= (~am_att)&0x3F;
      am_att= (am_att<<1) >> (LFO_AM_SHIFT[chn->ams]);
      att+= am_att;
    }
  
  // Comprova que l'atenuació no supera el màxim
  if ( att > EG_MAX_ATTENUATION )
    att= EG_MAX_ATTENUATION;
  
  // Obté atenuació (Exida SIN + att)
  // NOTA!! out_att és 13 bits en format 5.8
  sin_ind= (uint8_t) (phase&0xFF);
  if ( phase&0x100 ) sin_ind= ~sin_ind;
  out_att= SIN_TABLE[sin_ind] + (att<<2);
  
  // Db a lineal
  // out és un valor de 13bits
  w= (uint8_t) (out_att>>8);
  f= (uint8_t) (out_att&0xFF);
  out= (POW_TABLE[f]<<2)>>w;
  
  // Aplica signe (Complement a 2)
  // out és un valor de 14bits amb signe
  op->out= (int32_t) out;
  if ( phase&0x200 && out != 0 )
    op->out= -op->out;
  
} // end op_clock


static void
op_init (
         const channel_t *chn,
         op_t            *op,
         const bool       init
         )
{

  op->keyon= false;
  op->out= 0;
  op->phase= 0;
  op->eg.out= EG_MAX_ATTENUATION;
  op->eg.cc= 0;
  op->eg.state= EG_RELEASE;
  op->eg.counter= 0;
  op->eg.ssg.inverted= false;
  op_set_fnum2_block ( chn, op, 0x00, init );
  op_set_fnum1 ( chn, op, 0x00, init );
  op_set_det_mul ( chn, op, 0x00, init );
  op_set_sl_rr ( op, 0x00 );
  op_set_ssg_eg ( op, 0x00 );
  op_set_am_dr ( op, 0x00 );
  op_set_ks_ar ( op, 0x00 );
  op_set_tl ( op, 0xFF );
  op_set_sr ( op, 0x00 );
  
} // end op_init


// CANAL ///////////////////////////////////////////////////////////////////////
static void
channel_lfo_changed (
                     channel_t *chn
                     )
{

  int i;
  
  
  for ( i= 0; i < 4; ++i )
    op_update_pg_keycode ( chn, &(chn->slots[i]), false );
  
} // end channel_lfo_changed


static void
channel_set_lr_ams_pms (
                        channel_t     *chn,
                        const uint8_t  val,
                        bool           init
                        )
{

  chn->regs.lr_ams_pms= val;
  chn->pms= val&0x7;
  chn->ams= (val>>4)&0x3;
  chn->l= (val&0x80)!=0;
  chn->r= (val&0x40)!=0;
  if ( !init && _lfo.enabled )
    channel_lfo_changed ( chn );
  
} // end channel_set_lr_ams_pms


static void
channel_set_fb_alg (
                    channel_t     *chn,
                    const uint8_t  val
                    )
{

  chn->regs.fb_alg= val;
  chn->alg= val&0x7;
  chn->feedback= (val>>3)&0x7;
  
} // end channel_set_lr_ams_pms


static void
channel_set_fnum1 (
                   channel_t     *chn,
                   const uint8_t  val,
                   const bool     init
                   )
{

  int i;

  
  chn->regs.fnum1= val;
  if ( !chn->op_freqs )
    for ( i= 0; i < 4; ++i )
      op_update_pg_keycode ( chn, &(chn->slots[i]), init );
  
} // end channel_set_fnum1


static void
channel_set_fnum2_block (
                         channel_t     *chn,
                         const uint8_t  val,
                         const bool     init
                         )
{

  int i;

  
  chn->regs.fnum2_block= val;
  if ( !chn->op_freqs )
    for ( i= 0; i < 4; ++i )
      op_update_pg_keycode ( chn, &(chn->slots[i]), init );
  
} // end channel_set_fnum2_block


static int16_t
channel_calc_feedback (
                       channel_t *chn
                       )
{

  int16_t ret;
  int32_t tmp;
  
  
  if ( chn->feedback != 0 )
    {
      tmp= chn->fb_buf[0] + chn->fb_buf[1];
      ret= (tmp>>(10-chn->feedback))&0x3FF;
    }
  else ret= 0;
  
  return ret;
  
} // end channel_calc_feedback


static void
channel_clock_alg0 (
                    channel_t *chn
                    )
{
  
  //
  // Four serial connection mode
  //
  
  // S1
  op_clock ( chn, &(chn->slots[SLOT1]), channel_calc_feedback ( chn ) );
  chn->fb_buf[0]= chn->fb_buf[1];
  chn->fb_buf[1]= chn->slots[SLOT1].out;
  
  // S2
  op_clock ( chn, &(chn->slots[SLOT2]), OUT2PHASEMOD(chn->slots[SLOT1].out) );
  
  // S3
  op_clock ( chn, &(chn->slots[SLOT3]), OUT2PHASEMOD(chn->slots[SLOT2].out) );
  
  // S4
  op_clock ( chn, &(chn->slots[SLOT4]), OUT2PHASEMOD(chn->slots[SLOT3].out) );
  chn->out= chn->slots[SLOT4].out;
  
} // end channel_clock_alg0


static void
channel_clock_alg1 (
                    channel_t *chn
                    )
{

  int32_t s1_s2;

  
  //
  // Three double modulation serial connection mode
  //
  
  // S1
  op_clock ( chn, &(chn->slots[SLOT1]), channel_calc_feedback ( chn ) );
  chn->fb_buf[0]= chn->fb_buf[1];
  chn->fb_buf[1]= chn->slots[SLOT1].out;
  
  // S2
  op_clock ( chn, &(chn->slots[SLOT2]), 0 );
  
  // S3
  s1_s2= chn->slots[SLOT1].out + chn->slots[SLOT2].out;
  op_clock ( chn, &(chn->slots[SLOT3]), OUT2PHASEMOD(s1_s2) );
  
  // S4
  op_clock ( chn, &(chn->slots[SLOT4]), OUT2PHASEMOD(chn->slots[SLOT3].out) );
  chn->out= chn->slots[SLOT4].out;
  
} // end channel_clock_alg1


static void
channel_clock_alg2 (
                    channel_t *chn
                    )
{

  int32_t s1_s3;

  
  //
  // Double modulation mode 1
  //
  
  // S1
  op_clock ( chn, &(chn->slots[SLOT1]), channel_calc_feedback ( chn ) );
  chn->fb_buf[0]= chn->fb_buf[1];
  chn->fb_buf[1]= chn->slots[SLOT1].out;
  
  // S2
  op_clock ( chn, &(chn->slots[SLOT2]), 0 );

  // S3
  op_clock ( chn, &(chn->slots[SLOT3]), OUT2PHASEMOD(chn->slots[SLOT2].out) );
  
  // S4
  s1_s3= chn->slots[SLOT1].out + chn->slots[SLOT3].out;
  op_clock ( chn, &(chn->slots[SLOT4]), OUT2PHASEMOD(s1_s3) );
  chn->out= chn->slots[SLOT4].out;
  
} // end channel_clock_alg2


static void
channel_clock_alg3 (
                    channel_t *chn
                    )
{

  int32_t s2_s3;

  
  //
  // Double modulation mode 2
  //
  
  // S1
  op_clock ( chn, &(chn->slots[SLOT1]), channel_calc_feedback ( chn ) );
  chn->fb_buf[0]= chn->fb_buf[1];
  chn->fb_buf[1]= chn->slots[SLOT1].out;
  
  // S2
  op_clock ( chn, &(chn->slots[SLOT2]), OUT2PHASEMOD(chn->slots[SLOT1].out) );

  // S3
  op_clock ( chn, &(chn->slots[SLOT3]), 0 );
  
  // S4
  s2_s3= chn->slots[SLOT2].out + chn->slots[SLOT3].out;
  op_clock ( chn, &(chn->slots[SLOT4]), OUT2PHASEMOD(s2_s3) );
  chn->out= chn->slots[SLOT4].out;
  
} // end channel_clock_alg3


static void
channel_clock_alg4 (
                    channel_t *chn
                    )
{
  
  //
  // Two serial connection and two parallel modes
  //
  
  // S1
  op_clock ( chn, &(chn->slots[SLOT1]), channel_calc_feedback ( chn ) );
  chn->fb_buf[0]= chn->fb_buf[1];
  chn->fb_buf[1]= chn->slots[SLOT1].out;
  
  // S2
  op_clock ( chn, &(chn->slots[SLOT2]), OUT2PHASEMOD(chn->slots[SLOT1].out) );
  
  // S3
  op_clock ( chn, &(chn->slots[SLOT3]), 0 );
  
  // S4
  op_clock ( chn, &(chn->slots[SLOT4]), OUT2PHASEMOD(chn->slots[SLOT3].out) );

  // Eixida
  chn->out= chn->slots[SLOT2].out + chn->slots[SLOT4].out;
  
} // end channel_clock_alg4


static void
channel_clock_alg5 (
                    channel_t *chn
                    )
{
  
  //
  // Common modulation 3 parallel mode
  //
  
  // S1
  op_clock ( chn, &(chn->slots[SLOT1]), channel_calc_feedback ( chn ) );
  chn->fb_buf[0]= chn->fb_buf[1];
  chn->fb_buf[1]= chn->slots[SLOT1].out;
  
  // S2
  op_clock ( chn, &(chn->slots[SLOT2]), OUT2PHASEMOD(chn->slots[SLOT1].out) );
  
  // S3
  op_clock ( chn, &(chn->slots[SLOT3]), OUT2PHASEMOD(chn->slots[SLOT1].out) );
  
  // S4
  op_clock ( chn, &(chn->slots[SLOT4]), OUT2PHASEMOD(chn->slots[SLOT1].out) );

  // Eixida
  chn->out= 
    chn->slots[SLOT2].out +
    chn->slots[SLOT3].out +
    chn->slots[SLOT4].out;
  
} // end channel_clock_alg5


static void
channel_clock_alg6 (
                    channel_t *chn
                    )
{
  
  //
  // Two serial connection + two sine mode
  //
  
  // S1
  op_clock ( chn, &(chn->slots[SLOT1]), channel_calc_feedback ( chn ) );
  chn->fb_buf[0]= chn->fb_buf[1];
  chn->fb_buf[1]= chn->slots[SLOT1].out;
  
  // S2
  op_clock ( chn, &(chn->slots[SLOT2]), OUT2PHASEMOD(chn->slots[SLOT1].out) );
  
  // S3
  op_clock ( chn, &(chn->slots[SLOT3]), 0 );
  
  // S4
  op_clock ( chn, &(chn->slots[SLOT4]), 0 );

  // Eixida
  chn->out=
    chn->slots[SLOT2].out +
    chn->slots[SLOT3].out +
    chn->slots[SLOT4].out;
  
} // end channel_clock_alg6


static void
channel_clock_alg7 (
                    channel_t *chn
                    )
{
  
  //
  // Four parallel sine synthesis mode
  //
  
  // S1
  op_clock ( chn, &(chn->slots[SLOT1]), channel_calc_feedback ( chn ) );
  chn->fb_buf[0]= chn->fb_buf[1];
  chn->fb_buf[1]= chn->slots[SLOT1].out;
  
  // S2
  op_clock ( chn, &(chn->slots[SLOT2]), 0 );
  
  // S3
  op_clock ( chn, &(chn->slots[SLOT3]), 0 );
  
  // S4
  op_clock ( chn, &(chn->slots[SLOT4]), 0 );
  
  // Eixida
  chn->out=
    chn->slots[SLOT1].out +
    chn->slots[SLOT2].out +
    chn->slots[SLOT3].out +
    chn->slots[SLOT4].out;
  
} // end channel_clock_alg7


static void
channel_clock (
               channel_t *chn
               )
{
  
  switch ( chn->alg )
    {
    case 0: channel_clock_alg0 ( chn ); break;
    case 1: channel_clock_alg1 ( chn ); break;
    case 2: channel_clock_alg2 ( chn ); break;
    case 3: channel_clock_alg3 ( chn ); break;
    case 4: channel_clock_alg4 ( chn ); break;
    case 5: channel_clock_alg5 ( chn ); break;
    case 6: channel_clock_alg6 ( chn ); break;
    case 7: channel_clock_alg7 ( chn ); break;
    }
  
} // end channel_clock


static void
channel_init (
              channel_t  *chn,
              const bool  init
              )
{

  int i;

  
  for ( i= 0; i < 4; ++i )
    op_init ( chn, &(chn->slots[i]), init );
  chn->out= 0;
  chn->fb_buf[0]= 0;
  chn->fb_buf[1]= 0;
  chn->op_freqs= false;
  chn->csm_on= false;
  channel_set_fnum1 ( chn, 0x00, init );
  channel_set_fnum2_block ( chn, 0x00, init );
  channel_set_fb_alg ( chn, 0x00 );
  channel_set_lr_ams_pms ( chn, 0xC0, init );
  
} // end channel_init


// LFO /////////////////////////////////////////////////////////////////////////
static void
set_lfo_freq (
              const uint8_t val,
              const bool    init
              )
{

  bool old_enabled;
  int i;
  

  old_enabled= _lfo.enabled;
  _regs.lfo_freq= val;
  _lfo.enabled= (val&0x8)!=0;
  _lfo.freq= val&0x7;
  if ( old_enabled != _lfo.enabled && _lfo.enabled && !init )
    {
      for ( i= 0; i < 6; ++i )
        channel_lfo_changed ( &(_chns[i]) );
      _lfo.counter= 0x00;
      _lfo.cc= LFO_FREQ2CC[_lfo.freq];
    }
  
} // end set_lfo_freq


static void
lfo_clock (void)
{

  int i;

  
  if ( _lfo.enabled && --_lfo.cc == 0 )
    {
      ++_lfo.counter;
      _lfo.cc= LFO_FREQ2CC[_lfo.freq];
      for ( i= 0; i < 6; ++i )
        channel_lfo_changed ( &(_chns[i]) );
    }
  
} // end lfo_clock


static void
lfo_init (
          const bool init
          )
{

  _lfo.enabled= false;
  _lfo.cc= 0;
  _lfo.counter= 0;
  set_lfo_freq ( 0x00, init );
  
} // end lfo_init


// DAC /////////////////////////////////////////////////////////////////////////

static void
dac_set_dac(
            const uint8_t val
            )
{
  
  _dac.regs.dac= val;
  // Passe d'unsigned 8bit i després multiplica per 64 (passa a
  // 14bit), però representat amb 32bits.
  _dac.out= (((int32_t) ((uint32_t) val)) - 0x80)*64;
  
} // end dac_set_dac


static void
dac_set_dac_enabled (
                     const uint8_t val
                     )
{

  _dac.regs.dac_enabled= val;
  _dac.enabled= (val!=0x00);
  
} // end dac_set_dac_enabled


static void
dac_init (void)
{

  dac_set_dac ( 0x00 );
  dac_set_dac_enabled ( 0x00 );
  
} // end dac_init


// TIMERS //////////////////////////////////////////////////////////////////////

static void
set_timers_ch3mode (
                    const uint8_t val,
                    const bool    init
                    )
{

  int i;
  
  
  _regs.timers_ch3mode= val;
  
  // CHN3 mode
  if ( (val>>6) == 0 )
    {
      _chns[2].op_freqs= false;
      for ( i= 0; i < 4; ++i )
        op_update_pg_keycode ( &(_chns[2]), &(_chns[2].slots[i]), init );
    }
  else
    {
      _chns[2].op_freqs= true;
      _chns[2].csm_on= (val&0x80)!=0x0;
      for ( i= 0; i < 4; ++i )
        op_update_pg_keycode ( &(_chns[2]), &(_chns[2].slots[i]), init );
    }

  // Timer A
  // --> Load
  if ( (val&0x01) != 0 )
    {
      if ( !_timers.a_enabled )
        {
          _timers.a_counter= _timers.a_val;
          _timers.a_enabled= true;
        }
    }
  else _timers.a_enabled= false;
  // --> Enable
  _timers.a_set_flag_enabled= ((val&0x4) != 0);
  // --> Reset
  if ( (val&0x10) != 0 )
    _timers.status&= ~0x01;

  // Timer B
  // --> Load
  if ( (val&0x02) != 0 )
    {
      if ( !_timers.b_enabled )
        {
          _timers.b_counter= _timers.b_val;
          _timers.b_enabled= true;
        }
    }
  else _timers.b_enabled= false;
  // --> Enable
  _timers.b_set_flag_enabled= ((val&0x8) != 0);
  // --> Reset
  if ( (val&0x20) != 0 )
    _timers.status&= ~0x02;
  
} // end set_timers_ch3mode


static void
timers_set_timera_high (
                        const uint8_t val
                        )
{
  _timers.a_val= (_timers.a_val&0x003) | (((uint16_t) val)<<2);
} // end timers_set_timera_high


static void
timers_set_timera_low (
                       const uint8_t val
                       )
{
  _timers.a_val= (_timers.a_val&0x3FC) | ((uint16_t) (val&0x3));
} // end timers_set_timera_low


static void
timers_set_timerb (
                   const uint8_t val
                   )
{
  _timers.b_val= val;
} // end timers_set_timerb


static void
timers_timera_clock (void)
{

  int i;

  
  if ( _timers.a_enabled )
    {
      _timers.a_counter= (_timers.a_counter+1)&0x3FF;
      if ( _timers.a_counter == 0 )
        {
          _timers.a_counter= _timers.a_val;
          if ( _timers.a_set_flag_enabled )
            {
              _timers.status|= 0x01;
              if ( _chns[2].csm_on )
                for ( i= 0; i < 4; ++i )
                  op_eg_keyon ( &(_chns[2].slots[i]) );
            }
        }
    }
  
} // end timers_timera_clock


static void
timers_timerb_clock (void)
{
  
  if ( _timers.b_enabled )
    {
      ++_timers.b_counter;
      if ( _timers.b_counter == 0x00 )
        {
          _timers.b_counter= _timers.b_val;
          if ( _timers.b_set_flag_enabled )
            _timers.status|= 0x02;
        }
    }
  
} // end timers_timerb_clock


static void
timers_init (
             const bool init
             )
{
  
  _timers.a_counter= 0;
  _timers.a_val= 0;
  _timers.a_enabled= false;
  _timers.a_set_flag_enabled= false;
  _timers.b_counter= 0;
  _timers.b_val= 0;
  _timers.b_enabled= false;
  _timers.b_set_flag_enabled= false;
  _timers.status= 0x00;
  set_timers_ch3mode ( 0x00, init );
  
} // end timers_init


// FUNCIONS PRIVADES ///////////////////////////////////////////////////////////

// Executa un cicle FM.
static void
run_fm_cycle (void)
{

  int i;
  int32_t l,r,val;
  channel_t *chn;
  
  
  // Calcula mostres L i R
  l= r= 0;
  lfo_clock ();
  for ( i= 0; i < 5; ++i )
    {
      chn= &(_chns[i]);
      channel_clock ( chn );
      val= 4*chn->out;
      if      ( val > 32767 )  val= 32767;
      else if ( val < -32768 ) val= -32768;
      if ( chn->l ) l+= val;
      if ( chn->r ) r+= val;
    }
  chn= &(_chns[i]);
  channel_clock ( chn );
  val= 4*(_dac.enabled ? _dac.out : chn->out);
  if      ( val > 32767 )  val= 32767;
  else if ( val < -32768 ) val= -32768;
  if ( chn->l ) l+= val;
  if ( chn->r ) r+= val;
  l/= 6;
  r/= 6;
  
  // Envia mostra al mesclador.
  MD_audio_fm_play ( (int16_t) l, (int16_t) r );
  
} // end run_fm_cycle


static void
write_slot_reg (
                channel_t     *chn,
                const int      nop,
                const uint8_t  addr,
                const uint8_t  data
                )
{

  op_t *op;


  op= &(chn->slots[nop]);
  switch ( addr )
    {
    case 0x30: op_set_det_mul ( chn, op, data, false ); break;
    case 0x40: op_set_tl ( op, data ); break;
    case 0x50: op_set_ks_ar ( op, data ); break;
    case 0x60: op_set_am_dr ( op, data ); break;
    case 0x70: op_set_sr ( op, data ); break;
    case 0x80: op_set_sl_rr ( op, data ); break;
    case 0x90: op_set_ssg_eg ( op, data ); break;
    default: break;
    }
  
} // end write_slot_reg


static void
write_channel_reg (
                   const int     nch,
                   const uint8_t addr,
                   const uint8_t data
                   )
{

  channel_t *chn;
  
  
  chn= &(_chns[nch]);
  switch ( addr )
    {
    case 0xa0:
      if ( nch == 2 ) // Cas especial Canal 3
        op_set_fnum1 ( chn, &(_chns[2].slots[SLOT4]), data, false );
      channel_set_fnum1 ( chn, data, false );
      break;
    case 0xa4:
      if ( nch == 2 ) // Cas especial Canal 3
        op_set_fnum2_block ( chn, &(_chns[2].slots[SLOT4]), data, false );
      channel_set_fnum2_block ( chn, data, false );
      break;
    case 0xa8:
      // NOTA!!! nch fa referència a l'operador del canal 3.
      // 0 - SLOT3
      // 1 - SLOT1
      // 2 - SLOT2
      // L'operador 1 està en la posició habitual per al channel 3.
      if ( nch >= 3 ) return; // Inclou valor 3 i part2
      switch ( nch )
        {
        case 0:
          op_set_fnum1 ( &_chns[2], &(_chns[2].slots[SLOT3]), data, false );
          break;
        case 1:
          op_set_fnum1 ( &_chns[2], &(_chns[2].slots[SLOT1]), data, false );
          break;
        case 2:
          op_set_fnum1 ( &_chns[2], &(_chns[2].slots[SLOT2]), data, false );
          break;
        }
      break;
    case 0xac:
      if ( nch >= 3 ) return; // Inclou valor 3 i part2
      switch ( nch )
        {
        case 0:
          op_set_fnum2_block ( chn, &(_chns[2].slots[SLOT3]), data, false );
          break;
        case 1:
          op_set_fnum2_block ( chn, &(_chns[2].slots[SLOT1]), data, false );
          break;
        case 2:
          op_set_fnum2_block ( chn, &(_chns[2].slots[SLOT2]), data, false );
          break;
        }
      break;
    case 0xb0: channel_set_fb_alg ( chn, data ); break;
    case 0xb4: channel_set_lr_ams_pms ( chn, data, false ); break;
    default: break;
    }
  
} // end write_channel_reg


static void
clock (void)
{

  // Timer A abans
  while ( _timing.timerA_cc >= TIMERA_CC )
    {
      timers_timera_clock ();
      _timing.timerA_cc-= TIMERA_CC;
    }

  // Timer B abans
  while ( _timing.timerB_cc >= TIMERB_CC )
    {
      timers_timerb_clock ();
      _timing.timerB_cc-= TIMERB_CC;
    }
  
  // FM
  while ( _timing.fm_cc >= 144 )
    {
      run_fm_cycle ();
      _timing.fm_cc-= 144;
    }
  
} // end clock




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
MD_fm_clock (
             const int cc
             )
{

  _timing.fm_cc+= cc;
  _timing.timerA_cc+= cc;
  _timing.timerB_cc+= cc;
  clock ();
  
} // end MD_fm_clock


void
MD_fm_init (
            MD_Warning *warning,
            void       *udata
            )
{

  _warning= warning;
  _udata= udata;
  MD_fm_init_state ();
  
} // end MD_fm_init


void
MD_fm_init_state (void)
{
  
  int i;
  
  
  lfo_init ( true ); // Important abans canals
  dac_init ();
  for ( i= 0; i < 6; ++i )
    channel_init ( &(_chns[i]), true );
  timers_init ( true );
  _timing.fm_cc= 0;
  _timing.timerA_cc= 0;
  _timing.timerB_cc= 0;
  _current_addr.addr= 0x22;
  _current_addr.ispart1= true;
  
} // end MD_fm_init_state


void
MD_fm_part1_set_addr (
                      const MDu8 data
                      )
{

  clock ();

  if ( data < 0x22 || data >= 0xB8 )
    {
      _warning ( _udata, "FM: adreça de part1 fora de rang: %02X", data );
      //return; AVISA PERÒ NO IMPEDEIX
    }
  _current_addr.addr= (uint8_t) data;
  _current_addr.ispart1= true;
  
} // end MD_fm_part1_set_addr


void
MD_fm_part1_write_data (
                        const MDu8 data
                        )
{
  
  int c;
  
  
  clock ();
  
  if ( !_current_addr.ispart1 ) return;
  
  // Registres globals
  if ( _current_addr.addr < 0x30 )
    {
      switch ( _current_addr.addr )
        {
        case 0x22: set_lfo_freq ( data, false ); break;
        case 0x24: timers_set_timera_high ( data ); break;
        case 0x25: timers_set_timera_low ( data ); break;
        case 0x26: timers_set_timerb ( data ); break;
        case 0x27: set_timers_ch3mode ( data, false ); break;
        case 0x28:
          c= data&0x03;
          if ( c == 3 ) break;
          if( data&0x04 ) c+= 3;
          if ( data&0x10 ) op_eg_keyon  ( &(_chns[c].slots[SLOT1]) );
          else             op_eg_keyoff ( &(_chns[c].slots[SLOT1]) );
          if ( data&0x20 ) op_eg_keyon  ( &(_chns[c].slots[SLOT2]) );
          else             op_eg_keyoff ( &(_chns[c].slots[SLOT2]) );
          if ( data&0x40 ) op_eg_keyon  ( &(_chns[c].slots[SLOT3]) );
          else             op_eg_keyoff ( &(_chns[c].slots[SLOT3]) );
          if ( data&0x80 ) op_eg_keyon  ( &(_chns[c].slots[SLOT4]) );
          else             op_eg_keyoff ( &(_chns[c].slots[SLOT4]) );
          break;
        case 0x2a: dac_set_dac ( data ); break;
        case 0x2b: dac_set_dac_enabled ( data ); break;
        }
    }

  // Registres slots (operadors).
  else if ( _current_addr.addr < 0xa0 )
    {
      if ( (c= _current_addr.addr&0x3) == 3 ) return;
      write_slot_reg ( &_chns[c], (_current_addr.addr>>2)&0x3,
                       _current_addr.addr&0xF0, data );
    }
  
  // Registres canals.
  else if ( _current_addr.addr < 0xb8 )
    {
      if ( (c= _current_addr.addr&0x3) == 3 ) return;
      write_channel_reg ( c, _current_addr.addr&0xFC, data );
    }
  
} // end MD_fm_part1_write_data


void
MD_fm_part2_set_addr (
                      const MDu8 data
                      )
{
  
  clock ();

  if ( data < 0x30 || data >= 0xB8 )
    {
      _warning ( _udata, "FM: adreça de part2 fora de rang: %02X", data );
      //return; AVISA PERÒ NO IMPEDEIX
    }
  _current_addr.addr= (uint8_t) data;
  _current_addr.ispart1= false;
  
} // end MD_fm_part2_set_addr


void
MD_fm_part2_write_data (
                        const MDu8 data
                        )
{
  
  int c;
  
  
  clock ();
  
  if ( _current_addr.ispart1 ) return;
  
  // Registres slots (operadors).
  if ( _current_addr.addr >= 0x30 )
    {
      if ( _current_addr.addr < 0xa0 )
        {
          if ( (c= _current_addr.addr&0x3) == 3 ) return;
          write_slot_reg ( &_chns[c+3], (_current_addr.addr>>2)&0x3,
                           _current_addr.addr&0xF0, data );
        }
      
      // Registres canals.
      else if ( _current_addr.addr < 0xb8 )
        {
          if ( (c= _current_addr.addr&0x3) == 3 ) return;
          write_channel_reg ( c+3, _current_addr.addr&0xFC, data );
        }
    }
  
} // end MD_fm_part2_write_data


void
MD_fm_reset (void)
{

  int i;

  
  clock ();
  
  lfo_init ( false );
  dac_init ();
  for ( i= 0; i < 6; ++i )
    channel_init ( &(_chns[i]), false );
  timers_init ( false );
  _current_addr.addr= 0x22;
  _current_addr.ispart1= true;
  
} // end MD_fm_reset


MDu8
MD_fm_status (void)
{

  MDu8 ret;

  
  clock ();
  ret= (MDu8) _timers.status;
  
  return ret;
  
} // end MD_fm_status


int
MD_fm_save_state (
                  FILE *f
                  )
{

  SAVE ( _lfo );
  SAVE ( _chns );
  SAVE ( _regs );
  SAVE ( _dac );
  SAVE ( _timers );
  SAVE ( _timing );
  SAVE ( _current_addr );
  
  return 0;
  
} // end MD_fm_save_state


int
MD_fm_load_state (
                  FILE *f
                  )
{

  int c,s;

  
  LOAD ( _lfo );
  CHECK ( _lfo.freq >= 0 && _lfo.freq < 8 );
  LOAD ( _chns );
  for ( c= 0; c < 6; ++c )
    {
      CHECK ( _chns[c].pms >= 0 && _chns[c].pms < 8 );
      CHECK ( _chns[c].ams >= 0 && _chns[c].ams < 4 );
      for ( s= 0; s < 4; ++s )
        {
          CHECK ( _chns[c].slots[s].eg.ar_rate >= 0 &&
                  _chns[c].slots[s].eg.ar_rate < 64 );
          CHECK ( _chns[c].slots[s].eg.dr_rate >= 0 &&
                  _chns[c].slots[s].eg.dr_rate < 64 );
          CHECK ( _chns[c].slots[s].eg.sr_rate >= 0 &&
                  _chns[c].slots[s].eg.sr_rate < 64 );
          CHECK ( _chns[c].slots[s].eg.rr_rate >= 0 &&
                  _chns[c].slots[s].eg.rr_rate < 64 );
        }
    }
  LOAD ( _regs );
  LOAD ( _dac );
  LOAD ( _timers );
  LOAD ( _timing );
  LOAD ( _current_addr );
  
  return 0;
  
} // end MD_fm_load_state
