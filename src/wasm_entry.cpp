// src/wasm_entry.cpp — entry point for the Emscripten build.
//
// The exported symbols are cml_execute / cml_free / cml_version from c_api.h;
// this file only provides main() so the module initialises cleanly.
#include "cml/c_api.h"

int main() {
    (void)cml_version();
    return 0;
}
