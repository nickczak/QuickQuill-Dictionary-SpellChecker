#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <iterator>
#include <string>

#include "core/Trie.h"
#include "data/Database.h"
#include "dct/dct.h"

namespace
{

std::filesystem::path makeTempDbPath()
{
  auto dir = std::filesystem::temp_directory_path();
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  return dir / ("qq-databasetests-" + std::to_string(stamp) + ".db");
}

struct TestDb
{
  std::filesystem::path path;
  Database db;

  TestDb() : path(makeTempDbPath()), db(path.string()) { db.createTables(); }

  ~TestDb()
  {
    std::error_code ec;
    std::filesystem::remove(path, ec);
  }
};
} // end namespace

TEST_CASE("Database(filename)", "[database]")
{
  SECTION("invalid filename throws std::runtime_error")
  {
    REQUIRE_THROWS_AS(Database("/fakedir/test/test.db"), std::runtime_error);
  }

  SECTION("valid file opens without throw and handle() returns non-null & handle returns the same "
          "pointer on repeated calls")
  {
    TestDb test;
    REQUIRE(test.db.handle() != nullptr);
    REQUIRE(test.db.handle() == test.db.handle());
  }
}

TEST_CASE("Database::createTables", "[database]")
{
  SECTION("Database::createTables::fresh db has all tables and all indexes")
  {
    TestDb test;
    REQUIRE(test.db.isEmpty());
  }

  SECTION("Database::createTables::when tables already exist")
  {
    TestDb test;
    test.db.createTables();
    REQUIRE(test.db.isEmpty());
  }
}

TEST_CASE("Database::isEmpty", "[database]")
{
  SECTION("Database::isEmpty::empty db returns true")
  {
    TestDb test;
    REQUIRE(test.db.isEmpty());
  }

  SECTION("Database::isEmpty::after inserting a word returns false")
  {
    TestDb test;
    test.db.insertWord("which", "Which", dct::Frequency{1});
    REQUIRE_FALSE(test.db.isEmpty());
  }

  SECTION("Database::isEmpty::after clearDB returns true")
  {
    TestDb test;
    test.db.insertWord("which", "Which", dct::Frequency{1});
    CHECK_FALSE(test.db.isEmpty());
    test.db.clearDB();
    REQUIRE(test.db.isEmpty());
  }
}

TEST_CASE("Database::contains", "[database]")
{
  SECTION("Database::contains::existing word returns true")
  {
    TestDb test;
    test.db.insertWord("waves", "Waves", dct::Frequency{1});
    REQUIRE(test.db.contains("waves"));

    SECTION("Database::contains::after clearDB, previously existing word returns false")
    {
      test.db.clearDB();
      REQUIRE_FALSE(test.db.contains("waves"));
    }
  }

  SECTION("Database::contains::non-existing word returns false")
  {
    TestDb test;
    test.db.insertWord("waves", "Waves", dct::Frequency{1});
    REQUIRE_FALSE(test.db.contains("in"));
  }

  SECTION("Database::contains::empty db returns false")
  {
    TestDb test;
    REQUIRE_FALSE(test.db.contains("waves"));
  }
}

TEST_CASE("Database::insertWord", "[database]")
{
  SECTION("Database::insertWord::new word returns valid id")
  {
    TestDb test;
    const auto wid = test.db.insertWord("hath", "Hath", dct::Frequency{1});
    REQUIRE(wid.value == 1);
  }

  SECTION("Database::insertWord::new words gets autoincrement id")
  {
    TestDb test;
    const auto wid1 = test.db.insertWord("hath", "Hath", dct::Frequency{1});
    const auto wid2 = test.db.insertWord("impaired", "Impaired", dct::Frequency{1});
    REQUIRE(wid1.value == 1);
    REQUIRE(wid2.value == 2);
  }

  SECTION("Database::insertWord::duplicate lemma returns same id")
  {
    TestDb test;
    const auto wid = test.db.insertWord("hath", "Hath", dct::Frequency{1});
    const auto dupeWid = test.db.insertWord("hath", "Hath", dct::Frequency{1});
    REQUIRE(wid.value == dupeWid.value);
  }

  SECTION("Database::insertWord::empty lemma returns g_defaultId")
  {
    TestDb test;
    const auto emptyWid = test.db.insertWord("", "", dct::Frequency{0});
    REQUIRE(emptyWid.value == dct::g_defaultId);
  }
}

