#include "sfpng.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

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
};

typedef struct {
  const char* buf;
  int len;
} stream;
static void stream_consume(stream* stream, int bytes) {
  stream->buf += bytes;
  stream->len -= bytes;
}

//#define PNG_TAG(a,b,c,d) (*uint32_t)((a<<24)|(b<<16)|(c<<8)|d))

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

sfpng_status sfpng_decoder_write(sfpng_decoder* decoder,
                                 const void* buf,
                                 size_t bytes) {
  stream src = { buf, bytes };

  while (src.len > 0) {
    switch (decoder->state) {
    case STATE_SIGNATURE: {
      /* Want 8 bytes of signature in buffer. */
      int needed_bytes = 8 - decoder->in_len;
      int have_bytes = min(needed_bytes, src.len);
      memcpy(decoder->in_buf + decoder->in_len, src.buf, have_bytes);
      decoder->in_len += have_bytes;
      stream_consume(&src, have_bytes);

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
      int needed_bytes = 8 - decoder->in_len;
      int have_bytes = min(needed_bytes, src.len);
      memcpy(decoder->in_buf + decoder->in_len, src.buf, have_bytes);
      decoder->in_len += have_bytes;
      stream_consume(&src, have_bytes);

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

      decoder->chunk_buf = realloc(decoder->chunk_buf, decoder->chunk_len);

      decoder->state = STATE_CHUNK_DATA;
      decoder->chunk_ofs = 0;
      /* Fall through. */
    }
    case STATE_CHUNK_DATA: {
      int wanted_bytes = decoder->chunk_len - decoder->chunk_ofs;
      int have_bytes = min(wanted_bytes, src.len);
      memcpy(decoder->chunk_buf + decoder->chunk_ofs, src.buf, have_bytes);
      decoder->chunk_ofs += have_bytes;
      stream_consume(&src, have_bytes);

      if (decoder->chunk_ofs < decoder->chunk_len)
        return SFPNG_SUCCESS;

      decoder->state = STATE_CHUNK_CRC;
      decoder->in_len = 0;
      /* Fall through. */
    }
    case STATE_CHUNK_CRC: {
      uint32_t expected_crc;

      int wanted_bytes = 4 - decoder->in_len;
      int have_bytes = min(wanted_bytes, src.len);
      memcpy(decoder->in_buf + decoder->in_len, src.buf, have_bytes);
      decoder->in_len += have_bytes;
      stream_consume(&src, have_bytes);

      if (decoder->in_len < 4)
        return SFPNG_SUCCESS;

      memcpy(&expected_crc, decoder->in_buf, 4);
      expected_crc = ntohl(expected_crc);

      uint32_t actual_crc =
          crc_compute(decoder->crc_table, decoder->chunk_type,
                      decoder->chunk_buf, decoder->chunk_len);

      if (actual_crc != expected_crc)
        return SFPNG_ERROR_BAD_CRC;

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
