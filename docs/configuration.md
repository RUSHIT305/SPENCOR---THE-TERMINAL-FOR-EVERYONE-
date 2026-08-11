# Configuration

SPENCER reads a small, line-oriented configuration file from `$XDG_CONFIG_HOME/spencer/config`. When `XDG_CONFIG_HOME` is not set, the path is `~/.config/spencer/config`. On first launch, SPENCER writes an editable example only if no file is present.

The parser accepts `key = value` lines and `#` comments. Invalid or unknown lines generate an in-memory warning and leave the prior safe value unchanged; they do not prevent the terminal window from launching.

| Key | Type and valid range | Default | Current effect |
|---|---|---:|---|
| `font_family` | Text, maximum 128 characters | `Monospace` | Pango font description used to draw terminal cells |
| `font_size` | Integer from 6 through 72 | `14` | Text size in pixels; grid dimensions are recalculated on redraw |
| `scrollback_lines` | Integer from 0 through 1,000,000 | `10000` | Maximum retained logical rows; `0` disables history retention |
| `shell` | Executable absolute path | `$SHELL`, then `/bin/sh` | Requested child shell; invalid/non-executable values use the safe fallback |
| `working_directory` | Absolute path | Inherited process directory | Child shell working directory; a failure causes shell startup to fail visibly |
| `padding` | Integer from 0 through 100 | `8` | Pixel padding surrounding the terminal grid |

## Example

```ini
# SPENCER configuration
font_family = Monospace
font_size = 14
scrollback_lines = 10000
# shell = /bin/bash
# working_directory = /home/you
padding = 8
```

Custom color palettes, opacity, keybinding remapping, configurable cursor styles, and custom themes are not parsed in `v0.1.0`. The application uses its built-in dark palette. Do not add unsupported keys expecting behavior; they will be ignored safely.
