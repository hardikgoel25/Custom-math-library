// cml/c_api.cpp — C ABI wrapper around the engine.
#include "cml/c_api.h"
#include "cml/engine.hpp"

#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {

const char* cml_version(void) { return cml::engine::version(); }

char* cml_execute(const char* request_json) {
    std::string response;
    try {
        if (!request_json) {
            response = "{\"ok\":false,\"error\":{\"code\":\"INVALID_REQUEST\",\"message\":\"null request\"}}";
        } else {
            response = cml::engine::execute(std::string(request_json));
        }
    } catch (...) {
        response = "{\"ok\":false,\"error\":{\"code\":\"INTERNAL\",\"message\":\"unexpected failure\"}}";
    }
    char* out = static_cast<char*>(std::malloc(response.size() + 1));
    if (!out) return nullptr;
    std::memcpy(out, response.c_str(), response.size() + 1);
    return out;
}

void cml_free(char* ptr) { std::free(ptr); }

} // extern "C"
