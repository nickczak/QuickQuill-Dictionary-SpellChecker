<p align="center"><img src="QuickQuill-logo.png" alt="QuickQuill Logo" width=600 style="background: transparent;" /></p>

<h4 align="center">A Quick Lookup Dictionary at your service.</h4>
<p align="center">
  <a href="https://github.com/nickczak/QuickQuill-Dictionary-SpellChecker/actions/workflows/ci.yml"><img src="https://img.shields.io/github/actions/workflow/status/nickczak/QuickQuill-Dictionary-SpellChecker/ci.yml?label=Backend&logo=github" alt="Backend Build & Test"></a>
  <a href="https://github.com/nickczak/QuickQuill-Dictionary-SpellChecker/actions/workflows/github-pages.yml"><img src="https://img.shields.io/github/actions/workflow/status/nickczak/QuickQuill-Dictionary-SpellChecker/github-pages.yml?label=Frontend&logo=github" alt="Frontend Deploy"></a>
  <a href="https://github.com/nickczak/QuickQuill-Dictionary-SpellChecker/releases"><img src="https://img.shields.io/github/v/release/nickczak/QuickQuill-Dictionary-SpellChecker?color=purple&cachebust=1" alt="Release">
  <a href="https://quickquill.ink"><img src="https://img.shields.io/badge/website-quickquill.ink-black" alt="Website"></a>
</p>

---
### Description

QuickQuill is a full-stack dictionary and spell-check application. The core engine is written in C++ (SQLite-backed, trie-based autocomplete), exposed to a Spring Boot backend via Java's Foreign Function & Memory API (Panama FFM). The frontend is Angular.

### Features
  - Spellchecking (Did you mean ...?)
  - Similar search
  - Word suggestion (Synonym selection)
  - Autocomplete with ghost text
  - Lightweight frontend
  - Dictionary data including:
    - Multi-sense entries with POS and definitions
    - Synonyms and antonyms per sense (when present in source data)
    - Examples
    - Forms/inflections and etymology

> **Note:** The live deployment on Render's free tier ships the committed `dictionary.db` (baked into the backend image). The full database supports **over 1.28 million words** at the same speed — see the [analytics](#analytics) section below. The only words cut are obscure, obsolete word forms (rare plurals, scientific jargon, archaic inflections, etc.) — no common English vocabulary was removed.

<p align="center"><img src="showcase-desktop.gif" alt="QuickQuill Showcase" width=800></p>

### Analytics
```
Import complete:
  Entries     : 1,472,850
  Words       : 1,277,185
  Senses      : 1,761,078
  Forms       : 973,401
  Examples    : 745,823
  Synonyms    : 655,080
  Antonyms    : 33,768
  Etymologies : 1,349,364
```

### Technical Highlights
  - C++17 engine with trie-based autocomplete and thread-local SQLite connections
  - Java FFM (Foreign Function & Memory API) for native calls — no JNI
  - Spring Boot REST API with Spring Data JPA + PostgreSQL for user auth
  - BCrypt password hashing with session token management
  - Angular 21 frontend with RxJS debounced search streams
  - In-memory LRU caching with thread-safe access
  - Backend Dockerized for Render; Angular frontend served from GitHub Pages
  - Catch2 for C++ tests

> The C++ engine is compiled into `libquickquill_engine.so` with a flat C ABI (`extern "C"`). Spring Boot calls it through Panama FFM — no JNI or C glue code needed.

---
## Setting Up / Building this Project Locally

### Prerequisites

  - Java (25+)
  - CMake (3.16+)
  - vcpkg (latest) 
  - Node.js (20+)
  - Clang-format (17)
  - PostgreSQL (16+)

### Database Download
To run this with the full prebuilt database, download:

```
https://www.dropbox.com/home/dictionary-db-sql/dictionary-db?preview=dictionary.db
```

Then place `dictionary.db` in the project root.

### Tech Stack
  - **Backend:** Java 22, Spring Boot 4.1, Spring Data JPA, Spring Security Crypto, Foreign Function & Memory API (Panama FFM)
  - **Engine:** C++17, SQLite3, nlohmann/json, CMake, vcpkg
  - **Database:** PostgreSQL 16 (user data), SQLite (dictionary)
  - **Frontend:** Angular 21, RxJS, TypeScript
  - **Tests:** Catch2 (C++), JUnit (Java)
  - **Deploy:** Render (backend), GitHub Pages (frontend) 

