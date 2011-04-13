#!/usr/bin/python

import struct

def header():
    return struct.pack('8B', 137, 80, 78, 71, 13, 10, 26, 10)

def chunk(chunk_type, data, crc=None, length=None):
    if length is None:
        length = len(data)
    if crc is None:
        crc = 1
    return (struct.pack('>L', length) +
            chunk_type +
            data +
            struct.pack('>L', crc))
