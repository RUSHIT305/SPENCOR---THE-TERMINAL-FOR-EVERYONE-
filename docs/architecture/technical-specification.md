# SPENCER Technical Specification

**Status:** Approved implementation baseline for `v0.2.0`
**Target:** Linux x86_64, with Linux ARM64 portability designed in but not advertised until it is built and tested.  
**Scope:** A new native graphical terminal emulator. This specification intentionally does not reuse any earlier implementation.

## 1. Product specification

> **SPENCER — The Terminal for Everyone** is a native Linux terminal application that starts the user’s shell in a pseudo-terminal (PTY), renders the resulting terminal screen, and provides dependable keyboard-driven command-line interaction.

The initial production-quality milestone is a **technical MVP**, not a claim of full VT compatibility. It must create a window, launch the user’s shell, safely relay PTY input and output, maintain a grid-based terminal state, render ANSI-styled text, resize the child terminal, and exit without leaking a child process. Subsequent milestones add selection, clipboard, themes, configuration, scrollback navigation, alternate-screen behavior, and broader compatibility.

| Release | Implemented commitment | Deliberately deferred |
|---|---|---|
| `v0.1.0` | Linux GTK window, POSIX PTY, shell discovery, state-machine parser, grid, basic ANSI SGR/colors, resize, scrollback storage, text input, mouse-wheel scroll, graceful exit | Selection, clipboard, full DEC private modes, hyperlinks, mouse reporting, configurable keybindings |
| `v0.2.0` | Portability matrix, Debian/RPM/Flatpak/AppImage/source release paths, stable package metadata, and representative distribution CI | ARM64 and other architectures, full terminal-control conformance |
| `v0.2.0` | Validated configuration, themes, configurable fonts, copy/paste, selection, expanded Unicode handling | Full grapheme-cluster shaping, all xterm extensions |
| `v0.5.0` | Alternate screen, bracketed paste, title updates, more VT/DEC controls, hyperlink and mouse-reporting support where tested | Complete xterm conformance claim |
| `v1.0.0` | Only after compatibility, accessibility, package-install, performance, and security acceptance criteria are met | Unsupported-platform promises |

## 2. Technical requirements

SPENCER must use a real POSIX PTY and execute the selected shell as a child process. The application must keep parser logic independent from state mutation, avoid unbounded scrollback growth, detect malformed configuration without preventing startup, and never log private command contents by default. Its terminal identity defaults to `xterm-256color` so applications can choose a widely understood terminal capability profile.

The terminal core uses a fixed visible grid plus a bounded deque of logical scrollback rows. Each cell carries a Unicode scalar value or a continuation/empty marker, foreground and background colors, SGR attributes, and a width classification. The first milestone provides deterministic handling for ASCII, UTF-8 scalar decoding, combining-mark attachment where a preceding cell exists, and common East Asian wide characters. Complex grapheme-cluster shaping is expressly documented as a later compatibility enhancement rather than silently approximated.

The parser follows a state-machine model consistent with ECMA-48 control-sequence classes: ground, escape, CSI, OSC, and ignored string states. It translates bytes into terminal operations rather than allowing escape sequences to mutate the grid directly. OSC input is size-limited and only the title operation is accepted in the initial milestone; unsafe or unsupported strings are discarded.

## 3. Technology selection

| Technology | Purpose | Why selected | Alternatives considered | Advantages | Disadvantages | License / platform |
|---|---|---|---|---|---|---|
| C++20 | Application and terminal core | Strong RAII model, Linux system API access, and modern portable language features | Rust, C, Qt/QML | Deterministic ownership, no runtime dependency, efficient byte processing | Requires careful memory-safety discipline | ISO C++; Linux target |
| CMake + Ninja | Configure and build | Standard Linux CI tooling and reproducible presets | Meson, Bazel | Widely packaged, CTest/CPack integration | More verbose than Meson | BSD-3-Clause / Linux |
| GTK 4 + Pango/Cairo | Window, input, clipboard-ready integration, font layout, drawing | Mature Linux desktop stack with accessibility and high-DPI integration | SDL2, GLFW, Qt 6 | Native Wayland/X11 integration, input methods, GSK-composited rendering path | Larger runtime dependency; initial glyph cache is not custom | LGPL-2.1-or-later; Linux |
| POSIX PTY APIs | Shell process and terminal device | Correct Unix terminal semantics without a shell subprocess wrapper | `popen`, embedded terminal widget | Job control, resize, signal, and termios behavior | Linux/Unix-specific implementation | POSIX / Linux |
| CTest with in-tree harness | Unit and integration tests | Avoids an early third-party C++ test dependency while keeping CI deterministic | Catch2, GoogleTest | Small dependency surface, simple TAP-like output | Fewer rich assertions | CMake / Linux |
| CPack DEB/RPM generators | Native package formats | Included in CMake and produce packages for Debian-family and RPM-family systems | Hand-authored distro scripts, Flatpak | Reproducible package metadata and install layout | Native artifacts remain architecture and runtime specific | BSD-3-Clause / Linux |

GTK 4’s renderer is used for the initial native window compositing and drawing path. A custom OpenGL/Vulkan glyph atlas is **not** claimed in `v0.2.0`; introducing one before parser and PTY correctness are proven would raise delivery risk. The renderer boundary keeps that upgrade possible without changing the terminal core.

## 4. System architecture

