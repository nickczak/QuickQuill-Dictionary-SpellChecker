/**
 * quickquill.cpp — Implementation of the C ABI interface.
 *
 * This file bridges the flat C functions declared in quickquill.h to the
 * underlying C++ Dictionary, SpellChecker, and WordService classes. It
 * manages the lifecycle of those C++ objects via raw pointers protected
 * by a mutex, and converts all results to JSON strings written into
 * caller-provided buffers.
 *
 * The WordService is constructed per-call rather than stored as a
 * singleton because it holds references to Dictionary and SpellChecker
 * which may not exist yet when the service would be created. This keeps
 * the lifecycle simple and avoids dangling reference issues.
 */

#include "quickquill.h"

#include "core/Dictionary.h"
#include "core/SpellChecker.h"
#include "http/dto/WordResponse.h"
#include "http/services/WordService.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

namespace
{
// Global engine state — protected by g_mutex during init/shutdown.
// Once initialized, reads from g_dict and g_checker are safe from
// multiple threads because Dictionary uses thread_local SQLite
// connections and an internally-synchronized LRU cache.
Dictionary *g_dict = nullptr;
SpellChecker *g_checker = nullptr;
std::mutex g_mutex;

/**
 * Copy a std::string into a caller-provided C buffer.
 * Returns the number of bytes written (excluding null terminator),
 * or -1 if the buffer is invalid.
 */
int copyToBuf(const std::string &src, char *buf, int buf_size)
{
  if (!buf || buf_size <= 0)
  {
    return -1;
  }

  int len = static_cast<int>(src.size());
  if (len >= buf_size)
  {
    len = buf_size - 1;
  }

  std::memcpy(buf, src.c_str(), len);
  buf[len] = '\0';
  return len;
}

/**
 * Parse a JSON array string (e.g. `["word1","word2"]`) into a
 * vector of strings. Returns an empty vector on parse failure
 * or if the input is null/empty.
 */
std::vector<std::string> parseJsonArray(const char *json_str)
{
  if (!json_str || json_str[0] == '\0')
  {
    return {};
  }

  try
  {
    auto arr = nlohmann::json::parse(json_str);
    if (!arr.is_array())
    {
      return {};
    }

    std::vector<std::string> result;
    for (const auto &item : arr)
    {
      if (item.is_string())
      {
        result.push_back(item.get<std::string>());
      }
    }
    return result;
  }
  catch (...)
  {
    return {};
  }
}

/**
 * Run a WordService call against the shared engine state, copying its
 * JSON body into the caller-provided buffer. Returns the bytes written,
 * or -1 if the engine is uninitialized, the input is null, or the call
 * threw.
 */
template <typename Fn> int runService(const char *input, char *buf, int buf_size, Fn call)
{
  if (!g_dict || !g_checker || !input)
  {
    return -1;
  }

  try
  {
    http::WordService svc(*g_dict, *g_checker);
    auto result = call(svc);
    return copyToBuf(result.body, buf, buf_size);
  }
  catch (...)
  {
    return -1;
  }
}

/**
 * Run a WordService call that takes only a day number, copying its JSON body
 * into the caller-provided buffer. Returns the bytes written, or -1 if the
 * engine is uninitialized or the call threw.
 */
template <typename Fn> int runService(long dayNumber, char *buf, int buf_size, Fn call)
{
  if (!g_dict || !g_checker)
  {
    return -1;
  }

  try
  {
    http::WordService svc(*g_dict, *g_checker);
    auto result = call(svc);
    return copyToBuf(result.body, buf, buf_size);
  }
  catch (...)
  {
    return -1;
  }
}

} // namespace

// ---------------------------------------------------------------------------
// C ABI functions — callable from Java FFM or any C-capable language.
// ---------------------------------------------------------------------------

extern "C"
{

  int qq_init(const char *db_path)
  {
    std::lock_guard<std::mutex> lock(g_mutex);

    // Allow re-initialization — if already running, return success.
    if (g_dict)
    {
      return 0;
    }

    try
    {
      // Dictionary() reads its path from Config::getDatabasePath() which checks
      // the DATABASE_PATH environment variable first. Set it so the Dictionary
      // uses the path we received from Java regardless of the process CWD.
      if (db_path && db_path[0] != '\0')
      {
        setenv("DATABASE_PATH", db_path, 1);
      }

      g_dict = new Dictionary();
      g_checker = new SpellChecker(*g_dict);
      return 0;
    }
    catch (...)
    {
      return -1;
    }
  }

  void qq_shutdown(void)
  {
    std::lock_guard<std::mutex> lock(g_mutex);

    // Delete in reverse order of construction.
    delete g_checker;
    g_checker = nullptr;
    delete g_dict;
    g_dict = nullptr;

    // Clear the shared in-memory LRU cache.
    Dictionary::clearGlobalCache();
  }

  int qq_lookup(const char *word, char *buf, int buf_size)
  {
    return runService(
        word, buf, buf_size, [&](http::WordService &svc) { return svc.search(word); });
  }

  int qq_suggest(const char *word, char *buf, int buf_size)
  {
    return runService(
        word, buf, buf_size, [&](http::WordService &svc) { return svc.suggest(word); });
  }

  int qq_synonym(const char *word, char *buf, int buf_size)
  {
    return runService(
        word, buf, buf_size, [&](http::WordService &svc) { return svc.suggestSynonym(word); });
  }

  int qq_autofill(
      const char *prefix,
      const char *history_json,
      const char *suggested_json,
      char *buf,
      int buf_size)
  {
    return runService(
        prefix, buf, buf_size,
        [&](http::WordService &svc) {
          return svc.autofill(prefix, parseJsonArray(history_json), parseJsonArray(suggested_json));
        });
  }

  int qq_wotd(long day_number, char *buf, int buf_size)
  {
    return runService(
        day_number, buf, buf_size,
        [&](http::WordService &svc) { return svc.wordOfTheDay(day_number); });
  }

} // extern "C"
