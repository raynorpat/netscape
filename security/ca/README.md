# Packaged TLS trust store

`cacert.pem` is the unmodified Mozilla-derived curl CA Extract bundle dated
2026-10-20, retrieved 2026-11-04. It contains 118 certificates.

- Exact source: <https://curl.se/ca/cacert-2026-10-20.pem>
- Published checksum: <https://curl.se/ca/cacert-2026-10-20.pem.sha256>
- SHA-256: `796f1389bfea13c0ed78f663b765baaff54a508e5483786bd6ece73c5e3e3cbe`
- CA Extract service and conversion description: <https://curl.se/docs/caextract.html>
- Mozilla source: <https://raw.githubusercontent.com/mozilla-firefox/firefox/refs/heads/release/security/nss/lib/ckfw/builtins/certdata.txt>

The converted certificate data is distributed under the Mozilla Public License
2.0, the same license as Mozilla's source file. See [MPL 2.0](LICENSE-MPL-2.0.txt).
The bundle header preserves the upstream source attribution, conversion date,
and mk-ca-bundle.pl version. This is an extracted certificate list: Firefox's
additional root restrictions are not carried in the PEM bundle. See curl's
CA Extract description for this limitation.

The native Windows Runtime install places this file at
`share/netscape/ca/cacert.pem`, relative to the installation prefix. Updates
should replace the complete file with a reviewed dated CA Extract bundle,
update this provenance and checksum, and run the full trust-loader tests.
An explicitly selected CA file replaces the packaged roots; trust stores are
never merged implicitly. Loading a bundle is all-or-nothing, including any
unsupported certificate. No background updater is provided.

The loader accepts blank lines, `#` comments, and optional certificate label
lines followed by a line of at least three `=` signs before their certificate.
Blank lines/comments may separate these parts. Labels permit non-control text
bytes, including the shipped UTF-8 NetLock label. Each PEM object must use the
exact `CERTIFICATE` banners. Pending labels, separators, malformed blocks,
unknown PEM objects, control bytes, and trailing non-comment text reject the
entire file. The limits are 4 MiB per file, 512 certificates, and 64 KiB of DER
per certificate. The adapter owns immutable copies of each subject DN and RSA
or supported NIST P-256/P-384/P-521 public key; parser buffers are discarded.
Reference operations run on the owning adapter event loop.
