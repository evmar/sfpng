#include "sfpng.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static void row_func(sfpng_decoder* decoder,
                     int row,
                     const void* buf,
                     size_t bytes) {
  if (row == 0) {
    printf("P3\n");
    printf("%d %d\n",
           sfpng_decoder_get_width(decoder),
           sfpng_decoder_get_height(decoder));
    printf("255\n");
  }

  const uint8_t* buf_bytes = buf;
  int x;
  switch (sfpng_decoder_get_color_type(decoder)) {
  case SFPNG_COLOR_TRUECOLOR:
    for (x = 0; x < bytes; x += 3)
      printf("%d %d %d ", buf_bytes[x], buf_bytes[x+1], buf_bytes[x+2]);
    break;
  case SFPNG_COLOR_TRUECOLOR_ALPHA:
    for (x = 0; x < bytes; x += 4) {
      int a = buf_bytes[x+3];
      printf("%d %d %d ",
             buf_bytes[x] * a / 255,
             buf_bytes[x+1] * a / 255,
             buf_bytes[x+2] * a / 255);
    }
    break;
  default:
    fprintf(stderr, "color format unhandled\n");
  }
  printf("\n");
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s input.png > output.pnm\n", argv[0]);
    return 1;
  }

  FILE* f = fopen(argv[1], "rb");

  sfpng_decoder* decoder = sfpng_decoder_new();
  sfpng_decoder_set_row_func(decoder, row_func);

  char buf[4096];
  size_t len;
  while ((len = fread(buf, 1, sizeof(buf), f)) > 0) {
    sfpng_status status = sfpng_decoder_write(decoder, buf, len);
    if (status != SFPNG_SUCCESS) {
      fprintf(stderr, "decode error %d\n", status);
      return 1;
    }
  }
  sfpng_decoder_free(decoder);

  return 0;
}
