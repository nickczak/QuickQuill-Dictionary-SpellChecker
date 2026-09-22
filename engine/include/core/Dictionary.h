#ifndef DICTIONARY_H
#define DICTIONARY_H
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_set>
#include <vector>

#include "core/Trie.h"
#include "data/Database.h"
#include "dct/WordInfo.h"

class Dictionary
{
public:
  explicit Dictionary();
  ~Dictionary() = default;

  static void clearGlobalCache();

  WordInfo getWordInfo(std::string_view word) const;
  bool contains(std::string_view word) const;
  std::vector<std::string> getAlternativeSearches(
      std::string_view word, dct::WordId currentId = dct::WordId{dct::g_defaultId}) const;

  std::vector<std::string> suggestSynonyms(std::string_view word) const;
  std::vector<std::string> suggestFromPrefix(std::string_view word) const;
  std::vector<std::string> suggestSpelling(std::string_view word) const;

  /**
   * Deterministic per-day word: the same day number (e.g. days since epoch)
   * always yields the same lemma for every user. Empty string when the
   * dictionary has no words with definitions.
   */
  std::string wordOfTheDay(long dayNumber) const;

  // Ghost Autofill: returns best completion for prefix, searching history→suggested→cache→db
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  std::string autofillFromTrie(
      std::string_view prefix,
      const std::vector<std::string> &history,
      const std::vector<std::string> &suggested) const;

private:
  /*
   * Each request thread gets its own sqlite connection and cache
   */
  struct ThreadResources;

  Trie m_trie;
  std::string m_dbPath;

  std::string cleanWord(std::string_view word) const;
  void loadTrie();
  std::unordered_set<std::string> collectSuggestedWords(std::string_view word) const;
  Database &db() const;
  ThreadResources &resources() const;

  // Cache-first fetch: returns cached WordInfo or loads it from the DB and caches it.
  WordInfo loadWordInfo(dct::WordId id) const;
};

#endif // DICTIONARY_H
