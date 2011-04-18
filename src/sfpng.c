#include "sfpng.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdint.h>

#include "decoder.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

typedef struct {
  const uint8_t* buf;
  int len;
} stream;

static void stream_consume(stream* stream, int bytes) {
  stream->buf += bytes;
  stream->len -= bytes;
}
static uint32_t stream_read_uint32(stream* stream) {
  uint32_t i;
  memcpy(&i, stream->buf, 4);
  stream_consume(stream, 4);
  return ntohl(i);
}
static uint16_t stream_read_uint16(stream* stream) {
  uint16_t i;
  memcpy(&i, stream->buf, 2);
  stream_consume(stream, 2);
  return ntohs(i);
}
static uint8_t stream_read_byte(stream* stream) {
  uint8_t i = stream->buf[0];
  stream_consume(stream, 1);
  return i;
}

static void fill_buffer(uint8_t* buf, int* have_len, int want_len,
                        stream* src) {
  int want_bytes = want_len - *have_len;
  int take_bytes = min(src->len, want_bytes);

  memcpy(buf + *have_len, src->buf, take_bytes);
  *have_len += take_bytes;
  stream_consume(src, take_bytes);
}

#define PNG_TAG(a,b,c,d) ((uint32_t)((a<<24)|(b<<16)|(c<<8)|d))

static const char png_signature[8] = {
  137, 80, 78, 71, 13, 10, 26, 10
};

sfpng_decoder* sfpng_decoder_new() {
  sfpng_decoder* decoder;

  decoder = malloc(sizeof(*decoder));
  if (!decoder)
    return NULL;

  memset(decoder, 0, sizeof(*decoder));
  crc_init_table(decoder->crc_table);

  return decoder;
}

void sfpng_decoder_set_context(sfpng_decoder* decoder, void* context) {
  decoder->context = context;
}

void* sfpng_decoder_get_context(sfpng_decoder* decoder) {
  return decoder->context;
}

enum filter_type {
  FILTER_NONE = 0,
  FILTER_SUB,
  FILTER_UP,
  FILTER_AVERAGE,
  FILTER_PAETH
};

static int paeth(int a, int b, int c) {
  int p = a + b - c;
  int pa = abs(p - a);
  int pb = abs(p - b);
  int pc = abs(p - c);
  if (pa <= pb && pa <= pc)
    return a;
  else if (pb <= pc)
    return b;
  else
    return c;
}

static sfpng_status reconstruct_filter(sfpng_decoder* decoder)
  SFPNG_WARN_UNUSED_RESULT;
static sfpng_status reconstruct_filter(sfpng_decoder* decoder) {
  /* 9.2 Filter types for filter method 0 */
  int filter_type = decoder->scanline_buf[0];
  uint8_t* buf = decoder->scanline_buf + 1;
  uint8_t* prev = decoder->scanline_prev_buf + 1;
  int i;
  int bpp = decoder->bytes_per_pixel;

  switch (filter_type) {
  case FILTER_NONE:
    break;
  case FILTER_SUB:
    for (i = bpp; i < decoder->stride; ++i)
      buf[i] = buf[i] + buf[i - bpp];
    break;
  case FILTER_UP:
    for (i = 0; i < decoder->stride; ++i)
      buf[i] = buf[i] + prev[i];
    break;
  case FILTER_AVERAGE:
    for (i = 0; i < decoder->stride; ++i) {
      int last = i - bpp;
      int a = last >= 0 ? buf[last] : 0;
      int b = prev[i];
      int avg = (a + b) / 2;
      buf[i] = buf[i] + avg;
    }
    break;
  case FILTER_PAETH:
    for (i = 0; i < decoder->stride; ++i) {
      int last = i - bpp;
      int a = last >= 0 ? buf[last] : 0;
      int b = prev[i];
      int c = last >= 0 ? prev[last] : 0;
      buf[i] = buf[i] + paeth(a, b, c);
    }
    break;
  default:
    return SFPNG_ERROR_BAD_FILTER;
  }
  return SFPNG_SUCCESS;
}

