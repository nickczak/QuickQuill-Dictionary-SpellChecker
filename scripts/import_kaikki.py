#!/usr/bin/env python3
"""
Populate dictionary.db with the Kaikki English dictionary dump.

This script streams the Kaikki JSONL file and normalizes each entry so it matches the
word format expected by the C++ Dictionary/Database classes, it writes everything into
SQLite in batches so we can efficiently seed the database once.
"""
from __future__ import annotations

import argparse
import gzip
import json
import sqlite3
from pathlib import Path
from typing import Dict, Iterable, List, Tuple

DEFAULT_JSON_PATH = Path("nlohmann/kaikki.org-dictionary-English.jsonl.gz")
DEFAULT_DB_PATH = Path("dictionary.db")


def sanitize_word(word: str | None) -> str:
    """Match dct::sanitizeWord: keep only alphanumeric chars, lowercase."""
    if not word:
        return ""
    return "".join(ch.lower() for ch in word if ch.isalnum())


def table_exists(conn: sqlite3.Connection, table: str) -> bool:
    cur = conn.execute(
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name=? LIMIT 1;",
        (table,),
    )
    row = cur.fetchone()
    cur.close()
    return bool(row)


def table_has_column(conn: sqlite3.Connection, table: str, column: str) -> bool:
    if not table_exists(conn, table):
        return False
    cur = conn.execute(f"PRAGMA table_info({table});")
    columns = {row[1] for row in cur.fetchall()}
    cur.close()
    return column in columns


def ensure_tables(conn: sqlite3.Connection) -> None:
    """Mirror Database::createTables so the script can run stand-alone."""

    # Older databases created an "antonyms" column named "antonyms" instead of "antonym".
    # Drop the table so we recreate it with the schema the C++ Database class expects.
    if table_exists(conn, "antonyms") and not table_has_column(conn, "antonyms", "antonym"):
        conn.execute("DROP TABLE antonyms;")

    # Add display_lemma column if missing.
    if table_exists(conn, "words") and not table_has_column(conn, "words", "display_lemma"):
        conn.execute("ALTER TABLE words ADD COLUMN display_lemma TEXT;")

    statements = [
        """CREATE TABLE IF NOT EXISTS words (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                lemma TEXT UNIQUE,
                display_lemma TEXT,
                frequency INTEGER DEFAULT 0
            );""",
        """CREATE TABLE IF NOT EXISTS etymologies (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                word_id INTEGER NOT NULL,
                etymology TEXT NOT NULL,
                FOREIGN KEY(word_id) REFERENCES words(id) ON DELETE CASCADE
            );""",

        """CREATE TABLE IF NOT EXISTS forms (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                word_id INTEGER NOT NULL,
                form TEXT NOT NULL,
                tag TEXT,
                FOREIGN KEY(word_id) REFERENCES words(id) ON DELETE CASCADE
            );""",
        """CREATE TABLE IF NOT EXISTS senses (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                word_id INTEGER NOT NULL,
                pos TEXT,
                definition TEXT NOT NULL,
                FOREIGN KEY(word_id) REFERENCES words(id) ON DELETE CASCADE
            );""",
        """CREATE TABLE IF NOT EXISTS examples (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                sense_id INTEGER NOT NULL,
                example TEXT NOT NULL,
                FOREIGN KEY(sense_id) REFERENCES senses(id) ON DELETE CASCADE
            );""",
        """CREATE TABLE IF NOT EXISTS synonyms (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                sense_id INTEGER NOT NULL,
                synonym TEXT,
                FOREIGN KEY(sense_id) REFERENCES senses(id) ON DELETE CASCADE
            );""",
        """CREATE TABLE IF NOT EXISTS antonyms (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                sense_id INTEGER NOT NULL,
                antonym TEXT,
                FOREIGN KEY(sense_id) REFERENCES senses(id) ON DELETE CASCADE
            );""",
    ]

    cur = conn.cursor()
    try:
        for stmt in statements:
            cur.execute(stmt)
    finally:
        cur.close()


