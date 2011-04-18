#include <zlib.h>  /* z_stream */

#include "crc.h"  /* crc_table */

typedef enum {
  STATE_SIGNATURE,
  STATE_CHUNK_HEADER,
  STATE_CHUNK_DATA,
  STATE_CHUNK_CRC,
} decode_state;

/* 5.6 Chunk ordering */
typedef enum {
  CHUNK_STATE_NONE,
  CHUNK_STATE_IHDR,
  CHUNK_STATE_PLTE,
  CHUNK_STATE_IDAT,
  CHUNK_STATE_IEND,
} decode_chunk_state;

typedef struct {
  uint8_t* bytes;
  int entries;
} palette;

typedef struct {
  /* Depending on image type: either a list of trans palette entries,
     an rgb value, or a grayscale value. */
  palette palette;
  int r, g, b, value;
} trans;

struct _sfpng_decoder {
  crc_table crc_table;

  /* User-specified context pointer. */
  void* context;

  sfpng_info_func info_func;
  sfpng_row_func row_func;
  sfpng_text_func text_func;
  sfpng_unknown_chunk_func unknown_chunk_func;

  /* Header decoding state. */
  decode_state state;
  uint8_t in_buf[8];
  int in_len;

  /* Chunk decoding state. */
  decode_chunk_state chunk_state;
  int chunk_len;
  char chunk_type[4];
  int chunk_ofs;
  uint8_t* chunk_buf;

  /* Image properties, read from IHDR chunk. */
  uint32_t width;
  uint32_t height;
  int bit_depth;
  sfpng_color_type color_type;
  int interlaced;

  /* Derived image properties, computed from above. */
  int stride;
  int bytes_per_pixel;

  /* Palette, from PLTE. */
  palette palette;

  /* Gamma, from gAMA. */
  uint32_t gamma;

  /* Transparency info, from tRNS. */
  int has_trans;
  trans trans;

  /* IDAT decoding state. */
  z_stream zlib_stream;
  uint8_t* scanline_buf;
  uint8_t* scanline_prev_buf;
  int scanline_row;
};
