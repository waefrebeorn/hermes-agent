#!/bin/bash
# Build example .so plugins
set -e
cd "$(dirname "$0")"
mkdir -p out
echo "Building example plugins..."
gcc -shared -fPIC -o out/hello_plugin.so hello_plugin.c
echo "  out/hello_plugin.so"
echo "Done. Plugins in out/"
ls -la out/
