#include "crc.h"

/* Make the table for a fast CRC. */
void crc_init_table(crc_table table) {
  uint32_t c;
  int n, k;

  for (n = 0; n < 256; n++) {
    c = (uint32_t)n;
    for (k = 0; k < 8; k++) {
      if (c & 1)
        c = 0xedb88320L ^ (c >> 1);
      else
        c = c >> 1;
    }
    table[n] = c;
  }
}

static uint32_t update_crc(const crc_table table,
                           uint32_t crc,
                           const unsigned char* buf,
                           int len) {
  uint32_t c = crc;
  int n;

  for (n = 0; n < len; n++) {
    c = table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
  }
  return c;
}

uint32_t crc_compute(const crc_table table, const void* type,
                     const void* buf, int len) {
  return update_crc(table, update_crc(table, 0xffffffffL, type, 4), buf, len) ^ 0xffffffffL;
}
