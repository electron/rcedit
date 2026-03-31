#!/usr/bin/env bash
set -eo pipefail

VERSION="$1"

dist/rcedit-x64.exe dist/rcedit-x86.exe --set-product-version "$VERSION" --set-file-version "$VERSION"
dist/rcedit-x86.exe -h | grep -q "Rcedit v$VERSION"
dist/rcedit-x86.exe dist/rcedit-x64.exe --set-product-version "$VERSION" --set-file-version "$VERSION"
dist/rcedit-x64.exe -h | grep -q "Rcedit v$VERSION"
