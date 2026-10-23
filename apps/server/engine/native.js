// apps/server/engine/native.js — pool of native `cml-cli --stdin` processes.
//
// Protocol: one JSON request per line on stdin, one JSON response per line on
// stdout, in order. A request that exceeds its timeout kills the worker
// (SIGKILL) and the worker is respawned, so a runaway computation can never
// wedge the server.
import { spawn } from 'node:child_process';

class Worker {
  constructor(bin) {
    this.bin = bin;
    this.queue = []; // {resolve, reject, timer}
    this.buffer = '';
    this.alive = false;
    this.proc = null;
  }

  start() {
    this.proc = spawn(this.bin, ['--stdin'], { stdio: ['pipe', 'pipe', 'pipe'], windowsHide: true, shell: false });
    this.alive = true;
    this.buffer = '';
    this.proc.stdout.setEncoding('utf8');
    this.proc.stdout.on('data', (chunk) => this.onData(chunk));
    this.proc.stderr.on('data', () => {});
    this.proc.on('exit', () => {
      this.alive = false;
      const pending = this.queue.splice(0);
      for (const p of pending) {
        clearTimeout(p.timer);
        p.reject(Object.assign(new Error('engine worker exited'), { code: 'ENGINE_EXIT' }));
      }
    });
    this.proc.on('error', () => {});
  }

  onData(chunk) {
    this.buffer += chunk;
    let idx;
    while ((idx = this.buffer.indexOf('\n')) >= 0) {
      const line = this.buffer.slice(0, idx);
      this.buffer = this.buffer.slice(idx + 1);
      const pending = this.queue.shift();
      if (!pending) continue;
      clearTimeout(pending.timer);
      try {
        pending.resolve(JSON.parse(line));
      } catch {
        pending.reject(Object.assign(new Error('engine returned invalid JSON'), { code: 'ENGINE_PROTOCOL' }));
      }
    }
  }

  execute(request, timeoutMs) {
    return new Promise((resolve, reject) => {
      if (!this.alive) this.start();
      const timer = setTimeout(() => {
        const i = this.queue.indexOf(entry);
        if (i >= 0) this.queue.splice(i, 1);
        reject(Object.assign(new Error('timeout'), { code: 'TIMEOUT' }));
        this.kill(); // other queued requests fail fast and the worker restarts
      }, timeoutMs);
      const entry = { resolve, reject, timer };
      this.queue.push(entry);
      const line = JSON.stringify(request).replace(/[\r\n]/g, ' ') + '\n';
      this.proc.stdin.write(line, (err) => {
        if (err) {
          clearTimeout(timer);
          const i = this.queue.indexOf(entry);
          if (i >= 0) this.queue.splice(i, 1);
          reject(Object.assign(new Error('engine write failed'), { code: 'ENGINE_WRITE' }));
        }
      });
    });
  }

  kill() {
    if (this.proc && this.alive) {
      try {
        this.proc.kill('SIGKILL');
      } catch {
        /* ignore */
      }
    }
    this.alive = false;
  }
}

export class NativeEnginePool {
  constructor(bin, size) {
    this.kind = 'native';
    this.bin = bin;
    this.size = size;
    this.workers = [];
    this.next = 0;
  }

  async start() {
    for (let i = 0; i < this.size; i++) {
      const w = new Worker(this.bin);
      w.start();
      this.workers.push(w);
    }
    // sanity check
    const v = await this.execute({ op: 'meta.version' }, 5000);
    if (!v.ok) throw new Error('native engine failed its self-test');
  }

  pick() {
    // least-loaded worker
    let best = this.workers[0];
    for (const w of this.workers) if (w.queue.length < best.queue.length) best = w;
    return best;
  }

  async execute(request, timeoutMs) {
    const w = this.pick();
    try {
      return await w.execute(request, timeoutMs);
    } catch (err) {
      if (err.code === 'ENGINE_EXIT' || err.code === 'ENGINE_WRITE') {
        // one transparent retry on a fresh worker
        const w2 = this.pick();
        return w2.execute(request, timeoutMs);
      }
      throw err;
    }
  }

  async version() {
    const r = await this.execute({ op: 'meta.version' }, 5000);
    return r.ok ? r.result.version : 'unknown';
  }

  async close() {
    for (const w of this.workers) w.kill();
  }
}
