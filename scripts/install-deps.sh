#!/usr/bin/env bash
set -e

OS="$(uname)"

if [[ "$OS" == "Darwin" ]]; then
  # macOS (Homebrew)
  if command -v brew >/dev/null 2>&1; then
    brew install tinyxml2
  else
    echo "Homebrew not found. Install Homebrew or use CMake FetchContent."
    exit 1
  fi
else
  # assume Debian/Ubuntu
  sudo apt-get update
  sudo apt-get install -y libtinyxml2-dev
fi