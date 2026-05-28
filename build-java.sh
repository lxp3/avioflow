#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
CLASSIFIER="${1:-$(uname -s | tr '[:upper:]' '[:lower:]')-$(uname -m)}"
BUILD_DIR="${ROOT_DIR}/build-java"

case "$CLASSIFIER" in
  linux-x86_64|linux-amd64) CLASSIFIER="linux-x86_64" ;;
  linux-aarch64|linux-arm64) CLASSIFIER="linux-aarch64" ;;
  darwin-x86_64|macos-x86_64) CLASSIFIER="macos-x86_64" ;;
  darwin-arm64|darwin-aarch64|macos-arm64|macos-aarch64) CLASSIFIER="macos-aarch64" ;;
esac

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DENABLE_WASAPI=OFF \
  -DENABLE_PYTHON=OFF \
  -DENABLE_NODE_JS=OFF \
  -DENABLE_JAVA=ON \
  -DENABLE_BINARY=OFF \
  -DBUILD_TESTS=OFF

cmake --build "$BUILD_DIR" --config Release -j "$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"

NATIVE_DIR="$ROOT_DIR/java/build/native/$CLASSIFIER"
mkdir -p "$NATIVE_DIR"
cp "$BUILD_DIR/bin/libavioflow_jni.so" "$NATIVE_DIR/" 2>/dev/null || \
cp "$BUILD_DIR/bin/libavioflow_jni.dylib" "$NATIVE_DIR/"

GRADLE_CMD="${GRADLE_CMD:-gradle}"
GRADLE_TASKS=(nativeJar)
if [ "${AVIOFLOW_SKIP_TESTS:-}" != "1" ]; then
  GRADLE_TASKS=(test nativeJar)
fi

"$GRADLE_CMD" -p "$ROOT_DIR/java" \
  -Pavioflow.nativeClassifier="$CLASSIFIER" \
  -Pavioflow.nativeLibraryDir="$NATIVE_DIR" \
  "${GRADLE_TASKS[@]}"
