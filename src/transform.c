#include "sfpng.h"

#include "decoder.h"

void sfpng_decoder_transform(sfpng_decoder* decoder, const uint8_t* in,
                             uint8_t* out) {
  const int depth = decoder->bit_depth;
  int in_len_pixels = decoder->width;
  int bit = 8 - depth;

  const int mask = (1 << depth) - 1;

  while (in_len_pixels) {
    int r = 0, g = 0, b = 0, a = 0xFFFF;
    int value = 0;
    if (depth < 8) {
      if (bit < 0) {
        bit = 8 - depth;
        ++in;
      }
      value = (*in >> bit) & mask;
      r = g = b = value * (255 / mask);
      bit -= depth;
    } else if (depth == 8) {
      if (decoder->color_type == SFPNG_COLOR_TRUECOLOR ||
          decoder->color_type == SFPNG_COLOR_TRUECOLOR_ALPHA) {
        r = *in++;
        g = *in++;
        b = *in++;
      } else {
        value = r = g = b = *in++;
      }
      if (decoder->color_type & SFPNG_COLOR_MASK_ALPHA)
        a = *in++;
    } else if (depth == 16) {
      if (decoder->color_type & SFPNG_COLOR_MASK_COLOR) {
        r = in[0] << 8 | in[1]; in += 2;
        g = in[0] << 8 | in[1]; in += 2;
        b = in[0] << 8 | in[1]; in += 2;
      } else {
        value = r = g = b = in[0] << 8 | in[1];
        in += 2;
      }
      if (decoder->color_type & SFPNG_COLOR_MASK_ALPHA) {
        a = in[0] << 8 | in[1];
        in += 2;
      }
    }

    if (decoder->color_type == SFPNG_COLOR_INDEXED) {
      if (value > decoder->palette.entries) {
        /* This is an error by the spec, but we don't have an error
           return path.  Just use 0 values to match libpng. */
        r = g = b = 0;
      } else {
        const uint8_t* palette = decoder->palette.bytes + (3 * value);
        r = palette[0];
        g = palette[1];
        b = palette[2];
      }
    }

    if (decoder->has_trans) {
      if (decoder->color_type == SFPNG_COLOR_INDEXED) {
        int i;
        for (i = 0; i < decoder->trans.palette.entries; ++i)
          if (value == decoder->trans.palette.bytes[i])
            a = 0;
      } else if (decoder->color_type & SFPNG_COLOR_MASK_COLOR) {
        if (r == decoder->trans.r &&
            g == decoder->trans.g &&
            b == decoder->trans.b) {
          a = 0;
        }
      } else {
        if (value == decoder->trans.value)
          a = 0;
      }
    }

    if (decoder->bit_depth == 16) {
      r >>= 8;
      g >>= 8;
      b >>= 8;
      a >>= 8;
    }

    *out++ = r;
    *out++ = g;
    *out++ = b;
    *out++ = a;
    --in_len_pixels;
  }
}
