# Contributing to SPENCER

SPENCER is a new Linux terminal emulator. Contributions should improve terminal correctness, reliability, accessibility, packaging, or developer experience without turning unimplemented capabilities into claims. Small, testable changes are preferred over broad unverified rewrites.

## Development workflow

Create a focused branch, configure the sanitizer-enabled debug build, and run all tests before opening a pull request.

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

| Change area | Required evidence |
|---|---|
| Parser or UTF-8 decoder | Focused unit tests with fragmented and malformed input where relevant |
| Grid, cursor, or styling | Unit tests that assert terminal state rather than renderer internals |
| Linux PTY lifecycle | Integration coverage using a real child process |
| GTK UI and input | Successful strict build; describe manual smoke-test steps in the pull request |
| Desktop metadata or packaging | `desktop-file-validate`, `appstreamcli validate --no-net`, and release package generation |

## Code standards

Use C++20, RAII, strong value types, and clear ownership. Do not introduce raw owning pointers, unchecked integer conversions, unbounded parser buffers, or logging that can expose terminal input, environment variables, tokens, or command contents. Keep platform-specific headers and system calls within the PTY/platform boundary.

All new observable parser behavior needs a regression test. If a feature is partial, document the supported subset and do not imply full terminal compatibility. Run `clang-tidy -p build/debug` against changed source files when modifying core or GTK code.

## Pull requests

Explain the problem, behavior change, test evidence, and limitations. Do not submit generated build directories, package artifacts, private configuration files, or credentials. The CI workflow must pass before changes are merged.
