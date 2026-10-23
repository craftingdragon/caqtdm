#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/checks.sh"

run_with_heartbeat() {
  local label="$1"
  shift
  "$@" &
  local command_pid=$!
  (
    while kill -0 "${command_pid}" 2>/dev/null; do
      sleep 60
      if kill -0 "${command_pid}" 2>/dev/null; then
        echo "$(date -u '+%Y-%m-%dT%H:%M:%SZ') Still running: ${label} (pid ${command_pid})"
      fi
    done
  ) &
  local heartbeat_pid=$!
  local result=0
  wait "${command_pid}" || result=$?
  kill "${heartbeat_pid}" 2>/dev/null || true
  wait "${heartbeat_pid}" 2>/dev/null || true
  if (( result != 0 )); then
    echo "::error::${label} failed with exit code ${result}" >&2
  fi
  return "${result}"
}

qt_source="${QT_ROOT}/${QT_VERSION}/source/qt-everywhere-src-${QT_VERSION}"
qt_host_root="${QT_HOST_ROOT}"
qt_build_root="${RUNNER_TEMP}/qt-build-host"

mkdir -p "${qt_build_root}"
bash "${GITHUB_WORKSPACE}/ci/ios/ensure-qt-source.sh"
require_file "${qt_source}/configure"

qt_host_submodules='qtbase,qt5compat,qtsvg,qttools,qtimageformats,qtpositioning,qtopcua'
cd "${qt_build_root}"
echo "::group::Configure Qt ${QT_VERSION} host"
"${qt_source}/configure" \
  -prefix "${qt_host_root}" \
  -submodules "${qt_host_submodules}" \
  -release -opensource -confirm-license \
  -nomake tests -nomake examples
echo "::endgroup::"
echo "::group::Build and install Qt host"
run_with_heartbeat "Build Qt host" cmake --build . --parallel
run_with_heartbeat "Install Qt host" cmake --install .
bash "${GITHUB_WORKSPACE}/ci/ios/verify-qt-host.sh" "${qt_host_root}"
echo "::endgroup::"