TEST_CASE("Database::insertSense", "[database]")
{
  SECTION("Database::insertSense::with pos")
  {
    TestDb test;
    const auto wid = test.db.insertWord("impaired", "Impaired", dct::Frequency{1});
    const auto senseId = test.db.insertSense(wid, "adj", "Rendered less effective");
    REQUIRE(senseId.value != dct::g_defaultId);
  }

  SECTION("Database::insertSense::without pos")
  {
    TestDb test;
    const auto wid = test.db.insertWord("nameless", "Nameless", dct::Frequency{1});
    const auto senseId = test.db.insertSense(wid, "", "Not having a name");
    REQUIRE(senseId.value != dct::g_defaultId);
  }

  SECTION("Database::insertSense::empty definition returns g_defaultId")
  {
    TestDb test;
    const auto wid = test.db.insertWord("emptydef", "EmptyDef", dct::Frequency{1});
    const auto senseId = test.db.insertSense(wid, "n.", "");
    REQUIRE(senseId.value == dct::g_defaultId);
  }

  SECTION("Database::insertSense::invalid word_id returns g_defaultId")
  {
    TestDb test;
    const auto senseId = test.db.insertSense(dct::WordId{9999}, "n.", "ghost");
    REQUIRE(senseId.value == dct::g_defaultId);
  }
}

TEST_CASE("Database::insertEtymology", "[database]")
{
  SECTION("Database::insertEtymology::single line returns true")
  {
    TestDb test;
    const auto wid = test.db.insertWord("grace", "Grace", dct::Frequency{1});
    REQUIRE(test.db.insertEtymology(wid, {"From Latin"}) == true);
  }

  SECTION("Database::insertEtymology::multiple lines returned in order")
  {
    TestDb test;
    const auto wid = test.db.insertWord("grace", "Grace", dct::Frequency{1});
    test.db.insertEtymology(wid, {"From Latin 'gratia'", "Via Old French"});
    auto info = test.db.getInfo(wid);
    REQUIRE(info.etymology.size() == 2);
    CHECK(info.etymology[0] == "From Latin 'gratia'");
    CHECK(info.etymology[1] == "Via Old French");
  }

  SECTION("Database::insertEtymology::invalid word_id returns false")
  {
    TestDb test;
    REQUIRE(test.db.insertEtymology(dct::WordId{9999}, {"test"}) == false);
  }

  SECTION("Database::insertEtymology::empty vector returns true")
  {
    TestDb test;
    const auto wid = test.db.insertWord("grace", "Grace", dct::Frequency{1});
    REQUIRE(test.db.insertEtymology(wid, {}) == true);
  }
}

TEST_CASE("Database::insertForm", "[database]")
{
  SECTION("Database::insertForm::with tag returns true")
  {
    TestDb test;
    const auto wid = test.db.insertWord("grace", "Grace", dct::Frequency{1});
    const auto formSucc = test.db.insertForm(wid, "graces", "plural");
    REQUIRE(formSucc == true);
  }

  SECTION("Database::insertForm::without tag returns true")
  {
    TestDb test;
    const auto wid = test.db.insertWord("grace", "Grace", dct::Frequency{1});
    REQUIRE(test.db.insertForm(wid, "graces", "") == true);
  }

  SECTION("Database::insertForm::multiple forms returned by getInfo")
  {
    TestDb test;
    const auto wid = test.db.insertWord("axis", "axis", dct::Frequency{1});
    test.db.insertForm(wid, "axes", "plural");
    test.db.insertForm(wid, "axis", "singular");
    auto info = test.db.getInfo(wid);
    REQUIRE(info.forms.size() == 2);
  }

  SECTION("Database::insertForm::empty form string is accepted")
  {
    TestDb test;
    const auto wid = test.db.insertWord("grace", "Grace", dct::Frequency{1});
    CHECK(test.db.insertForm(wid, "", "tag") == true);
  }

  SECTION("Database::insertForm::invalid word_id returns false")
  {
    TestDb test;
    REQUIRE(test.db.insertForm(dct::WordId{9999}, "test", "") == false);
  }
}

