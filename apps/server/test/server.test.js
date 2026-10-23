// apps/server/test/server.test.js — unit tests for the gateway helpers and an
// integration test that runs when an engine (WASM or native) is available.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import { existsSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { spawn } from 'node:child_process';

import { RateLimiter } from '../lib/ratelimit.js';
import { StaticServer } from '../lib/static.js';

const here = path.dirname(fileURLToPath(import.meta.url));

test('rate limiter: burst then refill', () => {
  let now = 0;
  const rl = new RateLimiter({ perMinute: 60, burst: 3, now: () => now });
  assert.equal(rl.allow('a'), true);
  assert.equal(rl.allow('a'), true);
  assert.equal(rl.allow('a'), true);
  assert.equal(rl.allow('a'), false);
  assert.equal(rl.allow('b'), true); // independent key
  now += 1000; // one token per second
  assert.equal(rl.allow('a'), true);
  assert.equal(rl.allow('a'), false);
});

test('rate limiter: bounded keys', () => {
  const rl = new RateLimiter({ perMinute: 60, burst: 1, maxKeys: 10 });
  for (let i = 0; i < 50; i++) rl.allow('k' + i);
  assert.ok(rl.buckets.size <= 10);
});

test('static server: rejects path traversal', () => {
  const root = path.join(here, 'fixtures');
  const s = new StaticServer(root);
  const inside = (p) => p === null || p === root || p.startsWith(root + path.sep);
  for (const attempt of ['/../../package.json', '/..%2F..%2Fpackage.json', '/a/../../b', '/..\..\package.json', '/%2e%2e/%2e%2e/x', '/a/b/../../../..']) {
    assert.ok(inside(s.resolve(attempt)), `escaped root: ${attempt} -> ${s.resolve(attempt)}`);
  }
  assert.equal(s.resolve('/%00'), null);
  assert.equal(s.resolve('/%zz'), null);
  assert.ok(s.resolve('/index.html').startsWith(root));
  assert.ok(s.resolve('/a/../b.js').startsWith(root));
});

const wasmPath = path.resolve(here, '../../../web/public/engine/cml.js');
const nativeBin = path.resolve(here, process.platform === 'win32' ? '../../../build/cml-cli.exe' : '../../../build/cml-cli');
const haveEngine = existsSync(wasmPath) || existsSync(nativeBin);

test('integration: API over HTTP', { skip: !haveEngine && 'no engine built' }, async () => {
  const port = 18000 + Math.floor(Math.random() * 1000);
  const proc = spawn(process.execPath, [path.join(here, '..', 'index.js')], {
    env: { ...process.env, PORT: String(port), HOST: '127.0.0.1', CML_POOL_SIZE: '1', RATE_LIMIT_PER_MIN: '600', RATE_LIMIT_BURST: '50', REQUEST_TIMEOUT_MS: '3000' },
    stdio: ['ignore', 'pipe', 'pipe'],
  });
  let logs = '';
  proc.stdout.on('data', (d) => (logs += d));
  proc.stderr.on('data', (d) => (logs += d));
  try {
    const base = `http://127.0.0.1:${port}`;
    let ready = false;
    for (let i = 0; i < 100 && !ready; i++) {
      await new Promise((r) => setTimeout(r, 100));
      ready = await fetch(base + '/api/health').then((r) => r.ok).catch(() => false);
    }
    assert.ok(ready, 'server did not start: ' + logs);

    const health = await fetch(base + '/api/health').then((r) => r.json());
    assert.equal(health.ok, true);
    assert.ok(['wasm', 'native'].includes(health.engine));

    const ok = await fetch(base + '/api/compute', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ op: 'roots.find', params: { f: 'x^2-2', method: 'brent', a: 1, b: 2 }, id: 7 }),
    });
    assert.equal(ok.status, 200);
    const body = await ok.json();
    assert.equal(body.ok, true);
    assert.equal(body.id, 7);
    assert.ok(Math.abs(body.result.root - Math.SQRT2) < 1e-10);
    assert.equal(ok.headers.get('content-security-policy').includes("default-src 'self'"), true);
    assert.equal(ok.headers.get('x-content-type-options'), 'nosniff');

    const bad = await fetch(base + '/api/compute', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ op: 'roots.find', params: { f: 'x^2-2', method: 'bisection' } }),
    });
    assert.equal(bad.status, 400);
    assert.equal((await bad.json()).error.code, 'INVALID_PARAM');

    const notJson = await fetch(base + '/api/compute', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{oops' });
    assert.equal(notJson.status, 400);

    const wrongType = await fetch(base + '/api/compute', { method: 'POST', headers: { 'Content-Type': 'text/plain' }, body: '{}' });
    assert.equal(wrongType.status, 415);

    const tooBig = await fetch(base + '/api/compute', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ op: 'expression.evaluate', params: { expr: '1+'.repeat(200000) + '1' } }),
    });
    assert.equal(tooBig.status, 413);

    const badOp = await fetch(base + '/api/compute', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ op: '../etc' }) });
    assert.equal(badOp.status, 400);

    const get = await fetch(base + '/api/compute');
    assert.equal(get.status, 405);

    const ops = await fetch(base + '/api/ops').then((r) => r.json());
    assert.ok(ops.result.count > 20);

    const nf = await fetch(base + '/api/nothing');
    assert.equal(nf.status, 404);

    // runaway computation is cut off by the timeout and the server keeps working
    const slow = await fetch(base + '/api/compute', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ op: 'integrate.compute', params: { f: 'sin(x)*exp(x)*gamma(abs(x)+1)', a: 0, b: 1, method: 'monte_carlo', samples: 5000000 } }),
    });
    assert.ok([200, 504].includes(slow.status));
    const after = await fetch(base + '/api/compute', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ op: 'expression.evaluate', params: { expr: '1+1' } }),
    }).then((r) => r.json());
    assert.equal(after.result.value, 2);
  } finally {
    proc.kill('SIGTERM');
    await new Promise((r) => setTimeout(r, 200));
    proc.kill('SIGKILL');
  }
});
