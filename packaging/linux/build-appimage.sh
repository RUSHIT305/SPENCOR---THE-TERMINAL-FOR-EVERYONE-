#!/usr/bin/env bash
set -euo pipefail

# Produces an AppImage from an already built release tree. This script deliberately
# fails rather than publishing an unverified or incomplete artifact when appimagetool
# is not installed on the release environment.

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="${1:-"${root_dir}/build/release"}"
appdir="${root_dir}/build/AppDir"
version="$(grep -E '^project\(SPENCER' "${root_dir}/CMakeLists.txt" >/dev/null && sed -n 's/.*VERSION \([0-9][0-9.]*\).*/\1/p' "${root_dir}/CMakeLists.txt" | head -1)"
version="${version:-0.1.0}"

if ! command -v appimagetool >/dev/null 2>&1; then
  echo "appimagetool is required to create an AppImage; no artifact was produced." >&2
  exit 2
fi
if [[ ! -f "${build_dir}/cmake_install.cmake" ]]; then
  echo "Release build directory not found: ${build_dir}" >&2
  exit 2
fi

rm -rf "${appdir}"
DESTDIR="${appdir}" cmake --install "${build_dir}" --prefix /usr
mkdir -p "${appdir}/usr/share/icons/hicolor/256x256/apps"
cp "${root_dir}/assets/icons/io.github.rushit305.Spencer.svg" \
   "${appdir}/usr/share/icons/hicolor/256x256/apps/io.github.rushit305.Spencer.svg"
ln -sf "usr/share/applications/io.github.rushit305.Spencer.desktop" \
       "${appdir}/io.github.rushit305.Spencer.desktop"
ln -sf "usr/share/icons/hicolor/256x256/apps/io.github.rushit305.Spencer.svg" \
       "${appdir}/io.github.rushit305.Spencer.svg"

output="${root_dir}/build/SPENCER-${version}-linux-x86_64.AppImage"
ARCH=x86_64 appimagetool "${appdir}" "${output}"
printf 'Created %s\n' "${output}"