```text
┌──────────────────────────────────────────────────────────────────┐
│                           SPENCER APP                              │
│ GTK application / window / actions / event loop                    │
├──────────────────────────────────────────────────────────────────┤
│ Input adapter         │ Renderer                                  │
│ Key and IME events    │ GTK drawing area + Pango/Cairo layout     │
│ Mouse wheel           │ grid, colors, cursor, viewport            │
├──────────────────────────────────────────────────────────────────┤
│ Terminal core                                                     │
│ UTF-8 decoder → ECMA-48 parser → terminal operations → state     │
│ grid, cursor, modes, scrollback, damage tracking                  │
├──────────────────────────────────────────────────────────────────┤
│ PTY / process layer                                               │
│ posix_openpt → fork → session leader → exec user shell            │
│ non-blocking read/write, resize via TIOCSWINSZ, SIGCHLD cleanup   │
├──────────────────────────────────────────────────────────────────┤
│ Configuration / themes / diagnostics                               │
│ XDG config parser, built-in palette, privacy-aware structured log │
└──────────────────────────────────────────────────────────────────┘
```

The GTK thread owns the terminal state and renderer. A GLib file-descriptor source calls the PTY reader only on readable events, preventing a busy-loop. Writes are queued to the PTY and use non-blocking I/O. No worker thread mutates terminal state in the first milestone. This preserves a simple ordering guarantee between received bytes, terminal operations, and rendered damage.

### PTY lifecycle

1. Resolve `$SHELL`; accept it only if it is absolute and executable, otherwise use `/bin/sh`.
2. Open a master PTY, grant and unlock the slave, and configure initial dimensions.
3. Fork. The child creates a session, makes the slave its controlling terminal, duplicates it to standard streams, establishes `TERM`, and executes the shell as a login shell where possible.
4. The parent sets the master non-blocking, watches it with the GTK event loop, and sends input/resizes to the master.
5. On application shutdown, the parent sends `SIGHUP` to the child process group, waits with a bounded timeout, escalates only if needed, and reaps the child.

## 5. Repository structure

```text
SPENCER/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── LICENSE
├── CONTRIBUTING.md
├── SECURITY.md
├── CHANGELOG.md
├── cmake/
├── include/spencer/
│   ├── config/
│   ├── parser/
│   ├── pty/
│   ├── terminal/
│   └── util/
├── src/
│   ├── app/
│   ├── config/
│   ├── parser/
│   ├── pty/
│   ├── terminal/
│   └── util/
├── tests/
│   ├── unit/
│   └── integration/
├── assets/
│   ├── icons/
│   └── themes/
├── packaging/linux/
├── docs/
│   ├── architecture/
│   ├── configuration/
│   ├── development/
│   └── releases/
└── .github/workflows/
```

Directories are introduced only when their matching implementation exists. The repository contains no copied source or compatibility layer for an earlier SPENCER project.

## 6. Testing strategy

The build must run parser, UTF-8, grid, state, configuration, and PTY lifecycle tests through CTest. Tests use an actual `/bin/sh` child for PTY integration and assert observable output, terminal resizing, EOF/process cleanup, and shutdown behavior. Parser tests cover fragmented control sequences, malformed UTF-8, cursor movement, erase operations, SGR 16/256/RGB colors, and OSC input bounds.

Debug presets enable warnings and sanitizers. Release presets enable warnings, position-independent code, and linker hardening. A dedicated CI job runs the configured build, CTest, `desktop-file-validate`, and `appstreamcli validate`; a static-analysis job runs `clang-tidy` against generated compilation commands. Sanitizer failures block CI.

## 7. Security strategy

SPENCER treats terminal output as untrusted. It bounds parser parameter counts, sequence length, OSC payload size, scrollback rows, and queued write volume. It never evaluates terminal output as code, opens OSC-provided URLs, or writes OSC clipboard data in the initial version. PTY file descriptors are close-on-exec where appropriate and are always closed through RAII wrappers. Child processes are reaped, and the parent only starts an executable shell from a validated path.

The project uses compiler warnings, AddressSanitizer and UndefinedBehaviorSanitizer in debug validation, C++ ownership types, bounds checks, and a minimal third-party dependency surface. Runtime diagnostics record subsystem and error category, but omit keystrokes, terminal bytes, environment secrets, and command text.

## 8. Linux platform and packaging strategy

The current support boundary is **Linux only**. Linux-specific PTY code is isolated in `src/pty/linux_pty.cpp`; generic core code does not include platform headers. This makes a future ConPTY or macOS implementation feasible without presenting it as supported now.

The first locally verified packaging target is a Debian package built by CPack. The repository also supplies a documented AppImage staging script, but no AppImage is declared available until an AppImage build has completed in CI. A desktop entry and AppStream metainfo file are validated as part of the Linux packaging test workflow.

## 9. CI/CD and release strategy

Pull requests run the Linux build, unit/integration tests, package validation, and static analysis. A version tag workflow rebuilds from a clean runner, executes the same tests, creates the tested package, calculates SHA-256 checksums, and publishes only the verified artifacts. Code-signing and AppImage credentials are deliberately absent from the repository and are documented as release secrets rather than embedded in source.

## 10. Sources and standards

The implementation draws terminal-control terminology from ECMA-48, terminal capability conventions from `terminfo`, and Linux PTY lifecycle behavior from the POSIX interfaces. The product does not claim conformance beyond the behaviors covered by its test suite.

[1]: https://ecma-international.org/publications-and-standards/standards/ecma-48/ "ECMA-48: Control Functions for Coded Character Sets"
[2]: https://pubs.opengroup.org/onlinepubs/9699919799/functions/posix_openpt.html "POSIX posix_openpt"
[3]: https://man7.org/linux/man-pages/man4/tty_ioctl.4.html "Linux TTY ioctl documentation"
[4]: https://specifications.freedesktop.org/desktop-entry-spec/latest/ "Desktop Entry Specification"
[5]: https://www.freedesktop.org/software/appstream/docs/ "AppStream documentation"
