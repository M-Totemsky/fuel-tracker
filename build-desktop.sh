#!/usr/bin/env bash
# Desktop build helper for Fuel Tracker (MX Linux / Linux Desktop)
# Uses system Qt5 libraries. Requires: qtbase5-dev, qtdeclarative5-dev,
# qtquickcontrols2-5-dev, libqt5sql5-sqlite, cmake, g++

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build-desktop"

echo "=== Fuel Tracker Desktop Build ==="

for pkg in qmake cmake g++; do
    if ! command -v "$pkg" >/dev/null 2>&1; then
        echo "Fehler: '$pkg' nicht gefunden. Installiere:"
        echo "  sudo apt install qtbase5-dev qtdeclarative5-dev qtquickcontrols2-5-dev"
        echo "                libqt5sql5-sqlite cmake g++"
        exit 1
    fi
done

export DESKTOP_BUILD=1
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo
echo "Fertig! Binary: ${BUILD_DIR}/fuel-tracker"
echo "Start:"
echo "  export FUELTRACKER_QML=\"${SCRIPT_DIR}/qml/Main.qml\""
echo "  ${BUILD_DIR}/fuel-tracker"