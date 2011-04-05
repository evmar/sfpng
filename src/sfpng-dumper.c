#include "sfpng.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "dumper.h"

typedef struct {
  int transform;
  uint8_t* transform_buf;
  int transform_len;
} decode_context;

static void dump_attrs(sfpng_decoder* decoder) {
  printf("dimensions: %dx%d\n",
         sfpng_decoder_get_width(decoder),
         sfpng_decoder_get_height(decoder));
  const char* color_type_name = "unknown";
  sfpng_color_type color_type = sfpng_decoder_get_color_type(decoder);
  if (color_type >= 0 && color_type <= 6 &&
      dumper_color_type_names[color_type]) {
    color_type_name = dumper_color_type_names[color_type];
  }
  printf("bit depth: %d  color type: %s\n",
         sfpng_decoder_get_depth(decoder),
         color_type_name);

  int interlaced = sfpng_decoder_get_interlaced(decoder);
  printf("interlaced: %s\n", interlaced ? "yes" : "no");

  if (sfpng_decoder_has_gamma(decoder))
    printf("gamma: %.2f\n", sfpng_decoder_get_gamma(decoder));

  const uint8_t* palette = sfpng_decoder_get_palette(decoder);
  if (palette) {
    printf("palette:");
    int entries = sfpng_decoder_get_palette_entries(decoder);
    int i;
    for (i = 0; i < entries * 3; i += 3) {
      printf(" %02x%02x%02x",
             palette[i],
             palette[i+1],
             palette[i+2]);
    }
    printf("\n");
  }
}

static void row_func(sfpng_decoder* decoder,
                     int row,
                     const uint8_t* buf,
                     int len) {
  decode_context* context = (decode_context*)sfpng_decoder_get_context(decoder);

  if (context->transform) {
    sfpng_decoder_transform(decoder, buf, context->transform_buf);
    buf = context->transform_buf;
    len = context->transform_len;
  }

  printf("%3d:", row);
  int i;
  for (i = 0; i < len; ++i)
    printf("%02x", buf[i]);
  printf("\n");
}

static void info_func(sfpng_decoder* decoder) {
  decode_context* context = (decode_context*)sfpng_decoder_get_context(decoder);

  if (context->transform) {
    if (sfpng_decoder_get_interlaced(decoder))
      return;
    printf("decoded bytes:\n");
    context->transform_len = sfpng_decoder_get_width(decoder) * 4;
    context->transform_buf = malloc(context->transform_len);
    int depth = sfpng_decoder_get_depth(decoder);
    sfpng_decoder_set_row_func(decoder, row_func);
  } else {
    dump_attrs(decoder);
    if (sfpng_decoder_get_interlaced(decoder))
      return;
    printf("raw data bytes:\n");
    sfpng_decoder_set_row_func(decoder, row_func);
  }
}

static void text_func(sfpng_decoder* decoder,
                      const char* keyword,
                      const uint8_t* text,
                      int text_len) {
  return;  /* TODO: implement for libpng as well. */
  printf("comment: %s\n  ", keyword);
  int i;
  for (i = 0; i < text_len; ++i) {
    if (0x20 <= text[i] && text[i] < 0x7F)
      printf("%c", text[i]);
    else
      printf("\\x%02x", text[i]);
  }
  printf("\n");
}

static void unknown_chunk(sfpng_decoder* decoder,
                          char chunk_type[4],
                          const uint8_t* buf, int len) {
  return;
  printf("unknown chunk: %c%c%c%c, length %d\n",
         chunk_type[0], chunk_type[1], chunk_type[2], chunk_type[3],
         len);
  int i;
  for (i = 0; i < len; ++i)
    printf(" %02x", buf[i]);
  printf("\n");
}

static int dump_file(const char* filename, int transform) {
  int ret = 1;
  FILE* f = fopen(filename, "rb");
  if (!f) {
    perror("fopen");
    return 1;
  }

  decode_context context = {0};
  context.transform = transform;

  sfpng_decoder* decoder = sfpng_decoder_new();
  sfpng_decoder_set_context(decoder, &context);
  sfpng_decoder_set_info_func(decoder, info_func);
  sfpng_decoder_set_text_func(decoder, text_func);
  sfpng_decoder_set_unknown_chunk_func(decoder, unknown_chunk);

  char buf[4096];
  size_t len;
  while ((len = fread(buf, 1, sizeof(buf), f)) > 0) {
    sfpng_status status = sfpng_decoder_write(decoder, buf, len);
    if (status != SFPNG_SUCCESS) {
      if (status == SFPNG_ERROR_ALLOC_FAILED)
        printf("alloc failed\n");
      else if (status == SFPNG_ERROR_BAD_FILTER &&
               sfpng_decoder_get_interlaced(decoder))
        /* XXX ignore unimpl interlaced bits for now */;
      else
        printf("invalid image\n");
      goto out;
    }
    if (len == 0)
      break;
  }
  if (ferror(f)) {
    perror("fread");
    goto out;
  }

  ret = 0;

 out:
  sfpng_decoder_free(decoder);
  if (context.transform_buf)
    free(context.transform_buf);
  return ret;
}

int main(int argc, char* argv[]) {
  const char* filename = argv[1];
  if (!filename) {
    fprintf(stderr, "usage: %s pngfile\n", argv[0]);
    return 1;
  }

  int status = dump_file(filename, 0);
  if (status != 0)
    return status;
  status = dump_file(filename, 1);
  return status;
}
