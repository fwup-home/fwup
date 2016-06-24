# Release checklist

  1. Pre-release: Check version numbers on static link script
     (`scripts/download_deps.sh`). There may be security updates! Verify that
     they work.
  2. Run a coverity scan. See https://scan.coverity.com/projects/fhunleth-fwup.
     Scans must be manually uploaded since the travis-ci integration isn't
     working. Fix issues.
  3. Update CHANGELOG.md with a bulletpoint list of new features and bug fixes
  4. Remove the `-dev` from the version numbers in `CHANGELOG.md` and `configure.ac`. If
     doing an `rc` release, mark them appropriately.
  5. For non-rc releases, update the version numbers in `README.md`. They'll be
     broken links until the release files are uploaded, but "that's ok".
  6. Tag
  7. Push last commit(s) *and* tag to GitHub
  8. On a Linux machine, create the `.deb` and `.rpm` packages. Do this by
     starting with a clean release: `git clean -fdx`. Then run `./autogen.sh` and
     `scripts/build_pkg.sh`
  9. Wait for the Travis builds to complete successfully. They should work since
     no code changes were made, but wait to be safe.
  10. Copy the latest CHANGELOG.md entry to the GitHub releases description.
  11. Upload the `.deb`, `.rpm`, and source tarball to the release. Check the
     links in `README.md` (from GitHub) to make sure they work.
  12. Start the next dev cycle. Start a new section in `CHANGELOG.md` and
      update the version in `configure.ac` to a `-dev` version.
  13. Push changes up to GitHub

