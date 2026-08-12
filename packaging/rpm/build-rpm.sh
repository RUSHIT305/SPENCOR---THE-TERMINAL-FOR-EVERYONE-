#!/usr/bin/env bash
set -euo pipefail

# Build and inspect the native RPM from an already configured release tree.
# The package intentionally keeps GTK/GLib dependencies native so Fedora,
# RHEL-family, and openSUSE package managers can resolve the correct ABI.
root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="${1:-"${root_dir}/build/release"}"

if ! command -v cpack >/dev/null 2>&1; then
  echo "cpack is required to build the RPM." >&2
  exit 2
fi
if ! command -v rpmbuild >/dev/null 2>&1; then
  echo "rpmbuild is required to build the RPM." >&2
  exit 2
fi
if [[ ! -f "${build_dir}/CPackConfig.cmake" ]]; then
  echo "Release build directory not found: ${build_dir}" >&2
  exit 2
fi

cd "${build_dir}"
rm -f ./*.rpm
cpack -G RPM
rpm_file="$(find "${build_dir}" -maxdepth 1 -type f -name '*.rpm' -print -quit)"
if [[ -z "${rpm_file}" ]]; then
  echo "CPack completed without producing an RPM." >&2
  exit 3
fi

rpm -qip "${rpm_file}"
rpm -qp --requires "${rpm_file}"
printf 'Created %s\n' "${rpm_file}"
