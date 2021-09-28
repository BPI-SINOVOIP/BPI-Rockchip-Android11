#!/usr/bin/env python3
import struct
import sys
import argparse

parser=argparse.ArgumentParser(
            description='''Script for converting shaders from binary to hex ''' )
parser = argparse.ArgumentParser(prog='converter.py', usage='%(prog)s binary_file')
parser.add_argument('binary', nargs=1, help='binary_file')
args=parser.parse_args()

print "static const uint32_t kernel[][4] = {"
with open(sys.argv[1], 'r') as f:
    fmt = '<LLLL'
    step = struct.calcsize(fmt)
    while True:
        buf = f.read(step)
        if not buf:
            break
        elif len(buf) < step:
            buf += '\x00' * (step - len(buf))

        val = struct.unpack('<LLLL', buf)
        print "\t{{ 0x{:08x}, 0x{:08x}, 0x{:08x}, 0x{:08x} }},".format(*val)

print "};"
