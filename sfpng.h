#include <stddef.h>

typedef struct _sfpng_decoder sfpng_decoder;

typedef enum {
  SFPNG_SUCCESS = 0,
  SFPNG_ERROR_BAD_SIGNATURE,
  SFPNG_ERROR_BAD_CRC,
  SFPNG_ERROR_ALLOC_FAILED,
  SFPNG_ERROR_BAD_ATTRIBUTE,
  SFPNG_ERROR_ZLIB_ERROR,
  SFPNG_ERROR_BAD_FILTER,
} sfpng_status;

sfpng_decoder* sfpng_decoder_new();
void sfpng_decoder_free(sfpng_decoder* decoder);

typedef void (*sfpng_row_func)(void* context,
                               sfpng_decoder* decoder,
                               int row,
                               const void* buf,
                               size_t bytes);

void sfpng_decoder_set_row_func(sfpng_decoder* decoder,
                                sfpng_row_func row_func);
int sfpng_decoder_get_width(sfpng_decoder* decoder);
int sfpng_decoder_get_height(sfpng_decoder* decoder);

sfpng_status sfpng_decoder_write(sfpng_decoder* decoder,
                                 const void* buf,
                                 size_t bytes);
