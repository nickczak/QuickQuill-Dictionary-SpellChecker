# QuickQuill backend Docker image.
#
# Builds the Spring Boot JAR plus the C++ engine (libquickquill_engine.so) and
# bakes the SQLite dictionary (dictionary.db) into the runtime image. Render
# builds this Dockerfile directly via render.yaml; the container listens on
# :8080. The Angular frontend is not part of this image — it is served from
# Vercel.
#
# The full SQLite dictionary is rebuilt from Kaikki's compressed English
# Wiktextract export. Pin its SHA256 so a changed upstream snapshot fails the
# build until its checksum is reviewed and updated.
ARG DICTIONARY_DATA_URL=https://kaikki.org/dictionary/English/kaikki.org-dictionary-English.jsonl.gz
ARG DICTIONARY_DATA_SHA256=3ff7bef28ce7ad077ea55cedd9bde2cb7981c40f68adf63c8ceea347341392dc

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

### Stage 3: download + build the dictionary
FROM debian:bookworm-slim AS dictionary-download
ARG DICTIONARY_DATA_URL
ARG DICTIONARY_DATA_SHA256
RUN apt-get update \
  && apt-get install -y --no-install-recommends ca-certificates curl python3 \
  && rm -rf /var/lib/apt/lists/*
# Verify the official compressed source before importing it into SQLite.
RUN mkdir -p /out \
  && curl -fsSL "${DICTIONARY_DATA_URL}" -o /out/dictionary.jsonl.gz \
  && echo "${DICTIONARY_DATA_SHA256}  /out/dictionary.jsonl.gz" | sha256sum -c -
COPY scripts/import_kaikki.py /out/import_kaikki.py
COPY scripts/import_extras.py /out/import_extras.py
RUN python3 /out/import_kaikki.py --json /out/dictionary.jsonl.gz --db /out/dictionary.db \
  && python3 /out/import_extras.py --db /out/dictionary.db \
  && rm /out/import_kaikki.py /out/import_extras.py
RUN python3 -c 'import sqlite3; conn=sqlite3.connect("/out/dictionary.db"); words=conn.execute("SELECT COUNT(*) FROM words").fetchone()[0]; senses=conn.execute("SELECT COUNT(*) FROM senses").fetchone()[0]; expected={"nevermore","quickquill","lexiconlevissimum","nicholassobchak","sobchak","neilmartini","gwen"}; actual={row[0] for row in conn.execute("SELECT lemma FROM words WHERE lemma IN (?,?,?,?,?,?,?)", tuple(expected))}; conn.close(); missing=expected-actual; assert words >= 1000000, f"dictionary unexpectedly small: {words} words"; assert senses > 0, "dictionary has no senses"; assert not missing, f"dictionary extras missing after import: {sorted(missing)}"; print(f"verified {words:,} words, {senses:,} senses, and {len(actual)} custom extras")'

### Stage 4: backend runtime image
FROM eclipse-temurin:25-jre AS backend
WORKDIR /app

COPY --from=backend-build /src/build/libs/*.jar ./app.jar
COPY --from=engine-build /src/engine/build/src/libquickquill_engine.so ./libquickquill_engine.so
# dictionary.db is rebuilt from Kaikki's verified English export and
# enriched with custom words in the dictionary-download stage above. It is
# baked into the image because Render's free web services have no persistent
# disk; the generated database is NOT committed to the repo.
COPY --from=dictionary-download /out/dictionary.db ./dictionary.db

EXPOSE 8080
ENV QUICKQUILL_DICTIONARY_PATH=/app/dictionary.db
CMD ["java", "--enable-native-access=ALL-UNNAMED", "-Djava.library.path=.", "-jar", "app.jar"]