static sfpng_status parse_color(sfpng_decoder* decoder,
                                int bit_depth,
                                int color_type)
  SFPNG_WARN_UNUSED_RESULT;
static sfpng_status parse_color(sfpng_decoder* decoder,
                                int bit_depth,
                                int color_type) {
  switch (color_type) {
  case SFPNG_COLOR_GRAYSCALE: {
    if (bit_depth != 1 && bit_depth != 2 && bit_depth != 4 &&
        bit_depth != 8 && bit_depth != 16) {
      return SFPNG_ERROR_BAD_ATTRIBUTE;
    }
    break;
  }
  case SFPNG_COLOR_TRUECOLOR: {
    if (bit_depth != 8 && bit_depth != 16)
      return SFPNG_ERROR_BAD_ATTRIBUTE;
    break;
  }
  case SFPNG_COLOR_INDEXED: {
    if (bit_depth != 1 && bit_depth != 2 && bit_depth != 4 && bit_depth != 8)
      return SFPNG_ERROR_BAD_ATTRIBUTE;
    break;
  }
  case SFPNG_COLOR_GRAYSCALE_ALPHA: {
    if (bit_depth != 8 && bit_depth != 16)
      return SFPNG_ERROR_BAD_ATTRIBUTE;
    break;
  }
  case SFPNG_COLOR_TRUECOLOR_ALPHA: {
    if (bit_depth != 8 && bit_depth != 16)
      return SFPNG_ERROR_BAD_ATTRIBUTE;
    break;
  }
  default:
    return SFPNG_ERROR_BAD_ATTRIBUTE;
  }

  /* If we get here, the bit depth / color type combination has been
     verified. */
  decoder->bit_depth = bit_depth;
  decoder->color_type = color_type;
  return SFPNG_SUCCESS;
}


static sfpng_status update_header_derived_values(sfpng_decoder* decoder)
  SFPNG_WARN_UNUSED_RESULT;
static sfpng_status update_header_derived_values(sfpng_decoder* decoder) {
  int scanline_bits;
  switch (decoder->color_type) {
  case SFPNG_COLOR_GRAYSCALE:
  case SFPNG_COLOR_INDEXED:
    scanline_bits = decoder->width * decoder->bit_depth;
    decoder->bytes_per_pixel =
      decoder->bit_depth < 8 ? 1 : (decoder->bit_depth / 8);
    break;
  case SFPNG_COLOR_TRUECOLOR:
    scanline_bits = decoder->width * decoder->bit_depth * 3;
    decoder->bytes_per_pixel = 3 * (decoder->bit_depth / 8);
    break;
  case SFPNG_COLOR_GRAYSCALE_ALPHA:
    scanline_bits = decoder->width * decoder->bit_depth * 2;
    decoder->bytes_per_pixel = 2 * (decoder->bit_depth / 8);
    break;
  case SFPNG_COLOR_TRUECOLOR_ALPHA:
    scanline_bits = decoder->width * decoder->bit_depth * 4;
    decoder->bytes_per_pixel = 4 * (decoder->bit_depth / 8);
    break;
  }
  /* Round scanline_bits up to the nearest byte. */
  decoder->stride = ((scanline_bits + 7) / 8);

  /* Allocate scanline buffers, with extra byte for filter tag. */
  int scanline_size = 1 + decoder->stride;
  decoder->scanline_buf = malloc(scanline_size * 2);
  if (!decoder->scanline_buf)
    return SFPNG_ERROR_ALLOC_FAILED;
  decoder->scanline_prev_buf = decoder->scanline_buf + scanline_size;
  memset(decoder->scanline_prev_buf, 0, scanline_size);

  return SFPNG_SUCCESS;
}

static sfpng_status process_header_chunk(sfpng_decoder* decoder,
                                         stream* src)
  SFPNG_WARN_UNUSED_RESULT;
