# RPM packaging

SPENCER uses CPack’s native RPM generator so the package records the shared-library requirements discovered from the built executable. This keeps GTK 4 and GLib dependencies native to the target RPM distribution instead of pretending that a Debian package can be installed safely on Fedora, RHEL-family, or openSUSE systems.

From a configured release build, run:

```sh
cmake --preset release
cmake --build --preset release
packaging/rpm/build-rpm.sh build/release
```

The helper requires `cpack`, `rpmbuild`, and `rpm`. It prints the package metadata and dependency list after creation. On Fedora/RHEL-family systems, install the resulting file with the distribution package manager so native dependencies can be resolved, for example:

```sh
sudo dnf install ./spencer-0.2.1-1*.x86_64.rpm
```

The RPM artifact is architecture-specific and the first release publishes an x86_64 build. ARM64 and other architectures require separate native builds and are not implied by the x86_64 artifact.
