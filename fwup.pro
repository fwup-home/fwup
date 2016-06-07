# This file is handy for debugging fwup in QtCreator

QT       += core

QT       -= gui

TARGET = fwup
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += \
    3rdparty/fatfs/src/ff.c \
    3rdparty/fatfs/src/option/unicode.c \
    src/fwup.c \
    src/fatfs.c \
    src/mbr.c \
    src/cfgfile.c \
    src/util.c \
    src/fwup_create.c \
    src/functions.c \
    src/fwup_apply.c \
    src/fwup_list.c \
    src/fwup_metadata.c \
    src/fwfile.c \
    src/fwup_genkeys.c \
    src/fwup_sign.c \
    src/fwup_verify.c \
    src/block_writer.c \
    src/fat_cache.c \
    src/mmc_osx.c \
    src/mmc_linux.c \
    src/requirement.c \
    src/cfgprint.c

osx {
    INCLUDEPATH += /usr/local/include /usr/local/opt/libarchive/include
    LIBS += -L/usr/local/lib -L/usr/local/opt/libarchive/lib
    SOURCES +=

    LIBS += -framework CoreFoundation -framework DiskArbitration
}

LIBS += -lconfuse -larchive -lsodium

HEADERS += \
    3rdparty/fatfs/src/ff.h \
    3rdparty/fatfs/src/ffconf.h \
    3rdparty/fatfs/src/integer.h \
    3rdparty/fatfs/src/diskio.h \
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
    src/fwup_genkeys.h \
    src/fwup_sign.h \
    src/fwup_verify.h \
    src/block_writer.h \
    src/fat_cache.h \
    src/requirement.h \
    src/cfgprint.h

OTHER_FILES += \
    fwupdate.conf \
    README.md \
    TODO.md

DISTFILES += \
    tests/Makefile.am \
    tests/common.sh \
    tests/001_simple_fw.test \
    tests/002_resource_subdir.test \
    tests/003_write_offset.test \
    tests/004_env_vars.test \
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
    tests/027_fat_timestamp.test \
    tests/028_osip.test \
    tests/029_5G_offset.test \
    tests/030_2T_offset.test \
    tests/031_fat_mkfs_5G.test \
    tests/032_fat_2T_offset.test \
    tests/033_bad_host_path.test \
    tests/034_missing_host_path.test \
    tests/035_streaming.test \
    tests/036_streaming_signed_fw.test \
    tests/037_streaming_bad_sig.test \
    tests/038_write_15M.test \
    tests/039_upgrade.test \
    tests/040_create_mini_fw.test \
    CHANGELOG.md \
    src/fwup.h2m \
    tests/041_version.test \
    tests/042_fat_am335x.test \
    tests/043_fat_touch.test \
    tests/044_require_file_exist.test \
    tests/045_legacy_require_partition1.test \
    tests/046_unknown_create_fails.test \
    tests/047_unknown_apply_succeeds.test \
    tests/048_assert_size_less_than_success.test \
    tests/049_assert_size_less_than_fail.test \
    tests/050_assert_size_greater_than_success.test \
    tests/051_assert_size_greater_than_fail.test