static sfpng_status process_header_chunk(sfpng_decoder* decoder,
                                         stream* src) {
  /* 11.2.2 IHDR Image header */
  if (decoder->chunk_state != CHUNK_STATE_NONE)
    return SFPNG_ERROR_BAD_ATTRIBUTE;  /* Must be the first chunk. */
  if (decoder->chunk_len != 13)
    return SFPNG_ERROR_BAD_ATTRIBUTE;

  decoder->width = stream_read_uint32(src);
  decoder->height = stream_read_uint32(src);
  if (decoder->width == 0 ||
      decoder->height == 0) {
    return SFPNG_ERROR_BAD_ATTRIBUTE;
  }

  int bit_depth = stream_read_byte(src);
  int color_type = stream_read_byte(src);
  sfpng_status status = parse_color(decoder, bit_depth, color_type);
  if (status != SFPNG_SUCCESS)
    return status;

  /* Compression/filter are currently unused. */
  int compression = stream_read_byte(src);
  if (compression != 0)
    return SFPNG_ERROR_BAD_ATTRIBUTE;
  int filter = stream_read_byte(src);
  if (filter != 0)
    return SFPNG_ERROR_BAD_ATTRIBUTE;

  int interlace = stream_read_byte(src);
  if (interlace != 0 && interlace != 1)
    return SFPNG_ERROR_BAD_ATTRIBUTE;
  decoder->interlaced = interlace;

  decoder->chunk_state = CHUNK_STATE_IHDR;

  return update_header_derived_values(decoder);
}

static sfpng_status process_palette_chunk(sfpng_decoder* decoder,
                                          stream* src)
  SFPNG_WARN_UNUSED_RESULT;
static sfpng_status process_palette_chunk(sfpng_decoder* decoder,
                                          stream* src) {
  if (decoder->chunk_state != CHUNK_STATE_IHDR)
    return SFPNG_ERROR_BAD_ATTRIBUTE;  /* Must be after IHDR. */
  if (src->len > 3*256 || src->len % 3 != 0)
    return SFPNG_ERROR_BAD_ATTRIBUTE;
  if (decoder->palette.bytes)
    return SFPNG_ERROR_BAD_ATTRIBUTE;  /* Multiple palettes? */
  decoder->palette.bytes = malloc(src->len);
  if (!decoder->palette.bytes)
    return SFPNG_ERROR_ALLOC_FAILED;
  memcpy(decoder->palette.bytes, src->buf, src->len);
  decoder->palette.entries = src->len / 3;
  decoder->chunk_state = CHUNK_STATE_PLTE;
  return SFPNG_SUCCESS;
}

static sfpng_status process_image_data_chunk(sfpng_decoder* decoder,
                                             stream* src)
  SFPNG_WARN_UNUSED_RESULT;
static sfpng_status process_image_data_chunk(sfpng_decoder* decoder,
                                             stream* src) {
  if (!(decoder->chunk_state >= CHUNK_STATE_IHDR ||
        decoder->chunk_state <= CHUNK_STATE_IDAT)) {
    return SFPNG_ERROR_BAD_ATTRIBUTE;  /* Must be after headers. */
  }

  int needs_init = !decoder->zlib_stream.next_in;

  decoder->zlib_stream.next_in = (uint8_t*)src->buf;
  decoder->zlib_stream.avail_in = src->len;
  if (needs_init) {
    if (inflateInit(&decoder->zlib_stream) != Z_OK)
      return SFPNG_ERROR_ZLIB_ERROR;

    decoder->zlib_stream.next_out = decoder->scanline_buf;
    decoder->zlib_stream.avail_out = 1 + decoder->stride;
  }

  while (decoder->zlib_stream.avail_in) {
    int status = inflate(&decoder->zlib_stream, Z_SYNC_FLUSH);
    if (status != Z_OK && status != Z_STREAM_END)
      return SFPNG_ERROR_ZLIB_ERROR;
    if (decoder->zlib_stream.avail_out == 0) {
      /* Decoded line. */
      sfpng_status status = reconstruct_filter(decoder);
      if (status != SFPNG_SUCCESS)
        return status;

      if (decoder->scanline_row == 0 && decoder->info_func) {
        /* 5.6 Chunk ordering says that all metadata chunks (other than comments)
           must appear before IDAT.  So we know that we're past all the metadata
           at this point. */
        decoder->info_func(decoder);
      }
      if (decoder->row_func) {
        decoder->row_func(decoder, decoder->scanline_row,
                          decoder->scanline_buf + 1, decoder->stride);
      }
      ++decoder->scanline_row;

      /* Swap buffers, so prev points at the row we just finished. */
      uint8_t* tmp = decoder->scanline_prev_buf;
      decoder->scanline_prev_buf = decoder->scanline_buf;
      decoder->scanline_buf = tmp;

      /* Set up for next line. */
      decoder->zlib_stream.next_out = decoder->scanline_buf;
      decoder->zlib_stream.avail_out = 1 + decoder->stride;

      /* Mark that we've sucessfully processed at least some of the IDAT. */
      decoder->chunk_state = CHUNK_STATE_IDAT;
    }
  }

  return SFPNG_SUCCESS;
}

