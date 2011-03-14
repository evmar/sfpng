#include "sfpng.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdint.h>

#include "crc.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

typedef enum {
  STATE_SIGNATURE,
  STATE_CHUNK_HEADER,
  STATE_CHUNK_DATA,
  STATE_CHUNK_CRC,
} decode_state;

struct _sfpng_decoder {
  crc_table crc_table;

  decode_state state;
  char in_buf[8];
  int in_len;

  int chunk_len;
  char chunk_type[4];
  int chunk_ofs;
  char* chunk_buf;

  /* Read from IHDR chunk. */
  uint32_t width;
  uint32_t height;
};

typedef struct {
  const char* buf;
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
static uint8_t stream_read_byte(stream* stream) {
  uint8_t i = stream->buf[0];
  stream_consume(stream, 1);
  return i;
}

static void fill_buffer(char* buf, int* have_len, int want_len,
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

static sfpng_status process_chunk(sfpng_decoder* decoder) {
  uint32_t type = PNG_TAG(decoder->chunk_type[0],
                          decoder->chunk_type[1],
                          decoder->chunk_type[2],
                          decoder->chunk_type[3]);
  stream src = { decoder->chunk_buf, decoder->chunk_len };

  switch (type) {
  case PNG_TAG('I', 'H', 'D', 'R'): {
    /* 11.2.2 IHDR Image header */
    if (decoder->chunk_len != 13)
      return SFPNG_ERROR_BAD_ATTRIBUTE;

    decoder->width = stream_read_uint32(&src);
    decoder->height = stream_read_uint32(&src);
    if (decoder->width == 0 ||
        decoder->height == 0) {
      return SFPNG_ERROR_BAD_ATTRIBUTE;
    }

    int bit_depth = stream_read_byte(&src);
    int color_type = stream_read_byte(&src);
    /* XXX validate depth/color info. */

    /* Compression/filter are currently unused. */
    int compression = stream_read_byte(&src);
    if (compression != 0)
      return SFPNG_ERROR_BAD_ATTRIBUTE;
    int filter = stream_read_byte(&src);
    if (filter != 0)
      return SFPNG_ERROR_BAD_ATTRIBUTE;

    int interlace = stream_read_byte(&src);
    if (interlace != 0 && interlace != 1)
      return SFPNG_ERROR_BAD_ATTRIBUTE;
    break;
  }
  case PNG_TAG('p', 'H', 'Y', 's'): {
    /* 11.3.5.3 pHYs Physical pixel dimensions */
    if (decoder->chunk_len != 9)
      return SFPNG_ERROR_BAD_ATTRIBUTE;
    /* Don't care. */
    break;
  }
  case PNG_TAG('I', 'D', 'A', 'T'): {
    /* image data */
    break;
  }
  case PNG_TAG('I', 'E', 'N', 'D'): {
    /* 11.2.5 IEND Image trailer */
    if (decoder->chunk_len != 0)
      return SFPNG_ERROR_BAD_ATTRIBUTE;
    break;
  }
  default:
    printf("WARN: unhandled tag %c%c%c%c\n",
           decoder->chunk_type[0],
           decoder->chunk_type[1],
           decoder->chunk_type[2],
           decoder->chunk_type[3]);
  }
  return SFPNG_SUCCESS;
}

sfpng_status sfpng_decoder_write(sfpng_decoder* decoder,
                                 const void* buf,
                                 size_t bytes) {
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

      memcpy(&decoder->chunk_len, decoder->in_buf, 4);
      decoder->chunk_len = ntohl(decoder->chunk_len);
      memcpy(&decoder->chunk_type, decoder->in_buf + 4, 4);
      printf("len %d chunk type %c%c%c%c\n",
             decoder->chunk_len,
             decoder->chunk_type[0],
             decoder->chunk_type[1],
             decoder->chunk_type[2],
             decoder->chunk_type[3]);

      if (decoder->chunk_len) {
        decoder->chunk_buf = realloc(decoder->chunk_buf, decoder->chunk_len);
        if (!decoder->chunk_buf)
          return SFPNG_ERROR_ALLOC_FAILED;
      }

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
  free(decoder);
}
