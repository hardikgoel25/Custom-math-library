// apps/cli/main.cpp — command-line front end for the engine.
//
//   cml-cli '{"op":"roots.find","params":{"f":"x^2-2","a":1,"b":2}}'   one-shot JSON
//   cml-cli eval "sin(pi/4)^2"                                          quick expression
//   cml-cli ops                                                         list operations
//   cml-cli --stdin                                                     JSON-lines server (one request per line)
//   cml-cli repl                                                        interactive expression REPL
//   cml-cli --pretty ...                                                pretty-print responses
#include "cml/engine.hpp"
#include "cml/expression.hpp"
#include "cml/core.hpp"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

namespace {

void usage() {
    std::printf(
        "cml-cli %s — Custom Math Library compute engine\n\n"
        "Usage:\n"
        "  cml-cli [--pretty] '<json request>'      execute one request\n"
        "  cml-cli [--pretty] eval \"<expression>\"   evaluate an expression (x unbound => error)\n"
        "  cml-cli ops                              list available operations\n"
        "  cml-cli --stdin                          read one JSON request per line, write one response per line\n"
        "  cml-cli repl                             interactive expression calculator\n"
        "  cml-cli --version\n",
        cml::engine::version());
}

std::string render(const std::string& response, bool pretty) {
    if (!pretty) return response;
    try {
        return cml::Json::parse(response).dump(2);
    } catch (...) {
        return response;
    }
}

int run_repl() {
    std::printf("cml %s expression calculator. Type an expression, 'd <expr>' for its derivative, 'q' to quit.\n",
                cml::engine::version());
    std::string line;
    cml::Expression::Variables vars;
    double last = 0.0;
    while (true) {
        std::printf("> ");
        std::fflush(stdout);
        if (!std::getline(std::cin, line)) break;
        if (line == "q" || line == "quit" || line == "exit") break;
        if (line.empty()) continue;
        try {
            if (line.size() > 2 && line[0] == 'd' && line[1] == ' ') {
                cml::Expression e = cml::Expression::parse(line.substr(2));
                std::printf("%s\n", e.derivative("x").to_string().c_str());
                continue;
            }
            const std::size_t eq = line.find('=');
            if (eq != std::string::npos && eq > 0 && line.find_first_of("<>!", 0) == std::string::npos) {
                std::string name = line.substr(0, eq);
                while (!name.empty() && name.back() == ' ') name.pop_back();
                cml::Expression e = cml::Expression::parse(line.substr(eq + 1));
                vars["ans"] = last;
                const double v = e.evaluate(vars);
                vars[name] = v;
                std::printf("%s = %s\n", name.c_str(), cml::to_string(v, 15).c_str());
                continue;
            }
            cml::Expression e = cml::Expression::parse(line);
            vars["ans"] = last;
            last = e.evaluate(vars);
            std::printf("%s\n", cml::to_string(last, 15).c_str());
        } catch (const cml::ExpressionError& ex) {
            std::printf("error: %s (at position %zu)\n", ex.what(), ex.position());
        } catch (const std::exception& ex) {
            std::printf("error: %s\n", ex.what());
        }
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    bool pretty = false;
    int argi = 1;
    while (argi < argc && argv[argi][0] == '-' && std::strcmp(argv[argi], "--stdin") != 0 &&
           std::strcmp(argv[argi], "--version") != 0 && std::strcmp(argv[argi], "--help") != 0 && std::strcmp(argv[argi], "-h") != 0) {
        if (std::strcmp(argv[argi], "--pretty") == 0 || std::strcmp(argv[argi], "-p") == 0) pretty = true;
        else { std::fprintf(stderr, "unknown option %s\n", argv[argi]); usage(); return 2; }
        ++argi;
    }
    if (argi >= argc) { usage(); return 2; }
    const std::string cmd = argv[argi];

    if (cmd == "--version") { std::printf("%s\n", cml::engine::version()); return 0; }
    if (cmd == "--help" || cmd == "-h") { usage(); return 0; }
    if (cmd == "ops") {
        for (const auto& op : cml::engine::ops()) std::printf("%-26s %s\n", op.name.c_str(), op.description.c_str());
        return 0;
    }
    if (cmd == "repl") return run_repl();
    if (cmd == "eval") {
        if (argi + 1 >= argc) { std::fprintf(stderr, "eval needs an expression\n"); return 2; }
        cml::Json req = cml::Json::object();
        req["op"] = "expression.evaluate";
        cml::Json params = cml::Json::object();
        params["expr"] = argv[argi + 1];
        if (argi + 2 < argc) params["x"] = std::atof(argv[argi + 2]);
        req["params"] = params;
        const cml::Json res = cml::engine::execute(req);
        if (res.at("ok").as_bool()) {
            std::printf("%s\n", res.at("result").at("text").as_string().c_str());
            return 0;
        }
        std::fprintf(stderr, "error: %s\n", res.at("error").at("message").as_string().c_str());
        return 1;
    }
    if (cmd == "--stdin") {
        // JSON-lines protocol: used by the Node gateway. Never exits on bad input.
        std::ios::sync_with_stdio(false);
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;
            std::string out = cml::engine::execute(line);
            std::fwrite(out.data(), 1, out.size(), stdout);
            std::fputc('\n', stdout);
            std::fflush(stdout);
        }
        return 0;
    }
    // Otherwise the argument is a JSON request.
    const std::string response = cml::engine::execute(cmd);
    std::printf("%s\n", render(response, pretty).c_str());
    try {
        return cml::Json::parse(response).at("ok").as_bool() ? 0 : 1;
    } catch (...) {
        return 1;
    }
}
