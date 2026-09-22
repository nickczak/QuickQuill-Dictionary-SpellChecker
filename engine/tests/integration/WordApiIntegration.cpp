#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "MockDB.h"
#include "core/Dictionary.h"
#include "core/SpellChecker.h"
#include "http/services/WordService.h"

namespace
{
// Seeds a shared temporary database once and points DATABASE_PATH at it. Each
// TEST_CASE can then build its own Dictionary/WordService on the same data.
void seedIntegrationDb()
{
  static bool seeded = []
  {
    const auto dbPath = test_support::tempDbPath("qq_integration.sqlite");
    auto db = test_support::makeFreshDb(dbPath);
    test_support::seedWord(db, "lumen", "unit of luminous flux", {"light"});
    test_support::seedWord(db, "glints", "brief flashes of light", {});
    test_support::seedWord(db, "nevermore", "never again", {});
    Dictionary::clearGlobalCache();
    return true;
  }();
  (void)seeded;
}

bool isValidReturnedLemma(const std::string &lemma)
{
  const std::vector<std::string> seeds{"lumen", "glints", "nevermore"};
  return std::find(seeds.begin(), seeds.end(), lemma) != seeds.end();
}
} // end namespace

TEST_CASE("WordService::search", "[integration][api]")
{
  seedIntegrationDb();

  Dictionary dict;
  SpellChecker checker(dict);
  http::WordService service(dict, checker);

  SECTION("returns 200 with found word")
  {
    auto res = service.search("lumen");

    REQUIRE((res.status == 200));
    auto json = nlohmann::json::parse(res.body);
    CHECK((json["lemma"] == "lumen"));
    CHECK((json["senses"].size() == 1));
    CHECK((json["senses"][0]["definition"] == "unit of luminous flux"));
  }

  SECTION("returns 404 with suggestion when missing")
  {
    auto res = service.search("lumon");

    REQUIRE((res.status == 404));
    auto json = nlohmann::json::parse(res.body);
    CHECK((json["found"] == false));
    CHECK(json["suggestion"].is_string());
  }

  SECTION("returns 404 without suggestion for gibberish far from any word")
  {
    auto res = service.search("zzzzz");

    REQUIRE((res.status == 404));
    auto json = nlohmann::json::parse(res.body);
    CHECK((json["found"] == false));
    // the unknown word must never be echoed back as its own suggestion
    CHECK_FALSE(json.contains("suggestion"));
  }
}

TEST_CASE("WordService::wordOfTheDay", "[integration][api]")
{
  seedIntegrationDb();

  Dictionary dict;
  SpellChecker checker(dict);
  http::WordService service(dict, checker);

  SECTION("returns 200 with a real dictionary word")
  {
    auto res = service.wordOfTheDay(40000);

    REQUIRE((res.status == 200));
    auto json = nlohmann::json::parse(res.body);
    REQUIRE(json["lemma"].is_string());
    CHECK(isValidReturnedLemma(json["lemma"]));
    CHECK((json["senses"][0]["definition"].is_string()));
    // the lemma is echoed as `query` so the frontend treats it as a lookup hit
    CHECK((json["query"] == json["lemma"]));
  }

  SECTION("same day number always returns the same word")
  {
    auto first = service.wordOfTheDay(40001);
    auto second = service.wordOfTheDay(40001);

    REQUIRE(first.status == 200);
    REQUIRE(second.status == 200);
    auto j1 = nlohmann::json::parse(first.body);
    auto j2 = nlohmann::json::parse(second.body);
    CHECK(j1["lemma"] == j2["lemma"]);
  }
}
