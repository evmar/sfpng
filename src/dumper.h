const char* dumper_color_type_names[] = {
  "grayscale",       NULL, "truecolor", "indexed",
  "grayscale alpha", NULL, "truecolor alpha"
};

static void dump_comment(const char* key, const char* val, int val_len) {
  printf("comment: %s\n  ", key);
  int i;
  for (i = 0; i < val_len; ++i) {
    if (0x20 <= val[i] && val[i] < 0x7F)
      printf("%c", val[i]);
    else
      printf("\\x%02x", (int)(unsigned char)val[i]);
  }
  printf("\n");
}
