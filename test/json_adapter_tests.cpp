#include "Json.h"

#include <cmath>
#include <iostream>
#include <string>

static int gFailures = 0;

static void expect(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "json adapter test failed: " << message << std::endl;
        gFailures += 1;
    }
}

static std::string nestedBrackets(int depth)
{
    std::string text;
    text.reserve(static_cast<size_t>(depth) * 2);
    for (int i = 0; i < depth; ++i)
        text.push_back('[');
    for (int i = 0; i < depth; ++i)
        text.push_back(']');
    return text;
}

int main()
{
    Json object = Json::parse("{\"name\":\"ada\",\"n\":3,\"ok\":true}");
    expect(object.is_object(), "parsed value should be an object");
    expect(object["name"].get<std::string>() == "ada", "string field should round-trip");
    expect(object["n"].get<int>() == 3, "integer field should round-trip");
    expect(object.value("absent", 7) == 7, "missing key should return the fallback");
    expect(object.contains("ok"), "contains should see a present key");

    JsonRef nameField = object["name"];
    nameField = "bea";
    expect(object["name"].get<std::string>() == "bea", "JsonRef assignment should update the parent");

    Json dumped = Json::parse(object.dump());
    expect(dumped["name"].get<std::string>() == "bea", "dump then parse should keep the assigned string");
    expect(dumped["ok"].get<bool>(), "dump then parse should keep the flag");

    Json nonFinite = Json::parse("{\"x\":inf,\"y\":NaN}");
    expect(std::isinf(nonFinite["x"].get<double>()), "inf should parse");
    expect(std::isnan(nonFinite["y"].get<double>()), "NaN should parse");
    Json nonFiniteAgain = Json::parse(nonFinite.dump());
    expect(std::isinf(nonFiniteAgain["x"].get<double>()), "dumped inf should parse again");
    expect(std::isnan(nonFiniteAgain["y"].get<double>()), "dumped NaN should parse again");

    Json discarded = Json::parse("{", nullptr, false);
    expect(discarded.is_discarded(), "malformed text with exceptions off should be discarded");

    bool threw = false;
    try
    {
        Json::parse("{");
    }
    catch (const Json::parse_error &)
    {
        threw = true;
    }
    expect(threw, "malformed text should throw parse_error");

    Json tooDeep = Json::parse(nestedBrackets(65), nullptr, false);
    expect(tooDeep.is_discarded(), "nesting past 64 should be discarded when exceptions are off");

    bool depthThrew = false;
    try
    {
        Json::parse(nestedBrackets(65));
    }
    catch (const Json::parse_error &)
    {
        depthThrew = true;
    }
    expect(depthThrew, "nesting past 64 should throw parse_error");

    Json shallow = Json::parse(nestedBrackets(64));
    expect(shallow.is_array(), "nesting of 64 should still parse");

    if (gFailures != 0)
    {
        std::cerr << gFailures << " json adapter test(s) failed" << std::endl;
        return 1;
    }
    std::cout << "json adapter tests passed" << std::endl;
    return 0;
}
