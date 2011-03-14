#include <stdint.h>

typedef uint32_t crc_table[256];

void crc_init_table(crc_table table);

/* |type| is the 4-byte chunk type used in the CRC. */
uint32_t crc_compute(const crc_table table, const void* type,
                     const void* buf, int len);
