#include "data/Database.h"
#include "logging.h"

#include "dct/dct.h"

Database::Database(std::string_view filename)
{
  sqlite3 *db_ptr = nullptr;
  if (sqlite3_open(filename.data(), &db_ptr))
  {
    throw std::runtime_error("Error: Can't open database\n");
  }
  m_db.reset(db_ptr);

  // Prevent requests from blocking indefinitely when another connection holds a lock.
  // Note: this is a per-connection setting.
  sqlite3_busy_timeout(m_db.get(), 2000);

  char *errMsg = nullptr;
  if (sqlite3_exec(m_db.get(), "PRAGMA foreign_keys = ON;", nullptr, nullptr, &errMsg) != SQLITE_OK)
  {
    if (errMsg)
    {
      QQ_LOG_ERROR << "SQL error: " << errMsg;
      sqlite3_free(errMsg);
    }
  }
}

void Database::createTables()
{
  /*
            Word
            ├── Etymology (one)
            └── Senses (many)
                ├── Definition (one)
                ├── POS (part of speech) (one)
                ├── Examples (many)
                ├── Synonyms (many)
                └── Antonyms (many)
  */

  const char *stmts[] = {
      "CREATE TABLE IF NOT EXISTS words ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "lemma TEXT UNIQUE,"
      "display_lemma TEXT,"
      "frequency INTEGER DEFAULT 0);",

      "CREATE TABLE IF NOT EXISTS etymologies ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "word_id INTEGER NOT NULL,"
      "etymology TEXT NOT NULL,"
      "FOREIGN KEY(word_id) REFERENCES words(id) ON DELETE CASCADE);",

      "CREATE TABLE IF NOT EXISTS forms ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "word_id INTEGER NOT NULL,"
      "form TEXT NOT NULL,"
      "tag TEXT,"
      "FOREIGN KEY(word_id) REFERENCES words(id) ON DELETE CASCADE);",

      "CREATE TABLE IF NOT EXISTS senses ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "word_id INTEGER NOT NULL,"
      "pos TEXT,"
      "definition TEXT NOT NULL,"
      "FOREIGN KEY(word_id) REFERENCES words(id) ON DELETE CASCADE);",

      "CREATE TABLE IF NOT EXISTS examples ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "sense_id INTEGER NOT NULL,"
      "example TEXT NOT NULL,"
      "FOREIGN KEY(sense_id) REFERENCES senses(id) ON DELETE "
      "CASCADE);",

      "CREATE TABLE IF NOT EXISTS synonyms ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "sense_id INTEGER NOT NULL,"
      "synonym TEXT,"
      "FOREIGN KEY(sense_id) REFERENCES senses(id) ON DELETE "
      "CASCADE);",

      "CREATE TABLE IF NOT EXISTS antonyms ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "sense_id INTEGER NOT NULL,"
      "antonym TEXT,"
      "FOREIGN KEY(sense_id) REFERENCES senses(id) ON DELETE "
      "CASCADE);"};

  char *errMsg = nullptr;
  for (auto s : stmts)
  {
    if (sqlite3_exec(m_db.get(), s, nullptr, nullptr, &errMsg) != SQLITE_OK)
    {
      QQ_LOG_ERROR << "SQL error: " << errMsg;
      sqlite3_free(errMsg);
    }
  }

  // index the database (makes access much quicker)
  const char *idx[] = {
      "CREATE INDEX IF NOT EXISTS idx_words_lemma ON words(lemma);",
      "CREATE INDEX IF NOT EXISTS idx_etymologies_word_id ON "
      "etymologies(word_id);",
      "CREATE INDEX IF NOT EXISTS idx_forms_word_id ON "
      "forms(word_id);",
      "CREATE INDEX IF NOT EXISTS idx_senses_word_id ON "
      "senses(word_id);",
      "CREATE INDEX IF NOT EXISTS idx_examples_sense_id ON "
      "examples(sense_id);",
      "CREATE INDEX IF NOT EXISTS idx_synonyms_sense_id ON "
      "synonyms(sense_id);",
      "CREATE INDEX IF NOT EXISTS idx_antonyms_sense_id ON "
      "antonyms(sense_id);",
  };

  for (auto s : idx)
  {
    if (sqlite3_exec(m_db.get(), s, nullptr, nullptr, &errMsg) != SQLITE_OK)
    {
      QQ_LOG_ERROR << "SQL error: " << errMsg;
      sqlite3_free(errMsg);
    }
  }
}

