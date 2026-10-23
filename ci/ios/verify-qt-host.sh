#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/checks.sh"

host_root="${1:?Usage: verify-qt-host.sh HOST_INSTALL_ROOT}"
host_bin="${host_root}/bin"
host_libexec="${host_root}/libexec"

for tool in qmake qmake6 qtpaths qtpaths6 qopcuaxmldatatypes2cpp; do
  require_executable "${host_bin}/${tool}"
done
for tool in moc rcc; do
  require_executable "${host_libexec}/${tool}"
done

require_file "${host_root}/lib/cmake/Qt6/Qt6Config.cmake"
require_file "${host_root}/lib/cmake/Qt6OpcUaTools/Qt6OpcUaToolsConfig.cmake"
require_equal "$("${host_bin}/qmake" -query QT_INSTALL_PREFIX)" "${host_root}" 'Qt host install prefix'
