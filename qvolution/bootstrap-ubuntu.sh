#!/usr/bin/env bash
set -euo pipefail
. /etc/os-release
if [[ "${ID:-}" != ubuntu || "${VERSION_ID:-}" != 26.04 ]]; then
  echo 'This dependency recipe targets Ubuntu 26.04. See upstream build instructions for other systems.' >&2
  exit 2
fi
# Uses Ubuntu packages only. apt displays the changes and asks before installing.
exec sudo apt-get install --no-install-recommends \
  cmake automake clang ninja-build git git-lfs bison flex ruby pkg-config \
  libfuse-dev libudev-dev libc6-dev-i386 gcc-multilib libcairo2-dev \
  libgl1-mesa-dev libglu1-mesa-dev libtiff-dev libfreetype6-dev libelf-dev \
  libxml2-dev libegl1-mesa-dev libfontconfig1-dev libbsd-dev libxrandr-dev \
  libxcursor-dev libgif-dev libavutil-dev libpulse-dev libavformat-dev \
  libavcodec-dev libswresample-dev libdbus-1-dev libxkbfile-dev libssl-dev \
  libstdc++-15-dev libcap-dev llvm-dev libvulkan-dev \
  libwayland-dev libwayland-egl-backend-dev libxkbcommon-dev wayland-protocols
