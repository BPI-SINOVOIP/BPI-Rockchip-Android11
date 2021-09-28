#!/usr/bin/python

import struct
import sys

if len(sys.argv) != 4:
    print sys.argv[0] + " ramdisk.img vendor_boot.img ramdisk-qemu.img"
    sys.exit(1)

f1name = sys.argv[1];
f2name = sys.argv[2];
f3name = sys.argv[3];

with open(f1name, mode='rb') as file:
    f1buffer = file.read()

with open(f2name, mode='rb') as file:
    f2buffer = file.read()

header = struct.unpack("QIIQI", f2buffer[:28])
if header[1] != 3:
    print "ERROR: can only combine version 3 vendor_boot.img to ramdisk.img"
    sys.exit(2)

offset = 4096
vendorramimg = f2buffer[offset:offset+header[4]];

print header
with open(f3name, mode='wb') as file:
    file.write(f1buffer)
    file.write(vendorramimg)
