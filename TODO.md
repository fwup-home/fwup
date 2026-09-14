## Release hardening

After the next release, review and land the remaining changes from `harden` one
at a time:

- Apply least-privilege workflow permissions and pin container/toolchain inputs.
- Validate signed release tags and the exact expected release asset set.
- Publish SHA-256 checksums, an SPDX SBOM, and artifact attestations.
- Fail releases with missing changelog notes and create drafts with `gh`.
- Update the release and download documentation for the hardened workflow.
