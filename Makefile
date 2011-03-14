CFLAGS=-g -Wall -Werror -Wno-unused
LDFLAGS=-lz

sfpng_test: sfpng.o sfpng_test.o crc.o
