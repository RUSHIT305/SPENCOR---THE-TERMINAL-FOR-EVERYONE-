# Linux portability and support matrix

SPENCER cannot honestly guarantee execution on **every** Linux distribution: Linux distributions vary in CPU architecture, kernel age, C library, graphics stack, display server, GTK/GLib runtime, and package-manager policy. This project therefore defines support by tested distribution families, artifact type, architecture, and required runtime capabilities.

## Support model

| Artifact | Intended distribution reach | Architecture | Runtime model | Verification standard |
|---|---|---:|---|---|
| Flatpak bundle | Distributions with Flatpak and a compatible GNOME runtime | x86_64 | GNOME Platform runtime plus sandbox permissions | Install, launch, PTY, resize, and clean exit smoke test |
| AppImage | Modern x86_64 distributions with a working FUSE/`--appimage-extract-and-run` path and compatible graphics stack | x86_64 | Bundled application files; host kernel, GPU, display server, and selected system interfaces remain required | Artifact launch under X11 and shell-output smoke test |
| Debian package | Debian-family systems with GTK 4 and GLib packages available from the configured repositories | x86_64 | Native system dependencies | Package metadata, install, launch, PTY, and uninstall checks |
| RPM package | Fedora/RHEL-family and compatible RPM systems with GTK 4 and GLib packages available | x86_64 | Native system dependencies | RPM metadata, install, launch, PTY, and uninstall checks |
| Source archive | Any Linux distribution meeting the documented compiler, CMake, GTK 4, GLib, Pango, Cairo, and POSIX requirements | Native | Built against the target distribution’s libraries | Clean build and core/PTY tests on representative images |

## Representative release targets

The release matrix targets current, supported representatives rather than claiming that one binary covers all historical distributions.

| Family | Representative CI target | Native package | Portable fallback |
|---|---|---|---|
| Debian-family | Ubuntu LTS and Debian stable | `.deb` | Flatpak, AppImage, source |
| Fedora/RPM-family | Fedora stable | `.rpm` | Flatpak, AppImage, source |
| RHEL-compatible | Rocky Linux / AlmaLinux where GTK 4 is available | `.rpm` or source | Flatpak, AppImage |
| Arch-family | Arch Linux rolling | source | Flatpak, AppImage |
| openSUSE-family | openSUSE Tumbleweed | `.rpm` or source | Flatpak, AppImage |
| Universal runtime | Any distribution with Flatpak | `.flatpak` | AppImage, source |

## What “functional” means here

For each release artifact, functional means that the artifact installs or launches on its target, opens a GTK window, starts the configured shell through a POSIX PTY, displays shell output, accepts input, propagates a resize, and terminates the child process cleanly. It does not mean that every terminal-control sequence, every GPU driver, every display server, every CPU architecture, or every legacy distribution is supported.

## Known boundaries

The first portability expansion remains x86_64-only. ARM64, ARMhf, ppc64le, s390x, and riscv64 need separate builds and CI evidence. Old distributions whose GTK runtime predates GTK 4, distributions with nonstandard display stacks, systems without a usable PTY/session environment, and systems whose C library or kernel cannot satisfy the binary’s requirements must use a source build or a separately produced target build.

## References

[1]: https://docs.fedoraproject.org/en-US/packaging-guidelines/ "Fedora Packaging Guidelines"
[2]: https://rpm-packaging-guide.github.io/ "RPM Packaging Guide"
[3]: https://docs.flatpak.org/en/latest/ "Flatpak Documentation"
[4]: https://cmake.org/cmake/help/latest/cpack_gen/rpm.html "CMake CPack RPM Generator"
