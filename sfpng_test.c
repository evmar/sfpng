#include "sfpng.h"

#include <stdio.h>

int main(int argc, char* argv[]) {
  FILE* f = fopen(argv[1], "rb");
  sfpng_decoder* decoder = sfpng_decoder_new();
  char buf[4096];
  size_t len;
  while ((len = fread(buf, 1, sizeof(buf), f)) > 0) {
    sfpng_status status = sfpng_decoder_write(decoder, buf, len);
    if (status != SFPNG_SUCCESS) {
      printf("decode error\n");
      return 1;
    }
  }
  sfpng_decoder_free(decoder);

  return 0;
}
