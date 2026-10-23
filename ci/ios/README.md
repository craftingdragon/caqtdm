# iOS CI build

The workflow builds Qt 6.12.0, OpenSSL from the latest patch in the selected
3.5 LTS series, Qwt 6.3.0, and EPICS Base R7.0.10 for arm64 iPhoneOS and
arm64 iOS Simulator. The iOS minimum version is 18.0 across these dependencies
and the caQtDM app. A minimal Qt host-tools build and the Qt source share one
cache so its development-build paths remain available after restore; the
device and simulator Qt SDKs have separate caches keyed to their OpenSSL
builds and Xcode/SDK versions. The two SDK jobs run concurrently.
OpenSSL, EPICS, and Qwt are cached separately. Both qmake and CMake produce
caQtDM app artifacts for each SDK.

The iPhoneOS artifact is unsigned. It verifies that caQtDM compiles and links
for a device, but it cannot be installed on an iPhone without a signing identity
and provisioning profile. The simulator artifact is also a build artifact; this
workflow does not run a simulator or test live EPICS, Modbus, GPS, or OPC UA
connections.
