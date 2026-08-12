# Building SPENCER

SPENCER currently supports Linux. The project uses CMake presets and Ninja so the development and CI commands are the same.

## Prerequisites

On Ubuntu 22.04 or a compatible Debian-family environment, install the following packages. Ubuntu 22.04 is the Debian packaging baseline because it produces dependencies compatible with newer Ubuntu releases without inheriting newer t64 or compiler-library floors.

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake ninja-build pkg-config libgtk-4-dev \
  desktop-file-utils appstream appstream-compose clang-tidy rpm rpmlint
```

| Dependency | Used for |
|---|---|
| C++20 compiler and CMake | Terminal core, executable, tests, and package configuration |
| Ninja | Fast reproducible build execution |
| GTK 4 development files | Native window, event loop, Pango/Cairo text drawing, desktop integration |
| `desktop-file-utils` and AppStream | Linux metadata validation |
| `clang-tidy` | Optional static analysis matching CI |
| `rpm`, `rpmbuild`, and `rpmlint` | RPM package generation and inspection |

## Debug build

The debug preset enables strict warnings as errors, AddressSanitizer, and UndefinedBehaviorSanitizer. It is the required local correctness build before submitting a change.

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
./build/debug/spencer
```

## Release build and package

The release preset uses `RelWithDebInfo` and retains strict warnings while disabling sanitizers. Build and test before generating native packages.

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
(cd build/release && cpack -G DEB && cpack -G RPM)
```

The generated `.deb` and `.rpm` are placed in `build/release/`. The RPM records native GTK/GLib shared-library requirements for RPM package managers; it is not interchangeable with the Debian package.

To build the portable artifacts, install Flatpak and the GNOME 50 runtime/SDK, then use the guarded helpers from the release workflow. The AppImage helper requires the checksum-verified `appimagetool`; the Flatpak manifest consumes `build/dist/SPENCER-source.tar.gz` and exports a single-file bundle.

```bash
desktop-file-validate packaging/linux/io.github.rushit305.spencer.desktop
appstreamcli validate --no-net packaging/linux/io.github.rushit305.spencer.metainfo.xml
```

## Static analysis

The debug configure step writes `build/debug/compile_commands.json`. Analyze source files using the same database.

```bash
clang-tidy -p build/debug \
  src/config/config.cpp src/parser/terminal_parser.cpp src/parser/utf8_decoder.cpp \
  src/pty/linux_pty.cpp src/terminal/terminal_state.cpp src/app/main.cpp
```

A build completing successfully does not establish full terminal compatibility. Parser and PTY changes must include relevant CTest coverage and update the compatibility documentation when the support boundary changes.
