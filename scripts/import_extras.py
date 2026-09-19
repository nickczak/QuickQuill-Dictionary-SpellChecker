#!/usr/bin/env python3
"""Add custom words and definitions to dictionary.db with dedup."""

import argparse
import sqlite3
from pathlib import Path

DB_PATH = Path(__file__).resolve().parent.parent / "dictionary.db"

def insert_word_items(cur, table, col, word_id, items, label):
    for item in items:
        cur.execute(f"SELECT 1 FROM {table} WHERE word_id = ? AND {col} = ?;", (word_id, item))
        if cur.fetchone():
            print(f"  {label} already exists: {item[:60]}...")
        else:
            cur.execute(f"INSERT INTO {table} (word_id, {col}) VALUES (?, ?);", (word_id, item))
            print(f"  Added {label}: {item[:60]}...")

def insert_sense_items(cur, table, col, sense_id, word_id, items, label):
    for item in items:
        cur.execute(
            f"SELECT 1 FROM {table} t JOIN senses s ON t.sense_id = s.id "
            f"WHERE s.word_id = ? AND t.{col} = ?;",
            (word_id, item),
        )
        if cur.fetchone():
            print(f"  {label} already exists: {item[:60]}...")
        else:
            cur.execute(f"INSERT INTO {table} (sense_id, {col}) VALUES (?, ?);", (sense_id, item))
            print(f"  Added {label}: {item[:60]}...")

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Add custom words and definitions to dictionary.db with dedup."
    )
    parser.add_argument(
        "--db",
        type=Path,
        default=DB_PATH,
        help="Path to dictionary.db",
    )
    args = parser.parse_args()

    conn = sqlite3.connect(args.db)
    conn.execute("PRAGMA foreign_keys = ON;")
    cur = conn.cursor()

    entries = [
        {
            "lemma": "nevermore",
            "display_lemma": "nevermore",
            "definitions": [
                "A prestigious private academy founded in 1791 in Jericho, Vermont, by Nathaniel Faulkner. Known for its historic Gothic architecture, extensive botanical gardens, and distinguished alumni. The institution is renowned for its programs in the arts, sciences, and outcast studies.",
                "The alumni network of Nevermore Academy, including notable figures such as Edgar Allan Poe, Gomez Addams, Morticia Addams (née Frump), and Ignatius Itt.",
            ],
        },
        {
            "lemma": "quickquill",
            "display_lemma": "QuickQuill",
            "frequency": 1000,
            "definitions": [
                "A lightweight online dictionary created by Nicholas Sobchak, contracted by Nevermore Academy, tagged with the slogan \"Lexicon Levissimum\".",
                "An open-source quick search dictionary and spell-checking application.",
            ],
            "examples": [
                "I used QuickQuill to look up the definition of lugubrious.",
                "QuickQuill's autofill suggested the word before I finished typing.",
            ],
            "synonyms": ["dictionary", "lexicon", "wordbook"],
            "etymologies": [
                "From English quick (rapid) + quill (a writing feather), reflecting the application's focus on swift word lookups.",
            ],
        },
        {
            "lemma": "lexiconlevissimum",
            "display_lemma": "Lexicon Levissimum",
            "frequency": 1000,
            "definitions": [
                "The official motto of QuickQuill, translating from Latin to \"Lightest Lexicon\" or \"Swift Dictionary.\" Emblazoned on the application's branding and interface.",
                "By extension, any lightweight or streamlined reference work; a concise dictionary.",
            ],
            "examples": [
                "The QuickQuill splash screen displays \"Lexicon Levissimum\" beneath the logo.",
            ],
            "etymologies": [
                "From Latin lexicon (dictionary, wordbook) + levissimum (lightest, superlative of levis). Coined by Nicholas Sobchak for the QuickQuill dictionary application.",
            ],
        },
        {
            "lemma": "nicholassobchak",
            "display_lemma": "Nicholas Sobchak",
            "frequency": 1000,
            "definitions": [
                "The creator and lead developer of the QuickQuill dictionary and spell-checking application. A software engineer and lexicography enthusiast.",
            ],
            "examples": [
                "Nicholas Sobchak designed QuickQuill as a lightweight, open-source dictionary tool.",
            ],
        },
        {
            "lemma": "sobchak",
            "display_lemma": "Sobchak",
            "frequency": 1000,
            "definitions": [
                "A surname of Ukrainian origin, belonging to a generational family of mathematicians and computer scientists. Notable bearer: Nicholas Sobchak, creator of the QuickQuill dictionary application.",
            ],
        },
        {
            "lemma": "neilmartini",
            "display_lemma": "Neil Martini",
            "frequency": 1000,
            "definitions": [
                "A charming and sophisticated consultant, art aficionado, and former convict turned dictionary expert. Known for his impeccable taste in suits, wine, and rare first editions. Works closely with QuickQuill on special projects, offering a unique perspective on lexical curation that only a man of his talents could provide.",
                "A world-class art forger and authentication expert who can identify any painting, manuscript, or rare book at a glance. His knowledge of historical documents makes him invaluable for verifying the provenance of antiquarian lexicons.",
                "An expert consultant in fraud detection, financial crimes, and document security. Works with dictionary publishers to identify counterfeit editions and protect intellectual property.",
                "A master of disguise and alias-crafting who maintains a network of contacts spanning the worlds of art, academia, and law enforcement. His ability to assume any identity makes him uniquely suited for undercover lexicographic research.",
            ],
            "examples": [
                "Neil Martini walked into the room like he owned it, adjusted his cuffs, and said, 'I've been meaning to add a few choice words to the dictionary.'",
                "It's not a crime to have impeccable vocabulary, but in Neil Martini's case, it was definitely a related skill.",
                "They brought in Neil Martini to authenticate the Gutenberg leaf. He took one look and said, 'Close, but no cigar.'",
                "Neil Martini can forge a medieval manuscript so convincingly that even carbon dating shrugs.",
                "The publisher didn't realize their thesaurus had been infiltrated with counterfeit entries until Neil Martini spotted the inconsistencies.",
                "When asked how he caught the fraud, Neil Martini just smiled and said, 'The devil is in the details — and I know the devil personally.'",
            ],
            "synonyms": ["gentleman", "raconteur", "bon vivant", "chameleon", "shape-shifter"],
        },
        {
            "lemma": "gwen",
            "display_lemma": "Gwen",
            "frequency": 1000,
            "definitions": [
                "The love of his life. The one who keeps him honest. The one who knows all his aliases. And the only one who's ever been three steps ahead.",
            ],
        },
    ]

    for entry in entries:
        cur.execute("SELECT id FROM words WHERE lemma = ?;", (entry["lemma"],))
        row = cur.fetchone()
        if row:
            word_id = row[0]
        else:
            cur.execute(
                "INSERT INTO words (lemma, display_lemma, frequency) VALUES (?, ?, ?);",
                (entry["lemma"], entry.get("display_lemma", entry["lemma"]), entry.get("frequency", 0)),
            )
            word_id = cur.lastrowid
            print(f"Added new word '{entry['lemma']}' (word_id={word_id})")

        insert_word_items(cur, "etymologies", "etymology", word_id, entry.get("etymologies", []), "etymology")

        for definition in entry.get("definitions", []):
            cur.execute(
                "SELECT id FROM senses WHERE word_id = ? AND definition = ?;",
                (word_id, definition),
            )
            sense_row = cur.fetchone()
            if sense_row:
                sense_id = sense_row[0]
                print(f"  Sense already exists (sense_id={sense_id}): {definition[:60]}...")
            else:
                cur.execute(
                    "INSERT INTO senses (word_id, pos, definition) VALUES (?, 'noun', ?);",
                    (word_id, definition),
                )
                sense_id = cur.lastrowid
                print(f"  Added sense (sense_id={sense_id}): {definition[:60]}...")

        if entry.get("examples") or entry.get("synonyms") or entry.get("antonyms"):
            cur.execute(
                "SELECT id FROM senses WHERE word_id = ? ORDER BY id DESC LIMIT 1;",
                (word_id,),
            )
            last_sense = cur.fetchone()
            if last_sense:
                sense_id = last_sense[0]
                insert_sense_items(cur, "examples", "example", sense_id, word_id, entry.get("examples", []), "example")
                insert_sense_items(cur, "synonyms", "synonym", sense_id, word_id, entry.get("synonyms", []), "synonym")
                insert_sense_items(cur, "antonyms", "antonym", sense_id, word_id, entry.get("antonyms", []), "antonym")

    conn.commit()
    conn.close()

if __name__ == "__main__":
    main()