sqlite3 *Database::handle() const noexcept { return m_db.get(); }

dct::WordId Database::insertWord(
    const std::string &lemma, const std::string &displayLemma, dct::Frequency frequency)
{
  if (lemma.empty())
  {
    return dct::WordId{dct::g_defaultId};
  }

  sqlite3_stmt *stmt = nullptr;
  const char *sql{"INSERT OR IGNORE INTO words (lemma, display_lemma, "
                  "frequency) VALUES (?, ?, ?);"};

  if (sqlite3_prepare_v2(m_db.get(), sql, -1, &stmt, nullptr) != SQLITE_OK)
  {
    return dct::WordId{dct::g_defaultId};
  }

  sqlite3_bind_text(stmt, 1, lemma.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, displayLemma.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 3, frequency.value);

  if (sqlite3_step(stmt) != SQLITE_DONE)
  {
    sqlite3_finalize(stmt);
    return dct::WordId{dct::g_defaultId};
  }
  if (stmt)
  {
    sqlite3_finalize(stmt);
  }

  if (sqlite3_changes(m_db.get()) > 0)
  {
    return dct::WordId{static_cast<int>(sqlite3_last_insert_rowid(m_db.get()))};
  }

  const char *selectSql{"SELECT id FROM words WHERE lemma = ?;"};
  if (sqlite3_prepare_v2(m_db.get(), selectSql, -1, &stmt, nullptr) != SQLITE_OK)
  {
    return dct::WordId{dct::g_defaultId};
  }

  sqlite3_bind_text(stmt, 1, lemma.c_str(), -1, SQLITE_TRANSIENT);

  dct::WordId existingId = dct::WordId{dct::g_defaultId};
  if (sqlite3_step(stmt) == SQLITE_ROW)
  {
    existingId.value = sqlite3_column_int(stmt, 0);
  }
  if (stmt)
  {
    sqlite3_finalize(stmt);
  }
  return existingId;
}

dct::WordId
Database::insertSense(dct::WordId word_id, const std::string &pos, const std::string &definition)
{
  if (definition.empty())
  {
    return dct::WordId{dct::g_defaultId};
  }

  sqlite3_stmt *stmt = nullptr;
  const char *sql{"INSERT INTO senses (word_id, pos, definition) VALUES "
                  "(?, ?, ?);"};
  if (sqlite3_prepare_v2(m_db.get(), sql, -1, &stmt, nullptr) != SQLITE_OK)
  {
    return dct::WordId{-1};
  }

  // fill values
  sqlite3_bind_int(stmt, 1, word_id.value);
  sqlite3_bind_text(stmt, 2, pos.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, definition.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt) != SQLITE_DONE)
  {
    sqlite3_finalize(stmt);
    return dct::WordId{dct::g_defaultId};
  }
  if (stmt)
  {
    sqlite3_finalize(stmt);
  }
  return dct::WordId{static_cast<int>(sqlite3_last_insert_rowid(m_db.get()))};
}

bool Database::insertEtymology(dct::WordId word_id, const std::vector<std::string> &etymology)
{
  sqlite3_stmt *stmt = nullptr;
  const char *sql{"INSERT INTO etymologies (word_id, etymology) VALUES (?, ?);"};
  if (sqlite3_prepare_v2(m_db.get(), sql, -1, &stmt, nullptr) != SQLITE_OK)
  {
    return false;
  }

  // fill value with vector
  for (const auto &line : etymology)
  {
    sqlite3_bind_int(stmt, 1, word_id.value);
    sqlite3_bind_text(stmt, 2, line.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
      if (stmt)
      {
        sqlite3_finalize(stmt);
      }
      return false;
    }
    // reuse stmt
    sqlite3_reset(stmt);
  }

  if (stmt)
  {
    sqlite3_finalize(stmt);
  }
  return true;
}

