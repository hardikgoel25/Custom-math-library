// cml/json.cpp — strict JSON parser/serialiser with depth and size limits.
#include "cml/json.hpp"
#include "cml/core.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace cml {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
Json::Json() : type_(Type::Null) {}
Json::Json(std::nullptr_t) : type_(Type::Null) {}
Json::Json(bool b) : type_(Type::Bool), bool_(b) {}
Json::Json(int v) : type_(Type::Number), num_(static_cast<double>(v)) {}
Json::Json(long v) : type_(Type::Number), num_(static_cast<double>(v)) {}
Json::Json(long long v) : type_(Type::Number), num_(static_cast<double>(v)) {}
Json::Json(unsigned v) : type_(Type::Number), num_(static_cast<double>(v)) {}
Json::Json(unsigned long v) : type_(Type::Number), num_(static_cast<double>(v)) {}
Json::Json(unsigned long long v) : type_(Type::Number), num_(static_cast<double>(v)) {}
Json::Json(double v) : type_(Type::Number), num_(v) {}
Json::Json(const char* s) : type_(Type::String), str_(s ? s : "") {}
Json::Json(std::string s) : type_(Type::String), str_(std::move(s)) {}
Json::Json(Array a) : type_(Type::Array), arr_(std::make_shared<Array>(std::move(a))) {}
Json::Json(Object o) : type_(Type::Object), obj_(std::make_shared<Object>(std::move(o))) {}
Json::Json(const std::vector<double>& v) : type_(Type::Array), arr_(std::make_shared<Array>()) {
    arr_->reserve(v.size());
    for (double d : v) arr_->emplace_back(d);
}
Json::Json(const std::vector<std::vector<double>>& m) : type_(Type::Array), arr_(std::make_shared<Array>()) {
    arr_->reserve(m.size());
    for (const auto& row : m) arr_->emplace_back(row);
}
Json::Json(const std::vector<std::string>& v) : type_(Type::Array), arr_(std::make_shared<Array>()) {
    arr_->reserve(v.size());
    for (const auto& s : v) arr_->emplace_back(s);
}
Json Json::array() { return Json(Array{}); }
Json Json::object() { return Json(Object{}); }

const char* Json::type_name() const {
    switch (type_) {
        case Type::Null: return "null";
        case Type::Bool: return "boolean";
        case Type::Number: return "number";
        case Type::String: return "string";
        case Type::Array: return "array";
        case Type::Object: return "object";
    }
    return "unknown";
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------
bool Json::as_bool() const {
    if (type_ != Type::Bool) throw JsonError(std::string("expected boolean, got ") + type_name());
    return bool_;
}
double Json::as_number() const {
    if (type_ != Type::Number) throw JsonError(std::string("expected number, got ") + type_name());
    return num_;
}
const std::string& Json::as_string() const {
    if (type_ != Type::String) throw JsonError(std::string("expected string, got ") + type_name());
    return str_;
}
const Json::Array& Json::as_array() const {
    if (type_ != Type::Array) throw JsonError(std::string("expected array, got ") + type_name());
    return *arr_;
}
Json::Array& Json::as_array() {
    if (type_ != Type::Array) throw JsonError(std::string("expected array, got ") + type_name());
    return *arr_;
}
const Json::Object& Json::as_object() const {
    if (type_ != Type::Object) throw JsonError(std::string("expected object, got ") + type_name());
    return *obj_;
}
Json::Object& Json::as_object() {
    if (type_ != Type::Object) throw JsonError(std::string("expected object, got ") + type_name());
    return *obj_;
}

bool Json::contains(const std::string& key) const { return find(key) != nullptr; }
const Json* Json::find(const std::string& key) const {
    if (type_ != Type::Object) return nullptr;
    for (const auto& kv : *obj_)
        if (kv.first == key) return &kv.second;
    return nullptr;
}
const Json& Json::at(const std::string& key) const {
    const Json* v = find(key);
    if (!v) throw JsonError("missing key '" + key + "'");
    return *v;
}
Json& Json::operator[](const std::string& key) {
    if (type_ == Type::Null) {
        type_ = Type::Object;
        obj_ = std::make_shared<Object>();
    }
    if (type_ != Type::Object) throw JsonError(std::string("expected object, got ") + type_name());
    for (auto& kv : *obj_)
        if (kv.first == key) return kv.second;
    obj_->emplace_back(key, Json());
    return obj_->back().second;
}
void Json::set(const std::string& key, Json value) { (*this)[key] = std::move(value); }
std::size_t Json::size() const {
    if (type_ == Type::Array) return arr_->size();
    if (type_ == Type::Object) return obj_->size();
    return 0;
}
const Json& Json::at(std::size_t index) const {
    const Array& a = as_array();
    if (index >= a.size()) throw JsonError("array index out of range");
    return a[index];
}
void Json::push_back(Json value) {
    if (type_ == Type::Null) {
        type_ = Type::Array;
        arr_ = std::make_shared<Array>();
    }
    as_array().push_back(std::move(value));
}

// ---------------------------------------------------------------------------
// Serialisation
// ---------------------------------------------------------------------------
namespace {
void dump_string(std::string& out, const std::string& s) {
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    out.push_back('"');
}
void normalize_exponent(char* buf) {
    char* e = std::strpbrk(buf, "eE");
    if (!e) return;
    char* d = e + 1;
    if (*d == '+' || *d == '-') ++d;
    char* z = d;
    while (*z == '0' && *(z + 1) != '\0') ++z;
    if (z != d) std::memmove(d, z, std::strlen(z) + 1);
}
void dump_number(std::string& out, double v) {
    // JSON has no NaN/Infinity: emit null for non-finite values so that
    // consumers never receive invalid JSON. Callers that need to convey
    // non-finite values use strings explicitly.
    if (!is_finite(v)) { out += "null"; return; }
    if (v == floor(v) && abs(v) < 1e15) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.0f", v);
        out += buf;
        return;
    }
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.17g", v);
    normalize_exponent(buf);
    // prefer the shortest representation that round-trips
    char shorter[32];
    for (int p = 15; p <= 17; ++p) {
        std::snprintf(shorter, sizeof shorter, "%.*g", p, v);
        if (std::strtod(shorter, nullptr) == v) { normalize_exponent(shorter); out += shorter; return; }
    }
    out += buf;
}
void newline(std::string& out, int indent, int level) {
    if (indent <= 0) return;
    out.push_back('\n');
    out.append(static_cast<std::size_t>(indent * level), ' ');
}
} // namespace

