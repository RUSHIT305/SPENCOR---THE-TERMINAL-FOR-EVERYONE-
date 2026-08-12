# Installation

SPENCER `0.2.0` provides verified Debian and RPM packages plus Flatpak, AppImage, and source artifacts. It targets Linux desktops with GTK 4 runtime support. Native packages install the `spencer` executable, desktop entry, AppStream metadata, and application icons.

## Install a locally built package

Build the release package as described in [Building SPENCER](building.md), then install the resulting file with the system package manager.

```bash
sudo apt install ./build/release/spencer_0.2.0_amd64.deb
```

On Fedora, RHEL-family, and compatible RPM systems, use the native package manager so GTK and GLib dependencies resolve from the target distribution:

```bash
sudo dnf install ./build/release/spencer-0.2.0-1*.x86_64.rpm
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

## Portable artifacts

The release workflow publishes an AppImage and a Flatpak bundle alongside the native packages. The AppImage is intended for modern x86_64 Linux systems with a working graphical session. Downloaded files commonly do not retain executable permission, so make the AppImage executable before launching it:

```bash
chmod +x SPENCER-0.2.0-linux-x86_64.AppImage
./SPENCER-0.2.0-linux-x86_64.AppImage
```

The Flatpak bundle requires Flatpak plus the pinned GNOME 50 runtime. On a system without that runtime, install it from Flathub before installing the bundle:

```bash
flatpak remote-add --if-not-exists --user flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak install --user flathub org.gnome.Platform//50
flatpak install --user ./SPENCER-0.2.0.flatpak
flatpak run --user io.github.rushit305.spencer
```

Verify the published `SHA256SUMS.txt` before running any downloaded artifact.
