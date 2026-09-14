#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 4 ]]; then
    echo "Usage: $0 OUTPUT VERSION TAG COMMIT" >&2
    exit 1
fi

OUTPUT=$1
FWUP_VERSION=$2
RELEASE_TAG=$3
COMMIT=$4
BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

source "$BASE_DIR/scripts/third_party_versions.sh"

if [[ -z "${SOURCE_DATE_EPOCH:-}" ]]; then
    SOURCE_DATE_EPOCH=$(git -C "$BASE_DIR" log -1 --format=%ct)
fi
if CREATED=$(date -u --date="@$SOURCE_DATE_EPOCH" +%Y-%m-%dT%H:%M:%SZ 2>/dev/null); then
    :
else
    CREATED=$(date -u -r "$SOURCE_DATE_EPOCH" +%Y-%m-%dT%H:%M:%SZ)
fi

jq -n \
    --arg version "$FWUP_VERSION" \
    --arg tag "$RELEASE_TAG" \
    --arg commit "$COMMIT" \
    --arg created "$CREATED" \
    --arg zlib_version "$ZLIB_VERSION" \
    --arg libarchive_version "$LIBARCHIVE_VERSION" \
    --arg confuse_version "$CONFUSE_VERSION" \
    '{
        spdxVersion: "SPDX-2.3",
        dataLicense: "CC0-1.0",
        SPDXID: "SPDXRef-DOCUMENT",
        name: ("fwup-" + $version + "-release"),
        documentNamespace: ("https://github.com/fwup-home/fwup/releases/tag/" + $tag + "/sbom/" + $commit),
        creationInfo: {
            created: $created,
            creators: ["Tool: fwup scripts/generate_sbom.sh"]
        },
        packages: [
            {
                name: "fwup",
                SPDXID: "SPDXRef-Package-fwup",
                versionInfo: $version,
                downloadLocation: ("git+https://github.com/fwup-home/fwup.git@" + $commit),
                filesAnalyzed: false,
                licenseConcluded: "NOASSERTION",
                licenseDeclared: "Apache-2.0",
                copyrightText: "NOASSERTION"
            },
            {
                name: "zlib",
                SPDXID: "SPDXRef-Package-zlib",
                versionInfo: $zlib_version,
                downloadLocation: ("https://github.com/madler/zlib/releases/download/v" + $zlib_version + "/zlib-" + $zlib_version + ".tar.gz"),
                filesAnalyzed: false,
                licenseConcluded: "NOASSERTION",
                licenseDeclared: "Zlib",
                copyrightText: "NOASSERTION"
            },
            {
                name: "libarchive",
                SPDXID: "SPDXRef-Package-libarchive",
                versionInfo: $libarchive_version,
                downloadLocation: ("https://libarchive.org/downloads/libarchive-" + $libarchive_version + ".tar.gz"),
                filesAnalyzed: false,
                licenseConcluded: "NOASSERTION",
                licenseDeclared: "NOASSERTION",
                copyrightText: "NOASSERTION"
            },
            {
                name: "libconfuse",
                SPDXID: "SPDXRef-Package-libconfuse",
                versionInfo: $confuse_version,
                downloadLocation: ("https://github.com/libconfuse/libconfuse/releases/download/v" + $confuse_version + "/confuse-" + $confuse_version + ".tar.gz"),
                filesAnalyzed: false,
                licenseConcluded: "NOASSERTION",
                licenseDeclared: "ISC",
                copyrightText: "NOASSERTION"
            },
            {
                name: "Monocypher",
                SPDXID: "SPDXRef-Package-Monocypher",
                versionInfo: "3.1.3",
                downloadLocation: "NOASSERTION",
                filesAnalyzed: false,
                licenseConcluded: "NOASSERTION",
                licenseDeclared: "BSD-2-Clause OR CC0-1.0",
                copyrightText: "NOASSERTION"
            },
            {
                name: "semver.c",
                SPDXID: "SPDXRef-Package-semver-c",
                versionInfo: "0.2.0",
                downloadLocation: "NOASSERTION",
                filesAnalyzed: false,
                licenseConcluded: "NOASSERTION",
                licenseDeclared: "MIT",
                copyrightText: "NOASSERTION"
            },
            {
                name: "xdelta3",
                SPDXID: "SPDXRef-Package-xdelta3",
                versionInfo: "3.1.1",
                downloadLocation: "NOASSERTION",
                filesAnalyzed: false,
                licenseConcluded: "NOASSERTION",
                licenseDeclared: "Apache-2.0",
                copyrightText: "NOASSERTION"
            },
            {
                name: "FatFs",
                SPDXID: "SPDXRef-Package-FatFs",
                versionInfo: "R0.15",
                downloadLocation: "NOASSERTION",
                filesAnalyzed: false,
                licenseConcluded: "NOASSERTION",
                licenseDeclared: "NOASSERTION",
                copyrightText: "NOASSERTION"
            },
            {
                name: "TF-PSA-Crypto AES",
                SPDXID: "SPDXRef-Package-TF-PSA-Crypto-AES",
                versionInfo: "5ed7d7245fdc96e3e1610f5f4854fa78ed847ea0",
                downloadLocation: "git+https://github.com/Mbed-TLS/TF-PSA-Crypto.git@5ed7d7245fdc96e3e1610f5f4854fa78ed847ea0",
                filesAnalyzed: false,
                licenseConcluded: "NOASSERTION",
                licenseDeclared: "Apache-2.0 OR GPL-2.0-or-later",
                copyrightText: "NOASSERTION"
            },
            {
                name: "libsodium Base64 utilities",
                SPDXID: "SPDXRef-Package-libsodium-base64",
                versionInfo: "c229663acf6083867b8df35aeb8647e6d4db1270",
                downloadLocation: "git+https://github.com/jedisct1/libsodium.git@c229663acf6083867b8df35aeb8647e6d4db1270",
                filesAnalyzed: false,
                licenseConcluded: "NOASSERTION",
                licenseDeclared: "CC0-1.0",
                copyrightText: "NOASSERTION"
            },
            {
                name: "Google strptime",
                SPDXID: "SPDXRef-Package-google-strptime",
                downloadLocation: "NOASSERTION",
                filesAnalyzed: false,
                licenseConcluded: "NOASSERTION",
                licenseDeclared: "Apache-2.0",
                copyrightText: "NOASSERTION"
            },
            {
                name: "mtd-utils UBI userspace API header",
                SPDXID: "SPDXRef-Package-mtd-utils-ubi-header",
                downloadLocation: "NOASSERTION",
                filesAnalyzed: false,
                licenseConcluded: "NOASSERTION",
                licenseDeclared: "GPL-2.0-or-later WITH Linux-syscall-note",
                copyrightText: "NOASSERTION"
            }
        ],
        relationships: [
            {spdxElementId: "SPDXRef-DOCUMENT", relationshipType: "DESCRIBES", relatedSpdxElement: "SPDXRef-Package-fwup"},
            {spdxElementId: "SPDXRef-Package-fwup", relationshipType: "DEPENDS_ON", relatedSpdxElement: "SPDXRef-Package-zlib"},
            {spdxElementId: "SPDXRef-Package-fwup", relationshipType: "DEPENDS_ON", relatedSpdxElement: "SPDXRef-Package-libarchive"},
            {spdxElementId: "SPDXRef-Package-fwup", relationshipType: "DEPENDS_ON", relatedSpdxElement: "SPDXRef-Package-libconfuse"},
            {spdxElementId: "SPDXRef-Package-fwup", relationshipType: "DEPENDS_ON", relatedSpdxElement: "SPDXRef-Package-Monocypher"},
            {spdxElementId: "SPDXRef-Package-fwup", relationshipType: "DEPENDS_ON", relatedSpdxElement: "SPDXRef-Package-semver-c"},
            {spdxElementId: "SPDXRef-Package-fwup", relationshipType: "DEPENDS_ON", relatedSpdxElement: "SPDXRef-Package-xdelta3"},
            {spdxElementId: "SPDXRef-Package-fwup", relationshipType: "DEPENDS_ON", relatedSpdxElement: "SPDXRef-Package-FatFs"},
            {spdxElementId: "SPDXRef-Package-fwup", relationshipType: "DEPENDS_ON", relatedSpdxElement: "SPDXRef-Package-TF-PSA-Crypto-AES"},
            {spdxElementId: "SPDXRef-Package-fwup", relationshipType: "DEPENDS_ON", relatedSpdxElement: "SPDXRef-Package-libsodium-base64"},
            {spdxElementId: "SPDXRef-Package-fwup", relationshipType: "DEPENDS_ON", relatedSpdxElement: "SPDXRef-Package-google-strptime"},
            {spdxElementId: "SPDXRef-Package-fwup", relationshipType: "DEPENDS_ON", relatedSpdxElement: "SPDXRef-Package-mtd-utils-ubi-header"}
        ]
    }' > "$OUTPUT"
