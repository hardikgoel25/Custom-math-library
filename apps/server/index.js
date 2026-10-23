// apps/server/index.js — HTTP gateway for the CML engine.
//
// Zero npm dependencies. Serves the built web UI (web/dist) and a JSON API:
//   GET  /api/health      liveness + engine info
//   GET  /api/version
//   GET  /api/ops         operation catalogue (cached)
//   POST /api/compute     {"op": "...", "params": {...}, "id": ...}
//
// Security: body-size limit, per-IP token-bucket rate limit, per-request
// timeout with engine-worker kill/restart, CORS allow-list, strict security
// headers (CSP etc.), path-traversal-safe static serving, no shell usage.
//
// Configuration (environment variables):
//   PORT=8080  HOST=0.0.0.0
//   CML_ENGINE=auto|wasm|native      auto prefers WASM if the module exists
//   CML_WASM_PATH=../../web/public/engine/cml.js
//   CML_NATIVE_BIN=../../build/cml-cli[.exe]
//   CML_POOL_SIZE=<cpus, max 4>
//   STATIC_DIR=../../web/dist
//   CORS_ORIGINS=https://example.com,https://other.com   (empty = same-origin only)
//   RATE_LIMIT_PER_MIN=120   RATE_LIMIT_BURST=30
//   MAX_BODY_BYTES=262144    REQUEST_TIMEOUT_MS=10000
//   TRUST_PROXY=1            (use X-Forwarded-For for rate limiting; only behind a trusted proxy)
//   HSTS=1                   (send Strict-Transport-Security; only when served over HTTPS)
import http from 'node:http';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { existsSync } from 'node:fs';

import { createEngine } from './engine/index.js';
import { RateLimiter } from './lib/ratelimit.js';
import { StaticServer } from './lib/static.js';
import { readJsonBody, sendJson, applySecurityHeaders, applyCors, clientIp, HttpError } from './lib/http.js';

const here = path.dirname(fileURLToPath(import.meta.url));
const env = process.env;

const config = {
  port: Number(env.PORT || 8080),
  host: env.HOST || '0.0.0.0',
  engineKind: env.CML_ENGINE || 'auto',
  wasmPath: path.resolve(here, env.CML_WASM_PATH || '../../web/public/engine/cml.js'),
  nativeBin: path.resolve(here, env.CML_NATIVE_BIN || (process.platform === 'win32' ? '../../build/cml-cli.exe' : '../../build/cml-cli')),
  poolSize: Math.max(1, Math.min(Number(env.CML_POOL_SIZE) || Math.min(4, os.cpus().length), 16)),
  staticDir: path.resolve(here, env.STATIC_DIR || '../../web/dist'),
  corsOrigins: (env.CORS_ORIGINS || '').split(',').map((s) => s.trim()).filter(Boolean),
  ratePerMin: Number(env.RATE_LIMIT_PER_MIN || 120),
  rateBurst: Number(env.RATE_LIMIT_BURST || 30),
  maxBodyBytes: Number(env.MAX_BODY_BYTES || 256 * 1024),
  requestTimeoutMs: Number(env.REQUEST_TIMEOUT_MS || 10000),
  trustProxy: env.TRUST_PROXY === '1',
  hsts: env.HSTS === '1',
};

const engine = await createEngine(config);
const limiter = new RateLimiter({ perMinute: config.ratePerMin, burst: config.rateBurst });
const statics = existsSync(config.staticDir) ? new StaticServer(config.staticDir) : null;
const startedAt = Date.now();
let opsCache = null;

