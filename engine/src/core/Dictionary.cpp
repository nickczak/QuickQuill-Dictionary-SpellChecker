#include "core/Dictionary.h"

#include <list>
#include <mutex>
#include <optional>
#include <unordered_map>

#include <set>

#include "app/Config.h"
#include "dct/dct.h"
#include "random.h"

namespace
{
struct CacheEntry
{
  WordInfo info;
  std::list<int>::iterator lruIt;
};

constexpr size_t kMaxCacheSize = 10000;
std::unordered_map<int, CacheEntry> g_cache;
std::list<int> g_lruList;
std::mutex g_cacheMutex;

std::optional<WordInfo> getFromCache(int wordId)
{
  std::scoped_lock lock(g_cacheMutex);
  auto it = g_cache.find(wordId);
  if (it != g_cache.end())
  {
    g_lruList.erase(it->second.lruIt);
    g_lruList.push_front(wordId);
    it->second.lruIt = g_lruList.begin();
    return it->second.info;
  }
  return std::nullopt;
}

bool isInCache(int wordId)
{
  std::scoped_lock lock(g_cacheMutex);
  return g_cache.find(wordId) != g_cache.end();
}

void setInCache(int wordId, const WordInfo &info)
{
  std::scoped_lock lock(g_cacheMutex);
  auto it = g_cache.find(wordId);
  if (it != g_cache.end())
  {
    g_lruList.erase(it->second.lruIt);
    g_lruList.push_front(wordId);
    it->second.lruIt = g_lruList.begin();
    it->second.info = info;
    return;
  }

  if (g_cache.size() >= kMaxCacheSize)
  {
    int lruKey = g_lruList.back();
    g_lruList.pop_back();
    g_cache.erase(lruKey);
  }

  g_lruList.push_front(wordId);
  g_cache[wordId] = {info, g_lruList.begin()};
}
} // end namespace

void Dictionary::clearGlobalCache()
{
  std::scoped_lock lock(g_cacheMutex);
  g_cache.clear();
  g_lruList.clear();
}

struct Dictionary::ThreadResources
{
  std::string dbPath;
  Database db;

  explicit ThreadResources(const std::string &path) : dbPath(path), db{path} {}
};

Dictionary::Dictionary() : m_dbPath{Config::getInstance().getDatabasePath()}
{
  // Ensure DB schema exists and populate trie using a short-lived connection.
  Database bootstrap{m_dbPath};
  bootstrap.createTables();
  loadTrie();
}

Dictionary::ThreadResources &Dictionary::resources() const
{
  thread_local ThreadResources resources{m_dbPath};
  /*
   * thread_local is initialized once per thread; recreate if a different
   * dictionary instance (i.e. in tests) uses a distinct database path.
   */
  if (resources.dbPath != m_dbPath)
  {
    resources = ThreadResources{m_dbPath};
  }
  return resources;
}

Database &Dictionary::db() const { return resources().db; }

WordInfo Dictionary::getWordInfo(std::string_view word) const
{
  WordInfo info;
  std::string clean{cleanWord(word)};
  if (clean.empty())
  {
    return info;
  }

  dct::WordId id = m_trie.getWordId(clean);
  if (id.value == dct::g_defaultId)
  {
    return info;
  }

  return loadWordInfo(id);
}

WordInfo Dictionary::loadWordInfo(dct::WordId id) const
{
  // try cache first
  auto cached = getFromCache(id.value);
  if (cached)
  {
    return *cached;
  }

  // cache miss - get from DB and store in cache
  WordInfo info = db().getInfo(id);
  if (info.id.value != dct::g_defaultId)
  {
    setInCache(id.value, info);
  }

  return info;
}