static sfpng_status process_iend_chunk(sfpng_decoder* decoder,
                                       stream* src)
  SFPNG_WARN_UNUSED_RESULT;
static sfpng_status process_iend_chunk(sfpng_decoder* decoder,
                                       stream* src) {
  if (decoder->chunk_state != CHUNK_STATE_IDAT)
    return SFPNG_ERROR_BAD_ATTRIBUTE;  /* Must be after image data. */

  if (src->len != 0)
    return SFPNG_ERROR_BAD_ATTRIBUTE;
  if (decoder->zlib_stream.next_in) {
    int status = inflateEnd(&decoder->zlib_stream);
    decoder->zlib_stream.next_in = NULL;
    if (status != Z_OK)
      return SFPNG_ERROR_ZLIB_ERROR;
  }

  decoder->chunk_state = CHUNK_STATE_IEND;

  return SFPNG_SUCCESS;
}

static sfpng_status process_trns_chunk(sfpng_decoder* decoder,
                                       stream* src)
  SFPNG_WARN_UNUSED_RESULT;
static sfpng_status process_trns_chunk(sfpng_decoder* decoder,
                                       stream* src) {
  if (!decoder->bit_depth)
    return SFPNG_ERROR_BAD_ATTRIBUTE; /* XXX handle chunk ordering deps */

  if (decoder->color_type == SFPNG_COLOR_INDEXED) {
    if (decoder->trans.palette.bytes)
      return SFPNG_ERROR_BAD_ATTRIBUTE;  /* Multiple trns chunks? */
    decoder->trans.palette.entries = src->len;
    decoder->trans.palette.bytes = malloc(src->len);
    if (!decoder->trans.palette.bytes)
      return SFPNG_ERROR_ALLOC_FAILED;
    memcpy(decoder->trans.palette.bytes, src->buf, src->len);
  } else {
    /* 16-bit color value; either rgb or grayscale. */
    if (decoder->color_type & SFPNG_COLOR_MASK_COLOR) {
      if (src->len != 6)
        return SFPNG_ERROR_BAD_ATTRIBUTE;
      decoder->trans.r = stream_read_uint16(src);
      decoder->trans.g = stream_read_uint16(src);
      decoder->trans.b = stream_read_uint16(src);
    } else {
      if (src->len != 2)
        return SFPNG_ERROR_BAD_ATTRIBUTE;
      decoder->trans.value = stream_read_uint16(src);
    }
  }

  decoder->has_trans = 1;

  return SFPNG_SUCCESS;
}

