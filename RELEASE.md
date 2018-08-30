# Release checklist

 1. Pre-release: Check version numbers on static link script
    (`scripts/third_party_versions.sh`). There may be security updates! Verify
    that they work.
 2. Run a coverity scan by changing to the coverity branch and fast forwarding
    it up to the latest master. Push and wait for travis-ci. Eventually the
    results will be at https://scan.coverity.com/projects/fhunleth-fwup. Fix
    issues.
 3. Update `CHANGELOG.md` with a bullet-point list of new features and bug fixes
 4. Remove the `-dev` from the version numbers in `CHANGELOG.md` and `VERSION`.
    If doing an `rc` release, mark them appropriately.
 5. For non-rc releases, update the version numbers in `README.md`. They'll be
    broken links until the release files are uploaded, but "that's ok".
 6. Tag
 7. Push last commit(s) *and* tag to GitHub
 8. Copy the latest `CHANGELOG.md` entry to the GitHub releases description.
    Save it as a "draft". You have until the Travis builds complete to do this,
    so don't delay. As soon as Travis uploads its artifacts, GitHub will send
    release emails out with new text.
 9. Wait for the Travis builds to complete successfully. They should work since
    no code changes were made, but wait to be safe.
 10. Check that the `.deb`, `.rpm`, `.exe`, and source tarball were uploaded
     properly to GitHub.
 11. Start the next dev cycle. Start a new section in `CHANGELOG.md` and update
     the version in `VERSION` to a `-dev` version.
 12. Push changes up to GitHub
