// cml/json.hpp — a small, strict JSON value type used by the compute engine.
//
// Design goals: no dependencies, insertion-ordered objects (so responses are
// stable), hard limits on nesting depth and input size, exceptions on
// malformed input, and helpers that make parameter validation terse.
#ifndef CML_JSON_HPP
#define CML_JSON_HPP

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace cml {

class JsonError : public std::runtime_error {
public:
    explicit JsonError(const std::string& msg) : std::runtime_error(msg) {}
};

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };
    using Array  = std::vector<Json>;
    using Object = std::vector<std::pair<std::string, Json>>;

    Json();                                   // null
    Json(std::nullptr_t);
    Json(bool b);
    Json(int v);
    Json(long v);
    Json(long long v);
    Json(unsigned v);
    Json(unsigned long v);
    Json(unsigned long long v);
    Json(double v);
    Json(const char* s);
    Json(std::string s);
    Json(Array a);
    Json(Object o);
    Json(const std::vector<double>& v);
    Json(const std::vector<std::vector<double>>& m);
    Json(const std::vector<std::string>& v);

    static Json array();
    static Json object();

    Type type() const { return type_; }
    bool is_null() const   { return type_ == Type::Null; }
    bool is_bool() const   { return type_ == Type::Bool; }
    bool is_number() const { return type_ == Type::Number; }
    bool is_string() const { return type_ == Type::String; }
    bool is_array() const  { return type_ == Type::Array; }
    bool is_object() const { return type_ == Type::Object; }
    const char* type_name() const;

    // Accessors (throw JsonError on type mismatch)
    bool                as_bool() const;
    double              as_number() const;
    const std::string&  as_string() const;
    const Array&        as_array() const;
    Array&              as_array();
    const Object&       as_object() const;
    Object&             as_object();

    // Object helpers
    bool        contains(const std::string& key) const;
    const Json* find(const std::string& key) const;   // nullptr if absent
    const Json& at(const std::string& key) const;     // throws if absent
    Json&       operator[](const std::string& key);   // inserts null if absent
    void        set(const std::string& key, Json value);
    std::size_t size() const;                         // array/object length

    // Array helpers
    const Json& at(std::size_t index) const;
    void        push_back(Json value);

    // Serialisation
    std::string dump(int indent = 0) const;
    static Json parse(const std::string& text, std::size_t max_depth = 64,
                      std::size_t max_bytes = 4u * 1024u * 1024u);

private:
    Type type_;
    bool bool_ = false;
    double num_ = 0.0;
    std::string str_;
    std::shared_ptr<Array> arr_;
    std::shared_ptr<Object> obj_;

    void dump_to(std::string& out, int indent, int level) const;
    friend class JsonParser;
};

} // namespace cml

#endif // CML_JSON_HPP
