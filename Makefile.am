AM_CFLAGS = -Wall -Wno-unused

noinst_LIBRARIES = libsfpng.a

libsfpng_a_SOURCES = crc.c crc.h sfpng.c sfpng.h

noinst_PROGRAMS = png2pnm

png2pnm_SOURCES = png2pnm.c
png2pnm_LDADD = libsfpng.a -lz

check_PROGRAMS = sfpng-dumper libpng-dumper
sfpng_dumper_SOURCES = sfpng-dumper.c
sfpng_dumper_LDADD = libsfpng.a -lz
libpng_dumper_SOURCES = libpng-dumper.c
libpng_dumper_LDADD = -lpng

TESTS = run-test-suite.sh