/* zlib-inflate a buffer, mallocing a buffer for the output. */
static sfpng_status inflate_fully(stream* src, uint8_t** out_buf, int* out_len) {
  int ret = SFPNG_SUCCESS;
  uint8_t buf[8 << 10];
  z_stream zlib = {};

  *out_buf = NULL;
  *out_len = 0;
  zlib.next_in = (uint8_t*)src->buf;
  zlib.avail_in = src->len;
  if (inflateInit(&zlib) != Z_OK)
    return SFPNG_ERROR_ZLIB_ERROR;
  zlib.next_out = buf;
  zlib.avail_out = sizeof(buf);

  while (zlib.avail_in) {
    int status = inflate(&zlib, Z_SYNC_FLUSH);
    if (status != Z_OK && status != Z_STREAM_END) {
      ret = SFPNG_ERROR_ZLIB_ERROR;
      goto out;
    }
    if (zlib.avail_out == 0) {
      /* XXX ran out of buffer.
         TODO: grow a buffer manually?  ugh.
       */
      ret = SFPNG_ERROR_NOT_IMPLEMENTED;
      goto out;
    }
  }

  *out_len = zlib.total_out;
  *out_buf = malloc(*out_len);
  memcpy(*out_buf, buf, *out_len);

 out:
  inflateEnd(&zlib);
  return ret;
}

static sfpng_status process_text_chunk(sfpng_decoder* decoder,
                                       int compressed,
                                       stream* src)
  SFPNG_WARN_UNUSED_RESULT;
static sfpng_status process_text_chunk(sfpng_decoder* decoder,
                                       int compressed,
                                       stream* src) {
  if (!decoder->text_func)
    return SFPNG_SUCCESS;

  uint8_t* nul = memchr(src->buf, 0, src->len);
  if (!nul || nul + 1 >= src->buf + src->len)
    return SFPNG_ERROR_BAD_ATTRIBUTE;

  const char* keyword = (const char*)src->buf;
  stream_consume(src, nul - src->buf + 1);

  if (compressed) {
    if (src->len < 1)
      return SFPNG_ERROR_BAD_ATTRIBUTE;
    int compression = stream_read_byte(src);
    if (compression != 0)
      return SFPNG_ERROR_BAD_ATTRIBUTE;

    /* 10.3 Other uses of compression */
    /* "Unlike the image data, [ztxt] datastreams are not split across
       chunks; each such chunk contains an independent zlib
       datastream" */

    uint8_t* buf;
    int len;
    sfpng_status status = inflate_fully(src, &buf, &len);
    if (status != SFPNG_SUCCESS)
      return status;
    decoder->text_func(decoder, keyword, buf, len);
    free(buf);
    return SFPNG_SUCCESS;
  } else {
    decoder->text_func(decoder, keyword, src->buf, src->len);
  }

  return SFPNG_SUCCESS;
}

static sfpng_status process_chunk(sfpng_decoder* decoder)
  SFPNG_WARN_UNUSED_RESULT;