### Project Layout
```
.
├── engine/                 
│   ├── native/            
│   ├── include/          
│   ├── src/             
│   └── tests/          
├── studio/            
│   └── src/main/java/com/quickquill/studio/
│       ├── config/
│       ├── controller/     # REST endpoints
│       ├── engine/         # FFM bridge to C++
│       ├── model/          # JPA entities (User, Session, Note)
│       ├── repository/     # Spring Data repos
│       └── service/        # AuthService, etc.
└── web/             
    └── src/app/    
```

### Configuration

The Spring Boot backend uses `studio/src/main/resources/application.properties`. Sensible defaults are provided for local development, overridable via environment variables:

```properties
spring.application.name=studio
server.port=8080
quickquill.dictionary-path=../dictionary.db
quickquill.cors.allowed-origins=${CORS_ALLOWED_ORIGINS:https://quickquill.ink}

# PostgreSQL — override with DB_URL, DB_USERNAME, DB_PASSWORD
spring.datasource.url=${DB_URL:jdbc:postgresql://localhost:5432/quickquill}
spring.datasource.username=${DB_USERNAME:quickquill}
spring.datasource.password=${DB_PASSWORD:quickquill}
spring.jpa.hibernate.ddl-auto=update
spring.jpa.properties.hibernate.dialect=org.hibernate.dialect.PostgreSQLDialect
```

A `.env` file in the project root is loaded automatically by compose. For local development, create one:

```bash
DB_URL=jdbc:postgresql://localhost:5432/quickquill
DB_USERNAME=quickquill
DB_PASSWORD=quickquill
```

**Before running locally**, start the database with Docker Compose:

```bash
docker compose up -d postgres
```

This creates the `quickquill` database and user automatically.

### Code Formatting (Pre-commit Hook)
To have consistent formatting across the project, configure `pre-commit`. It's a hook that automatically runs `clang-format` on your staged C++ files before each commit.

CI uses `clang-format-17` by default.
 
**Setup Instructions:**

1.  **Install `pre-commit`:** If you don't have it already, install `pre-commit`:
    ```bash
    sudo dnf install pre-commit # Fedora
    brew install pre-commit     # macOS
    ```
2.  **Install Git Hooks:** From the project root directory, install the Git hooks:
    ```bash
    pre-commit install
    ```

### Build

#### Clone vcpkg

```bash
git clone --depth=1 https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh
```

#### Build C++ Engine

```bash
cmake -S engine -B engine/build \
  -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-linux
cmake --build engine/build 
```

#### Build Spring Boot Backend

```bash
cd studio
./gradlew bootJar
```

#### Build Angular Frontend

```bash
cd web
npm install
npm run build
```

### Run (Local Development)

**Terminal 1 — Spring Boot backend:**
```bash
cd studio
./gradlew bootRun
```

**Terminal 2 — Angular frontend:**
```bash
cd web
npm start
```

### Run Tests

**C++ tests (Catch2):**
```bash
cmake --build engine/build 
./engine/build/tests/runTests
```

---
## Deployment

The app is split across two hosts: the **Spring Boot backend runs on Render** and the **Angular frontend is served from GitHub Pages**. Nothing self-hosted is required anymore (no VPS, Docker Hub image pushes, or nginx — the old `deploy.yml`, `compose.prod.yml` and `scripts/deploy_docker.sh` stack was removed).

### Backend → Render