TEST_CASE("Database::insertExample", "[database]")
{
  SECTION("Database::insertExample::valid sense_id returns true")
  {
    TestDb test;
    const auto wid = test.db.insertWord("grace", "Grace", dct::Frequency{1});
    const auto sid = test.db.insertSense(wid, "", "divine favor");
    REQUIRE(test.db.insertExample(sid, "By grace alone") == true);
  }

  SECTION("Database::insertExample::invalid sense_id returns false")
  {
    TestDb test;
    REQUIRE(test.db.insertExample(dct::WordId{9999}, "test") == false);
  }

  SECTION("Database::insertExample::multiple examples returned by getInfo")
  {
    TestDb test;
    const auto wid = test.db.insertWord("grace", "Grace", dct::Frequency{1});
    const auto sid = test.db.insertSense(wid, "", "divine favor");
    test.db.insertExample(sid, "By grace alone");
    test.db.insertExample(sid, "State of grace");
    auto info = test.db.getInfo(wid);
    REQUIRE(info.senses.size() == 1);
    REQUIRE(info.senses[0].examples.size() == 2);
  }

  SECTION("Database::insertExample::empty example string is accepted")
  {
    TestDb test;
    const auto wid = test.db.insertWord("grace", "Grace", dct::Frequency{1});
    const auto sid = test.db.insertSense(wid, "", "divine favor");
    CHECK(test.db.insertExample(sid, "") == true);
  }
}

TEST_CASE("Database::insertSynonym", "[database]")
{
  SECTION("Database::insertSynonym::valid sense_id returns true")
  {
    TestDb test;
    const auto wid = test.db.insertWord("grace", "Grace", dct::Frequency{1});
    const auto sid = test.db.insertSense(wid, "", "divine favor");
    REQUIRE(test.db.insertSynonym(sid, "blessing") == true);
  }

  SECTION("Database::insertSynonym::invalid sense_id returns false")
  {
    TestDb test;
    REQUIRE(test.db.insertSynonym(dct::WordId{9999}, "test") == false);
  }

  SECTION("Database::insertSynonym::multiple synonyms returned by getInfo")
  {
    TestDb test;
    const auto wid = test.db.insertWord("grace", "Grace", dct::Frequency{1});
    const auto sid = test.db.insertSense(wid, "", "divine favor");
    test.db.insertSynonym(sid, "blessing");
    test.db.insertSynonym(sid, "mercy");
    auto info = test.db.getInfo(wid);
    REQUIRE(info.senses.size() == 1);
    REQUIRE(info.senses[0].synonyms.size() == 2);
  }

  SECTION("Database::insertSynonym::empty synonym string is accepted")
  {
    TestDb test;
    const auto wid = test.db.insertWord("grace", "Grace", dct::Frequency{1});
    const auto sid = test.db.insertSense(wid, "", "divine favor");
    CHECK(test.db.insertSynonym(sid, "") == true);
  }
}