static sfpng_status process_chunk(sfpng_decoder* decoder) {
  uint32_t type = PNG_TAG(decoder->chunk_type[0],
                          decoder->chunk_type[1],
                          decoder->chunk_type[2],
                          decoder->chunk_type[3]);
  stream src = { decoder->chunk_buf, decoder->chunk_len };

  switch (type) {
  case PNG_TAG('I','H','D','R'):
    /* 11.2.2 IHDR Image header */
    return process_header_chunk(decoder, &src);
  case PNG_TAG('P', 'L', 'T', 'E'):
    /* 11.2.3 PLTE Palette */
    return process_palette_chunk(decoder, &src);
  case PNG_TAG('I','D','A','T'):
    /* 11.2.4 IDAT Image data */
    return process_image_data_chunk(decoder, &src);
  case PNG_TAG('I', 'E', 'N', 'D'):
    /* 11.2.5 IEND Image trailer */
    return process_iend_chunk(decoder, &src);
  case PNG_TAG('t','R','N','S'):
    /* 11.3.2.1 tRNS Transparency */
    return process_trns_chunk(decoder, &src);
  case PNG_TAG('c', 'H', 'R', 'M'):
    /* 11.3.3.1 cHRM Primary chromaticities and white point */
    /* This is related to gamma/white balance info. */
    /* Don't care.  TODO: expose this info to users? */
    break;
  case PNG_TAG('g', 'A', 'M', 'A'):
    /* 11.3.3.2 gAMA Image gamma */
    if (src.len != 4)
      return SFPNG_ERROR_BAD_ATTRIBUTE;
    decoder->gamma = stream_read_uint32(&src);
    /* Spec says: 0 gamma is meaningless and should be ignored. */
    break;
  case PNG_TAG('s','B','I','T'):
    /* 11.3.3.4 sBIT Significant bits */
    /* This is how many bits of the color info is significant. */
    /* Don't care.  TODO: expose this info to users? */
    break;
  case PNG_TAG('t', 'E', 'X', 't'):
    /* 11.3.4.3 tEXt Textual data */
    return process_text_chunk(decoder, 0, &src);
  case PNG_TAG('z', 'T', 'X', 't'):
    /* 11.3.4.4 xTXt Compressed textual data */
    return process_text_chunk(decoder, 1, &src);
  case PNG_TAG('b','K','G','D'):
    /* 11.3.5.1 bKGD Background color */
    /* This is the "preferred" background color; when part of a larger
       document, it should be ignored when rendering. */
    /* Don't care.  TODO: expose this info to users?  */
    break;
  case PNG_TAG('h','I','S','T'):
    /* 11.3.5.2 hIST Image histogram */
    /* Don't care.  TODO: expose this info to users?  */
    break;
  case PNG_TAG('p','H','Y','s'):
    /* 11.3.5.3 pHYs Physical pixel dimensions */
    if (src.len != 9)
      return SFPNG_ERROR_BAD_ATTRIBUTE;
    /* Don't care.  TODO: expose this info to users?  */
    break;
  case PNG_TAG('t','I','M','E'):
    /* 11.3.6.1 tIME Image last-modification time */
    /* Don't care.  TODO: expose this info to users?  */
    break;
  default:
    if (decoder->unknown_chunk_func) {
      decoder->unknown_chunk_func(decoder,
                                  decoder->chunk_type,
                                  src.buf,
                                  src.len);
    }
    break;
  }
  return SFPNG_SUCCESS;
}

void sfpng_decoder_set_info_func(sfpng_decoder* decoder,
                                 sfpng_info_func info_func) {
  decoder->info_func = info_func;
}
void sfpng_decoder_set_row_func(sfpng_decoder* decoder,
                                sfpng_row_func row_func) {
  decoder->row_func = row_func;
}
void sfpng_decoder_set_text_func(sfpng_decoder* decoder,
                                 sfpng_text_func text_func) {
  decoder->text_func = text_func;
}
void sfpng_decoder_set_unknown_chunk_func(sfpng_decoder* decoder,
                                          sfpng_unknown_chunk_func chunk_func) {
  decoder->unknown_chunk_func = chunk_func;
}


int sfpng_decoder_get_width(const sfpng_decoder* decoder) {
  return decoder->width;
}
int sfpng_decoder_get_height(const sfpng_decoder* decoder) {
  return decoder->height;
}
int sfpng_decoder_get_depth(const sfpng_decoder* decoder) {
  return decoder->bit_depth;
}
sfpng_color_type sfpng_decoder_get_color_type(const sfpng_decoder* decoder) {
  return decoder->color_type;
}
int sfpng_decoder_get_interlaced(const sfpng_decoder* decoder) {
  return decoder->interlaced;
}

const uint8_t* sfpng_decoder_get_palette(const sfpng_decoder* decoder) {
  return decoder->palette.bytes;
}
int sfpng_decoder_get_palette_entries(const sfpng_decoder* decoder) {
  return decoder->palette.entries;
}

int sfpng_decoder_has_gamma(const sfpng_decoder* decoder) {
  return decoder->gamma > 0;
}
float sfpng_decoder_get_gamma(const sfpng_decoder* decoder) {
  return decoder->gamma / (float)100000;
}

