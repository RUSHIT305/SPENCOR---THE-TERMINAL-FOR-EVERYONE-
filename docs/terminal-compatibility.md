# Terminal Compatibility

SPENCER `0.2.1` is a usable terminal with a deliberately scoped parser. It does **not** claim general xterm, DEC VT, tmux, editor, or full Unicode conformance. This document is the source of truth for the currently implemented behavior.

## Implemented and tested behavior

| Category | Supported behavior |
|---|---|
| Text input | ASCII and UTF-8 scalar decoding; malformed/incomplete encodings become U+FFFD |
| Cell geometry | Common East Asian wide ranges occupy two cells; selected combining-mark ranges attach to a preceding cell |
| C0 controls | Backspace, horizontal tab, line feed, vertical tab, form feed, carriage return |
| ESC | `ESC 7`/`ESC 8` save and restore cursor, `ESC c` reset, `ESC D` index, `ESC E` next line, CSI/OSC/DCS entry |
| CSI movement | `A`, `B`, `C`, `D`, `H`, and `f` with default parameters |
| CSI erasure | `J` and `K` modes 0, 1, 2; `J` mode 3 clears retained scrollback |
| SGR | Reset, bold, dim, italic, underline, inverse, strikethrough, standard/bright ANSI colors, 256-color, and RGB color forms |
| Private modes | Cursor visibility `?25h`/`?25l` and bracketed-paste state tracking `?2004h`/`?2004l` |
| OSC | Bounded `OSC 0` and `OSC 2` title updates terminated by BEL, ST, or `ESC \\` |
| Ignored strings | DCS/APC/PM strings are safely ignored until ST rather than rendered as text |

## Current limitations

The alternate-screen mode is not stored separately, although the parser safely ignores unsupported commands. The application does not yet transmit bracketed paste delimiters because clipboard paste is not implemented. It has no DECSET origin/scroll-region implementation, device-status reporting, mouse reporting, hyperlinks, synchronized update mode, image protocols, sixel, or OSC clipboard support.

Pango renders glyphs with desktop font fallback, but cell width is determined in the terminal core. Complex grapheme clusters, emoji sequences, ligature policy, bidirectional text, and every East Asian ambiguous-width policy require additional compatibility work. The renderer uses GTK/Pango/Cairo rather than a custom GPU glyph atlas, and each nonblank cell currently creates a layout; high-throughput rendering is therefore a known optimization target rather than a claimed performance result.

## Verification approach

Parser correctness is tested using fragmented byte feeds and state inspection. The PTY integration test starts `/bin/sh`, verifies actual output, changes the terminal to 40×100 with `TIOCSWINSZ`, queries `stty size`, exits, and asserts that the child is reaped. This provides evidence for the implemented subset but cannot substitute for a full compatibility suite with interactive programs such as `vim`, `tmux`, and `less`.
