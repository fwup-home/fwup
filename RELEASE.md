# Release checklist

1. Review the versions and hashes in `scripts/third_party_versions.sh` and
   `scripts/third_party.sha256`, including the pinned Raspberry Pi toolchain
   revision. Check for relevant dependency security updates.
2. Run the Coverity scan from the `coverity` branch and address new findings.
3. Update `CHANGELOG.md` with the release changes. Remove `-dev` from the
   release heading and `VERSION`; these versions must exactly match the tag.
4. Update the versioned release links in `README.md`. Include only assets that
   the release workflow publishes.
5. Commit the release changes and create a signed annotated tag:

   ```sh
   version=$(cat VERSION)
   git tag -s "v$version" -m "v$version release"
   git push origin main "v$version"
   ```

   GitHub must show the tag signature as **Verified**. The release workflow
   rejects lightweight, annotated-but-unsigned, and unverified tags.
6. Wait for the `CI` workflow to complete. All build and test matrix jobs must
   pass before the release job creates a draft. The release preflight verifies
   the tag, `VERSION`, and the complete expected asset set.
7. Download the draft assets and verify their checksums:

   ```sh
   gh release download "v$(cat VERSION)" --dir release-assets
   (cd release-assets && shasum -a 256 -c SHA256SUMS)
   ```

8. Verify GitHub's build provenance for each executable and package:

   ```sh
   for artifact in release-assets/*.deb \
                   release-assets/*.exe \
                   release-assets/*.nupkg \
                   release-assets/*.tar.gz; do
       gh attestation verify "$artifact" --repo fwup-home/fwup
   done
   ```

9. Review the generated SPDX SBOM, release notes, and these expected assets:
   AMD64, ARM64, and ARMHF Debian packages; the versioned Windows x86-64
   executable; the Chocolatey package; the source tarball; `CHANGELOG.md`;
   `SHA256SUMS`; and the SPDX JSON file.
10. Publish the draft. Release immutability prevents changing the tag or assets
    after publication, so corrections require a new version.
11. Start the next development cycle by adding the next `CHANGELOG.md` section,
    restoring the `-dev` suffix in `VERSION`, and pushing those changes.