static sfpng_status finish(sfpng_decoder* decoder) {
  if (decoder->chunk_state != CHUNK_STATE_IEND)
    return SFPNG_ERROR_EOF;
  return SFPNG_SUCCESS;
}

sfpng_status sfpng_decoder_write(sfpng_decoder* decoder,
                                 const void* buf,
                                 size_t bytes) {
  if (bytes == 0)
    return finish(decoder);

  stream src = { buf, bytes };

  while (src.len > 0) {
    switch (decoder->state) {
    case STATE_SIGNATURE: {
      /* Want 8 bytes of signature in buffer. */
      fill_buffer(decoder->in_buf, &decoder->in_len, 8, &src);
      if (decoder->in_len < 8)
        return SFPNG_SUCCESS;

      if (memcmp(decoder->in_buf, png_signature, 8) != 0)
        return SFPNG_ERROR_BAD_SIGNATURE;

      decoder->state = STATE_CHUNK_HEADER;
      decoder->in_len = 0;
      /* Fall through. */
    }
    case STATE_CHUNK_HEADER: {
      /* Want 8 bytes of chunk header. */
      fill_buffer(decoder->in_buf, &decoder->in_len, 8, &src);
      if (decoder->in_len < 8)
        return SFPNG_SUCCESS;

      int32_t chunk_len;
      memcpy(&chunk_len, decoder->in_buf, 4);
      chunk_len = ntohl(chunk_len);
      if (chunk_len < 0)
        return SFPNG_ERROR_BAD_ATTRIBUTE;

      memcpy(&decoder->chunk_type, decoder->in_buf + 4, 4);

      if (chunk_len) {
        if (decoder->chunk_len < chunk_len)
          decoder->chunk_buf = realloc(decoder->chunk_buf, chunk_len);
        if (!decoder->chunk_buf)
          return SFPNG_ERROR_ALLOC_FAILED;
      }
      decoder->chunk_len = chunk_len;

      decoder->state = STATE_CHUNK_DATA;
      decoder->chunk_ofs = 0;
      /* Fall through. */
    }
    case STATE_CHUNK_DATA: {
      fill_buffer(decoder->chunk_buf, &decoder->chunk_ofs, decoder->chunk_len,
                  &src);
      if (decoder->chunk_ofs < decoder->chunk_len)
        return SFPNG_SUCCESS;

      decoder->state = STATE_CHUNK_CRC;
      decoder->in_len = 0;
      /* Fall through. */
    }
    case STATE_CHUNK_CRC: {
      fill_buffer(decoder->in_buf, &decoder->in_len, 4, &src);
      if (decoder->in_len < 4)
        return SFPNG_SUCCESS;

      uint32_t expected_crc;
      memcpy(&expected_crc, decoder->in_buf, 4);
      expected_crc = ntohl(expected_crc);

      uint32_t actual_crc =
          crc_compute(decoder->crc_table, decoder->chunk_type,
                      decoder->chunk_buf, decoder->chunk_len);

      if (actual_crc != expected_crc)
        return SFPNG_ERROR_BAD_CRC;

      sfpng_status status = process_chunk(decoder);
      if (status != SFPNG_SUCCESS)
        return status;

      decoder->state = STATE_CHUNK_HEADER;
      decoder->in_len = 0;
      break;
    }
    }
  }

  return SFPNG_SUCCESS;
}

void sfpng_decoder_free(sfpng_decoder* decoder) {
  if (decoder->chunk_buf)
    free(decoder->chunk_buf);
  if (decoder->scanline_buf)
    free(min(decoder->scanline_buf, decoder->scanline_prev_buf));
  if (decoder->zlib_stream.next_in) {
    int status = inflateEnd(&decoder->zlib_stream);
    /* We don't care about a bad status at this point. */
  }
  if (decoder->palette.bytes)
    free(decoder->palette.bytes);
  if (decoder->trans.palette.bytes)
    free(decoder->trans.palette.bytes);
  free(decoder);
}
