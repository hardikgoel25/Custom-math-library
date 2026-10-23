// cml/engine.hpp — JSON compute engine: one entry point for every feature.
//
//   Json response = cml::engine::execute(Json::parse(R"({"op":"roots.find","params":{...}})"));
//
// Requests: {"op": "<category.name>", "params": {...}, "id": <anything, echoed back>}
// Responses: {"ok": true, "op": "...", "result": {...}, "timeMs": 0.3}
//            {"ok": false, "op": "...", "error": {"code": "INVALID_PARAM", "message": "...", "param": "tol"}}
//
// The engine never throws and never crashes on malformed input: every
// parameter is validated and every numeric routine is wrapped.  Resource caps
// (iterations, samples, matrix sizes, polynomial degrees, data lengths) are
// enforced so that a request cannot exhaust CPU or memory.
#ifndef CML_ENGINE_HPP
#define CML_ENGINE_HPP

#include "cml/json.hpp"

#include <string>
#include <vector>

namespace cml {
namespace engine {

struct OpInfo {
    std::string name;         // e.g. "roots.find"
    std::string category;     // e.g. "roots"
    std::string description;
    Json params;              // {name: {type, required, default, description, enum?}}
};

const std::vector<OpInfo>& ops();
bool has_op(const std::string& name);

Json execute(const Json& request);
std::string execute(const std::string& request_json);

const char* version();

struct Limits {
    int max_iterations = 100000;
    int max_samples = 200000;         // table / plot samples
    int max_integration_n = 2000000;  // fixed-rule subdivisions
    int max_ode_steps = 200000;
    std::size_t max_matrix_dim = 400;
    int max_polynomial_degree = 300;
    std::size_t max_data_points = 500000;
    std::size_t max_request_bytes = 4u * 1024u * 1024u;
};
const Limits& limits();

} // namespace engine
} // namespace cml

#endif // CML_ENGINE_HPP
