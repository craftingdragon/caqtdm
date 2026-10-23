#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/checks.sh"

build_root="${1:?Usage: verify-app.sh BUILD_ROOT SDK QT_TARGET_ROOT DEPLOYMENT_TARGET}"
sdk="${2:?Missing SDK}"
qt_target_root="${3:?Missing Qt target root}"
deployment_target="${4:?Missing deployment target}"

app="$(find "${build_root}" -type d -name caQtDM.app -print -quit)"
require_nonempty "${app}" "caQtDM.app under ${build_root}"
require_dir "${app}"
executable="${app}/caQtDM"
require_executable "${executable}"
require_arch "${executable}" arm64

build_info="$(xcrun vtool -show-build "${executable}")" || ci_error "Cannot read build version from ${executable}"
echo "${build_info}" >&2
if ! awk -v expected="${deployment_target}" '$1 == "minos" && $2 == expected { found=1 } END { exit !found }' <<< "${build_info}"; then
  ci_error "${executable}: expected minimum iOS version ${deployment_target}"
fi
if [[ "${sdk}" == iphoneos ]]; then
  grep -q 'platform IOS$' <<< "${build_info}" || ci_error "${executable}: expected iPhoneOS platform"
elif [[ "${sdk}" == iphonesimulator ]]; then
  grep -q 'platform IOSSIMULATOR$' <<< "${build_info}" || ci_error "${executable}: expected iOS Simulator platform"
else
  ci_error "Unsupported app SDK: ${sdk}"
fi

plist="${app}/Info.plist"
require_file "${plist}"
bundle_id="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "${plist}")" || ci_error "Cannot read CFBundleIdentifier from ${plist}"
require_equal "${bundle_id}" ch.psi.caQtDM 'Bundle identifier'
require_file "${app}/caQtDM_IOS_Config.xml"
require_file "${app}/fonts/lucida-sans-typewriter.ttf"
for key in NSLocationWhenInUseUsageDescription NSLocalNetworkUsageDescription; do
  value="$(/usr/libexec/PlistBuddy -c "Print :${key}" "${plist}")" || ci_error "Cannot read ${key} from ${plist}"
  require_nonempty "${value}" "${key} in ${plist}"
done
require_file "${qt_target_root}/plugins/opcua/libopen62541_backend.a"
strings "${executable}" | grep -F 'SecurityPolicy#Basic256Sha256' >/dev/null || ci_error "${executable}: missing SecurityPolicy#Basic256Sha256"

echo "${app}"
