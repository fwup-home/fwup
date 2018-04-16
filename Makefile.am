ACLOCAL_AMFLAGS=-I m4

if ENABLE_SCRIPTS
bin_SCRIPTS = img2fwup dir2fwup
endif

SUBDIRS=src tests docs
EXTRA_DIST=README.md \
	   VERSION \
	   CHANGELOG.md \
	   LICENSE \
	   img2fwup \
	   dir2fwup \
	   scripts/build_pkg.sh \
	   scripts/build_deps.sh \
	   scripts/ci_after_success.sh \
	   scripts/ci_build.sh \
	   scripts/ci_deploy.sh \
	   scripts/ci_install_deps.sh \
	   scripts/common.sh \
	   scripts/download_deps.sh \
	   scripts/fwup.nuspec \
	   scripts/third_party_versions.sh