def clear_db(conn: sqlite3.Connection) -> None:
    cur = conn.cursor()
    try:
        cur.execute("PRAGMA foreign_keys = OFF;")
        for table in (
            "examples",
            "synonyms",
            "antonyms",
            "senses",
            "forms",
            "etymologies",
            "words",
        ):
            cur.execute(f"DELETE FROM {table};")
        cur.execute("DELETE FROM sqlite_sequence;")
        cur.execute("PRAGMA foreign_keys = ON;")
    finally:
        cur.close()


def insert_word(cur: sqlite3.Cursor, lemma: str, display_lemma: str, frequency: int) -> Tuple[int | None, bool]:
    cur.execute(
        "INSERT OR IGNORE INTO words (lemma, display_lemma, frequency) VALUES (?, ?, ?);",
        (lemma, display_lemma, frequency),
    )
    inserted = cur.rowcount == 1
    cur.execute("SELECT id FROM words WHERE lemma = ?;", (lemma,))
    row = cur.fetchone()
    if not row:
        return None, False
    return int(row[0]), inserted


def insert_etymology(cur: sqlite3.Cursor, word_id: int, etymology_text: str | None) -> int:
    if not etymology_text:
        return 0
    lines = [line.strip() for line in etymology_text.splitlines() if line.strip()]
    count = 0
    for line in lines:
        cur.execute(
            "INSERT INTO etymologies (word_id, etymology) VALUES (?, ?);",
            (word_id, line),
        )
        count += 1
    return count


def insert_forms(cur: sqlite3.Cursor, word_id: int, forms: Iterable[Dict]) -> int:
    count = 0
    for form_entry in forms or []:
        if not isinstance(form_entry, dict):
            continue
        form_value = sanitize_word(form_entry.get("form"))
        if not form_value:
            continue
        tags = form_entry.get("tags")
        tag_string = ""
        if isinstance(tags, list) and tags:
            first_tag = tags[0]
            if isinstance(first_tag, str):
                tag_string = first_tag
        cur.execute(
            "INSERT INTO forms (word_id, form, tag) VALUES (?, ?, ?);",
            (word_id, form_value, tag_string),
        )
        count += 1
    return count


def _join_strings(items: Iterable[str | None]) -> str:
    return " ".join(part.strip() for part in items if isinstance(part, str) and part.strip()).strip()


def _definition_from_sense(sense: Dict) -> str:
    if not isinstance(sense, dict):
        return ""
    if sense.get("glosses"):
        return _join_strings(sense.get("glosses"))
    if sense.get("raw_glosses"):
        return _join_strings(sense.get("raw_glosses"))
    gloss = sense.get("gloss")
    if isinstance(gloss, str):
        return gloss.strip()
    definition = sense.get("definition")
    if isinstance(definition, str):
        return definition.strip()
    return ""


def _extract_text_list(items: Iterable, key: str = "text") -> List[str]:
    result: List[str] = []
    for item in items or []:
        if isinstance(item, str):
            value = item.strip()
        elif isinstance(item, dict):
            value = item.get(key)
            value = value.strip() if isinstance(value, str) else ""
            if not value and key == "word":
                fallback = item.get("sense") or item.get("translation")
                value = fallback.strip() if isinstance(fallback, str) else ""
        else:
            value = ""
        if value:
            result.append(value)
    return result


def insert_senses(cur: sqlite3.Cursor, word_id: int, entry_pos: str | None, senses: Iterable[Dict]) -> Dict[str, int]:
    stats = {"senses": 0, "examples": 0, "synonyms": 0, "antonyms": 0}
    for sense in senses or []:
        definition = _definition_from_sense(sense)
        if not definition:
            continue
        pos = sense.get("pos") or entry_pos or ""
        cur.execute(
            "INSERT INTO senses (word_id, pos, definition) VALUES (?, ?, ?);",
            (word_id, pos or "", definition),
        )
        sense_id = int(cur.lastrowid)
        stats["senses"] += 1

        examples = _extract_text_list(sense.get("examples"), key="text")
        for example in examples:
            cur.execute(
                "INSERT INTO examples (sense_id, example) VALUES (?, ?);",
                (sense_id, example),
            )
        stats["examples"] += len(examples)

        synonyms = _extract_text_list(sense.get("synonyms"), key="word")
        for synonym in synonyms:
            cur.execute(
                "INSERT INTO synonyms (sense_id, synonym) VALUES (?, ?);",
                (sense_id, synonym),
            )
        stats["synonyms"] += len(synonyms)

        antonyms = _extract_text_list(sense.get("antonyms"), key="word")
        for antonym in antonyms:
            cur.execute(
                "INSERT INTO antonyms (sense_id, antonym) VALUES (?, ?);",
                (sense_id, antonym),
            )
        stats["antonyms"] += len(antonyms)

    return stats


