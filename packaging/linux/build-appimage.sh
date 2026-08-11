#!/usr/bin/env bash
set -euo pipefail

# Produces an AppImage from an already built release tree. This script deliberately
# fails rather than publishing an unverified or incomplete artifact when appimagetool
# is not installed on the release environment.

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="${1:-"${root_dir}/build/release"}"
appdir="${root_dir}/build/AppDir"
# The tag/release workflow supplies SPENCER_VERSION for later releases.
version="${SPENCER_VERSION:-0.1.0}"

appimagetool="${APPIMAGETOOL:-appimagetool}"
if ! command -v "${appimagetool}" >/dev/null 2>&1; then
  echo "appimagetool is required to create an AppImage; no artifact was produced." >&2
  exit 2
fi
if [[ ! -f "${build_dir}/cmake_install.cmake" ]]; then
  echo "Release build directory not found: ${build_dir}" >&2
  exit 2
fi

rm -rf "${appdir}"
DESTDIR="${appdir}" cmake --install "${build_dir}" --prefix /usr
ln -sf "usr/bin/spencer" "${appdir}/AppRun"
ln -sf "usr/share/applications/io.github.rushit305.spencer.desktop" \
       "${appdir}/io.github.rushit305.spencer.desktop"
ln -sf "usr/share/icons/hicolor/256x256/apps/io.github.rushit305.spencer.png" \
       "${appdir}/io.github.rushit305.spencer.png"
cp "${appdir}/usr/share/metainfo/io.github.rushit305.spencer.metainfo.xml" \
   "${appdir}/usr/share/metainfo/io.github.rushit305.spencer.appdata.xml"

output="${root_dir}/build/SPENCER-${version}-linux-x86_64.AppImage"
ARCH=x86_64 APPIMAGE_EXTRACT_AND_RUN=1 "${appimagetool}" "${appdir}" "${output}"
printf 'Created %s\n' "${output}"
