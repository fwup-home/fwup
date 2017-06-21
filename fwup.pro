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
    3rdparty/strptime.c \
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
    src/mmc_osx.c \
    src/mmc_linux.c \
    src/requirement.c \
    src/cfgprint.c \
    src/simple_string.c \
    src/archive_open.c \
    src/mmc_windows.c \
    src/uboot_env.c \
    src/crc32.c \
    src/eval_math.c \
    src/sparse_file.c \
    src/progress.c \
    src/mmc_bsd.c \
    src/resources.c \
    src/block_cache.c \
    src/pad_to_block_writer.c

osx {
    INCLUDEPATH += /usr/local/include /usr/local/opt/libarchive/include
    LIBS += -L/usr/local/lib -L/usr/local/opt/libarchive/lib
    SOURCES +=

    LIBS += -framework CoreFoundation -framework DiskArbitration
}

win32 {
    INCLUDEPATH += deps/usr/include
    LIBS += -L../fwup/deps/usr/lib -llibconfuse.a
    QMAKE_CFLAGS += -std=gnu99
}

linux {
    QMAKE_CFLAGS += -std=gnu99
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
    src/requirement.h \
    src/cfgprint.h \
    src/simple_string.h \
    src/archive_open.h \
    src/uboot_env.h \
    src/crc32.h \
    src/eval_math.h \
    src/sparse_file.h \
    src/progress.h \
    src/resources.h \
    src/block_cache.h \
    src/fatfs.h \
    src/pad_to_block_writer.h

OTHER_FILES += \
    fwupdate.conf \
    README.md

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
    tests/051_assert_size_greater_than_fail.test \
    scripts/build_pkg.sh \
    scripts/ci_build.sh \
    scripts/ci_install_deps.sh \
    scripts/download_deps.sh \
    tests/077_fat_empty_file.test \
    tests/076_utf8_metadata.test \
    tests/075_big_fat_fs.test \
    tests/074_fat_cache_fail.test \
    tests/073_multistep_fat.test \
    tests/072_compression_works.test \
    tests/071_big_mbr_offset.test \
    tests/070_bad_mbr_offset.test \
    tests/069_define_bang.test \
    tests/068_readonly_output_error.test \
    tests/067_usage.test \
    tests/066_fat_attrib.test \
    tests/065_fat_mkdir.test \
    tests/064_mbr_bootcode.test \
    tests/063_detect.test \
    tests/062_long_meta_conf.test \
    tests/061_framed_partial_metadata.test \
    tests/060_framed_streaming.test \
    tests/059_framed_error.test \
    tests/058_framed_list_tasks.test \
    tests/057_list_tasks_empty.test \
    tests/056_list_tasks.test \
    tests/055_metadata_partial_file.test \
    tests/054_framed_metadata.test \
    tests/053_framed_progress.test \
    tests/052_file_concatenation.test \
    tests/079_uboot_setenv.test \
    tests/078_partial_mbr2.test \
    tests/080_uboot_empty_env.test \
    tests/081_uboot_clearenv.test \
    tests/082_uboot_upgrade.test \
    tests/083_uboot_unsetenv.test \
    tests/084_corrupt_uboot.test \
    tests/085_raw_memset.test \
    tests/100_easy_option.test \
    tests/101_progress_range.test \
    tests/102_multi_requires.test \
    tests/103_error_msg.test \
    tests/104_info_msg.test \
    tests/105_require_path_on_device.test \
    tests/106_large_length_field.test \
    tests/107_string_resource.test \
    tests/108_resource_order.test \
    tests/109_includes.test \
    tests/086_math.test \
    tests/087_crypto_compat.test \
    tests/088_missing_hash.test \
    tests/089_dev_null.test \
    tests/090_sparse_write.test \
    tests/091_sparse_write2.test \
    tests/092_sparse_write3.test \
    tests/093_sparse_concat.test \
    tests/094_sparse_concat2.test \
    tests/095_sparse_apply.test \
    tests/096_missing_resource.test \
    tests/097_reproduceable.test \
    tests/098_sparse_empty.test \
    tests/099_sparse_lasthole.test \
    tests/110_compression_level.test \
    tests/111_streaming_exit_fast.test \
    tests/112_fat_rm_missing_file.test \
    tests/113_fat_mv_error_cases.test \
    tests/114_fat_cp_error_cases.test \
    tests/115_fat_mkdir_error_cases.test \
    tests/116_fat_setlabel_error_cases.test \
    tests/117_on_error.test \
    tests/118_fat_touch_error_cases.test \
    tests/119_require_fat_file_match.test \
    tests/120_missing_file.test
