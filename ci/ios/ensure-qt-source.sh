#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/checks.sh"

qt_source_root="${QT_ROOT}/${QT_VERSION}/source"
qt_source="${qt_source_root}/qt-everywhere-src-${QT_VERSION}"
if [[ -f "${qt_source}/configure" ]]; then
  exit 0
fi

qt_tarball="qt-everywhere-src-${QT_VERSION}.tar.xz"
qt_minor="${QT_VERSION%.*}"
qt_url="https://download.qt.io/official_releases/qt/${qt_minor}/${QT_VERSION}/single/${qt_tarball}"
qt_archive="${RUNNER_TEMP}/${qt_tarball}"

mkdir -p "${qt_source_root}"
echo "::group::Download Qt ${QT_VERSION} source"
curl --fail --location --show-error --retry 3 --progress-bar \
  --write-out $'\nHTTP %{http_code}; downloaded %{size_download} bytes in %{time_total}s\n' \
  "${qt_url}" -o "${qt_archive}"
require_nonempty_file "${qt_archive}"
shasum -a 256 "${qt_archive}"
echo "::endgroup::"

echo "::group::Extract Qt ${QT_VERSION} source"
tar -xf "${qt_archive}" -C "${qt_source_root}"
require_file "${qt_source}/configure"
echo "::endgroup::"
