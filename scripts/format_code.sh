#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
if ! command -v clang-format >/dev/null 2>&1; then
    echo "clang-format fehlt" >&2
    exit 1
fi
find src include -type f \( -name '*.cpp' -o -name '*.h' \) -exec clang-format -i -style=file {} +
echo "C++-Dateien formatiert und Kontrollbloecke ergaenzt. Bitte kompilieren und testen."