TEST_CASE("Database::insertAntonym", "[database]")
{
  SECTION("Database::insertAntonym::valid sense_id returns true")
  {
    TestDb test;
    const auto wid = test.db.insertWord("grace", "Grace", dct::Frequency{1});
    const auto sid = test.db.insertSense(wid, "", "divine favor");
    REQUIRE(test.db.insertAntonym(sid, "disgrace") == true);
  }

  SECTION("Database::insertAntonym::invalid sense_id returns false")
  {
    TestDb test;
    REQUIRE(test.db.insertAntonym(dct::WordId{9999}, "test") == false);
  }

  SECTION("Database::insertAntonym::multiple antonyms returned by getInfo")
  {
    TestDb test;
    const auto wid = test.db.insertWord("grace", "Grace", dct::Frequency{1});
    const auto sid = test.db.insertSense(wid, "", "divine favor");
    test.db.insertAntonym(sid, "disgrace");
    test.db.insertAntonym(sid, "shame");
    auto info = test.db.getInfo(wid);
    REQUIRE(info.senses.size() == 1);
    REQUIRE(info.senses[0].antonyms.size() == 2);
  }

  SECTION("Database::insertAntonym::empty antonym string is accepted")
  {
    TestDb test;
    const auto wid = test.db.insertWord("grace", "Grace", dct::Frequency{1});
    const auto sid = test.db.insertSense(wid, "", "divine favor");
    CHECK(test.db.insertAntonym(sid, "") == true);
  }
}

TEST_CASE("Database::clearDB", "[database]")
{
  SECTION("Database::clearDB::makes isEmpty true after insert")
  {
    TestDb test;
    test.db.insertWord("which", "Which", dct::Frequency{1});
    REQUIRE_FALSE(test.db.isEmpty());
    test.db.clearDB();
    REQUIRE(test.db.isEmpty());
  }

  SECTION("Database::clearDB::can insert after clear, old words not found")
  {
    TestDb test;
    test.db.insertWord("which", "Which", dct::Frequency{1});
    test.db.clearDB();
    REQUIRE(test.db.isEmpty());
    test.db.insertWord("another", "Another", dct::Frequency{1});
    REQUIRE_FALSE(test.db.isEmpty());
    REQUIRE_FALSE(test.db.contains("which"));
    REQUIRE(test.db.contains("another"));
  }

  SECTION("Database::clearDB::already empty db is safe")
  {
    TestDb test;
    REQUIRE_NOTHROW(test.db.clearDB());
  }

  SECTION("Database::clearDB::foreign key cascade does not cause errors")
  {
    TestDb test;
    const auto wid = test.db.insertWord("grace", "Grace", dct::Frequency{1});
    const auto sid = test.db.insertSense(wid, "", "divine favor");
    test.db.insertExample(sid, "test");
    test.db.insertSynonym(sid, "blessing");
    test.db.insertAntonym(sid, "disgrace");
    REQUIRE_NOTHROW(test.db.clearDB());
    REQUIRE(test.db.isEmpty());
  }
}

