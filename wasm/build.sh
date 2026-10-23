#!/usr/bin/env bash
# Build the engine to WebAssembly with Emscripten.
#
#   source /path/to/emsdk/emsdk_env.sh
#   ./wasm/build.sh            # writes web/public/engine/cml.js + cml.wasm
#
# The module is an ES6 factory (createCmlModule) usable in browsers (main
# thread or Web Worker) and in Node. Exceptions use native WebAssembly
# exception handling (-fwasm-exceptions), supported by all current browsers.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=${OUT:-web/public/engine}
mkdir -p "$OUT"
command -v em++ >/dev/null 2>&1 || { echo "em++ not found: activate emsdk first (source emsdk_env.sh)"; exit 1; }
em++ -O3 -std=c++14 -fwasm-exceptions -Iinclude src/*.cpp \
  -o "$OUT/cml.js" \
  -s MODULARIZE=1 -s EXPORT_ES6=1 -s EXPORT_NAME=createCmlModule \
  -s ENVIRONMENT=web,worker,node \
  -s ALLOW_MEMORY_GROWTH=1 -s INITIAL_MEMORY=16MB -s MAXIMUM_MEMORY=512MB -s STACK_SIZE=1MB \
  -s FILESYSTEM=0 -s INVOKE_RUN=0 \
  -s "EXPORTED_FUNCTIONS=['_cml_execute','_cml_free','_cml_version','_malloc','_free']" \
  -s "EXPORTED_RUNTIME_METHODS=['cwrap','UTF8ToString','stringToUTF8','lengthBytesUTF8']"
ls -la "$OUT"/cml.js "$OUT"/cml.wasm