def open_jsonl(json_path: Path):
    if json_path.suffix == ".gz":
        return gzip.open(json_path, "rt", encoding="utf-8")
    return json_path.open("r", encoding="utf-8")


def process_file(
    conn: sqlite3.Connection,
    json_path: Path,
    max_entries: int | None = None,
    progress_interval: int = 10000,
) -> Dict[str, int]:
    stats = {
        "entries": 0,
        "words": 0,
        "etymologies": 0,
        "forms": 0,
        "senses": 0,
        "examples": 0,
        "synonyms": 0,
        "antonyms": 0,
    }

    cur = conn.cursor()
    batch_size = 5000
    pending = 0

    try:
        conn.execute("BEGIN;")
        with open_jsonl(json_path) as handle:
            for line_number, line in enumerate(handle, start=1):
                if max_entries is not None and stats["entries"] >= max_entries:
                    break

                try:
                    entry = json.loads(line)
                except json.JSONDecodeError as exc:
                    print(f"Skipping malformed JSON at line {line_number}: {exc}")
                    continue

                if entry.get("lang_code") not in {None, "en"}:
                    continue

                raw_word = entry.get("word") or ""
                frequency = entry.get("freq") or 0
                lemma = sanitize_word(raw_word)
                if not lemma:
                    continue
                word_id, inserted = insert_word(cur, lemma, raw_word, frequency)
                if not word_id:
                    continue

                stats["entries"] += 1
                if inserted:
                    stats["words"] += 1

                stats["etymologies"] += insert_etymology(cur, word_id, entry.get("etymology_text"))
                stats["forms"] += insert_forms(cur, word_id, entry.get("forms"))

                sense_stats = insert_senses(cur, word_id, entry.get("pos"), entry.get("senses"))
                for key, value in sense_stats.items():
                    stats[key] += value

                pending += 1
                if pending >= batch_size:
                    conn.commit()
                    conn.execute("BEGIN;")
                    pending = 0

                if progress_interval and stats["entries"] % progress_interval == 0:
                    print(
                        f"Processed {stats['entries']:,} entries | "
                        f"{stats['words']:,} unique lemmas | "
                        f"{stats['senses']:,} senses"
                    )
        conn.commit()
    finally:
        cur.close()

    return stats


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Populate dictionary.db from Kaikki JSONL dump.")
    parser.add_argument(
        "--json",
        type=Path,
        default=DEFAULT_JSON_PATH,
        help="Path to Kaikki's English JSONL export (plain or gzip-compressed).",
    )
    parser.add_argument(
        "--db",
        type=Path,
        default=DEFAULT_DB_PATH,
        help="Path to dictionary.db",
    )
    parser.add_argument(
        "--max-entries",
        type=int,
        default=None,
        help="Import only the first N parsed entries (useful for testing).",
    )
    parser.add_argument(
        "--progress",
        type=int,
        default=10000,
        help="Print progress every N imported entries.",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if not args.json.is_file():
        raise SystemExit(f"Dictionary JSONL file not found: {args.json}")

    conn = sqlite3.connect(args.db)
    conn.isolation_level = None  # use explicit BEGIN/COMMIT control
    conn.execute("PRAGMA journal_mode = WAL;")
    conn.execute("PRAGMA synchronous = OFF;")
    conn.execute("PRAGMA foreign_keys = ON;")

    ensure_tables(conn)
    clear_db(conn)

    print(f"Importing from {args.json} into {args.db}...")
    stats = process_file(conn, args.json, args.max_entries, args.progress)
    conn.close()

    print("Import complete:")
    for key in ("entries", "words", "senses", "forms", "examples", "synonyms", "antonyms", "etymologies"):
        print(f"  {key.capitalize():<12}: {stats.get(key, 0):,}")


if __name__ == "__main__":
    main()