TEST_CASE("Database::getInfo", "[database]")
{
  SECTION("Database::getInfo::returns correct lemma, displayLemma, frequency")
  {
    TestDb test;
    const auto wid = test.db.insertWord("grace", "Grace", dct::Frequency{42});
    auto info = test.db.getInfo(wid);
    CHECK(info.lemma == "grace");
    CHECK(info.displayLemma == "Grace");
    CHECK(info.frequency.value == 42);
  }

  SECTION("Database::getInfo::correct etymology vector")
  {
    TestDb test;
    const auto wid = test.db.insertWord("grace", "Grace", dct::Frequency{1});
    test.db.insertEtymology(wid, {"From Latin", "Via French"});
    auto info = test.db.getInfo(wid);
    REQUIRE(info.etymology.size() == 2);
    CHECK(info.etymology[0] == "From Latin");
    CHECK(info.etymology[1] == "Via French");
  }

  SECTION("Database::getInfo::correct forms vector")
  {
    TestDb test;
    const auto wid = test.db.insertWord("axis", "axis", dct::Frequency{1});
    test.db.insertForm(wid, "axes", "plural");
    auto info = test.db.getInfo(wid);
    REQUIRE(info.forms.size() == 1);
    CHECK(info.forms[0].form == "axes");
    CHECK(info.forms[0].tag == "plural");
  }

  SECTION("Database::getInfo::correct senses with pos and definition")
  {
    TestDb test;
    const auto wid = test.db.insertWord("grace", "Grace", dct::Frequency{1});
    test.db.insertSense(wid, "noun", "divine favor");
    auto info = test.db.getInfo(wid);
    REQUIRE(info.senses.size() == 1);
    CHECK(info.senses[0].pos == "noun");
    CHECK(info.senses[0].definition == "divine favor");
  }

  SECTION("Database::getInfo::each sense has examples, synonyms, antonyms")
  {
    TestDb test;
    const auto wid = test.db.insertWord("grace", "Grace", dct::Frequency{1});
    const auto sid = test.db.insertSense(wid, "noun", "divine favor");
    test.db.insertExample(sid, "By grace alone");
    test.db.insertSynonym(sid, "blessing");
    test.db.insertAntonym(sid, "disgrace");
    auto info = test.db.getInfo(wid);
    REQUIRE(info.senses.size() == 1);
    REQUIRE(info.senses[0].examples.size() == 1);
    CHECK(info.senses[0].examples[0] == "By grace alone");
    REQUIRE(info.senses[0].synonyms.size() == 1);
    CHECK(info.senses[0].synonyms[0] == "blessing");
    REQUIRE(info.senses[0].antonyms.size() == 1);
    CHECK(info.senses[0].antonyms[0] == "disgrace");
  }

  SECTION("Database::getInfo::invalid word_id returns empty WordInfo")
  {
    TestDb test;
    auto info = test.db.getInfo(dct::WordId{9999});
    CHECK(info.lemma.empty());
  }

  SECTION("Database::getInfo::word with 0 senses returns empty senses vector")
  {
    TestDb test;
    const auto wid = test.db.insertWord("wordless", "Wordless", dct::Frequency{1});
    auto info = test.db.getInfo(wid);
    CHECK(info.senses.empty());
  }

  SECTION("Database::getInfo::full roundtrip with all fields populated")
  {
    TestDb test;
    const auto wid = test.db.insertWord("grace", "Grace", dct::Frequency{10});
    test.db.insertEtymology(wid, {"Latin origin"});
    test.db.insertForm(wid, "graces", "plural");
    const auto sid = test.db.insertSense(wid, "noun", "divine favor");
    test.db.insertExample(sid, "example sentence");
    test.db.insertSynonym(sid, "mercy");
    test.db.insertAntonym(sid, "cruelty");

    auto info = test.db.getInfo(wid);
    CHECK(info.lemma == "grace");
    CHECK(info.displayLemma == "Grace");
    CHECK(info.frequency.value == 10);
    CHECK(info.etymology.size() == 1);
    CHECK(info.forms.size() == 1);
    CHECK(info.senses.size() == 1);
    CHECK(info.senses[0].pos == "noun");
    CHECK(info.senses[0].definition == "divine favor");
    CHECK(info.senses[0].examples.size() == 1);
    CHECK(info.senses[0].synonyms.size() == 1);
    CHECK(info.senses[0].antonyms.size() == 1);
  }
}

TEST_CASE("Database::findMatchingWordIds", "[database]")
{
  SECTION("Database::findMatchingWordIds::match by lemma returns correct id")
  {
    TestDb test;
    const auto wid = test.db.insertWord("grace", "Grace", dct::Frequency{1});
    auto ids = test.db.findMatchingWordIds("grace");
    REQUIRE(ids.size() == 1);
    CHECK(ids[0].value == wid.value);
  }

  SECTION("Database::findMatchingWordIds::match by form returns correct id")
  {
    TestDb test;
    const auto wid = test.db.insertWord("axis", "axis", dct::Frequency{1});
    test.db.insertForm(wid, "axes", "plural");
    auto ids = test.db.findMatchingWordIds("axes");
    REQUIRE(ids.size() == 1);
    CHECK(ids[0].value == wid.value);
  }

  SECTION("Database::findMatchingWordIds::word that is both lemma and form returns both")
  {
    TestDb test;
    const auto axisId = test.db.insertWord("axis", "axis", dct::Frequency{1});
    const auto axesId = test.db.insertWord("axes", "axes", dct::Frequency{10});
    test.db.insertForm(axisId, "axes", "plural");
    auto ids = test.db.findMatchingWordIds("axes");
    REQUIRE(ids.size() == 2);
  }

  SECTION("Database::findMatchingWordIds::empty string returns empty vector")
  {
    TestDb test;
    auto ids = test.db.findMatchingWordIds("");
    CHECK(ids.empty());
  }

  SECTION("Database::findMatchingWordIds::no match returns empty vector")
  {
    TestDb test;
    auto ids = test.db.findMatchingWordIds("nonexistent");
    CHECK(ids.empty());
  }

  SECTION("Database::findMatchingWordIds::multiple forms return same word_id")
  {
    TestDb test;
    const auto wid = test.db.insertWord("light", "light", dct::Frequency{1});
    test.db.insertForm(wid, "lights", "plural");
    test.db.insertForm(wid, "lighted", "past");
    auto ids = test.db.findMatchingWordIds("lights");
    REQUIRE(ids.size() == 1);
    CHECK(ids[0].value == wid.value);
  }
}