bool Database::insertForm(dct::WordId word_id, const std::string &form, const std::string &tag)
{
  sqlite3_stmt *stmt = nullptr;
  const char *sql{"INSERT INTO forms (word_id, form, tag) VALUES (?, ?, ?);"};
  if (sqlite3_prepare_v2(m_db.get(), sql, -1, &stmt, nullptr) != SQLITE_OK)
  {
    return false;
  }

  // fill value
  sqlite3_bind_int(stmt, 1, word_id.value);
  sqlite3_bind_text(stmt, 2, form.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, tag.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt) != SQLITE_DONE)
  {
    if (stmt)
    {
      sqlite3_finalize(stmt);
    }
    return false;
  }
  if (stmt)
  {
    sqlite3_finalize(stmt);
  }
  return true;
}

bool Database::insertExample(dct::WordId sense_id, const std::string &example)
{
  sqlite3_stmt *stmt = nullptr;
  const char *sql{"INSERT INTO examples (sense_id, example) VALUES (?, ?);"};
  if (sqlite3_prepare_v2(m_db.get(), sql, -1, &stmt, nullptr) != SQLITE_OK)
  {
    return false;
  }

  sqlite3_bind_int(stmt, 1, sense_id.value);
  sqlite3_bind_text(stmt, 2, example.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt) != SQLITE_DONE)
  {
    if (stmt)
    {
      sqlite3_finalize(stmt);
    }
    return false;
  }
  if (stmt)
  {
    sqlite3_finalize(stmt);
  }
  return true;
}

bool Database::insertSynonym(dct::WordId sense_id, const std::string &synonym)
{
  sqlite3_stmt *stmt = nullptr;
  const char *sql{"INSERT INTO synonyms (sense_id, synonym) VALUES (?, ?);"};
  if (sqlite3_prepare_v2(m_db.get(), sql, -1, &stmt, nullptr) != SQLITE_OK)
  {
    return false;
  }

  sqlite3_bind_int(stmt, 1, sense_id.value);
  sqlite3_bind_text(stmt, 2, synonym.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt) != SQLITE_DONE)
  {
    if (stmt)
    {
      sqlite3_finalize(stmt);
    }
    return false;
  }
  if (stmt)
  {
    sqlite3_finalize(stmt);
  }
  return true;
}

bool Database::insertAntonym(dct::WordId sense_id, const std::string &antonym)
{
  sqlite3_stmt *stmt = nullptr;
  const char *sql{"INSERT INTO antonyms (sense_id, antonym) VALUES (?, ?);"};
  if (sqlite3_prepare_v2(m_db.get(), sql, -1, &stmt, nullptr) != SQLITE_OK)
  {
    return false;
  }

  sqlite3_bind_int(stmt, 1, sense_id.value);
  sqlite3_bind_text(stmt, 2, antonym.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt) != SQLITE_DONE)
  {
    if (stmt)
    {
      sqlite3_finalize(stmt);
    }
    return false;
  }
  if (stmt)
  {
    sqlite3_finalize(stmt);
  }
  return true;
}

bool Database::isEmpty() const
{
  sqlite3_stmt *stmt = nullptr;
  const char *sql{"SELECT 1 FROM words LIMIT 1;"};
  if (sqlite3_prepare_v2(m_db.get(), sql, -1, &stmt, nullptr) != SQLITE_OK)
  {
    return true; // error = empty
  }

  int rc = sqlite3_step(stmt);
  if (stmt)
  {
    sqlite3_finalize(stmt);
  }

  return (rc != SQLITE_ROW);
}

bool Database::contains(std::string_view word) const
{
  if (isEmpty())
  {
    return false;
  }

  sqlite3_stmt *stmt = nullptr;
  const char *sql{"SELECT 1 FROM words WHERE lemma = ? LIMIT 1;"};
  if (sqlite3_prepare_v2(m_db.get(), sql, -1, &stmt, nullptr) != SQLITE_OK)
  {
    return false;
  }

  if (sqlite3_bind_text(stmt, 1, word.data(), static_cast<int>(word.size()), SQLITE_TRANSIENT) !=
      SQLITE_OK)
  {
    if (stmt)
    {
      sqlite3_finalize(stmt);
    }
    return false;
  }

  bool exists = (sqlite3_step(stmt) == SQLITE_ROW);

  if (stmt)
  {
    sqlite3_finalize(stmt);
  }
  return exists;
}

