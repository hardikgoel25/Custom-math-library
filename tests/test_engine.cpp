// tests/test_engine.cpp — contract tests for the JSON engine and the C ABI.
#include "cml_test.hpp"
#include "cml/c_api.h"
#include "cml/engine.hpp"

#include <cstring>
#include <string>

using cml::Json;
namespace eng = cml::engine;

namespace {
Json run(const std::string& op, const std::string& params_json) {
    Json req = Json::object();
    req["op"] = op;
    req["params"] = Json::parse(params_json);
    return eng::execute(req);
}
const Json& result_of(const Json& r) { return r.at("result"); }
bool ok(const Json& r) { return r.at("ok").as_bool(); }
std::string code(const Json& r) { return r.at("error").at("code").as_string(); }
} // namespace

TEST_CASE("engine: request validation and error codes") {
    CHECK_EQ(code(eng::execute(Json(3))), std::string("INVALID_REQUEST"));
    CHECK_EQ(code(eng::execute(Json::parse(R"json({"params":{}})json"))), std::string("INVALID_REQUEST"));
    CHECK_EQ(code(eng::execute(Json::parse(R"json({"op":"nope.nope"})json"))), std::string("UNKNOWN_OP"));
    CHECK_EQ(code(eng::execute(Json::parse(R"json({"op":"roots.find","params":[1]})json"))), std::string("INVALID_REQUEST"));
    CHECK_EQ(code(Json::parse(eng::execute(std::string("{not ")))), std::string("PARSE_ERROR"));
    CHECK_EQ(code(Json::parse(eng::execute(std::string(5 * 1024 * 1024, ' ')))), std::string("PARSE_ERROR"));
    const Json missing = run("roots.find", R"json({"method":"bisection"})json");
    CHECK(!ok(missing));
    CHECK_EQ(code(missing), std::string("INVALID_PARAM"));
    CHECK_EQ(missing.at("error").at("param").as_string(), std::string("f"));
    const Json badexpr = run("expression.evaluate", R"json({"expr":"2 +"})json");
    CHECK_EQ(code(badexpr), std::string("EXPRESSION_ERROR"));
    CHECK_EQ(badexpr.at("error").at("position").as_number(), 3.0);
    CHECK_EQ(code(run("roots.find", R"json({"f":"x^2-2","method":"bogus","a":1,"b":2})json")), std::string("INVALID_PARAM"));
    CHECK_EQ(code(run("roots.find", R"json({"f":"x^2-2","method":"bisection","a":1,"b":2,"tol":-1})json")), std::string("INVALID_PARAM"));
    CHECK_EQ(code(run("roots.find", R"json({"f":"x^2-2","method":"bisection","a":1,"b":2,"maxIter":1e9})json")), std::string("INVALID_PARAM"));
    CHECK_EQ(code(run("roots.find", R"json({"f":"x^2-2","method":"bisection","a":"abc","b":2})json")), std::string("INVALID_PARAM"));
    CHECK_EQ(code(run("roots.find", R"json({"f":"x*y","method":"bisection","a":1,"b":2})json")), std::string("INVALID_PARAM"));
    CHECK_EQ(code(run("matrix.compute", R"json({"A":[[1,2],[3]],"fn":"determinant"})json")), std::string("INVALID_PARAM"));
    CHECK_EQ(code(run("matrix.compute", R"json({"A":[[1,2,3],[4,5,6]],"fn":"determinant"})json")), std::string("INVALID_PARAM"));
    CHECK_EQ(code(run("matrix.compute", R"json({"A":[[1,2],[2,4]],"fn":"inverse"})json")), std::string("NUMERIC_ERROR"));
    CHECK_EQ(code(run("polynomial.roots", R"json({"p":"x^"})json")), std::string("INVALID_PARAM"));
    CHECK_EQ(code(run("special.numberTheory", R"json({"fn":"fibonacci","n":500})json")), std::string("INVALID_PARAM"));
    CHECK_EQ(code(run("special.numberTheory", R"json({"fn":"primes","n":1e9})json")), std::string("INVALID_PARAM"));
    CHECK_EQ(code(run("ode.solve", R"json({"f":"y","y0":[1],"method":"rk4"})json")), std::string("INVALID_PARAM")); // missing h
    CHECK_EQ(code(run("ode.solve", R"json({"f":"y + z","y0":[1],"method":"rk45"})json")), std::string("INVALID_PARAM")); // unknown variable z
    CHECK_EQ(code(run("stats.describe", R"json({"data":[]})json")), std::string("INVALID_PARAM"));
    CHECK_EQ(code(run("integrate.compute", R"json({"f":"x","a":0,"b":1,"method":"simpson","n":0})json")), std::string("INVALID_PARAM"));
    // id is echoed back on success and failure
    Json req = Json::parse(R"json({"op":"expression.evaluate","params":{"expr":"1+1"},"id":"abc"})json");
    CHECK_EQ(eng::execute(req).at("id").as_string(), std::string("abc"));
    req = Json::parse(R"json({"op":"expression.evaluate","params":{"expr":"1+"},"id":42})json");
    CHECK_EQ(eng::execute(req).at("id").as_number(), 42.0);
    // unknown parameters are ignored, params may be omitted
    CHECK(ok(eng::execute(Json::parse(R"json({"op":"core.constants"})json"))));
    CHECK(ok(eng::execute(Json::parse(R"json({"op":"core.constants","params":{"bogus":1}})json"))));
}