void Json::dump_to(std::string& out, int indent, int level) const {
    switch (type_) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += bool_ ? "true" : "false"; break;
        case Type::Number: dump_number(out, num_); break;
        case Type::String: dump_string(out, str_); break;
        case Type::Array: {
            out.push_back('[');
            bool first = true;
            for (const Json& v : *arr_) {
                if (!first) out.push_back(',');
                first = false;
                newline(out, indent, level + 1);
                v.dump_to(out, indent, level + 1);
            }
            if (!arr_->empty()) newline(out, indent, level);
            out.push_back(']');
            break;
        }
        case Type::Object: {
            out.push_back('{');
            bool first = true;
            for (const auto& kv : *obj_) {
                if (!first) out.push_back(',');
                first = false;
                newline(out, indent, level + 1);
                dump_string(out, kv.first);
                out.push_back(':');
                if (indent > 0) out.push_back(' ');
                kv.second.dump_to(out, indent, level + 1);
            }
            if (!obj_->empty()) newline(out, indent, level);
            out.push_back('}');
            break;
        }
    }
}

std::string Json::dump(int indent) const {
    std::string out;
    dump_to(out, indent, 0);
    return out;
}

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------
class JsonParser {
public:
    JsonParser(const std::string& s, std::size_t max_depth) : s_(s), max_depth_(max_depth) {}

    Json parse_document() {
        skip_ws();
        Json v = parse_value(0);
        skip_ws();
        if (pos_ != s_.size()) fail("trailing characters after JSON value");
        return v;
    }

private:
    const std::string& s_;
    std::size_t pos_ = 0;
    std::size_t max_depth_;

