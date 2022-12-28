from distutils.core import setup, Extension

module= Extension ( 'MD',
                    sources= [ 'mdmodule.c',
                               '../src/main.c',
                               '../src/cpu.c',
                               '../src/cpu_dis.c',
                               '../src/fm.c',
                               '../src/psg.c',
                               '../src/audio.c',
                               '../src/io.c',
                               '../src/mem.c',
                               '../src/rom.c',
                               '../src/unpack.c',
                               '../src/eeprom.c',
                               '../src/svp.c',
                               '../src/vdp.c',
                               '../src/z80.c',
                               'Z80/src/z80.c',
                               'Z80/src/z80_dis.c'],
                    depends= [ '../src/MD.h',
                               '../src/unpack.h',
                               'Z80/src/Z80.h' ],
                    libraries= [ 'SDL' ],
                    define_macros= [('__LITTLE_ENDIAN__',None)],
                    include_dirs= [ '../src', 'Z80/src' ])

setup ( name= 'MD',
        version= '1.0',
        description= 'Mega Drive simulator',
        ext_modules= [module] )