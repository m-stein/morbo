#!/usr/bin/env python
# -*- Mode: Python -*-

import os
import sys
import array

def fsize(f):
    old = f.tell()
    f.seek(0, os.SEEK_END)
    size = f.tell()
    f.seek(old, os.SEEK_SET)
    return size

infile = open(sys.argv[1], "rb")
outfile = open(sys.argv[2], "wb")

rom = array.array('B')          # unsigned byte
rom.fromfile(infile, fsize(infile))

# Clear checksum in ROM image
rom[6] = 0

# Compute checksum
checksum = 0
for byte in rom:
    checksum = (checksum + byte) & 0xFF

print(checksum)

rom[6] = 256 - checksum

rom.tofile(outfile)


# EOF
