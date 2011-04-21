#include <libpng/png.h>
#include <setjmp.h>

#include "dumper.h"

static void swallow_warnings_function(png_structp png,
                                      png_const_charp msg) {
  /* Ignore. */
}
static void swallow_errors_function(png_structp png,
                                    png_const_charp msg) {
  /* Don't print anything, but we must at least longjmp for libpng's sake. */
  longjmp(png_jmpbuf(png), 1);
}

static void dump_png_metadata(png_structp png, png_infop info) {
  int width = png_get_image_width(png, info);
  int height = png_get_image_height(png, info);
  printf("dimensions: %dx%d\n", width, height);
  const char* color_type_name = "unknown";
  int color_type = png_get_color_type(png, info);
  if (color_type >= 0 && color_type <= 6 &&
      dumper_color_type_names[color_type]) {
    color_type_name = dumper_color_type_names[color_type];
  }
  printf("bit depth: %d  color type: %s\n",
         png_get_bit_depth(png, info),
         color_type_name);
  int interlaced = png_get_interlace_type(png, info) != PNG_INTERLACE_NONE;
  printf("interlaced: %s\n", interlaced ? "yes" : "no");

  double gamma;
  if (png_get_gAMA(png, info, &gamma))
    printf("gamma: %.2f\n", gamma);

  png_color* palette;
  int entries;
  if (png_get_PLTE(png, info, &palette, &entries)) {
    printf("palette:");
    int i;
    for (i = 0; i < entries; ++i) {
      printf(" %02x%02x%02x",
             palette[i].red,
             palette[i].green,
             palette[i].blue);
    }
    printf("\n");
  }
}

static void dump_png_rows(png_structp png, png_infop info) {
  int height = png_get_image_height(png, info);
  png_byte** rows = png_get_rows(png, info);
  int stride = png_get_rowbytes(png, info);
  int y;
  for (y = 0; y < height; ++y)
    dump_row(y, rows[y], stride);
}

static int dump_file(const char* filename, int transform) {
  int ret = 1;
  png_structp png;
  png_infop info = NULL;

  FILE* f = fopen(filename, "rb");
  if (!f) {
    perror("fopen");
    return 1;
  }

  png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                               NULL,
                               swallow_errors_function,
                               swallow_warnings_function);
  if (setjmp(png_jmpbuf(png))) {
    /* Error happened. */
    printf("invalid image\n");
    ret = 1;
    goto out;
  }
  info = png_create_info_struct(png);

  png_init_io(png, f);
  if (transform) {
    /* Always dump as RGBA. */
    png_set_add_alpha(png, 0xFF, PNG_FILLER_AFTER);
    int flags = 0
      | PNG_TRANSFORM_STRIP_16
      | PNG_TRANSFORM_PACKING
      | PNG_TRANSFORM_EXPAND
      | PNG_TRANSFORM_GRAY_TO_RGB;
    png_read_png(png, info, flags, NULL);
    int interlaced = png_get_interlace_type(png, info) != PNG_INTERLACE_NONE;
    if (!interlaced) {
      printf("decoded bytes:\n");
      dump_png_rows(png, info);
    }

    png_text* texts = NULL;
    int comments = png_get_text(png, info, &texts, NULL);
    int i;
    for (i = 0; i < comments; ++i)
      dump_comment(texts[i].key, texts[i].text, texts[i].text_length);
  } else {
    png_read_png(png, info, PNG_TRANSFORM_IDENTITY, NULL);
    dump_png_metadata(png, info);
    int interlaced = png_get_interlace_type(png, info) != PNG_INTERLACE_NONE;
    if (!interlaced) {
      printf("raw data bytes:\n");
      dump_png_rows(png, info);
    }
  }

  ret = 0;

 out:
  if (f)
    fclose(f);
  png_destroy_read_struct(&png, &info, NULL);

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
