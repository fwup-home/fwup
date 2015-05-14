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
    src/fwup_genkeys.c \
    src/fwup_sign.c \
    src/fwup_verify.c

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
    src/fwup_genkeys.h \
    src/fwup_sign.h \
    src/fwup_verify.h

OTHER_FILES += \
    fwupdate.conf \
    README.md \
    TODO.md

DISTFILES += \
    tests/001_simple_fw.test \
    tests/002_resource_subdir.test \
    tests/003_write_offset.test \
    tests/common.sh \
    tests/005_define.test \
    tests/006_metadata.test \
    tests/007_mbr.test \
    tests/008_partial_mbr.test \
    tests/009_metadata_cmdline.test \
    tests/010_fat_mkfs.test \
    tests/011_fat_setlabel.test \
    tests/012_fat_write.test \
    tests/013_fat_mv.test \
    tests/014_fat_cp.test \
    tests/015_fat_bad_sha2.test \
    tests/016_raw_bad_sha2.test \
    tests/017_fat_rm.test \
    tests/018_numeric_progress.test \
    tests/019_quiet_progress.test \
    tests/020_normal_progress.test \
    tests/021_create_keys.test \
    tests/022_signed_fw.test \
    tests/023_missing_sig.test \
    tests/024_metadata_sig.test \
    tests/025_bad_sig.test \
    tests/026_sign_again.test \
    tests/Makefile.am \
    tests/004_env_vars.test
