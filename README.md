# SPENCER

> **The Terminal for Everyone**

SPENCER is a new native Linux terminal emulator written in modern C++20. It launches the user’s shell in a real POSIX pseudo-terminal, parses a focused subset of ECMA-48/VT-style output through a state machine, maintains a typed terminal grid, and renders that grid in a GTK 4 desktop window.

The current release is **`v0.2.0`, a tested portability release**. It is an actual usable shell terminal, not a mockup, but it does not claim complete xterm, DEC, or Unicode grapheme-cluster compatibility. See the [technical specification](docs/architecture/technical-specification.md), [compatibility notes](docs/terminal-compatibility.md), and [Linux portability matrix](docs/portability.md) for the exact boundary.

| Capability | `v0.2.0` status |
|---|---|
| Linux GTK desktop window | Implemented and startup-smoke-tested under a virtual X display |
| POSIX PTY and interactive shell | Implemented and integration-tested against `/bin/sh` |
| Keyboard input, common navigation keys, and terminal output | Implemented |
| UTF-8 decoding, basic combining marks, common wide characters | Implemented with documented limits |
| ANSI SGR attributes, 16/256/RGB colors, cursor movement, erase controls | Implemented and unit-tested |
| Bounded in-memory scrollback and mouse/Page Up/Page Down navigation | Implemented |
| Configuration and built-in dark theme | Implemented |
| Copy/paste selection, alternate screen, mouse reporting, hyperlinks | Not implemented in this release |

## Build and run

SPENCER targets Linux and requires a C++20 compiler, CMake, Ninja, `pkg-config`, and GTK 4 development files. Ubuntu 24.04 packages that satisfy this are `build-essential cmake ninja-build pkg-config libgtk-4-dev`.

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
./build/debug/spencer
```

The first launch writes an editable example configuration file under `$XDG_CONFIG_HOME/spencer/config`, or `~/.config/spencer/config` when `XDG_CONFIG_HOME` is unset. Invalid entries are ignored with safe defaults, so a malformed file cannot lock the user out of the application.

| Preset | Intended use | Sanitizers |
|---|---|---|
| `debug` | Development and correctness checks | AddressSanitizer and UndefinedBehaviorSanitizer enabled |
| `release` | Optimized production-style build and package generation | Disabled |

## Package

A native Debian package and RPM package are generated from the release build and include the executable, desktop entry, AppStream metadata, and application icons. The same release publishes Flatpak, AppImage, source, and checksum artifacts.

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
(cd build/release && cpack -G DEB)
(cd build/release && cpack -G RPM)
```

The repository includes guarded AppImage and Flatpak build paths. The release workflow creates only artifacts that pass build, metadata, checksum, and launch checks on representative Linux environments.

## Architecture

SPENCER separates terminal byte parsing from state mutation and keeps Linux PTY code outside of the platform-neutral terminal core.

```text
PTY bytes → UTF-8 decoder → terminal parser → terminal operations → grid/state → GTK renderer
```

The GTK application owns the state and event loop. A non-blocking PTY file descriptor is watched by GLib, so shell output is handled without a polling loop. The process layer creates a session and controlling terminal for the child shell, applies resize events with `TIOCSWINSZ`, and sends a bounded graceful shutdown sequence before reaping the child.

## Quality checks

The in-tree test suite covers UTF-8 decoding, parser transitions, SGR colors and attributes, grid/cursor/scrollback behavior, configuration fallbacks, and a real PTY shell lifecycle with resize verification. GitHub Actions runs representative Ubuntu and Fedora-family builds, metadata validation, CTest, native package generation, portable artifact generation, and `clang-tidy`.

For the exact commands, manual validation scope, and known limitations, consult the documents below.

| Document | Purpose |
|---|---|
| [Technical specification](docs/architecture/technical-specification.md) | Product boundary, design, security, and standards basis |
| [Build guide](docs/building.md) | Prerequisites and reproducible builds |
| [Configuration](docs/configuration.md) | All supported settings and safe fallback behavior |
| [Terminal compatibility](docs/terminal-compatibility.md) | Implemented control sequences and limits |
| [Linux portability](docs/portability.md) | Distribution matrix, artifact reach, and functional support definition |
| [Development guide](docs/development.md) | Tests, analysis, code organization, and contribution workflow |
| [Release process](docs/release-process.md) | Verified packaging and tag/release procedure |

## License

SPENCER is available under the [MIT License](LICENSE).
