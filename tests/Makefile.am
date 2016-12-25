ACLOCAL_AMFLAGS=-I m4

noinst_PROGRAMS=framing-helper
framing_helper_SOURCES=framing-helper.c

TESTS = 001_simple_fw.test \
	002_resource_subdir.test \
	003_write_offset.test \
	004_env_vars.test \
	005_define.test \
	006_metadata.test \
	007_mbr.test \
	008_partial_mbr.test \
	009_metadata_cmdline.test \
	010_fat_mkfs.test \
	011_fat_setlabel.test \
	012_fat_write.test \
	013_fat_mv.test \
	014_fat_cp.test \
	015_fat_bad_sha2.test \
	016_raw_bad_sha2.test \
	017_fat_rm.test \
	018_numeric_progress.test \
	019_quiet_progress.test \
	020_normal_progress.test \
	021_create_keys.test \
	022_signed_fw.test \
	023_missing_sig.test \
	024_metadata_sig.test \
	025_bad_sig.test \
	026_sign_again.test \
        027_fat_timestamp.test \
        028_osip.test \
	029_5G_offset.test \
	030_2T_offset.test \
	031_fat_mkfs_5G.test \
	032_fat_2T_offset.test \
	033_bad_host_path.test \
        034_missing_host_path.test \
        035_streaming.test \
        036_streaming_signed_fw.test \
        037_streaming_bad_sig.test \
        038_write_15M.test \
	039_upgrade.test \
	041_version.test \
	042_fat_am335x.test \
	043_fat_touch.test \
	044_require_file_exist.test \
	045_legacy_require_partition1.test \
	046_unknown_create_fails.test \
	047_unknown_apply_succeeds.test \
	048_assert_size_less_than_success.test \
	049_assert_size_less_than_fail.test \
	050_assert_size_greater_than_success.test \
	051_assert_size_greater_than_fail.test \
	052_file_concatenation.test \
	053_framed_progress.test \
	054_framed_metadata.test \
	055_metadata_partial_file.test \
	056_list_tasks.test \
	057_list_tasks_empty.test \
	058_framed_list_tasks.test \
	059_framed_error.test \
	060_framed_streaming.test \
	061_framed_partial_metadata.test \
	062_long_meta_conf.test \
	063_detect.test \
	064_mbr_bootcode.test \
	065_fat_mkdir.test \
	066_fat_attrib.test \
	067_usage.test \
	068_readonly_output_error.test \
	069_define_bang.test \
	070_bad_mbr_offset.test \
	071_big_mbr_offset.test \
	072_compression_works.test \
	073_multistep_fat.test \
	074_fat_cache_fail.test \
	075_big_fat_fs.test \
	076_utf8_metadata.test \
	077_fat_empty_file.test \
	078_partial_mbr2.test \
	079_uboot_setenv.test \
	080_uboot_empty_env.test \
	081_uboot_clearenv.test \
	082_uboot_upgrade.test \
	083_uboot_unsetenv.test \
	084_corrupt_uboot.test \
	085_raw_memset.test \
	086_math.test \
	087_crypto_compat.test \
	088_missing_hash.test \
	089_dev_null.test \
	095_sparse_apply.test \
	096_missing_resource.test \
	097_reproduceable.test \
	098_sparse_empty.test \
	099_sparse_lasthole.test \
	100_easy_option.test \
	101_progress_range.test \
	102_multi_requires.test \
	103_error_msg.test \
	104_info_msg.test

if HAVE_SPARSE_SEEK
TESTS += \
	090_sparse_write.test \
	091_sparse_write2.test \
	092_sparse_write3.test \
	093_sparse_concat.test \
	094_sparse_concat2.test
endif

EXTRA_DIST = $(TESTS) common.sh 1K.bin 1K-corrupt.bin 150K.bin
