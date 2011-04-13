#!/usr/bin/python

import os

import pngforge

def png_invalid_empty_file():
    return ''

def png_invalid_truncated_sig():
    return pngforge.sig()[0:-3]

def png_invalid_sig_only():
    return pngforge.sig()

def png_invalid_chunk_size_overflow():
    """Overflow a signed integer for the chunk size."""
    return pngforge.sig() + pngforge.chunk('BIGC', length=2**31)

def png_invalid_chunk_size_big():
    """Return a chunk that claims to be 1gb."""
    return pngforge.sig() + pngforge.chunk('BIGC', length=2**30)

def png_invalid_bad_crc():
    """Return a chunk with a bad CRC."""
    return pngforge.sig() + pngforge.chunk('ABCD', crc=1)

def png_invalid_short_idat():
    """Create a image with a too-short IDAT."""
    return (pngforge.sig() + pngforge.ihdr(1, 1) +
            pngforge.chunk('IDAT', '') +
            pngforge.iend())

def png_valid_tiny():
    """Create a valid, though tiny, image."""
    return (pngforge.sig() + pngforge.ihdr(width=1, height=1) +
            pngforge.idat(pngforge.scanline(0, '\1\2\3')) +
            pngforge.iend())

if __name__ == '__main__':
    for key, val in globals().items():
        if not key.startswith('png_'):
            continue
        filename = key[len('png_'):] + '.png'
        content = val()
        with open(os.path.join('generated', filename), 'wb') as f:
            f.write(content)
