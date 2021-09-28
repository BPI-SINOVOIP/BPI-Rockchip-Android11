Step1: Building IGA (Intel Graphics Assembler)
========================================================================

1. Download or clone IGC (Intel Graphics Compiler)

   https://github.com/intel/intel-graphics-compiler.git

2. Chdir into 'intel-graphics-compiler' (or any other workspace folder of choice)

   It should read the following folder strucutre:

   workspace
      |- visa
      |- IGC
      |- inc
      |- 3d
      |- skuwa

3. Chdir into IGA sub-component

   cd visa/iga

4. Create build directory

    mkdir build

5. Change into build directory

    cd build

6. Run cmake

   cmake ../

7. Run make to build IGA project

   make

8. Get the output executable "iga64" in IGAExe folder

   usage: ./iga64 OPTIONS ARGS
   where OPTIONS:
     -h     --help                     shows help on an option
     -d     --disassemble              disassembles the input file
     -a     --assemble                 assembles the input file
     -n     --numeric-labels           use numeric labels
     -p     --platform        DEVICE   specifies the platform (e.g. "GEN9")
     -o     --output          FILE     specifies the output file

   EXAMPLES:
   ./iga64  file.gxa  -p=11 -a  -o file.krn

Step2: Building ASM code
========================================================================
1. Command line to convert asm code to binary:

   iga64 media_vme.gxa -p=11 -a -o media_vme.krn

2. Pad 128 bytes zeros to the kernel:

   dd if=/dev/zero bs=1 count=128 >> media_vme.krn

3. Generate hexdump:

   hexdump -v  -e '4/4 "0x%08x " "\n"' media_vme.krn > media_vme.hex
