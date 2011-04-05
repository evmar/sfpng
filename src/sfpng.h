#include <stddef.h>
#include <stdint.h>

#if defined(__GNUC__)
#define SFPNG_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define SFPNG_WARN_UNUSED_RESULT
#endif

typedef struct _sfpng_decoder sfpng_decoder;

typedef enum {
  SFPNG_SUCCESS = 0,
  SFPNG_ERROR_ALLOC_FAILED,
  SFPNG_ERROR_NOT_IMPLEMENTED,

  /* All of these errors are related to errors in the file content.
     I'm considering just merging these together. */
  SFPNG_ERROR_BAD_SIGNATURE,
  SFPNG_ERROR_BAD_CRC,
  SFPNG_ERROR_BAD_ATTRIBUTE,
  SFPNG_ERROR_ZLIB_ERROR,
  SFPNG_ERROR_BAD_FILTER,
} sfpng_status;

/* Possible types of color spaces used by png files.
   These values come from the png spec and are not going to change. */
typedef enum {
  SFPNG_COLOR_GRAYSCALE       = 0,
  SFPNG_COLOR_TRUECOLOR       = 2,
  SFPNG_COLOR_INDEXED         = 3,
  SFPNG_COLOR_GRAYSCALE_ALPHA = 4,
  SFPNG_COLOR_TRUECOLOR_ALPHA = 6,
} sfpng_color_type;

/* Masks for testing bits in the color type.
   These values come from the png spec and are not going to change. */
enum {
  SFPNG_COLOR_MASK_PALETTE = 1 << 0,  /* Set if image is paletted. */
  SFPNG_COLOR_MASK_COLOR   = 1 << 1,  /* Set if image is not grayscale. */
  SFPNG_COLOR_MASK_ALPHA   = 1 << 2,  /* Set if image has alpha channel. */
};

sfpng_decoder* sfpng_decoder_new();
void sfpng_decoder_free(sfpng_decoder* decoder);

void sfpng_decoder_set_context(sfpng_decoder* decoder, void* context);
void* sfpng_decoder_get_context(sfpng_decoder* decoder);

typedef void (*sfpng_info_func)(sfpng_decoder* decoder);
void sfpng_decoder_set_info_func(sfpng_decoder* decoder,
                                 sfpng_info_func info_func);

typedef void (*sfpng_row_func)(sfpng_decoder* decoder,
                               int row,
                               const uint8_t* buf,
                               int len);
void sfpng_decoder_set_row_func(sfpng_decoder* decoder,
                                sfpng_row_func row_func);

typedef void (*sfpng_text_func)(sfpng_decoder* decoder,
                                const char* keyword,
                                const uint8_t* text,
                                int text_len);
void sfpng_decoder_set_text_func(sfpng_decoder* decoder,
                                 sfpng_text_func text_func);

typedef void (*sfpng_unknown_chunk_func)(sfpng_decoder* decoder,
                                         char chunk_type[4],
                                         const uint8_t* buf,
                                         int len);
void sfpng_decoder_set_unknown_chunk_func(sfpng_decoder* decoder,
                                          sfpng_unknown_chunk_func chunk_func);

int sfpng_decoder_get_width(const sfpng_decoder* decoder);
int sfpng_decoder_get_height(const sfpng_decoder* decoder);
int sfpng_decoder_get_depth(const sfpng_decoder* decoder);
sfpng_color_type sfpng_decoder_get_color_type(const sfpng_decoder* decoder);
int sfpng_decoder_get_interlaced(const sfpng_decoder* decoder);

const uint8_t* sfpng_decoder_get_palette(const sfpng_decoder* decoder);
int sfpng_decoder_get_palette_entries(const sfpng_decoder* decoder);

int sfpng_decoder_has_gamma(const sfpng_decoder* decoder);
float sfpng_decoder_get_gamma(const sfpng_decoder* decoder);

sfpng_status sfpng_decoder_write(sfpng_decoder* decoder,
                                 const void* buf,
                                 size_t bytes) SFPNG_WARN_UNUSED_RESULT;


void sfpng_decoder_transform(sfpng_decoder* decoder, const uint8_t* row,
                             uint8_t* out);
