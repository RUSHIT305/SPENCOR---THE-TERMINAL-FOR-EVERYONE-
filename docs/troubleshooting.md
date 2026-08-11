# Troubleshooting

SPENCER displays a native error dialog when terminal-session creation fails. For issues that prevent the window from appearing, launch `spencer` from an existing terminal to retain GTK and system diagnostics.

| Symptom | Likely cause | Resolution |
|---|---|---|
| Build cannot find GTK 4 | GTK development files or `pkg-config` are missing | Install `pkg-config libgtk-4-dev` and reconfigure with the desired preset |
| Window starts but shell session reports an error | `$SHELL` is invalid, the configured shell is unavailable, or the configured working directory cannot be entered | Remove or correct `shell`/`working_directory` in the XDG config; SPENCER falls back to `/bin/sh` for an invalid shell |
| Text is too large or too small | `font_size` is outside the desired range | Set `font_size` to an integer from 6 through 72 and restart SPENCER |
| Configuration appears ignored | The file is in a different XDG location or contains invalid syntax | Check `$XDG_CONFIG_HOME/spencer/config`, otherwise `~/.config/spencer/config`; use `key = value` syntax |
| Application does not start in a headless session | No usable graphical display is available | Start it from a Linux X11/Wayland desktop; use `xvfb-run` only for automated smoke tests |
| Editor or multiplexer behaves incorrectly | The application uses a scoped VT compatibility subset | Review [terminal compatibility](terminal-compatibility.md) and report a minimal reproduction with escape output when possible |

## Collecting a useful report

Include the SPENCER revision, Linux distribution, GTK version, shell path, terminal dimensions, exact steps, and expected versus actual behavior. Do not include command history, tokens, passwords, or private terminal output. For parser issues, a minimal escaped byte sequence is more useful than a large screenshot.