void Database::clearDB()
{
  if (isEmpty())
  {
    return;
  }

  const char *stmts[] = {"PRAGMA foreign_keys = OFF;",   "DELETE FROM examples;",
                         "DELETE FROM synonyms;",        "DELETE FROM antonyms;",
                         "DELETE FROM senses;",          "DELETE FROM forms;",
                         "DELETE FROM etymologies;",     "DELETE FROM words;",
                         "DELETE FROM sqlite_sequence;", "PRAGMA foreign_keys = ON;"};

  char *errMsg = nullptr;

  for (auto s : stmts)
  {
    int rc = sqlite3_exec(m_db.get(), s, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK)
    {
      QQ_LOG_ERROR << "Error clearing database: " << errMsg;
      sqlite3_free(errMsg);
    }
  }
}

WordInfo Database::getInfo(dct::WordId word_id) const
{
  WordInfo info;
  info.id = word_id;

  sqlite3_stmt *stmt = nullptr;
  const char *wordSql = "SELECT lemma, display_lemma, frequency FROM "
                        "words WHERE id = ?;";
  if (sqlite3_prepare_v2(m_db.get(), wordSql, -1, &stmt, nullptr) == SQLITE_OK)
  {
    sqlite3_bind_int(stmt, 1, word_id.value);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
      const char *lemmaTxt = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
      const char *displayTxt = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
      info.lemma = lemmaTxt ? lemmaTxt : "";
      info.displayLemma = displayTxt ? displayTxt : info.lemma;
      info.frequency = dct::Frequency{sqlite3_column_int(stmt, 2)};
    }
  }
  if (stmt)
  {
    sqlite3_finalize(stmt);
  }

  // Etymology
  info.etymology = fetchStrings("SELECT etymology FROM etymologies WHERE word_id = ?;", word_id);

  // Forms
  info.forms = fetchForms(word_id);

  // Senses
  const char *senseSql = "SELECT id, pos, definition FROM senses WHERE word_id = ?;";
  sqlite3_stmt *senseStmt = nullptr;
  if (sqlite3_prepare_v2(m_db.get(), senseSql, -1, &senseStmt, nullptr) == SQLITE_OK)
  {
    sqlite3_bind_int(senseStmt, 1, word_id.value);
    while (sqlite3_step(senseStmt) == SQLITE_ROW)
    {
      Sense sense;
      sense.id.value = sqlite3_column_int(senseStmt, 0); // preserve DB id for callers
      const char *pos = reinterpret_cast<const char *>(sqlite3_column_text(senseStmt, 1));
      const char *def = reinterpret_cast<const char *>(sqlite3_column_text(senseStmt, 2));
      sense.pos = pos ? pos : "";
      sense.definition = def ? def : "";

      sense.examples = fetchStrings(
          "SELECT example FROM examples "
          "WHERE sense_id = ?;",
          sense.id);
      sense.synonyms = fetchStrings(
          "SELECT synonym FROM synonyms "
          "WHERE sense_id = ?;",
          sense.id);
      sense.antonyms = fetchStrings(
          "SELECT antonym FROM antonyms "
          "WHERE sense_id = ?;",
          sense.id);

      info.senses.push_back(sense);
    }
  }
  if (senseStmt)
  {
    sqlite3_finalize(senseStmt);
  }

  return info;
}

std::string Database::pickLemmaForDay(long dayNumber) const
{
  if (dayNumber < 0 || isEmpty())
  {
    return "";
  }

  // Count words that actually carry at least one definition.
  sqlite3_stmt *stmt = nullptr;
  long count = 0;
  const char *countSql = "SELECT COUNT(*) FROM words w "
                         "WHERE EXISTS (SELECT 1 FROM senses s WHERE s.word_id = w.id);";
  if (sqlite3_prepare_v2(m_db.get(), countSql, -1, &stmt, nullptr) == SQLITE_OK)
  {
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
      count = sqlite3_column_int64(stmt, 0);
    }
  }
  if (stmt)
  {
    sqlite3_finalize(stmt);
  }
  if (count <= 0)
  {
    return "";
  }

  // Multiply by a large odd constant and reduce modulo the count so successive
  // days map to well-separated rows instead of advancing one row per day.
  constexpr long kSpread = 2654435761L; // golden-ratio hash constant
  const long long offset = static_cast<long long>(dayNumber) * kSpread % count;

  std::string lemma;
  stmt = nullptr;
  const char *selectSql = "SELECT w.lemma FROM words w "
                          "WHERE EXISTS (SELECT 1 FROM senses s WHERE s.word_id = w.id) "
                          "ORDER BY w.id LIMIT 1 OFFSET ?;";
  if (sqlite3_prepare_v2(m_db.get(), selectSql, -1, &stmt, nullptr) == SQLITE_OK)
  {
    sqlite3_bind_int64(stmt, 1, offset);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
      const char *text = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
      if (text)
      {
        lemma = text;
      }
    }
  }
  if (stmt)
  {
    sqlite3_finalize(stmt);
  }
  return lemma;
}

