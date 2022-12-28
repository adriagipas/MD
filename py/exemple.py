import MD
import sys

if len(sys.argv)!=2:
    sys.exit('%s <ROM>'%sys.argv[0])
rom_fn= sys.argv[1]

MD.init()
with open(rom_fn,'rb') as f:
    MD.set_rom(f.read(),False)
MD.loop()
MD.close()
