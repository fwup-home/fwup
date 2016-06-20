# Release checklist

  1. Pre-release: Check version numbers on static link script
     (`scripts/download_deps.sh`). There may be security updates! Verify that
     they work.
  2. Update CHANGELOG.md with a bulletpoint list of new features and bug fixes
  3. Remove the `-dev` from the version numbers in `CHANGELOG.md` and `configure.ac`. If
     doing an `rc` release, mark them appropriately.
  4. For non-rc releases, update the version numbers in `README.md`. They'll be
     broken links until the release files are uploaded, but "that's ok".
  5. Tag
  6. Push last commit(s) *and* tag to GitHub
  7. On a Linux machine, create the `.deb` and `.rpm` packages. Do this by
     starting with a clean release: `git clean -fdx`. Then run `./configure` and
     `scripts/build_pkg.sh`
  8. Wait for the Travis builds to complete successfully. They should work since
     no code changes were made, but wait to be safe.
  9. Copy the latest CHANGELOG.md entry to the GitHub releases description.
  10. Upload the `.deb`, `.rpm`, and source tarball to the release. Check the
     links in `README.md` (from GitHub) to make sure they work.
  11. Start the next dev cycle. Update the version in `CHANGELOG.md` and
      `configure.ac` to the expected next version and append `-dev`.
  12. Push changes up to GitHub