std::vector<dct::WordId> Database::findMatchingWordIds(std::string_view word) const
{
  std::vector<dct::WordId> ids;
  if (word.empty())
  {
    return ids;
  }

  sqlite3_stmt *stmt = nullptr;
  const char *sql = "SELECT DISTINCT id FROM ("
                    "SELECT id FROM words WHERE lemma = ? "
                    "UNION "
                    "SELECT word_id AS id FROM forms WHERE form = ?"
                    ");";

  if (sqlite3_prepare_v2(m_db.get(), sql, -1, &stmt, nullptr) != SQLITE_OK)
  {
    return ids;
  }

  sqlite3_bind_text(stmt, 1, word.data(), static_cast<int>(word.size()), SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, word.data(), static_cast<int>(word.size()), SQLITE_TRANSIENT);

  while (sqlite3_step(stmt) == SQLITE_ROW)
  {
    ids.push_back(dct::WordId{sqlite3_column_int(stmt, 0)});
  }

  if (stmt)
  {
    sqlite3_finalize(stmt);
  }

  return ids;
}

void Database::streamAllWordsAndForms(const WordRecordProcessor &processor) const
{
  sqlite3 *sqlDB = m_db.get();
  sqlite3_stmt *stmt = nullptr;

  // Stream lemmas first, then forms. This prevents a form record from
  // "winning" over the lemma in the Trie when they share the same
  // sanitized text.
  const char *query = "SELECT id, text, frequency FROM ("
                      "  SELECT id AS id, lemma AS text, frequency AS "
                      "frequency, 1 AS is_lemma"
                      "  FROM words"
                      "  UNION ALL "
                      "  SELECT word_id AS id, form AS text, 0 AS "
                      "frequency, 0 AS is_lemma"
                      "  FROM forms"
                      ") "
                      "ORDER BY is_lemma DESC, frequency DESC;";

  if (sqlite3_prepare_v2(sqlDB, query, -1, &stmt, nullptr) != SQLITE_OK)
  {
    // You should probably log an error here
    return;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW)
  {
    dct::WordId id{sqlite3_column_int(stmt, 0)};
    const unsigned char *text = sqlite3_column_text(stmt, 1);
    dct::Frequency frequency{sqlite3_column_int(stmt, 2)};
    if (text)
    {
      // Execute the callback with the data from the current
      // row
      processor(id, reinterpret_cast<const char *>(text), frequency);
    }
  }

  sqlite3_finalize(stmt);
}

/*********************************
// Helper functions go here
**********************************/
std::vector<std::string> Database::fetchStrings(std::string_view sql, dct::WordId id) const
{
  std::vector<std::string> result;
  sqlite3_stmt *stmt{nullptr};

  if (sqlite3_prepare_v2(m_db.get(), sql.data(), -1, &stmt, nullptr) == SQLITE_OK)
  {
    sqlite3_bind_int(stmt, 1, id.value);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
      const char *text = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
      if (text)
      {
        result.push_back(text);
      }
    }
  }
  if (stmt)
  {
    sqlite3_finalize(stmt);
  }

  return result;
}

std::vector<Form> Database::fetchForms(dct::WordId word_id) const
{
  std::vector<Form> forms;
  sqlite3_stmt *stmt{nullptr};

  const char *sql{"SELECT form, tag FROM forms WHERE word_id = ?;"};
  if (sqlite3_prepare_v2(m_db.get(), sql, -1, &stmt, nullptr) == SQLITE_OK)
  {
    sqlite3_bind_int(stmt, 1, word_id.value);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
      Form f;
      const char *form = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
      const char *tag = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
      f.form = form ? form : "";
      f.tag = tag ? tag : "";
      forms.push_back(f);
    }
  }
  if (stmt)
  {
    sqlite3_finalize(stmt);
  }

  return forms;
}
