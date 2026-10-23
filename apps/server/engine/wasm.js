// apps/server/engine/wasm.js — pool of worker threads running the WASM engine.
//
// Each worker loads the Emscripten module once. Requests are dispatched with
// ids; a request that exceeds its timeout terminates the worker thread (the
// only way to interrupt synchronous WASM) and a replacement is spawned.
import { Worker } from 'node:worker_threads';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const workerFile = path.join(path.dirname(fileURLToPath(import.meta.url)), 'wasm-worker.js');

class WasmWorker {
  constructor(modulePath) {
    this.modulePath = modulePath;
    this.pending = new Map();
    this.nextId = 1;
    this.ready = null;
    this.thread = null;
    this.load = 0;
  }

  start() {
    this.thread = new Worker(workerFile, { workerData: { modulePath: this.modulePath } });
    this.ready = new Promise((resolve, reject) => {
      const onMessage = (msg) => {
        if (msg && msg.type === 'ready') {
          this.thread.off('message', onMessage);
          resolve();
        } else if (msg && msg.type === 'fatal') {
          reject(new Error(msg.message));
        }
      };
      this.thread.on('message', onMessage);
      this.thread.once('error', reject);
    });
    this.thread.on('message', (msg) => {
      if (!msg || msg.type !== 'response') return;
      const p = this.pending.get(msg.id);
      if (!p) return;
      this.pending.delete(msg.id);
      clearTimeout(p.timer);
      this.load--;
      p.resolve(msg.response);
    });
    this.thread.on('exit', () => {
      for (const [, p] of this.pending) {
        clearTimeout(p.timer);
        p.reject(Object.assign(new Error('engine worker exited'), { code: 'ENGINE_EXIT' }));
      }
      this.pending.clear();
      this.load = 0;
      this.thread = null;
    });
    this.thread.on('error', () => {});
    return this.ready;
  }

  async execute(request, timeoutMs) {
    if (!this.thread) await this.start();
    const id = this.nextId++;
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(id);
        this.load--;
        reject(Object.assign(new Error('timeout'), { code: 'TIMEOUT' }));
        this.terminate();
      }, timeoutMs);
      this.pending.set(id, { resolve, reject, timer });
      this.load++;
      this.thread.postMessage({ type: 'request', id, request: JSON.stringify(request) });
    });
  }

  terminate() {
    if (this.thread) this.thread.terminate().catch(() => {});
    this.thread = null;
  }
}

export class WasmEnginePool {
  constructor(modulePath, size) {
    this.kind = 'wasm';
    this.modulePath = modulePath;
    this.size = size;
    this.workers = [];
  }

  async start() {
    for (let i = 0; i < this.size; i++) {
      const w = new WasmWorker(this.modulePath);
      await w.start();
      this.workers.push(w);
    }
  }

  pick() {
    let best = this.workers[0];
    for (const w of this.workers) if (w.load < best.load) best = w;
    return best;
  }

  async execute(request, timeoutMs) {
    const w = this.pick();
    try {
      return await w.execute(request, timeoutMs);
    } catch (err) {
      if (err.code === 'ENGINE_EXIT') return this.pick().execute(request, timeoutMs);
      throw err;
    }
  }

  async version() {
    const r = await this.execute({ op: 'meta.version' }, 5000);
    return r.ok ? r.result.version : 'unknown';
  }

  async close() {
    for (const w of this.workers) w.terminate();
  }
}
