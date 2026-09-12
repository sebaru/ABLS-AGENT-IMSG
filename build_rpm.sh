#!/bin/bash
# build_rpm.sh -- Build RPM via CPack
set -euo pipefail

PACKAGE_ONLY=false

for arg in "$@"; do
  case "$arg" in
    --package-only|-p)
      PACKAGE_ONLY=true
      ;;
    -h|--help)
      echo "Usage: $0 [--package-only|-p]"
      exit 0
      ;;
    *)
      echo "Usage: $0 [--package-only|-p]"
      exit 2
      ;;
  esac
done

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

echo "Building RPM package for abls-agent-imsg..."
echo "Project directory: $PROJECT_DIR"
echo "Build directory:   $BUILD_DIR"
echo "Package-only mode: $PACKAGE_ONLY"
echo "Signing mode:      disabled (centralized in ABLS-PKGS)"
echo "Install prefix:    /usr (forced for RPM packaging)"

mkdir -p "$BUILD_DIR"

cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=RelWithDebInfo

if [[ "$PACKAGE_ONLY" == "false" ]]; then
  cmake --build "$BUILD_DIR" -- -j"$(nproc)"
elif [[ ! -f "$BUILD_DIR/CPackConfig.cmake" ]]; then
  echo "Missing $BUILD_DIR/CPackConfig.cmake. Run ./build.sh first or use full mode."
  exit 1
fi

rm -f "$BUILD_DIR"/abls-agent-imsg-*.rpm

pushd "$BUILD_DIR" >/dev/null
cpack -G RPM
popd >/dev/null

runtime_rpm=$(find "$BUILD_DIR" -maxdepth 1 -type f -name 'abls-agent-imsg-[0-9]*.rpm' | sort | tail -n 1)
debuginfo_rpm=$(find "$BUILD_DIR" -maxdepth 1 -type f -name '*debuginfo*.rpm' | sort | tail -n 1)

if [[ -z "$runtime_rpm" ]]; then
  echo "RPM generation failed: expected runtime package in $BUILD_DIR"
  exit 1
fi

if [[ -z "$debuginfo_rpm" ]]; then
  echo "RPM generation failed: expected debuginfo package in $BUILD_DIR"
  exit 1
fi

pubdir="${ABLS_PKGS_REPO_DIR:-$PROJECT_DIR/../ABLS-PKGS}"
if [[ -d "$pubdir/public" ]]; then
  for rpm_file in "$BUILD_DIR"/*.rpm; do
    arch="$(rpm -qp --qf '%{ARCH}' "$rpm_file" 2>/dev/null || true)"
    outdir="$pubdir/public/rpms"
    [[ -n "$arch" ]] && outdir="$outdir/$arch"
    mkdir -p "$outdir"
    cp -f "$rpm_file" "$outdir/"
  done
fi

echo "RPM generated:"
echo "  $runtime_rpm"
echo "  $debuginfo_rpm"
echo "RPM build complete."
