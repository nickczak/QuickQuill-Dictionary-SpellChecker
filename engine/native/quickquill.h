/**
 * quickquill.h — C ABI interface to the QuickQuill dictionary engine.
 *
 * This header exposes a flat C API that can be called from any language
 * capable of calling C functions — including Java via the Foreign Function
 * and Memory API (Panama FFM). It wraps the C++ Dictionary, SpellChecker,
 * and WordService classes behind simple functions that take and return
 * null-terminated C strings.
 *
 * All output functions write into a caller-provided buffer and return the
 * number of bytes written (excluding the null terminator). The caller is
 * responsible for allocating a buffer large enough for the response.
 * All functions return -1 on error.
 */

#ifndef QUICKQUILL_H
#define QUICKQUILL_H

#ifdef __cplusplus
extern "C"
{
#endif

  int qq_init(const char *db_path);
  void qq_shutdown(void);
  int qq_lookup(const char *word, char *buf, int buf_size);
  int qq_suggest(const char *word, char *buf, int buf_size);
  int qq_synonym(const char *word, char *buf, int buf_size);
  int qq_autofill(
      const char *prefix,
      const char *history_json,
      const char *suggested_json,
      char *buf,
      int buf_size);
  int qq_wotd(long day_number, char *buf, int buf_size);

#ifdef __cplusplus
}
#endif

#endif /* QUICKQUILL_H */
