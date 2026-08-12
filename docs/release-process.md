# Release Process

A release is a verified package of a specific source revision. Do not create a public release merely because a version number has been chosen. The current repository automation verifies Debian, RPM, Flatpak, AppImage, source, metadata, and checksum paths before publication.

## Pre-release gate

| Gate | Required command or evidence |
|---|---|
| Clean source tree | `git status --short` has no unintended changes |
| Debug correctness | `cmake --preset debug`, build, and CTest pass with sanitizers |
| Release correctness | `cmake --preset release`, build, and CTest pass |
| Linux metadata | `desktop-file-validate` and `appstreamcli validate --no-net` pass |
| Packages | `cpack -G DEB` and `cpack -G RPM` produce the expected architecture-specific packages |
| Security | Review `SECURITY.md` and ensure no credentials, logs, or private config are included |
| Documentation | README, changelog, compatibility notes, and installation instructions match actual behavior |

## Tag and release

After all gates pass, update `CHANGELOG.md`, commit the release state, and use a semantic version tag.

```bash
git tag -a v0.2.0 -m "SPENCER 0.2.0"
git push origin main --tags
```

The release workflow should rebuild from the tag in clean Ubuntu and Fedora-family environments, execute tests, generate native and portable packages, calculate SHA-256 checksums, and attach only successful artifacts to the GitHub Release. Signing keys must be supplied as CI secrets and must never be committed.

## Post-release validation

Install the generated package in a clean Linux environment, launch SPENCER through its desktop entry, confirm the shell starts, verify configuration creation, and uninstall it. Record the exact environments tested and any known compatibility limitations in release notes.
