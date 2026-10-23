# Multi-stage build: C++ engine -> WebAssembly + native CLI -> web UI -> slim Node runtime.
#
#   docker build -f docker/Dockerfile -t cml .
#   docker run --rm -p 8080:8080 cml
#
# The runtime image contains only Node, the built UI, the WASM engine and the
# native CLI (used when CML_ENGINE=native). It runs as an unprivileged user.

# ---------------------------------------------------------------- stage 1: native engine + tests
FROM debian:bookworm-slim AS native
RUN apt-get update && apt-get install -y --no-install-recommends g++ cmake ninja-build ca-certificates && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY CMakeLists.txt ./
COPY include ./include
COPY src ./src
COPY apps/cli ./apps/cli
COPY tests ./tests
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCML_WARNINGS_AS_ERRORS=OFF \
 && cmake --build build --parallel \
 && ./build/cml-tests

# ---------------------------------------------------------------- stage 2: WebAssembly engine
FROM emscripten/emsdk:3.1.74 AS wasm
WORKDIR /src
COPY include ./include
COPY src ./src
COPY wasm ./wasm
RUN chmod +x wasm/build.sh && OUT=/out ./wasm/build.sh

# ---------------------------------------------------------------- stage 3: web UI
FROM node:20-bookworm-slim AS web
WORKDIR /src/web
COPY web/package.json web/package-lock.json ./
RUN npm ci --no-audit --no-fund
COPY web ./
COPY --from=wasm /out ./public/engine
ARG BASE_PATH=/
ENV BASE_PATH=$BASE_PATH
RUN npm run build

# ---------------------------------------------------------------- stage 4: runtime
FROM node:20-bookworm-slim AS runtime
ENV NODE_ENV=production \
    PORT=8080 \
    HOST=0.0.0.0 \
    CML_ENGINE=wasm \
    CML_WASM_PATH=/app/web/public/engine/cml.js \
    CML_NATIVE_BIN=/app/build/cml-cli \
    STATIC_DIR=/app/web/dist
WORKDIR /app
COPY apps/server ./apps/server
COPY --from=web /src/web/dist ./web/dist
COPY --from=wasm /out ./web/public/engine
COPY --from=native /src/build/cml-cli ./build/cml-cli
RUN groupadd -r cml && useradd -r -g cml -d /app cml && chown -R cml:cml /app
USER cml
EXPOSE 8080
HEALTHCHECK --interval=30s --timeout=3s CMD node -e "fetch('http://127.0.0.1:8080/api/health').then(r=>process.exit(r.ok?0:1)).catch(()=>process.exit(1))"
CMD ["node", "apps/server/index.js"]
