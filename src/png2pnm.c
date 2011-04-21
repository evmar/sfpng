#include "sfpng.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

uint8_t* g_transform_buf = NULL;

static void row_func(sfpng_decoder* decoder,
                     int row,
                     const uint8_t* buf,
                     int len) {
  if (row == 0) {
    printf("P3\n");
    printf("%d %d\n",
           sfpng_decoder_get_width(decoder),
           sfpng_decoder_get_height(decoder));
    printf("255\n");

    g_transform_buf = malloc(sfpng_decoder_get_width(decoder) * 4);
  }

  sfpng_decoder_transform(decoder, row, buf, g_transform_buf);

  int x;
  for (x = 0; x < sfpng_decoder_get_width(decoder) * 4; x += 4) {
    printf("%d %d %d ",
           g_transform_buf[x],
           g_transform_buf[x+1],
           g_transform_buf[x+2]);
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
  while ((len = fread(buf, 1, sizeof(buf), f)) >= 0) {
    sfpng_status status = sfpng_decoder_write(decoder, buf, len);
    if (status != SFPNG_SUCCESS) {
      fprintf(stderr, "decode error %d\n", status);
      return 1;
    }
    if (len == 0)
      break;
  }
  sfpng_decoder_free(decoder);

  return 0;
}
