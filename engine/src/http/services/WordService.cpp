#include "http/services/WordService.h"

#include "http/dto/WordResponse.h"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <mutex>

namespace
{
// The daily word is a per-day constant, but deriving it is expensive: it counts
// every word that has a sense, walks an OFFSET into that set, then loads the
// entry. Against a large dictionary that runs about a second per request for an
// answer that cannot change until the next day, so the finished response is
// memoized. WordService is constructed per call by the FFM bridge, so the cache
// is process-global rather than a member. Only successful lookups are stored --
// pinning a transient engine failure for a whole day would be worse than
// recomputing it.
std::mutex g_wotdMutex;
long g_wotdDay = 0;
http::ServiceResult g_wotdResult;
} // end namespace

namespace http
{
std::string WordService::decodeInput(const std::string &in)
{
  std::string out;
  out.reserve(in.size());

  for (size_t i = 0; i < in.size(); ++i)
  {
    if (in[i] == '%')
    {
      if (i + 2 >= in.size())
      {
        return "";
      }

      const auto hex = in.substr(i + 1, 2);
      char *end = nullptr;
      const long val = std::strtol(hex.c_str(), &end, 16);
      if (end != hex.c_str() + 2)
      {
        return "";
      }

      out.push_back(static_cast<char>(val));
      i += 2;
    }
    else if (in[i] == '+')
    {
      out.push_back(' ');
    }
    else
    {
      out.push_back(in[i]);
    }
  }

  return out;
}

WordService::WordService(Dictionary &dict, SpellChecker &checker) : m_dict{dict}, m_checker{checker}
{
}

ServiceResult WordService::search(const std::string &word) const
{
  const std::string decoded = decodeInput(word);
  const std::string sanitized = dct::sanitizeWord(decoded);
  if (sanitized.empty())
  {
    nlohmann::json body = {{"error", "Enter a valid word"}};
    return {body.dump(), 400};
  }

  const bool allowedChars = std::all_of(
      decoded.begin(), decoded.end(),
      [](unsigned char c)
      { return std::isalnum(c) || c == '\'' || c == '-' || c == ' ' || c == '.'; });

  if (!allowedChars)
  {
    nlohmann::json body = {{"error", "Enter a valid word"}};
    return {body.dump(), 400};
  }

  WordInfo info = m_dict.getWordInfo(sanitized);
  if (info.lemma.empty())
  {
    const std::string correctWord = m_checker.correct(sanitized);
    nlohmann::json body = {{"query", sanitized}, {"found", false}};
    // Only attach a suggestion that is a real dictionary entry and not an
    // echo of the unknown query itself.
    if (!correctWord.empty() && correctWord != sanitized && m_dict.contains(correctWord))
    {
      body["suggestion"] = correctWord;
    }
    return {body.dump(), 404};
  }

  // alternative searches comes from words with the same id's (same lemmas)
  const auto alternativeSearches = m_dict.getAlternativeSearches(sanitized, info.id);
  return {toWordJson(info, decoded, alternativeSearches), 200};
}

/**
 * Provides similar searches using the suggest function
 */
ServiceResult WordService::suggest(const std::string &word) const
{
  const std::string decoded = decodeInput(word);
  const std::string sanitized = dct::sanitizeWord(decoded);
  if (sanitized.empty())
  {
    return {"[]", 200};
  }

  std::vector<std::string> suggestions = m_checker.suggest(sanitized);
  nlohmann::json body = suggestions;
  return {body.dump(), 200};
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
ServiceResult WordService::autofill(
    const std::string &prefix,
    const std::vector<std::string> &history,
    const std::vector<std::string> &suggested) const
{
  const std::string decoded = decodeInput(prefix);
  const std::string sanitized = dct::sanitizeWord(decoded);
  if (sanitized.empty())
  {
    nlohmann::json body = {{"completion", ""}};
    return {body.dump(), 200};
  }

  std::string completion = m_checker.autofill(sanitized, history, suggested);
  nlohmann::json body = {{"completion", completion}};
  return {body.dump(), 200};
}

ServiceResult WordService::suggestSynonym(const std::string &word) const
{
  const std::string decoded = decodeInput(word);
  const std::string sanitized = dct::sanitizeWord(decoded);
  if (sanitized.empty())
  {
    return {"[]", 200};
  }

  std::vector<std::string> synonyms = m_dict.suggestSynonyms(sanitized);
  nlohmann::json body = synonyms;
  return {body.dump(), 200};
}

ServiceResult WordService::wordOfTheDay(long dayNumber) const
{
  {
    std::scoped_lock lock(g_wotdMutex);
    // A populated body is what marks the entry valid, so an unseeded cache can
    // never satisfy a lookup (day 0 is otherwise indistinguishable from unset).
    if (g_wotdDay == dayNumber && !g_wotdResult.body.empty())
    {
      return g_wotdResult;
    }
  }

  const std::string lemma = m_dict.wordOfTheDay(dayNumber);
  WordInfo info;
  if (!lemma.empty())
  {
    info = m_dict.getWordInfo(lemma);
  }

  if (lemma.empty() || info.lemma.empty())
  {
    nlohmann::json body = {{"error", "No words available"}};
    return {body.dump(), 500};
  }

  // Echo the lemma as `query` so the frontend can treat it like a lookup hit.
  ServiceResult result = {toWordJson(info, lemma), 200};

  std::scoped_lock lock(g_wotdMutex);
  g_wotdDay = dayNumber;
  g_wotdResult = result;
  return result;
}
} // end namespace http
