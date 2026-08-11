# Changelog

All notable changes to SPENCER are documented here. Versions reflect verified engineering milestones rather than intended future scope.

## [0.1.0] — 2026-08-11

### Added

- A new C++20 Linux terminal codebase with CMake, debug/release presets, CTest, and strict compiler warnings.
- A real POSIX PTY engine that discovers a safe shell, creates a controlling terminal session, relays non-blocking I/O, propagates terminal resize, and reaps the child process.
- A state-machine terminal parser with printable UTF-8 input, C0 controls, ESC, CSI, bounded OSC title handling, and safe ignored string states.
- A typed terminal grid with cursor positioning, erase controls, ANSI SGR attributes, 16-color, 256-color, and RGB colors, basic combining-mark attachment, common wide-character cells, and bounded scrollback.
- A GTK 4 desktop application with Pango/Cairo text rendering, keyboard input, common navigation/function keys, mouse-wheel/Page Up/Page Down scrollback navigation, cursor display, and safe error dialogs.
- XDG configuration for font family, font size, scrollback, shell, working directory, and padding, including safe defaults for invalid files.
- Linux desktop, AppStream, icon, CPack Debian package, GitHub Actions CI, static-analysis workflow, and AppImage staging script.
- Unit and integration tests, including a live `/bin/sh` PTY resize and reaping test.

### Not yet implemented

- Text selection, clipboard copy/paste, custom theme parsing, alternate-screen state storage, bracketed-paste transmission, terminal hyperlinks, mouse reporting, full grapheme-cluster behavior, image protocols, and a custom GPU glyph atlas.
- A verified public AppImage, Flatpak, or repository-hosted release artifact.

[0.1.0]: https://github.com/RUSHIT305/SPENCOR---THE-TERMINAL-FOR-EVERYONE-/compare/main
