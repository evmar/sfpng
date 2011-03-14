#include <stddef.h>

typedef struct _sfpng_decoder sfpng_decoder;

typedef enum {
  SFPNG_SUCCESS = 0,
  SFPNG_ERROR_BAD_SIGNATURE,
  SFPNG_ERROR_BAD_CRC,
} sfpng_status;

sfpng_decoder* sfpng_decoder_new();
sfpng_status sfpng_decoder_write(sfpng_decoder* decoder,
                                 const void* buf,
                                 size_t bytes);
void sfpng_decoder_free(sfpng_decoder* decoder);
