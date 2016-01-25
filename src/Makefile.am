ACLOCAL_AMFLAGS=-I m4

bin_PROGRAMS=fwup
fwup_SOURCES=cfgfile.c \
	fatfs.c \
	fat_cache.c \
	functions.c \
	fwfile.c \
	fwup_apply.c \
	fwup.c \
	fwup_create.c \
	fwup_list.c \
	fwup_sign.c \
	fwup_verify.c \
	fwup_metadata.c \
	fwup_genkeys.c \
	block_writer.c \
	mbr.c \
	mmc_linux.c \
	mmc_osx.c \
	util.c \
	cfgfile.h \
	fatfs.h \
	functions.h \
	fat_cache.h \
	fwfile.h \
	fwup_apply.h \
	fwup_create.h \
	fwup_list.h \
	fwup_metadata.h \
	fwup_genkeys.h \
	fwup_sign.h \
	fwup_verify.h \
	block_writer.h \
	mbr.h \
	mmc.h \
	util.h \
	../3rdparty/fatfs/src/diskio.h \
	../3rdparty/fatfs/src/integer.h \
	../3rdparty/fatfs/src/ff.c \
	../3rdparty/fatfs/src/ff.h \
	../3rdparty/fatfs/src/ffconf.h \
	../3rdparty/fatfs/src/option/unicode.c

if !HAS_OPEN_MEMSTREAM
fwup_SOURCES+=../3rdparty/memstream.c \
	../3rdparty/memstream.h
endif

EXTRA_DIST=../3rdparty/fatfs/src/option/ccsbcs.c \
	../3rdparty/memstream.h \
	../3rdparty/memstream.c


fwup_CFLAGS = -Wall -D_FILE_OFFSET_BITS=64
