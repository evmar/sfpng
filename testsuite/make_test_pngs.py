#!/usr/bin/python

import os

def png_invalid_empty_file():
    return ''

def generate(filename, func):
    content = func()
    with open(os.path.join('generated', filename), 'wb') as f:
        f.write(content)

if __name__ == '__main__':
    for key, val in globals().items():
        if not key.startswith('png_'):
            continue
        filename = key[len('png_'):] + '.png'
        generate(filename, val)
