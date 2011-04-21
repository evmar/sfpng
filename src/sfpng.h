#include <stddef.h>
#include <stdint.h>

/* It is very important to check the return value of sfpng_decoder_write,
   so let the compiler help check if possible. */
#if defined(__GNUC__)
#define SFPNG_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define SFPNG_WARN_UNUSED_RESULT
#endif

/** The opaque type storing the decode state. */
typedef struct _sfpng_decoder sfpng_decoder;

/** Status of a decode.

Any value other than success is an error. */
typedef enum {
  SFPNG_SUCCESS = 0,
  SFPNG_ERROR_ALLOC_FAILED,
  SFPNG_ERROR_NOT_IMPLEMENTED,

  /* All of these errors are related to errors in the file content.
     I'm considering just merging these together into a single "bad file"
     error. */
  SFPNG_ERROR_BAD_SIGNATURE,
  SFPNG_ERROR_BAD_CRC,
  SFPNG_ERROR_BAD_ATTRIBUTE,
  SFPNG_ERROR_EOF,
  SFPNG_ERROR_ZLIB_ERROR,
  SFPNG_ERROR_BAD_FILTER,
} sfpng_status;

/** Possible types of color spaces used by png files.

These values come from the png spec and are not going to change. */
typedef enum {
  SFPNG_COLOR_GRAYSCALE       = 0,
  SFPNG_COLOR_TRUECOLOR       = 2,
  SFPNG_COLOR_INDEXED         = 3,
  SFPNG_COLOR_GRAYSCALE_ALPHA = 4,
  SFPNG_COLOR_TRUECOLOR_ALPHA = 6,
} sfpng_color_type;

/** Masks for testing bits in the color type.

These values come from the png spec and are not going to change. */
enum {
  SFPNG_COLOR_MASK_PALETTE = 1 << 0,  /** Set if image is paletted. */
  SFPNG_COLOR_MASK_COLOR   = 1 << 1,  /** Set if image is not grayscale. */
  SFPNG_COLOR_MASK_ALPHA   = 1 << 2,  /** Set if image has alpha channel. */
};

/** Allocate and initialize a new decoder. */
sfpng_decoder* sfpng_decoder_new();
/** Free a decoder. */
void sfpng_decoder_free(sfpng_decoder* decoder);

/** Set an arbitrary pointer on a decoder.

This is useful when hooking up callbacks back into application data
structures. */
void sfpng_decoder_set_context(sfpng_decoder* decoder, void* context);

/** Get the pointer set by _set_context(). */
void* sfpng_decoder_get_context(sfpng_decoder* decoder);

/** The type of the callback when image information has been fetched. */
typedef void (*sfpng_info_func)(sfpng_decoder* decoder);
/** Set the callback for when image information has been fetched. */
void sfpng_decoder_set_info_func(sfpng_decoder* decoder,
                                 sfpng_info_func info_func);

/** The type of the callback per row of image pixels. */
typedef void (*sfpng_row_func)(sfpng_decoder* decoder,
                               int row,
                               const uint8_t* buf,
                               int len);
/** Set the callback called per row of image pixels. */
void sfpng_decoder_set_row_func(sfpng_decoder* decoder,
                                sfpng_row_func row_func);

/** The type of the callback for the various PNG comment metadata.

TODO: add a param distinguishing between the latin-1 and UTF-8
comment types. */
typedef void (*sfpng_text_func)(sfpng_decoder* decoder,
                                const char* keyword,
                                const uint8_t* text,
                                int text_len);
/** Set the callback called per PNG comment. */
void sfpng_decoder_set_text_func(sfpng_decoder* decoder,
                                 sfpng_text_func text_func);

/** The type of the callback called for PNG chunks unknown to sfpng. */
typedef void (*sfpng_unknown_chunk_func)(sfpng_decoder* decoder,
                                         char chunk_type[4],
                                         const uint8_t* buf,
                                         int len);
/** Set the callback called for PNG chunks unknown to sfpng. */
void sfpng_decoder_set_unknown_chunk_func(sfpng_decoder* decoder,
                                          sfpng_unknown_chunk_func chunk_func);

/** Get the image width in pixels.

(Only valid after the info callback). */
int sfpng_decoder_get_width(const sfpng_decoder* decoder);

/** Get the image height in pixels.

(Only valid after the info callback). */
int sfpng_decoder_get_height(const sfpng_decoder* decoder);

/** Get the image bit depth.

E.g. a 4bpp image has a depth of 4.

(Only valid after the info callback). */
int sfpng_decoder_get_depth(const sfpng_decoder* decoder);

/** Get the image color format.

See the color type enum for details.

(Only valid after the info callback). */
sfpng_color_type sfpng_decoder_get_color_type(const sfpng_decoder* decoder);

/** Get whether the image is interlaced.

(Only valid after the info callback). */
int sfpng_decoder_get_interlaced(const sfpng_decoder* decoder);

/** Get the image palette.

The palette format is RGB, 8 bits per channel, 24 bits per entry.

(Only valid after the info callback). */
const uint8_t* sfpng_decoder_get_palette(const sfpng_decoder* decoder);

/** Get the number of entries in the image palette, or zero if no palette.

The palette format is RGB, 8 bits per channel, 24 bits per entry.

(Only valid after the info callback). */
int sfpng_decoder_get_palette_entries(const sfpng_decoder* decoder);

/** Get whether the image has gamma metadata.

(Only valid after the info callback). */
int sfpng_decoder_has_gamma(const sfpng_decoder* decoder);

/** Get the image gamma metadata.

XXX See the PNG spec for details.

(Only valid after the info callback). */
float sfpng_decoder_get_gamma(const sfpng_decoder* decoder);

/** Write some PNG bytes into the decoder.

This may cause callbacks to fire.

It is an error to continue to call sfpng functions after this function
returning any status other than success. */
sfpng_status sfpng_decoder_write(sfpng_decoder* decoder,
                                 const void* buf,
                                 size_t bytes) SFPNG_WARN_UNUSED_RESULT;


/** Transform blah blah.

XXX finish me. */
void sfpng_decoder_transform(sfpng_decoder* decoder,
                             int row, const uint8_t* buf,
                             uint8_t* out);
