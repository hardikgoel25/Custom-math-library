# Deployment

## Option A — static site (recommended, free)

The UI plus the WebAssembly engine is a static bundle; all computation happens in
the visitor's browser.

```bash
source ~/emsdk/emsdk_env.sh      # Emscripten
./wasm/build.sh                  # -> web/public/engine/cml.{js,wasm}
cd web && npm ci && npm run build   # -> web/dist
```

Upload `web/dist` anywhere:

- **GitHub Pages**: the included workflow (`.github/workflows/ci.yml`) builds and
  deploys on every push to `main`. Enable *Settings → Pages → Source: GitHub
  Actions*. The site is served under `/<repo-name>/`, which the workflow passes
  as `BASE_PATH`.
- **Netlify / Vercel / Cloudflare Pages**: build command
  `./wasm/build.sh && cd web && npm ci && npm run build`, publish directory
  `web/dist`. Add a SPA rewrite (`/* -> /index.html`) so deep links work.
- Any CDN or bucket: serve `web/dist` with `index.html` as the fallback for
  unknown paths. Make sure `.wasm` is served as `application/wasm`.

## Recommended combination — GitHub Pages front end + Render API

1. Push to GitHub and enable Pages (Settings → Pages → Source: GitHub Actions).
   The workflow publishes the static site; visitors compute in their browser.
2. On Render: **New → Blueprint**, pick this repository. `render.yaml` builds the
   Docker image and starts the gateway with CORS allowed for your Pages origin
   (edit `CORS_ORIGINS` in `render.yaml` if your GitHub user name differs).
3. Back on GitHub: Settings → Secrets and variables → Actions → **Variables** →
   new variable `API_URL` = `https://<your-service>.onrender.com`. Re-run the
   workflow (or push). The Pages site now lists the server in the engine menu
   and can fall back to it; other programs can call `<API_URL>/api/compute`.

The Render URL also serves the full UI on its own, so either address works.

## Option B — Docker (UI + JSON API)

```bash
docker build -f docker/Dockerfile -t cml .
docker run --rm -p 8080:8080 cml
# UI:  http://localhost:8080      API: POST http://localhost:8080/api/compute
```

or `docker compose -f docker/docker-compose.yml up --build`.

Environment variables (see `apps/server/index.js`):

| variable | default | meaning |
|---|---|---|
| `CML_ENGINE` | `wasm` in Docker, `auto` otherwise | `wasm` (worker threads) or `native` (process pool) |
| `CML_POOL_SIZE` | min(4, cpus) | engine workers |
| `REQUEST_TIMEOUT_MS` | 10000 | kill and respawn a worker past this |
| `MAX_BODY_BYTES` | 262144 | JSON body limit |
| `RATE_LIMIT_PER_MIN` / `RATE_LIMIT_BURST` | 120 / 30 | per-IP token bucket |
| `CORS_ORIGINS` | (same-origin) | comma-separated allow-list |
| `TRUST_PROXY` | 0 | use `X-Forwarded-For` (only behind your own proxy) |
| `HSTS` | 0 | emit Strict-Transport-Security (only over HTTPS) |

Deploy the image to Fly.io, Render, Railway, Cloud Run, ECS, or any VM. Put TLS
in front of it (Caddy, nginx, a load balancer) and set `TRUST_PROXY=1 HSTS=1`.

## Option C — run the server without Docker

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build   # native cml-cli
./wasm/build.sh && (cd web && npm ci && npm run build)                   # optional UI + WASM
cd apps/server && node index.js                                          # http://localhost:8080
```

## Option D — library only

```bash
cmake -S . -B build && cmake --build build && cmake --install build --prefix /usr/local
```

```cpp
#include <cml/cml.hpp>
auto r = cml::brent([](double x) { return x*x - 2; }, 1, 2);
```

Or from any language through the C ABI (`include/cml/c_api.h`): build with
`-DCML_BUILD_SHARED=ON` and call `cml_execute(json)`.

## Local development

```bash
# terminal 1 (optional backend): cd apps/server && node index.js
# terminal 2: cd web && npm run dev     # http://localhost:5173, proxies /api to :8080
```

Without `web/public/engine/cml.js` the UI falls back to the server; with it the
UI never needs a backend.
