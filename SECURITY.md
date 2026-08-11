# Security Policy

## Supported version

Security fixes are applied to the latest development state on the `main` branch. The current published engineering milestone is `0.1.0`.

## Reporting a vulnerability

Please do not file a public issue for a suspected vulnerability involving arbitrary code execution, terminal escape-sequence handling, parser denial of service, process isolation, file disclosure, or sensitive-data logging. Instead, open a private security advisory through the repository’s GitHub Security tab and include a minimal reproduction, affected revision, operating system, and expected impact.

A report should not include passwords, tokens, private command history, or production secrets. Maintainers will acknowledge receipt, assess reproducibility, and coordinate a fix before public disclosure when the issue is confirmed.

## Security design commitments

| Area | Current protection |
|---|---|
| Terminal input/output | Terminal output is treated as untrusted data; it is parsed, never evaluated as application code |
| Parser bounds | CSI parameter count and OSC/string payloads are bounded; unsupported long strings are discarded |
| UTF-8 | Overlong sequences, surrogates, out-of-range values, and incomplete sequences become replacement characters |
| Process lifecycle | POSIX PTY descriptors use RAII; the shell runs in a controlled terminal session and is reaped on shutdown |
| Shell choice | `$SHELL` or configured values must be executable absolute paths, with `/bin/sh` fallback |
| Diagnostics | The application avoids logging terminal bytes, keystrokes, environment contents, passwords, tokens, and command text |
| Build hardening | Debug verification uses AddressSanitizer and UndefinedBehaviorSanitizer; CI runs strict warnings and static analysis |

Security-sensitive changes require tests and a review of the relevant parser, process, configuration, or package boundary. Release signing is not enabled in this source tree; any future signing credentials must remain in CI secret storage rather than the repository.
