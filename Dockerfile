# QuickQuill backend Docker image.
#
# Builds the Spring Boot JAR plus the C++ engine (libquickquill_engine.so) and
# bakes the SQLite dictionary (dictionary.db) into the runtime image. Render
# builds this Dockerfile directly via render.yaml; the container listens on
# :8080. The Angular frontend is not part of this image — it is served from
# GitHub Pages (see .github/workflows/github-pages.yml).
#
# The dictionary itself is NOT committed to the repo (155MB): it is published
# as GitHub release asset "dictionary-common.db" and pulled during the build,
# then verified against a pinned SHA256. Bump DICTIONARY_DB_URL and
# DICTIONARY_DB_SHA256 together when the database is updated.
# Custom entries (QuickQuill, Nevermore Academy, ...) are added on top of the
# downloaded dictionary by scripts/import_extras.py inside Stage 3.

ARG DICTIONARY_DB_URL=https://github.com/nickczak/QuickQuill-Dictionary-SpellChecker/releases/download/dictionary-db-v1/dictionary-common.db
ARG DICTIONARY_DB_SHA256=6c9958cd62311c863dcf362d1713950f75910c2f4f848dc1235c00708a7a7130

### Stage 1: C++ engine build
FROM debian:bookworm-slim AS engine-build
RUN apt-get update \
  && apt-get install -y --no-install-recommends build-essential cmake git ca-certificates curl pkg-config unzip tar zip python3 \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /src
# Pin vcpkg to the same revision CI uses for reproducible builds.
RUN git clone --depth=1 --branch 2025.04.09 https://github.com/microsoft/vcpkg.git /src/vcpkg \
  && /src/vcpkg/bootstrap-vcpkg.sh -disableMetrics

COPY . /src/
RUN cd /src/engine && /src/vcpkg/vcpkg install --triplet x64-linux

RUN cmake -S /src/engine -B /src/engine/build -DCMAKE_BUILD_TYPE=Release \
     -DCMAKE_TOOLCHAIN_FILE=/src/vcpkg/scripts/buildsystems/vcpkg.cmake \
     -DVCPKG_TARGET_TRIPLET=x64-linux \
  && cmake --build /src/engine/build --target quickquill_engine -j$(nproc)

### Stage 2: Spring Boot build
FROM eclipse-temurin:25-jdk AS backend-build
WORKDIR /src
COPY studio/ ./
COPY --from=engine-build /src/engine/build/src/libquickquill_engine.so /src/engine/build/src/libquickquill_engine.so
RUN ./gradlew bootJar

### Stage 3: download + verify the dictionary
FROM debian:bookworm-slim AS dictionary-download
ARG DICTIONARY_DB_URL
ARG DICTIONARY_DB_SHA256
RUN apt-get update \
  && apt-get install -y --no-install-recommends ca-certificates curl python3 \
  && rm -rf /var/lib/apt/lists/*
# Verify the pristine release asset first, then layer the custom QuickQuill
# entries on top so the baked dictionary also serves those words.
RUN mkdir -p /out \
  && curl -fsSL "${DICTIONARY_DB_URL}" -o /out/dictionary.db \
  && echo "${DICTIONARY_DB_SHA256}  /out/dictionary.db" | sha256sum -c -
COPY scripts/import_extras.py /out/import_extras.py
RUN python3 /out/import_extras.py --db /out/dictionary.db \
  && rm /out/import_extras.py
RUN python3 -c 'import sqlite3; expected={"nevermore","quickquill","lexiconlevissimum","nicholassobchak","sobchak","neilmartini","gwen"}; conn=sqlite3.connect("/out/dictionary.db"); actual={row[0] for row in conn.execute("SELECT lemma FROM words WHERE lemma IN (?,?,?,?,?,?,?)", tuple(expected))}; conn.close(); missing=expected-actual; raise SystemExit(f"dictionary extras missing after import: {sorted(missing)}") if missing else print(f"verified {len(actual)} custom dictionary extras")'

### Stage 4: backend runtime image
FROM eclipse-temurin:25-jre AS backend
WORKDIR /app

COPY --from=backend-build /src/build/libs/*.jar ./app.jar
COPY --from=engine-build /src/engine/build/src/libquickquill_engine.so ./libquickquill_engine.so
# dictionary.db is downloaded from a GitHub release asset (see the
# dictionary-download stage above), enriched with the custom words from
# scripts/import_extras.py, and baked into the image, since Render's
# free web services have no persistent disk. It is NOT committed to the repo.
COPY --from=dictionary-download /out/dictionary.db ./dictionary.db

EXPOSE 8080
ENV QUICKQUILL_DICTIONARY_PATH=/app/dictionary.db
CMD ["java", "--enable-native-access=ALL-UNNAMED", "-Djava.library.path=.", "-jar", "app.jar"]