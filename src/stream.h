#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

typedef struct {
  const uint8_t* buf;
  int len;
} stream;

static void stream_consume(stream* stream, int bytes) {
  stream->buf += bytes;
  stream->len -= bytes;
}
static uint32_t stream_read_uint32(stream* stream) {
  uint32_t i;
  memcpy(&i, stream->buf, 4);
  stream_consume(stream, 4);
  return ntohl(i);
}
static uint16_t stream_read_uint16(stream* stream) {
  uint16_t i;
  memcpy(&i, stream->buf, 2);
  stream_consume(stream, 2);
  return ntohs(i);
}
static uint8_t stream_read_byte(stream* stream) {
  uint8_t i = stream->buf[0];
  stream_consume(stream, 1);
  return i;
}

static void stream_fill_buffer(stream* src, uint8_t* buf,
                               int* have_len, int want_len) {
  int want_bytes = want_len - *have_len;
  int take_bytes = min(src->len, want_bytes);

  memcpy(buf + *have_len, src->buf, take_bytes);
  *have_len += take_bytes;
  stream_consume(src, take_bytes);
}
