#!/usr/bin/python

import os

import pngforge

def png_invalid_empty_file():
    return ''

def png_invalid_truncated_header():
    return pngforge.header()[0:-3]

def png_invalid_header_only():
    return pngforge.header()

def png_invalid_chunk_big():
    return pngforge.header() + pngforge.chunk('BIGC', '', length=2**31)

if __name__ == '__main__':
    for key, val in globals().items():
        if not key.startswith('png_'):
            continue
        filename = key[len('png_'):] + '.png'
        content = val()
        with open(os.path.join('generated', filename), 'wb') as f:
            f.write(content)
