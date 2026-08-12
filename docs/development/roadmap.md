# SPENCER Development Roadmap

This roadmap is an execution record for the new Linux codebase. A milestone is complete only when its stated build and test evidence exists. Version labels describe actual engineering state, not marketing intent.

| Phase | Scope | Acceptance gate | Status |
|---|---|---|---|
| 1 | Product specification and architecture | Reviewed technical specification committed with scope, risks, and test plan | Complete |
| 2 | Build system and terminal core | CMake presets build a reusable core; parser/grid/UTF-8 tests pass | In progress |
| 3 | Linux PTY and shell lifecycle | PTY integration test verifies shell output, resize, EOF, and child reaping | Planned |
| 4 | GTK application and renderer | Graphical window can render the live state and relay keyboard input | Planned |
| 5 | Configuration, themes, bounded scrollback | Valid/invalid config tests; user can change font size and palette | Planned |
| 6 | Packaging, CI, documentation | Native package validates; CI runs build/test/analysis; docs match behavior | Planned |
| 7 | Compatibility and release validation | Documented compatibility matrix, sanitizer run, release artifact evidence | Planned |

## `v0.2.0` acceptance criteria

The technical MVP must satisfy all of the following on a Linux test environment:

1. `cmake --preset debug`, `cmake --build --preset debug`, and `ctest --preset debug` complete successfully.
2. The application starts a GTK terminal window and uses a POSIX PTY, rather than a subprocess pipe.
3. It launches a validated user shell path, with `/bin/sh` as a safe fallback.
4. Printable UTF-8, line control characters, cursor movement, erase operations, SGR styling, and standard/256/RGB color sequences update terminal state deterministically.
5. Keyboard events produce terminal input; window resize updates the PTY’s `winsize`.
6. Scrollback storage has a finite configuration-defined cap.
7. Closing the window cleans up the PTY and reaps the shell process.
8. Debian and RPM packages are generated and inspected, while Flatpak, AppImage, and source artifacts have reproducible build paths.
9. No unsupported distribution, architecture, or VT feature is described as implemented; known limitations are documented.

## Risk register

| Risk | Mitigation | Residual limitation |
|---|---|---|
| VT behavior is broader than the MVP parser | State-machine parser, focused tests, and a documented compatibility matrix | Complex DCS, sixel, and many DEC extensions remain unsupported initially |
| Headless CI cannot make visual claims | Exercise core/PTY tests without a display and validate desktop metadata | Interactive rendering still needs a compositor-backed manual smoke test |
| Font shaping differs from cell geometry | Use Pango for font fallback while the core maintains terminal widths | Complete grapheme segmentation and ligature policy are deferred |
| Child process does not exit promptly | Process-group shutdown with bounded wait and reaping | Escalation handling requires real desktop smoke testing |
| AppImage tooling is not deterministic on every runner | Gate AppImage output on a dedicated verified build before publishing | Debian package is the first verified local package format |

## Definition of done for each code change

A source change must build under the applicable preset, carry tests for its externally visible behavior, keep documentation truthful, and pass static-analysis review when the compilation database is available. The project will not claim cross-platform support, GPU glyph-atlas rendering, code signing, or a released AppImage until direct verification proves it.
