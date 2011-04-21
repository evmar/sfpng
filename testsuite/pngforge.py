#!/usr/bin/python

import struct
import zlib

COLOR_GRAY = 0
COLOR_TRUE = 2
COLOR_INDEXED = 3

def rgb(r, g, b):
    return struct.pack('BBB', r, g, b)

def sig():
    return struct.pack('8B', 137, 80, 78, 71, 13, 10, 26, 10)

def chunk(chunk_type, data='', crc=None, length=None):
    if length is None:
        length = len(data)
    body = chunk_type + data
    if crc is None:
        crc = zlib.crc32(body) & 0xFFFFFFFF
    return struct.pack('>L', length) + body + struct.pack('>L', crc)

def ihdr(width, height, depth=8, color_type=COLOR_TRUE,
         compression=0, filter=0, interlace=0):
    data = struct.pack('>LLBBBBB', width, height, depth, color_type,
                       compression, filter, interlace)
    return chunk('IHDR', data)

def scanline(filter, pixels):
    return struct.pack('B', filter) + pixels

def idat(scanlines):
    return chunk('IDAT', zlib.compress(scanlines))

def iend():
    return chunk('IEND')