TEST_CASE("Database::pickLemmaForDay", "[database]")
{
  SECTION("Database::pickLemmaForDay::empty db returns empty string")
  {
    TestDb test;
    REQUIRE(test.db.pickLemmaForDay(0).empty());
  }

  SECTION("Database::pickLemmaForDay::negative day number returns empty string")
  {
    TestDb test;
    const auto wid = test.db.insertWord("alpha", "alpha", dct::Frequency{1});
    test.db.insertSense(wid, "", "first");
    REQUIRE(test.db.pickLemmaForDay(-1).empty());
  }

  SECTION("Database::pickLemmaForDay::same day returns same lemma")
  {
    TestDb test;
    const auto a = test.db.insertWord("alpha", "alpha", dct::Frequency{1});
    test.db.insertSense(a, "", "first");
    const auto b = test.db.insertWord("bravo", "bravo", dct::Frequency{1});
    test.db.insertSense(b, "", "second");
    const auto c = test.db.insertWord("charlie", "charlie", dct::Frequency{1});
    test.db.insertSense(c, "", "third");

    const auto first = test.db.pickLemmaForDay(20000);
    REQUIRE_FALSE(first.empty());
    CHECK(first == test.db.pickLemmaForDay(20000));
  }

  SECTION("Database::pickLemmaForDay::returns a seeded lemma")
  {
    TestDb test;
    const auto a = test.db.insertWord("alpha", "alpha", dct::Frequency{1});
    test.db.insertSense(a, "", "first");
    const auto b = test.db.insertWord("bravo", "bravo", dct::Frequency{1});
    test.db.insertSense(b, "", "second");
    const auto c = test.db.insertWord("charlie", "charlie", dct::Frequency{1});
    test.db.insertSense(c, "", "third");

    const std::string seeded[] = {"alpha", "bravo", "charlie"};
    for (long day = 0; day <= 100; ++day)
    {
      const auto picked = test.db.pickLemmaForDay(day);
      REQUIRE_FALSE(picked.empty());
      REQUIRE(std::find(std::begin(seeded), std::end(seeded), picked) != std::end(seeded));
    }
  }

  SECTION("Database::pickLemmaForDay::words without senses are never picked")
  {
    TestDb test;
    const auto a = test.db.insertWord("defined", "defined", dct::Frequency{1});
    test.db.insertSense(a, "", "has a definition");
    test.db.insertWord("undefined", "undefined", dct::Frequency{1}); // no sense

    for (long day = 0; day <= 100; ++day)
    {
      const auto picked = test.db.pickLemmaForDay(day);
      REQUIRE_FALSE(picked.empty());
      CHECK(picked == "defined");
    }
  }
}

TEST_CASE("Database::handle", "[database]")
{
  SECTION("Database::handle::returns non-null pointer")
  {
    TestDb test;
    REQUIRE(test.db.handle() != nullptr);
  }

  SECTION("Database::handle::can execute sql directly")
  {
    TestDb test;
    test.db.insertWord("grace", "Grace", dct::Frequency{1});
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(test.db.handle(), "SELECT lemma FROM words", -1, &stmt, nullptr);
    REQUIRE(sqlite3_step(stmt) == SQLITE_ROW);
    CHECK(std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0))) == "grace");
    sqlite3_finalize(stmt);
  }
}

