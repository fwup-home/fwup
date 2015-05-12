# This file is handy for debugging fwup in QtCreator

QT       += core

QT       -= gui

TARGET = fwup
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += \
    src/fwup.c \
    src/fatfs.c \
    3rdparty/fatfs/src/ff.c \
    src/mbr.c \
    src/cfgfile.c \
    src/util.c \
    src/mmc.c \
    src/fwup_create.c \
    src/functions.c \
    src/fwup_apply.c \
    src/fwup_list.c \
    src/fwup_metadata.c \
    src/fwfile.c \
    3rdparty/fatfs/src/option/unicode.c \
    src/fwup_genkeys.c

LIBS += -lconfuse -larchive -lsodium

HEADERS += \
    src/sha2.h \
    3rdparty/src/fatfs.h \
    3rdparty/fatfs/src/ff.h \
    3rdparty/fatfs/src/ffconf.h \
    src/mbr.h \
    src/cfgfile.h \
    src/util.h \
    src/mmc.h \
    src/fwup_create.h \
    src/functions.h \
    src/fwup_apply.h \
    src/fwup_list.h \
    src/fwup_metadata.h \
    src/fwfile.h \
    3rdparty/fatfs/src/integer.h \
    3rdparty/fatfs/src/diskio.h \
    src/fwup_genkeys.h

OTHER_FILES += \
    fwupdate.conf \
    README.md \
    TODO.md

DISTFILES += \
    tests/001_simple_fw.test \
    tests/002_resource_subdir.test \
    tests/003_write_offset.test \
    tests/common.sh