std::vector<std::string>
Dictionary::getAlternativeSearches(std::string_view word, dct::WordId currentId) const
{
  std::vector<std::string> alternatives;
  std::string clean{cleanWord(word)};
  if (clean.empty())
  {
    return alternatives;
  }

  const auto ids = db().findMatchingWordIds(clean);
  if (ids.size() <= 1)
  {
    return alternatives;
  }

  std::unordered_set<std::string> seen;
  for (const auto &id : ids)
  {
    if (currentId.value != dct::g_defaultId && id.value == currentId.value)
    {
      continue;
    }

    const WordInfo info = loadWordInfo(id);

    const std::string label = info.displayLemma.empty() ? info.lemma : info.displayLemma;
    if (label.empty())
    {
      continue;
    }

    const std::string normalized = cleanWord(label);
    if (normalized.empty() || !seen.insert(normalized).second)
    {
      continue;
    }
    alternatives.push_back(label);
  }

  return alternatives;
}

std::vector<std::string> Dictionary::suggestSynonyms(std::string_view word) const
{
  std::vector<std::string> synonymSuggestions;
  std::string clean{cleanWord(word)};
  if (clean.empty())
  {
    return synonymSuggestions;
  }

  dct::WordId id = m_trie.getWordId(clean);
  if (id.value == dct::g_defaultId)
  {
    return synonymSuggestions;
  }

  const WordInfo info = loadWordInfo(id);

  // create pool of unique synonyms
  std::set<std::string> uniquePool;
  for (const auto &s : info.senses)
  {
    for (const auto &syn : s.synonyms)
    {
      uniquePool.insert(syn);
    }
  }

  if (uniquePool.empty())
  {
    return synonymSuggestions;
  }

  // convert to vector for random access
  std::vector<std::string> uniqueVec(uniquePool.begin(), uniquePool.end());

  if (uniqueVec.empty())
  {
    return synonymSuggestions;
  }

  // pick random number of synonyms to return (1 to unique count)
  // (if using mutex, use thread_local Random::get to avoid two threads pulling a random number at
  // the same time)
  const auto numToPick = Random::get<std::size_t>(1, uniqueVec.size());

  // pick random unique items
  std::set<std::size_t> pickedIndices;
  while (synonymSuggestions.size() < numToPick)
  {
    const auto index = Random::get<std::size_t>(0, uniqueVec.size() - 1);
    if (pickedIndices.insert(index).second)
    {
      synonymSuggestions.push_back(uniqueVec[index]);
    }
  }
  return synonymSuggestions;
}

std::string Dictionary::wordOfTheDay(long dayNumber) const
{
  return db().pickLemmaForDay(dayNumber);
}

bool Dictionary::contains(std::string_view word) const
{
  std::string clean = cleanWord(word);
  if (clean.empty())
  {
    return false;
  }

  dct::WordId id = m_trie.getWordId(clean);
  return id.value != dct::g_defaultId;
}

std::vector<std::string> Dictionary::suggestFromPrefix(std::string_view word) const
{
  std::string clean{cleanWord(word)};
  if (clean.empty())
  {
    return {};
  }

  std::unordered_set<std::string> suggestionsSet = collectSuggestedWords(clean);
  std::vector<std::string> suggestions(suggestionsSet.begin(), suggestionsSet.end());

  suggestions.erase(std::remove(suggestions.begin(), suggestions.end(), clean), suggestions.end());
  // rank suggestions based on lambda calling Levenshteins's algorithm
  std::sort(
      suggestions.begin(), suggestions.end(),
      [&](const std::string &a, const std::string &b)
      {
        return dct::calculateLevenshteinDistance(clean, a) <
               dct::calculateLevenshteinDistance(clean, b);
      });

  return suggestions;
}

