#include "sfpng.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static void unknown_chunk(void* context, sfpng_decoder* decoder,
                          char chunk_type[4],
                          const void* buf, size_t bytes) {
  printf("unknown chunk: %c%c%c%c, length %d\n",
         chunk_type[0], chunk_type[1], chunk_type[2], chunk_type[3],
         (int)bytes);
}

int main(int argc, char* argv[]) {
  const char* file = argv[1];
  FILE* f = fopen(file, "rb");

  sfpng_decoder* decoder = sfpng_decoder_new();
  sfpng_decoder_set_unknown_chunk_func(decoder, unknown_chunk);

  char buf[4096];
  size_t len;
  while ((len = fread(buf, 1, 10, f)) >= 0) {
    sfpng_status status = sfpng_decoder_write(decoder, buf, len);
    if (status != SFPNG_SUCCESS) {
      printf("decode error %d\n", status);
      return 1;
    }
    if (len == 0)
      break;
  }
  int width = sfpng_decoder_get_width(decoder);
  int height = sfpng_decoder_get_height(decoder);
  printf("dimensions: %dx%d\n", width, height);
  sfpng_decoder_free(decoder);

  return 0;
}
