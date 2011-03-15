AM_CFLAGS = -Wall -Wno-unused

noinst_LIBRARIES = libsfpng.a

libsfpng_a_SOURCES = crc.c crc.h sfpng.c sfpng.h

noinst_PROGRAMS = sfpng_test
sfpng_test_SOURCES = sfpng_test.c
sfpng_test_LDADD = libsfpng.a -lz
