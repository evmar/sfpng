#!/usr/bin/python

import struct

def header():
    return struct.pack('8B', 137, 80, 78, 71, 13, 10, 26, 10)

def make_crc_table():
    table = []
    for n in range(256):
        c = long(n)
        for k in range(8):
            if c & 1:
                c = 0xedb88320L ^ (c >> 1)
            else:
                c = c >> 1
        table.append(c)
    return table
g_crc_table = make_crc_table()

def compute_crc(data):
    c = 0xFFFFFFFFL
    for b in data:
        c = g_crc_table[(c ^ ord(b)) & 0xFF] ^ (c >> 8)
    return c ^ 0xFFFFFFFFL

def chunk(chunk_type, data, crc=None, length=None):
    if length is None:
        length = len(data)
    body = chunk_type + data
    if crc is None:
        crc = compute_crc(body)
    return struct.pack('>L', length) + body + struct.pack('>L', crc)
