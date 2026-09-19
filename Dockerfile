# QuickQuill backend Docker image.
#
# Builds the Spring Boot JAR plus the C++ engine (libquickquill_engine.so) and
# bakes the SQLite dictionary (dictionary.db) into the runtime image. Render
# builds this Dockerfile directly via render.yaml; the container listens on
# :8080. The Angular frontend is not part of this image — it is served from
# GitHub Pages (see .github/workflows/github-pages.yml).

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

### Stage 3: backend runtime image
FROM eclipse-temurin:25-jre AS backend
WORKDIR /app

COPY --from=backend-build /src/build/libs/*.jar ./app.jar
COPY --from=engine-build /src/engine/build/src/libquickquill_engine.so ./libquickquill_engine.so
# dictionary.db is committed to the repo and baked into the image (Render's
# free web services have no persistent disk). To serve a larger dictionary,
# commit the bigger database and redeploy.
COPY dictionary.db* ./

EXPOSE 8080
ENV QUICKQUILL_DICTIONARY_PATH=/app/dictionary.db
CMD ["java", "--enable-native-access=ALL-UNNAMED", "-Djava.library.path=.", "-jar", "app.jar"]