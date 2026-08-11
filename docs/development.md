# Development Guide

The project is organized around a strict direction of dependency: the parser emits operations, terminal state applies them, and the GTK application consumes state. The parser does not include GTK; the grid does not include POSIX process APIs; and Linux-specific PTY code remains in its own implementation unit.

| Area | Location | Responsibility |
|---|---|---|
| Configuration | `src/config/` | XDG path handling, validation, and safe default fallback |
| Parser | `src/parser/` | Stateful byte decoding and ECMA-48 control classification |
| PTY | `src/pty/` | POSIX PTY lifecycle, shell execution, I/O, resize, and reaping |
| Terminal state | `src/terminal/` | Grid, cursor, scrollback, SGR styling, and parser operations |
| GTK application | `src/app/` | Event loop, input translation, visual rendering, and viewport management |
| Tests | `tests/unit/`, `tests/integration/` | Core behavior and real PTY lifecycle evidence |

## Test requirements

Use the debug preset during development. It enables AddressSanitizer and UndefinedBehaviorSanitizer in addition to strict compiler warnings.

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

A parser regression test must use input bytes representative of the actual sequence, including split reads when sequence boundaries matter. A state test should assert cells, style, cursor, title, or scrollback instead of relying on a rendered screenshot. PTY changes require integration coverage for the process condition being changed.

## Headless GTK smoke test

A headless Linux environment cannot provide ordinary desktop interaction, but it can verify that the GTK application creates a window, starts its PTY session, and generates first-run configuration under a virtual X display.

```bash
XDG_CONFIG_HOME=/tmp/spencer-xdg-test \
  xvfb-run -a timeout --signal=TERM --kill-after=3s 4s ./build/debug/spencer
```

The expected `timeout` exit status is `124`; this is an intentional termination after successful startup, not an application failure. A real compositor-backed desktop should still be used for manual input and rendering review.

## Static analysis

```bash
clang-tidy -p build/debug src/config/config.cpp src/parser/terminal_parser.cpp \
  src/parser/utf8_decoder.cpp src/pty/linux_pty.cpp src/terminal/terminal_state.cpp \
  src/app/main.cpp
```

Do not suppress a warning without explaining why the behavior is safe. In particular, review integer conversions in grid sizing, terminal parser bounds, process-group signaling, and any conversion between GTK and C++ ownership models.