TEST_CASE("engine: meta and expression ops") {
    const Json ops = run("meta.ops", "{}");
    CHECK(ok(ops));
    CHECK(result_of(ops).at("count").as_number() >= 30);
    CHECK(eng::has_op("roots.find"));
    CHECK(!eng::has_op("roots.magic"));
    CHECK_EQ(result_of(run("meta.version", "{}")).at("version").as_string(), std::string(eng::version()));
    Json r = run("expression.evaluate", R"json({"expr":"2x^2 + y","x":3,"y":1})json");
    CHECK(ok(r));
    CHECK_EQ(result_of(r).at("value").as_number(), 19.0);
    r = run("expression.evaluate", R"json({"expr":"a*b","vars":{"a":2,"b":"pi"}})json");
    CHECK_CLOSE(result_of(r).at("value").as_number(), 6.283185307179586, 1e-15);
    r = run("expression.evaluate", R"json({"expr":"1/0"})json");
    CHECK(ok(r));
    CHECK(result_of(r).at("value").is_null());        // non-finite numbers serialise as null
    CHECK_EQ(result_of(r).at("text").as_string(), std::string("Infinity"));
    CHECK(!result_of(r).at("finite").as_bool());
    r = run("expression.derivative", R"json({"expr":"x^3","at":2})json");
    CHECK_EQ(result_of(r).at("derivative").as_string(), std::string("3*x^2"));
    CHECK_EQ(result_of(r).at("value").as_number(), 12.0);
    r = run("expression.derivative", R"json({"expr":"sin(x)","order":2})json");
    CHECK_EQ(result_of(r).at("derivative").as_string(), std::string("-sin(x)"));
    r = run("expression.table", R"json({"expr":"x^2","from":0,"to":1,"samples":3,"derivative":true})json");
    CHECK_EQ(result_of(r).at("y").at(2).as_number(), 1.0);
    CHECK_EQ(result_of(r).at("dy").at(2).as_number(), 2.0);
    CHECK_EQ(result_of(r).at("x").size(), 3u);
    r = run("expression.table", R"json({"expr":"1/x","from":-1,"to":1,"samples":3})json");
    CHECK(result_of(r).at("y").at(1).is_null());
    CHECK_EQ(code(run("expression.table", R"json({"expr":"x","samples":1e7})json")), std::string("INVALID_PARAM"));
    r = run("expression.parse", R"json({"expr":"2x + 3y"})json");
    CHECK_EQ(result_of(r).at("variables").size(), 2u);
    CHECK(result_of(run("expression.functions", "{}")).at("functions").size() > 40);
    r = run("core.evaluate", R"json({"fn":"sin","x":1})json");
    CHECK_CLOSE(result_of(r).at("value").as_number(), 0.8414709848078965, 1e-15);
    r = run("core.compare", R"json({"fn":"exp","from":-2,"to":2,"samples":50})json");
    CHECK(ok(r));
    CHECK(result_of(r).at("worstUlps").as_number() < 4.0);
    CHECK_EQ(result_of(r).at("rows").size(), 50u);
    CHECK_EQ(code(run("core.evaluate", R"json({"fn":"nope","x":1})json")), std::string("INVALID_PARAM"));
    CHECK(result_of(run("core.constants", "{}")).contains("pi"));
    r = run("special.numberTheory", R"json({"fn":"primeFactors","n":360})json");
    CHECK_EQ(result_of(r).at("factors").size(), 6u);
    r = run("special.numberTheory", R"json({"fn":"fibonacci","n":90})json");
    CHECK_EQ(result_of(r).at("exact").as_string(), std::string("2880067194370816120"));
    r = run("special.numberTheory", R"json({"fn":"factorial","n":20})json");
    CHECK_EQ(result_of(r).at("exact").as_string(), std::string("2432902008176640000"));
}