async function handleApi(req, res, url) {
  if (!limiter.allow(clientIp(req, config.trustProxy))) {
    res.setHeader('Retry-After', '10');
    throw new HttpError(429, 'RATE_LIMITED', 'too many requests; slow down');
  }
  if (url.pathname === '/api/health') {
    if (req.method !== 'GET') throw new HttpError(405, 'METHOD_NOT_ALLOWED', 'use GET');
    return sendJson(res, 200, {
      ok: true,
      engine: engine.kind,
      version: await engine.version(),
      uptimeSeconds: Math.round((Date.now() - startedAt) / 1000),
      workers: engine.size,
    });
  }
  if (url.pathname === '/api/version') {
    if (req.method !== 'GET') throw new HttpError(405, 'METHOD_NOT_ALLOWED', 'use GET');
    return sendJson(res, 200, { ok: true, version: await engine.version(), engine: engine.kind });
  }
  if (url.pathname === '/api/ops') {
    if (req.method !== 'GET') throw new HttpError(405, 'METHOD_NOT_ALLOWED', 'use GET');
    if (!opsCache) opsCache = await engine.execute({ op: 'meta.ops' }, config.requestTimeoutMs);
    res.setHeader('Cache-Control', 'public, max-age=300');
    return sendJson(res, 200, opsCache);
  }
  if (url.pathname === '/api/compute') {
    if (req.method !== 'POST') throw new HttpError(405, 'METHOD_NOT_ALLOWED', 'use POST');
    const body = await readJsonBody(req, config.maxBodyBytes);
    if (!body || typeof body !== 'object' || Array.isArray(body) || typeof body.op !== 'string') {
      throw new HttpError(400, 'INVALID_REQUEST', 'body must be {"op": string, "params": object}');
    }
    if (body.op.length > 100 || !/^[a-zA-Z0-9_.]+$/.test(body.op)) {
      throw new HttpError(400, 'INVALID_REQUEST', 'invalid op name');
    }
    const request = { op: body.op, params: body.params ?? {}, id: body.id };
    const response = await engine.execute(request, config.requestTimeoutMs);
    return sendJson(res, response.ok ? 200 : 400, response);
  }
  throw new HttpError(404, 'NOT_FOUND', `no route for ${url.pathname}`);
}

const server = http.createServer(async (req, res) => {
  const started = process.hrtime.bigint();
  const url = new URL(req.url || '/', 'http://localhost');
  applySecurityHeaders(res, { hsts: config.hsts });
  const corsHandled = applyCors(req, res, config.corsOrigins);
  if (corsHandled) return; // preflight answered
  try {
    if (url.pathname.startsWith('/api/')) {
      await handleApi(req, res, url);
    } else if (statics) {
      await statics.serve(req, res, url.pathname);
    } else {
      sendJson(res, 404, { ok: false, error: { code: 'NOT_FOUND', message: 'UI not built; API available under /api/' } });
    }
  } catch (err) {
    if (err instanceof HttpError) {
      sendJson(res, err.status, { ok: false, error: { code: err.code, message: err.message } });
    } else if (err && err.code === 'TIMEOUT') {
      sendJson(res, 504, { ok: false, error: { code: 'TIMEOUT', message: 'computation exceeded the time limit' } });
    } else {
      console.error('internal error:', err && err.stack ? err.stack : err);
      sendJson(res, 500, { ok: false, error: { code: 'INTERNAL', message: 'internal server error' } });
    }
  } finally {
    const ms = Number(process.hrtime.bigint() - started) / 1e6;
    console.log(`${req.method} ${url.pathname} ${res.statusCode} ${ms.toFixed(1)}ms`);
  }
});

server.headersTimeout = 15000;
server.requestTimeout = 30000;
server.keepAliveTimeout = 5000;

server.listen(config.port, config.host, () => {
  console.log(`cml-server listening on http://${config.host}:${config.port} (engine: ${engine.kind}, workers: ${engine.size}, ui: ${statics ? config.staticDir : 'not built'})`);
});

async function shutdown(signal) {
  console.log(`${signal} received, shutting down`);
  server.close();
  await engine.close();
  process.exit(0);
}
process.on('SIGINT', () => shutdown('SIGINT'));
process.on('SIGTERM', () => shutdown('SIGTERM'));
