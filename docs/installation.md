# Installation

SPENCER `0.1.0` provides a verified Debian package build from the source tree. It targets Linux desktops with GTK 4 runtime support. The package installs the `spencer` executable, desktop entry, AppStream metadata, and scalable icon.

## Install a locally built package

Build the release package as described in [Building SPENCER](building.md), then install the resulting file with the system package manager.

```bash
sudo apt install ./build/release/spencer_0.1.0_amd64.deb
```

Launch **SPENCER** from the application menu or run `spencer` from an existing terminal. The application starts the shell selected by `$SHELL` when it is an executable absolute path; otherwise it uses `/bin/sh`.

## Runtime requirements

| Requirement | Reason |
|---|---|
| Linux desktop session | GTK 4 windowing on X11 or Wayland |
| GTK 4 runtime libraries | Native UI, Pango, and Cairo rendering |
| A valid executable shell | The PTY child shell; `$SHELL` is preferred with `/bin/sh` fallback |
| Writable XDG configuration directory, if available | Creation of the optional example configuration file |

## Uninstall

```bash
sudo apt remove spencer
```

Removing the package leaves the user configuration file in place. Delete `$XDG_CONFIG_HOME/spencer/` or `~/.config/spencer/` manually if the local configuration is no longer wanted.

## AppImage status

The repository contains `packaging/linux/build-appimage.sh`, which stages a release install tree and invokes `appimagetool` only when that tool is available. No AppImage artifact is currently published by this source tree, and users should not treat the script as evidence of an available release download.
