#include "sfpng.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(int argc, char* argv[]) {
  FILE* f = fopen("google.png", "rb");
  sfpng_decoder* decoder = sfpng_decoder_new();
  char buf[4096];
  size_t len;
  while ((len = fread(buf, 1, 10, f)) > 0) {
    sfpng_status status = sfpng_decoder_write(decoder, buf, len);
    if (status != SFPNG_SUCCESS) {
      printf("decode error %d\n", status);
      return 1;
    }
  }
  int width = sfpng_decoder_get_width(decoder);
  int height = sfpng_decoder_get_height(decoder);
  sfpng_decoder_free(decoder);

  return 0;
}
