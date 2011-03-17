#include <libpng/png.h>

static void swallow_warnings_function(png_structp png,
                                      png_const_charp msg) {
  /* Ignore. */
}
static void swallow_errors_function(png_structp png,
                                    png_const_charp msg) {
  /* Don't print anything, but we must at least longjmp for libpng's sake. */
  longjmp(png_jmpbuf(png), 1);
}

static void dump_bytes(png_byte** rows, int stride, int height) {
  int x, y;
  for (y = 0; y < height; ++y) {
    printf("%3d:", y);
    for (x = 0; x < stride; ++x)
      printf("%02x", (int)rows[y][x]);
    printf("\n");
  }
}

static void dump_png(png_structp png, png_infop info) {
  int width = png_get_image_width(png, info);
  int height = png_get_image_height(png, info);
  printf("dimensions: %dx%d\n", width, height);
  printf("bit depth: %d  color type: %d\n",
         png_get_bit_depth(png, info),
         png_get_color_type(png, info));
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

  if (!interlaced) {
    png_byte** row_pointers = png_get_rows(png, info);
    int stride = png_get_rowbytes(png, info);
    dump_bytes(row_pointers, stride, height);
  }
}

static int dump_file(const char* filename) {
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
    goto out;
  }
  info = png_create_info_struct(png);

  png_init_io(png, f);
  png_read_png(png, info, PNG_TRANSFORM_IDENTITY, NULL);
  dump_png(png, info);
  /*png_read_png(png, info, PNG_TRANSFORM_STRIP_16, NULL);
    dump_png(png, info);*/

 out:
  if (f)
    fclose(f);
  png_destroy_read_struct(&png, &info, NULL);
}

int main(int argc, char* argv[]) {
  const char* filename = argv[1];
  if (!filename) {
    fprintf(stderr, "usage: %s pngfile\n", argv[0]);
    return 1;
  }

  return dump_file(filename);
}