TEST_CASE("engine: numerical ops end to end") {
    Json r = run("roots.find", R"json({"f":"x^2 - 2","method":"newton","x0":1})json");
    CHECK(ok(r));
    CHECK(result_of(r).at("converged").as_bool());
    CHECK_CLOSE(result_of(r).at("root").as_number(), 1.4142135623730951, 1e-14);
    CHECK_EQ(result_of(r).at("derivative").as_string(), std::string("2*x"));
    CHECK(result_of(r).at("history").size() > 2);
    r = run("roots.find", R"json({"f":"cos(t) - t","method":"brent","a":0,"b":1})json");
    CHECK_EQ(result_of(r).at("var").as_string(), std::string("t"));
    CHECK_CLOSE(result_of(r).at("root").as_number(), 0.7390851332151607, 1e-12);
    r = run("roots.find", R"json({"f":"x^2 - 2","method":"bisection","a":2,"b":3})json");
    CHECK(ok(r));                                   // numerical failure is a result, not an error
    CHECK(!result_of(r).at("converged").as_bool());
    r = run("roots.compare", R"json({"f":"x^3 - x - 2","a":1,"b":2,"x0":1.5})json");
    CHECK(result_of(r).at("results").size() >= 8);
    r = run("roots.all", R"json({"f":"sin(x)","a":0.5,"b":10,"samples":500})json");
    CHECK_EQ(result_of(r).at("count").as_number(), 3.0);
    r = run("polynomial.roots", R"json({"p":"x^3 - 6x^2 + 11x - 6"})json");
    CHECK_EQ(result_of(r).at("realRoots").size(), 3u);
    CHECK_CLOSE(result_of(r).at("realRoots").at(2).as_number(), 3.0, 1e-9);
    r = run("polynomial.arithmetic", R"json({"a":[1,1],"b":[-1,1],"fn":"multiply"})json");
    CHECK_EQ(result_of(r).at("result").at("text").as_string(), std::string("x^2 - 1"));
    r = run("polynomial.calculus", R"json({"p":"x^2","fn":"definite_integral","a":0,"b":3})json");
    CHECK_EQ(result_of(r).at("value").as_number(), 9.0);
    r = run("polynomial.fromRoots", R"json({"roots":[1,2]})json");
    CHECK_EQ(result_of(r).at("text").as_string(), std::string("x^2 - 3x + 2"));
    r = run("matrix.compute", R"json({"A":[[2,1],[1,2]],"fn":"eigen"})json");
    CHECK(result_of(r).at("symmetric").as_bool());
    CHECK_CLOSE(result_of(r).at("values").at(0).as_number(), 3.0, 1e-12);
    r = run("matrix.compute", R"json({"A":"4 3; 6 3","fn":"determinant"})json");
    CHECK_CLOSE(result_of(r).at("value").as_number(), -6.0, 1e-12);
    r = run("matrix.compute", R"json({"A":[[2,1],[1,3]],"b":[3,5],"fn":"solve"})json");
    CHECK_CLOSE(result_of(r).at("x").at(0).as_number(), 0.8, 1e-12);
    CHECK(result_of(r).at("residual").as_number() < 1e-12);
    r = run("matrix.compute", R"json({"A":[[1,2],[3,4]],"B":[[1,0],[0,1]],"fn":"multiply"})json");
    CHECK_EQ(result_of(r).at("matrix").at(1).at(0).as_number(), 3.0);
    r = run("matrix.compute", R"json({"A":[[0,1,2],[3,4,5],[6,7,9]],"fn":"lu"})json");
    CHECK(result_of(r).contains("P"));
    r = run("optimize.minimize", R"json({"f":"(x-2)^2 + 1","method":"golden","a":0,"b":5})json");
    CHECK_CLOSE(result_of(r).at("x").as_number(), 2.0, 1e-6);
    r = run("optimize.minimize", R"json({"f":"-(x-2)^2","method":"brent","a":0,"b":5,"maximize":true})json");
    CHECK_CLOSE(result_of(r).at("x").as_number(), 2.0, 1e-6);
    CHECK_CLOSE_ABS(result_of(r).at("fx").as_number(), 0.0, 1e-10);
    r = run("optimize.minimizeNd", R"json({"f":"(1-x)^2 + 100(y-x^2)^2","x0":[-1.2,1],"method":"nelder_mead","tol":1e-10,"maxIter":5000})json");
    CHECK(result_of(r).at("converged").as_bool());
    CHECK_CLOSE(result_of(r).at("x").at(0).as_number(), 1.0, 1e-4);
    CHECK(result_of(r).at("path").size() > 10);
    r = run("optimize.surface", R"json({"f":"x^2+y^2","xSamples":3,"ySamples":3})json");
    CHECK_EQ(result_of(r).at("z").at(1).at(1).as_number(), 0.0);
    r = run("integrate.compute", R"json({"f":"sin(x)","a":0,"b":"pi","method":"romberg"})json");
    CHECK_CLOSE(result_of(r).at("value").as_number(), 2.0, 1e-12);
    CHECK(result_of(r).contains("table"));
    r = run("integrate.compute", R"json({"f":"exp(-x^2)","a":"-inf","b":"inf"})json");
    CHECK_CLOSE(result_of(r).at("value").as_number(), 1.7724538509055159, 1e-8);
    r = run("integrate.compare", R"json({"f":"x^3","a":0,"b":1,"n":10})json");
    CHECK(result_of(r).at("results").size() >= 8);
    r = run("integrate.convergence", R"json({"f":"sin(x)","a":0,"b":3,"method":"trapezoid","ns":[2,4,8]})json");
    CHECK(result_of(r).at("rows").at(2).at("absError").as_number() < result_of(r).at("rows").at(0).at("absError").as_number());
    r = run("ode.solve", R"json({"f":"y","y0":[1],"t0":0,"t1":1,"method":"rk4","h":0.01})json");
    CHECK(result_of(r).at("success").as_bool());
    CHECK_CLOSE(result_of(r).at("final").at(0).as_number(), 2.718281828459045, 1e-9);
    CHECK_EQ(result_of(r).at("t").size(), 101u);
    r = run("ode.solve", R"json({"equations":["v","-x"],"state":["x","v"],"y0":[1,0],"t0":0,"t1":6.283185307179586,"method":"rk45","tol":1e-10})json");
    CHECK(result_of(r).at("success").as_bool());
    CHECK_CLOSE(result_of(r).at("final").at(0).as_number(), 1.0, 1e-7);
    CHECK_EQ(result_of(r).at("y").size(), 2u);
    CHECK_EQ(result_of(r).at("y").at(0).size(), result_of(r).at("t").size());
    CHECK(result_of(r).at("y").at(0).at(3).as_number() != result_of(r).at("y").at(1).at(3).as_number()); // states are not aliased
    r = run("ode.solve", R"json({"f":"y","y0":[1],"t0":0,"t1":1,"method":"euler","h":0.0001,"maxPoints":50})json");
    CHECK(result_of(r).at("thinned").as_bool());
    CHECK(result_of(r).at("t").size() <= 52);
    r = run("interpolate.compute", R"json({"xs":[0,1,2,3],"ys":[1,2,5,10],"method":"lagrange","x":1.5,"samples":5})json");
    CHECK_EQ(result_of(r).at("polynomial").at("text").as_string(), std::string("x^2 + 1"));
    CHECK_CLOSE(result_of(r).at("value").as_number(), 3.25, 1e-12);
    CHECK_EQ(result_of(r).at("curve").at("x").size(), 5u);
    r = run("interpolate.compute", R"json({"points":[[0,0],[1,1],[2,0]],"method":"spline","x":0.5})json");
    CHECK(ok(r));
    CHECK_EQ(result_of(r).at("segments").size(), 2u);
    r = run("interpolate.compute", R"json({"xs":[1,2,3,4],"ys":[2,4,6,8],"method":"polyfit","degree":1})json");
    CHECK_CLOSE(result_of(r).at("rSquared").as_number(), 1.0, 1e-12);
    r = run("stats.describe", R"json({"data":"2, 4, 4, 4, 5, 5, 7, 9"})json");
    CHECK_EQ(result_of(r).at("mean").as_number(), 5.0);
    CHECK_EQ(result_of(r).at("stddevPopulation").as_number(), 2.0);
    CHECK(result_of(r).at("histogram").at("counts").size() > 0);
    r = run("stats.regression", R"json({"x":[1,2,3,4],"y":[2,4,6,8]})json");
    CHECK_CLOSE(result_of(r).at("slope").as_number(), 2.0, 1e-12);
    r = run("stats.transform", R"json({"data":[1,2,3,4,5],"fn":"moving_average","window":3})json");
    CHECK_EQ(result_of(r).at("result").size(), 3u);
    CHECK(r.at("timeMs").as_number() >= 0.0);
}

TEST_CASE("engine: C ABI") {
    CHECK_EQ(std::string(cml_version()), std::string(eng::version()));
    char* out = cml_execute(R"json({"op":"expression.evaluate","params":{"expr":"6*7"}})json");
    CHECK(out != nullptr);
    const Json j = Json::parse(out);
    CHECK(j.at("ok").as_bool());
    CHECK_EQ(j.at("result").at("value").as_number(), 42.0);
    cml_free(out);
    char* bad = cml_execute("not ");
    CHECK(std::strstr(bad, "PARSE_ERROR") != nullptr);
    cml_free(bad);
    char* nul = cml_execute(nullptr);
    CHECK(std::strstr(nul, "INVALID_REQUEST") != nullptr);
    cml_free(nul);
    cml_free(nullptr);
}