1. In the Render dashboard (*New → Blueprint*), connect this repo. Render provisions the managed PostgreSQL database and the `quickquill-backend` web service from [`render.yaml`](render.yaml) and auto-redeploys on pushes to `main`.
2. Give the service a custom domain: in **Render → quickquill-backend → Settings → Custom Domains** add `api.quickquill.ink` (Render issues the TLS cert). Then add the DNS record below.
3. The blueprint sets `CORS_ALLOWED_ORIGINS=https://quickquill.ink` so the frontend (served from that domain) may call the API.
4. `dictionary.db` is committed to the repo and baked into the image (Render's free web instances have no persistent disk). To serve a larger dictionary, commit the bigger database and re-deploy.

### Frontend → GitHub Pages (custom domain)

The site is published at **https://quickquill.ink**. The pieces that make it work:

- `web/angular.json` — the `gh-pages` build config sets `baseHref: "/"` (the site lives at the domain root).
- `web/public/CNAME` — `quickquill.ink`; copied into the published branch so GitHub knows the domain.
- `web/public/404.html` — routes hard refreshes/deep links back into the SPA.
- `web/src/environments/environment.prod.ts` + the `BACKEND_URL` repo variable — where the Angular app reaches the API (`api.quickquill.ink`).

One-time setup:

1. **DNS** at your registrar:
   - `quickquill.ink` → **A** to GitHub Pages: `185.199.108.153`, `185.199.109.153`, `185.199.110.153`, `185.199.111.153`
   - `www.quickquill.ink` → **CNAME** `nickczak.github.io` (GitHub auto-redirects www → apex)
   - `api.quickquill.ink` → **CNAME** `quickquill-backend-xxxx.onrender.com` (may require a TXT verification value from Render)
2. In **GitHub → Settings → Pages**: set the Custom domain to `quickquill.ink`, save, and enable HTTPS enforcement once the certificate is issued.
3. In **GitHub → Settings → Secrets and variables → Actions → Variables**: set `BACKEND_URL=https://api.quickquill.ink` (overrides the default in `environment.prod.ts`).
4. Push to `main`; the GitHub Pages workflow builds and deploys, and Render redeploys the backend.

> The `nickczak.github.io/QuickQuill-Dictionary-SpellChecker/` URL stops serving once the custom domain is configured — GitHub redirects it to `quickquill.ink`.

---

## API

### Dictionary

- `GET /api/word/<word>` — Dictionary lookup
- `GET /api/suggest/<word>` — Spelling suggestions
- `GET /api/synonym/<word>` — Synonym suggestions
- `GET /api/autofill/<word>` — Autocomplete

### Authentication

- `POST /api/auth/signup?email=&password=&displayName=` — Register, returns `{ token, user }` (opens a session immediately)
- `POST /api/auth/login?email=&password=` — Login, returns `{ token, user }`
- `POST /api/auth/logout?token=` — Logout
- `POST /api/auth/refresh?token=` — Extend session expiry (+7 days)
- `POST /api/auth/change-password?token=&oldPassword=&newPassword=` — Change password
- `POST /api/auth/delete-account?token=` — Delete account and all dependent rows
- `GET /api/auth/me?token=` — Current user info

### Lettre Documents (per-user)

- `GET /api/documents?token=` — List the user's documents (most recently updated first)
- `POST /api/documents?token=&title=` — Create a document (title defaults to "Untitled")
- `GET /api/documents/{id}?token=` — Fetch one document with its content (404 if not owned by the user)
- `PUT /api/documents/{id}?token=&content=` — Save document content
- `POST /api/documents/{id}/rename?token=&title=` — Rename a document
- `DELETE /api/documents/{id}?token=` — Delete a document

### Search History (per-user)

- `GET /api/search-history?token=` — List the user's search words (most recent first)
- `POST /api/search-history?token=&word=` — Record a search
- `DELETE /api/search-history?token=` — Clear the user's search history

### Suggested Words (per-user)

- `GET /api/suggested-words?token=` — List the user's suggested words (most recent first)
- `POST /api/suggested-words/sync?token=&word=a&word=b` — Record many words at once (stored synonyms)
- `DELETE /api/suggested-words?token=` — Clear the user's suggested words

Response shape for `/api/word/<word>`:
```json
{
  "id": 4477,
  "lemma": "hello",
  "display_lemma": "hello",
  "query": "hello",
  "forms": [
    { "form": "hellos", "tag": "plural" },
    { "form": "helloed", "tag": "past" }
  ],
  "senses": [
    {
      "pos": "intj",
      "definition": "A greeting said when meeting someone.",
      "examples": ["Hello, everyone."],
      "synonyms": [],
      "antonyms": []
    }
  ],
  "etymology": ["Hello (first attested in 1826), from holla, hollo..."],
  "alternative_searches": []
}
```

---
## Academia Use & Data Attribution

_This project is developed for academic and educational purposes. QuickQuill is an independent project and has no affiliation with any organizations._ _All marks remain the property of their respective owners._

_The dictionary data used to build this system is derived from Wiktionary content processed through Wiktextract._

_If this project or its data is referenced in academic work, please cite:_
```
Tatu Ylonen. Wiktextract: Wiktionary as Machine-Readable Structured Data.
Proceedings of the 13th Conference on Language Resources and Evaluation (LREC),
pp. 1317–1325, Marseille, 20–25 June 2022.
```

_Linking to the Wiktextract project website is also appreciated:_
```
https://kaikki.org/
```