    [[noreturn]] void fail(const std::string& msg) const {
        throw JsonError("JSON parse error at byte " + std::to_string(pos_) + ": " + msg);
    }
    bool eof() const { return pos_ >= s_.size(); }
    char peek() const { return eof() ? '\0' : s_[pos_]; }
    void skip_ws() {
        while (!eof() && (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n' || s_[pos_] == '\r')) ++pos_;
    }
    void expect(char c) {
        if (peek() != c) fail(std::string("expected '") + c + "'");
        ++pos_;
    }

    Json parse_value(std::size_t depth) {
        if (depth > max_depth_) fail("nesting too deep");
        switch (peek()) {
            case '{': return parse_object(depth);
            case '[': return parse_array(depth);
            case '"': return Json(parse_string());
            case 't': literal("true"); return Json(true);
            case 'f': literal("false"); return Json(false);
            case 'n': literal("null"); return Json();
            default:
                if (peek() == '-' || (peek() >= '0' && peek() <= '9')) return parse_number();
                fail("unexpected character");
        }
    }
    void literal(const char* word) {
        const std::size_t n = std::strlen(word);
        if (s_.compare(pos_, n, word) != 0) fail(std::string("invalid literal, expected ") + word);
        pos_ += n;
    }
    Json parse_number() {
        const std::size_t start = pos_;
        if (peek() == '-') ++pos_;
        if (peek() == '0') {
            ++pos_;
        } else if (peek() >= '1' && peek() <= '9') {
            while (peek() >= '0' && peek() <= '9') ++pos_;
        } else {
            fail("invalid number");
        }
        if (peek() == '.') {
            ++pos_;
            if (!(peek() >= '0' && peek() <= '9')) fail("invalid fraction");
            while (peek() >= '0' && peek() <= '9') ++pos_;
        }
        if (peek() == 'e' || peek() == 'E') {
            ++pos_;
            if (peek() == '+' || peek() == '-') ++pos_;
            if (!(peek() >= '0' && peek() <= '9')) fail("invalid exponent");
            while (peek() >= '0' && peek() <= '9') ++pos_;
        }
        const std::string text = s_.substr(start, pos_ - start);
        const double v = std::strtod(text.c_str(), nullptr);
        return Json(v);
    }
    static void append_utf8(std::string& out, unsigned cp) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    unsigned parse_hex4() {
        if (pos_ + 4 > s_.size()) fail("truncated \\u escape");
        unsigned v = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = s_[pos_++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned>(c - 'A' + 10);
            else fail("invalid hex digit in \\u escape");
        }
        return v;
    }
    std::string parse_string() {
        expect('"');
        std::string out;
        while (true) {
            if (eof()) fail("unterminated string");
            const unsigned char c = static_cast<unsigned char>(s_[pos_++]);
            if (c == '"') break;
            if (c < 0x20) fail("control character in string");
            if (c != '\\') { out.push_back(static_cast<char>(c)); continue; }
            if (eof()) fail("unterminated escape");
            const char e = s_[pos_++];
            switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    unsigned cp = parse_hex4();
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (pos_ + 6 <= s_.size() && s_[pos_] == '\\' && s_[pos_ + 1] == 'u') {
                            pos_ += 2;
                            const unsigned lo = parse_hex4();
                            if (lo >= 0xDC00 && lo <= 0xDFFF)
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            else
                                fail("invalid low surrogate");
                        } else {
                            fail("lone high surrogate");
                        }
                    }
                    append_utf8(out, cp);
                    break;
                }
                default: fail("invalid escape sequence");
            }
        }
        return out;
    }
    Json parse_array(std::size_t depth) {
        expect('[');
        Json::Array arr;
        skip_ws();
        if (peek() == ']') { ++pos_; return Json(std::move(arr)); }
        while (true) {
            skip_ws();
            arr.push_back(parse_value(depth + 1));
            skip_ws();
            if (peek() == ',') { ++pos_; continue; }
            if (peek() == ']') { ++pos_; break; }
            fail("expected ',' or ']' in array");
        }
        return Json(std::move(arr));
    }
    Json parse_object(std::size_t depth) {
        expect('{');
        Json::Object obj;
        skip_ws();
        if (peek() == '}') { ++pos_; return Json(std::move(obj)); }
        while (true) {
            skip_ws();
            if (peek() != '"') fail("expected string key");
            std::string key = parse_string();
            skip_ws();
            expect(':');
            skip_ws();
            Json value = parse_value(depth + 1);
            bool replaced = false;
            for (auto& kv : obj)
                if (kv.first == key) { kv.second = std::move(value); replaced = true; break; }
            if (!replaced) obj.emplace_back(std::move(key), std::move(value));
            skip_ws();
            if (peek() == ',') { ++pos_; continue; }
            if (peek() == '}') { ++pos_; break; }
            fail("expected ',' or '}' in object");
        }
        return Json(std::move(obj));
    }
};

Json Json::parse(const std::string& text, std::size_t max_depth, std::size_t max_bytes) {
    if (text.size() > max_bytes) throw JsonError("JSON input exceeds size limit");
    JsonParser p(text, max_depth);
    return p.parse_document();
}

} // namespace cml
