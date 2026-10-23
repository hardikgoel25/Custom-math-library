// tests/test_json.cpp
#include "cml_test.hpp"
#include "cml/json.hpp"

#include <string>

using cml::Json;
using cml::JsonError;

TEST_CASE("json: parse and dump round trip") {
    const std::string text = R"({"a":1,"b":[true,false,null,"x\ny",2.5e3,-0.125],"c":{"d":"\u00e9\u20ac"},"e":""})";
    Json j = Json::parse(text);
    CHECK(j.is_object());
    CHECK_EQ(j.at("a").as_number(), 1.0);
    CHECK_EQ(j.at("b").size(), 6u);
    CHECK(j.at("b").at(0).as_bool());
    CHECK(j.at("b").at(2).is_null());
    CHECK_EQ(j.at("b").at(3).as_string(), std::string("x\ny"));
    CHECK_EQ(j.at("b").at(4).as_number(), 2500.0);
    CHECK_EQ(j.at("b").at(5).as_number(), -0.125);
    CHECK_EQ(j.at("c").at("d").as_string(), std::string("\xC3\xA9\xE2\x82\xAC"));
    CHECK_EQ(j.at("e").as_string(), std::string(""));
    const std::string dumped = j.dump();
    Json again = Json::parse(dumped);
    CHECK_EQ(again.dump(), dumped);
    CHECK_EQ(Json::parse(j.dump(2)).dump(), dumped); // pretty printing is still valid JSON
}

TEST_CASE("json: number formatting") {
    CHECK_EQ(Json(3.0).dump(), std::string("3"));
    CHECK_EQ(Json(-0.5).dump(), std::string("-0.5"));
    CHECK_EQ(Json(0.1).dump(), std::string("0.1"));
    CHECK_EQ(Json(1e21).dump(), std::string("1e+21"));
    CHECK_EQ(Json(1.0 / 3.0).dump(), std::string("0.3333333333333333"));
    CHECK_EQ(Json(1.0 / 0.0).dump(), std::string("null"));   // non-finite becomes null
    CHECK_EQ(Json(123456789012.0).dump(), std::string("123456789012"));
    CHECK_EQ(Json::parse("1E2").as_number(), 100.0);
    CHECK_EQ(Json::parse("-0").as_number(), 0.0);
    CHECK_EQ(Json::parse("[1.5e-3]").at(0).as_number(), 0.0015);
}

TEST_CASE("json: strict parsing errors") {
    CHECK_THROWS(Json::parse(""), JsonError);
    CHECK_THROWS(Json::parse("{"), JsonError);
    CHECK_THROWS(Json::parse("[1,]"), JsonError);
    CHECK_THROWS(Json::parse("{\"a\":1,}"), JsonError);
    CHECK_THROWS(Json::parse("{a:1}"), JsonError);
    CHECK_THROWS(Json::parse("[1] x"), JsonError);
    CHECK_THROWS(Json::parse("01"), JsonError);
    CHECK_THROWS(Json::parse("1."), JsonError);
    CHECK_THROWS(Json::parse("\"abc"), JsonError);
    CHECK_THROWS(Json::parse("\"\\q\""), JsonError);
    CHECK_THROWS(Json::parse("\"\\u12\""), JsonError);
    CHECK_THROWS(Json::parse("\"\\ud800\""), JsonError);     // lone surrogate
    CHECK_THROWS(Json::parse("\"a\x01\""), JsonError);        // control character
    CHECK_THROWS(Json::parse("tru"), JsonError);
    CHECK_THROWS(Json::parse("NaN"), JsonError);
    CHECK_THROWS(Json::parse("[[[[[[[[[[1]]]]]]]]]]", 5), JsonError); // depth limit
    CHECK_THROWS(Json::parse("[1,2,3]", 64, 4), JsonError);          // size limit
    CHECK_NOTHROW(Json::parse("[[[[[[[[[[1]]]]]]]]]]", 10));
    CHECK_NOTHROW(Json::parse("  \n\t[ ]  "));
    CHECK_EQ(Json::parse("\"\\ud83d\\ude00\"").as_string(), std::string("\xF0\x9F\x98\x80"));
}

TEST_CASE("json: object helpers and type errors") {
    Json o = Json::object();
    o["x"] = 1;
    o["y"] = "s";
    o.set("z", Json::array());
    o["z"].push_back(1.5);
    o["x"] = 2; // overwrite keeps position
    CHECK_EQ(o.dump(), std::string(R"({"x":2,"y":"s","z":[1.5]})"));
    CHECK(o.contains("y"));
    CHECK(!o.contains("w"));
    CHECK(o.find("w") == nullptr);
    CHECK_THROWS(o.at("w"), JsonError);
    CHECK_THROWS(o.at("x").as_string(), JsonError);
    CHECK_THROWS(o.at("y").as_number(), JsonError);
    CHECK_THROWS(o.at("z").at(5), JsonError);
    CHECK_EQ(std::string(o.at("z").type_name()), std::string("array"));
    Json dup = Json::parse(R"({"k":1,"k":2})");
    CHECK_EQ(dup.size(), 1u);
    CHECK_EQ(dup.at("k").as_number(), 2.0);
    Json v(std::vector<double>{1, 2, 3});
    CHECK_EQ(v.dump(), std::string("[1,2,3]"));
    Json m(std::vector<std::vector<double>>{{1, 2}, {3, 4}});
    CHECK_EQ(m.dump(), std::string("[[1,2],[3,4]]"));
    Json esc("tab\there \"q\" back\\slash \x01");
    CHECK_EQ(esc.dump(), std::string(R"("tab\there \"q\" back\\slash \u0001")"));
}