TEST_CASE("Database::streamAllWordsAndForms", "[database]")
{
  SECTION("Database::streamAllWordsAndForms::lemma preferred over form collision")
  {
    TestDb test;

    const auto axisId = test.db.insertWord("axis", "axis", dct::Frequency{1});
    const auto axesId = test.db.insertWord("axes", "axes", dct::Frequency{10});
    REQUIRE(axisId.value != dct::g_defaultId);
    REQUIRE(axesId.value != dct::g_defaultId);

    REQUIRE(test.db.insertForm(axisId, "axes", "plural"));

    Trie trie;
    test.db.streamAllWordsAndForms(
        [&](dct::WordId id, std::string_view text, dct::Frequency frequency)
        { trie.insert(dct::sanitizeWord(text), id, frequency); });

    REQUIRE(trie.getWordId("axes").value == axesId.value);
  }

  SECTION("Database::streamAllWordsAndForms::empty db: processor never called")
  {
    TestDb test;
    int count = 0;
    test.db.streamAllWordsAndForms([&](dct::WordId, std::string_view, dct::Frequency) { count++; });
    CHECK(count == 0);
  }

  SECTION("Database::streamAllWordsAndForms::single lemma called once")
  {
    TestDb test;
    const auto wid = test.db.insertWord("grace", "Grace", dct::Frequency{42});
    std::vector<std::tuple<dct::WordId, std::string, dct::Frequency>> records;
    test.db.streamAllWordsAndForms([&](dct::WordId id, std::string_view text, dct::Frequency freq)
                                   { records.emplace_back(id, std::string(text), freq); });
    REQUIRE(records.size() == 1);
    CHECK(std::get<0>(records[0]).value == wid.value);
    CHECK(std::get<1>(records[0]) == "grace");
    CHECK(std::get<2>(records[0]).value == 42);
  }

  SECTION("Database::streamAllWordsAndForms::lemma with a form: called for both")
  {
    TestDb test;
    const auto wid = test.db.insertWord("axis", "axis", dct::Frequency{1});
    test.db.insertForm(wid, "axes", "plural");
    int count = 0;
    test.db.streamAllWordsAndForms([&](dct::WordId, std::string_view, dct::Frequency) { count++; });
    CHECK(count == 2);
  }

  SECTION("Database::streamAllWordsAndForms::lemmas before forms, higher frequency first")
  {
    TestDb test;
    const auto catId = test.db.insertWord("cat", "cat", dct::Frequency{50});
    const auto dogId = test.db.insertWord("dog", "dog", dct::Frequency{30});
    std::vector<std::string> order;
    test.db.streamAllWordsAndForms([&](dct::WordId, std::string_view text, dct::Frequency)
                                   { order.push_back(std::string(text)); });
    REQUIRE(order.size() == 2);
    CHECK(order[0] == "cat");
    CHECK(order[1] == "dog");
  }
}

TEST_CASE("Database::foreign key integrity", "[database]")
{
  SECTION("Database::insertSense with non-existent word_id fails gracefully")
  {
    TestDb test;
    auto sid = test.db.insertSense(dct::WordId{9999}, "n.", "ghost");
    CHECK(sid.value == dct::g_defaultId);
  }

  SECTION("Database::insertExample with non-existent sense_id fails gracefully")
  {
    TestDb test;
    CHECK(test.db.insertExample(dct::WordId{9999}, "test") == false);
  }

  SECTION("Database::insertSynonym with non-existent sense_id fails gracefully")
  {
    TestDb test;
    CHECK(test.db.insertSynonym(dct::WordId{9999}, "test") == false);
  }

  SECTION("Database::insertAntonym with non-existent sense_id fails gracefully")
  {
    TestDb test;
    CHECK(test.db.insertAntonym(dct::WordId{9999}, "test") == false);
  }
}
