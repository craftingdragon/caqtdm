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

target="$1"
case "${target}" in
  iphoneos)
    qt_prefix="${QT_IOS_DEVICE_ROOT}"
    openssl_prefix="${OPENSSL_IOS_ROOT}/${OPENSSL_VERSION}/iphoneos"
    ;;
  iphonesimulator)
    qt_prefix="${QT_IOS_SIMULATOR_ROOT}"
    openssl_prefix="${OPENSSL_IOS_ROOT}/${OPENSSL_VERSION}/iphonesimulator-arm64"
    ;;
  *)
    echo "Unsupported Qt target: ${target}" >&2
    exit 1
    ;;
esac

qt_source="${QT_ROOT}/${QT_VERSION}/source/qt-everywhere-src-${QT_VERSION}"
qt_host_root="${QT_HOST_ROOT}"
build_dir="${RUNNER_TEMP}/qt-build-${target}"
require_dir "${qt_source}"
bash "${GITHUB_WORKSPACE}/ci/ios/verify-qt-host.sh" "${qt_host_root}"
require_file "${openssl_prefix}/lib/libssl.a"

mkdir -p "${build_dir}"
cd "${build_dir}"

qt_ios_submodules='qtbase,qt5compat,qtsvg,qttools,qtimageformats,qtpositioning,qtopcua,qtserialbus'
echo "::group::Configure Qt ${QT_VERSION} for ${target}"
"${qt_source}/configure" \
  -prefix "${qt_prefix}" \
  -platform macx-ios-clang -sdk "${target}" \
  -qt-host-path "${qt_host_root}" \
  -submodules "${qt_ios_submodules}" \
  -openssl-linked -feature-open62541-security \
  -release -opensource -confirm-license \
  -nomake tests -nomake examples \
  -- \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="${IOS_DEPLOYMENT_TARGET}" \
  -DOPENSSL_ROOT_DIR="${openssl_prefix}" \
  -DOPENSSL_USE_STATIC_LIBS=ON \
  -DFEATURE_open62541_security=ON
summary_file="${build_dir}/config.summary"
require_file "${summary_file}"
if ! grep -Eq 'Open62541 security support[.[:space:]]+yes' "${summary_file}"; then
  cat "${summary_file}"
  echo "Open62541 security support was not enabled for ${target}" >&2
  exit 1
fi
echo "::endgroup::"

echo "::group::Build and install Qt ${QT_VERSION} for ${target}"
run_with_heartbeat "Build Qt for ${target}" cmake --build . --parallel
run_with_heartbeat "Install Qt for ${target}" cmake --install .
echo "::endgroup::"
