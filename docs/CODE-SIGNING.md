# Code signing policy

## Current status

Atypoi's application for free open-source code signing through SignPath Foundation has been submitted and is awaiting review. Approval and a production signing certificate have **not** been obtained. The current v0.1.7 release is unsigned. A checksum or a self-signed certificate is not a trusted publisher signature.

If the application is approved, the intended credit is: “Free code signing provided by [SignPath.io](https://about.signpath.io/), certificate by [SignPath Foundation](https://signpath.org/).” Until then, this is a proposed provider, not a claim of sponsorship or endorsement. Foundation certificates identify SignPath Foundation as the publisher, rather than the maintainer's personal name.

## Responsibility

The project maintainer [ssrihappy](https://github.com/ssrihappy) is responsible for authoring, reviewing external contributions and approving releases. Trusted signing must only be enabled after the responsible maintainer has confirmed multi-factor authentication for GitHub and the signing service. Each Foundation signing request requires the maintainer's manual approval. The current automated unsigned release workflow is not a Foundation signing integration.

## Artifacts and build provenance

Only Atypoi-owned product artifacts built from the public repository may be submitted: `Atypoi.exe`, `Atypoi.IME.dll` and `Atypoi.Setup.exe`. Preserve the product name `Atypoi` and the same product version on all three files. Record the source commit, GitHub Actions run and artifact hashes for each signed release. Never re-sign third-party binaries as Atypoi's own work.

Atypoi derives its algorithm and data from [Seamtype](https://github.com/aimdevlee/seamtype). All upstream and toolchain notices remain in [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md). Eligibility of upstream-derived components under the Foundation's own-source/fork rules must be confirmed during review; this policy does not assert that those conditions have already been accepted.

The intended signing order is: build and test the app and input service; obtain their approved signatures; embed those signed files in the installer; obtain the installer's approved signature; verify the embedded payload and all three signatures; then generate checksums and publish a new version. Do not replace files in an existing public release.

Private keys stay in the signing provider's protected service. Do not add certificates containing private keys, passwords or API tokens to source, documentation, release assets or logs. Signing credentials must not be available to untrusted pull requests.

## Release verification

`scripts/verify-signatures.ps1 -RequireTrustedSignatures` requires a valid Windows Authenticode trust result, a timestamp, consistent publisher certificates and matching product metadata on all three files. Invalid, partially signed or mixed-publisher packages are rejected. Set the repository variable `REQUIRE_SIGNED_RELEASE=true` when the approved signing workflow is ready; release publication will then reject unsigned packages.

Current unsigned installations can receive the first signed release through the existing official GitHub URL and SHA256 check with installation consent. Signed installations also require the same signing certificate for subsequent updates. Certificate renewal or rotation needs an explicit update migration before the old certificate is retired.

## Privacy

See the [privacy policy](PRIVACY.md). Input decisions run locally. Keystrokes, document contents and application rules are not transmitted. Optional update checks contact GitHub and download new release assets. These checks can be disabled in the tray menu.

Provider requirements: [SignPath Foundation terms](https://signpath.org/terms). Approval and service availability are controlled by the provider.