std::vector<std::string> Dictionary::suggestSpelling(std::string_view word) const
{
  std::string clean{cleanWord(word)};
  if (clean.empty())
  {
    return {};
  }
  if (m_trie.contains(clean))
  {
    return {}; // word is correct, no suggestions needed
  }

  std::unordered_set<std::string> suggestionsSet = collectSuggestedWords(clean);
  std::vector<std::string> suggestions(suggestionsSet.begin(), suggestionsSet.end());

  // rank suggestions based on lambda calling Levenshteins's alorithm
  std::sort(
      suggestions.begin(), suggestions.end(),
      [&](const std::string &a, const std::string &b)
      {
        return dct::calculateLevenshteinDistance(clean, a) <
               dct::calculateLevenshteinDistance(clean, b);
      });

  return suggestions;
}

std::string Dictionary::autofillFromTrie(
    std::string_view prefix,
    const std::vector<std::string> &history, // NOLINT(bugprone-easily-swappable-parameters)
    const std::vector<std::string> &suggested) const
{
  std::string clean = cleanWord(prefix);
  if (clean.empty())
  {
    return {};
  }

  std::vector<std::pair<std::string, dct::Frequency>> completions;
  m_trie.collectWithPrefix(clean, completions, 50);

  if (completions.empty())
  {
    return {};
  }

  std::unordered_set<std::string> historySet, suggestedSet;
  for (const auto &w : history)
  {
    historySet.insert(cleanWord(w));
  }
  for (const auto &w : suggested)
  {
    suggestedSet.insert(cleanWord(w));
  }

  // rank words based on a score system (0 being the best, 3 being the worst)
  // ranks are determined by searching history→suggested→cache→db (0-3)
  int bestScore{4};
  dct::Frequency bestFreq{0};
  std::string best;

  for (const auto &[word, freq] : completions)
  {
    std::string norm = cleanWord(word);
    int score = 3;

    if (historySet.count(norm))
    {
      score = 0;
    }
    else if (suggestedSet.count(norm))
    {
      score = 1;
    }
    else
    {
      dct::WordId id = m_trie.getWordId(norm);
      if (id.value != dct::g_defaultId && isInCache(id.value))
      {
        score = 2;
      }
    }

    if (score < bestScore || (score == bestScore && freq.value > bestFreq.value))
    {
      best = word;
      bestScore = score;
      bestFreq = freq;
    }
  }

  return best;
}

std::string Dictionary::cleanWord(std::string_view word) const { return dct::sanitizeWord(word); }

void Dictionary::loadTrie()
{
  auto trieLoader = [this](dct::WordId id, std::string_view text, dct::Frequency frequency)
  { m_trie.insert(cleanWord(text), id, frequency); };

  // pass to database to retrieve words
  Database loaderDb{m_dbPath};
  loaderDb.createTables();
  loaderDb.streamAllWordsAndForms(trieLoader);
}

std::unordered_set<std::string> Dictionary::collectSuggestedWords(std::string_view word) const
{
  // for safety
  std::string clean{cleanWord(word)};

  std::unordered_set<std::string> suggestionsSet;
  std::string candidate;

  // deletion
  for (int i = 0; i < clean.length(); ++i)
  {
    candidate = clean;
    candidate.erase(i, 1);
    if (m_trie.contains(candidate))
    {
      suggestionsSet.insert(candidate);
    }
  }

  // transpositions
  for (int i = 0; i < clean.length() - 1; ++i)
  {
    candidate = clean;
    std::swap(candidate[i], candidate[i + 1]);
    if (m_trie.contains(candidate))
    {
      suggestionsSet.insert(candidate);
    }
  }

  // substitutions
  for (int i = 0; i < clean.length(); ++i)
  {
    candidate = clean;
    for (char c = 'a'; c <= 'z'; ++c)
    {
      candidate[i] = c;
      if (m_trie.contains(candidate))
      {
        suggestionsSet.insert(candidate);
      }
    }
  }

  // insertions
  for (int i = 0; i <= clean.length(); ++i)
  {
    for (char c = 'a'; c <= 'z'; ++c)
    {
      candidate = clean;
      candidate.insert(i, 1, c);
      if (m_trie.contains(candidate))
      {
        suggestionsSet.insert(candidate);
      }
    }
  }

  return suggestionsSet;
}
