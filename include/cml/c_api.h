/* cml/c_api.h — C ABI for the compute engine (usable from C, WebAssembly, Python ctypes, Node FFI, ...). */
#ifndef CML_C_API_H
#define CML_C_API_H

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h> /* must be included outside extern "C" */
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && defined(CML_BUILD_DLL)
#define CML_API __declspec(dllexport)
#elif defined(__EMSCRIPTEN__)
#define CML_API EMSCRIPTEN_KEEPALIVE
#elif defined(__GNUC__) && !defined(_WIN32)
#define CML_API __attribute__((visibility("default")))
#else
#define CML_API
#endif

/* Engine version string, e.g. "2.0.0". Static storage; do not free. */
CML_API const char* cml_version(void);

/* Execute a JSON request ({"op": "...", "params": {...}}) and return a
 * newly allocated JSON response string. Never returns NULL on a valid
 * pointer input (errors are reported inside the JSON). Free with cml_free. */
CML_API char* cml_execute(const char* request_json);

/* Free a string returned by cml_execute. */
CML_API void cml_free(char* ptr);

#ifdef __cplusplus
}
#endif

#endif /* CML_C_API_H */
