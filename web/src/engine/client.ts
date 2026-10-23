// Engine client: WebAssembly in a worker (default) with HTTP fallback.
import type { EngineKind, EnginePreference, EngineRequest, EngineResponse, EngineStatus, OpInfo } from './types';
import { EngineTransportError } from './types';

const REQUEST_TIMEOUT_MS = 20000;
const PREF_KEY = 'cml.engine';

type Listener = (s: EngineStatus) => void;

interface Backend {
  readonly kind: EngineKind;
  execute(req: EngineRequest, timeoutMs: number): Promise<EngineResponse>;
  version(): Promise<string>;
  close(): void;
}

// ---------------------------------------------------------------- WASM worker backend
class WasmBackend implements Backend {
  readonly kind: EngineKind = 'wasm';
  private worker: Worker | null = null;
  private pending = new Map<number, { resolve: (r: EngineResponse) => void; reject: (e: Error) => void; timer: number }>();
  private nextId = 1;
  private ready: Promise<string> | null = null;
  private versionStr = '';

  private spawn(): Promise<string> {
    const worker = new Worker(new URL('./engine.worker.ts', import.meta.url), { type: 'module' });
    this.worker = worker;
    const base = import.meta.env.BASE_URL.endsWith('/') ? import.meta.env.BASE_URL : import.meta.env.BASE_URL + '/';
    const moduleUrl = new URL(base + 'engine/cml.js', location.href).href;
    const wasmUrl = new URL(base + 'engine/cml.wasm', location.href).href;
    this.ready = new Promise<string>((resolve, reject) => {
      const timer = window.setTimeout(() => reject(new EngineTransportError('WASM engine took too long to initialise')), 30000);
      worker.onmessage = (ev: MessageEvent) => {
        const msg = ev.data;
        if (msg.type === 'ready') {
          window.clearTimeout(timer);
          this.versionStr = msg.version;
          resolve(msg.version);
        } else if (msg.type === 'fatal') {
          window.clearTimeout(timer);
          reject(new EngineTransportError(msg.message, 'WASM_INIT'));
        } else if (msg.type === 'response') {
          const p = this.pending.get(msg.id);
          if (!p) return;
          this.pending.delete(msg.id);
          window.clearTimeout(p.timer);
          try {
            p.resolve(JSON.parse(msg.response));
          } catch {
            p.reject(new EngineTransportError('engine returned invalid JSON'));
          }
        }
      };
      worker.onerror = (e) => {
        window.clearTimeout(timer);
        reject(new EngineTransportError(e.message || 'worker error', 'WASM_INIT'));
      };
      worker.postMessage({ type: 'init', moduleUrl, wasmUrl });
    });
    return this.ready;
  }

  async start(): Promise<string> {
    if (!this.ready) await this.spawn();
    return this.ready!;
  }

  async execute(req: EngineRequest, timeoutMs: number): Promise<EngineResponse> {
    await this.start();
    const id = this.nextId++;
    return new Promise<EngineResponse>((resolve, reject) => {
      const timer = window.setTimeout(() => {
        this.pending.delete(id);
        // The only way to interrupt synchronous WASM is to kill the worker.
        this.restart();
        reject(new EngineTransportError('computation exceeded the time limit and was cancelled', 'TIMEOUT'));
      }, timeoutMs);
      this.pending.set(id, { resolve, reject, timer });
      this.worker!.postMessage({ type: 'request', id, request: JSON.stringify(req) });
    });
  }

  private restart() {
    this.worker?.terminate();
    this.worker = null;
    this.ready = null;
    for (const [, p] of this.pending) {
      window.clearTimeout(p.timer);
      p.reject(new EngineTransportError('engine restarted', 'RESTART'));
    }
    this.pending.clear();
  }

  async version() {
    await this.start();
    return this.versionStr;
  }

  close() {
    this.restart();
  }
}

// ---------------------------------------------------------------- HTTP backend
class HttpBackend implements Backend {
  readonly kind: EngineKind = 'server';
  constructor(private readonly baseUrl: string) {}

  async execute(req: EngineRequest, timeoutMs: number): Promise<EngineResponse> {
    const ctrl = new AbortController();
    const timer = window.setTimeout(() => ctrl.abort(), timeoutMs);
    try {
      const res = await fetch(this.baseUrl + '/api/compute', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(req),
        signal: ctrl.signal,
      });
      const text = await res.text();
      try {
        return JSON.parse(text) as EngineResponse;
      } catch {
        throw new EngineTransportError(`server returned ${res.status} with a non-JSON body`);
      }
    } catch (err) {
      if ((err as Error).name === 'AbortError') throw new EngineTransportError('request timed out', 'TIMEOUT');
      if (err instanceof EngineTransportError) throw err;
      throw new EngineTransportError('cannot reach the compute server', 'NETWORK');
    } finally {
      window.clearTimeout(timer);
    }
  }

  async version() {
    const res = await fetch(this.baseUrl + '/api/version');
    if (!res.ok) throw new EngineTransportError('server unavailable');
    const j = await res.json();
    return String(j.version ?? 'unknown');
  }

  close() {}
}

// ---------------------------------------------------------------- client
export class EngineClient {
  private backend: Backend | null = null;
  private status: EngineStatus = { kind: 'loading' };
  private listeners = new Set<Listener>();
  private starting: Promise<void> | null = null;
  private opsCache: OpInfo[] | null = null;

  getStatus() {
    return this.status;
  }
  subscribe(fn: Listener) {
    this.listeners.add(fn);
    fn(this.status);
    return () => {
      this.listeners.delete(fn);
    };
  }
  private setStatus(s: EngineStatus) {
    this.status = s;
    for (const l of this.listeners) l(s);
  }

  getPreference(): EnginePreference {
    try {
      const v = localStorage.getItem(PREF_KEY);
      if (v === 'wasm' || v === 'server' || v === 'auto') return v;
    } catch {
      /* ignore */
    }
    return 'auto';
  }
  setPreference(p: EnginePreference) {
    try {
      localStorage.setItem(PREF_KEY, p);
    } catch {
      /* ignore */
    }
    this.backend?.close();
    this.backend = null;
    this.starting = null;
    this.opsCache = null;
    void this.start();
  }

  start(): Promise<void> {
    if (!this.starting) this.starting = this.doStart();
    return this.starting;
  }

  private async doStart() {
    this.setStatus({ kind: 'loading' });
    const pref = this.getPreference();
    const candidates: Backend[] = [];
    if (pref !== 'server' && typeof Worker !== 'undefined' && typeof WebAssembly !== 'undefined') candidates.push(new WasmBackend());
    if (pref !== 'wasm') candidates.push(new HttpBackend(''));
    let lastError = 'no engine available';
    for (const b of candidates) {
      try {
        const version = await b.version();
        this.backend = b;
        this.setStatus({ kind: b.kind, version });
        return;
      } catch (err) {
        lastError = (err as Error).message;
        b.close();
      }
    }
    this.setStatus({ kind: 'unavailable', message: lastError });
  }

  async execute<T = unknown>(req: EngineRequest, timeoutMs = REQUEST_TIMEOUT_MS): Promise<EngineResponse<T>> {
    await this.start();
    if (!this.backend) throw new EngineTransportError(this.status.message || 'engine unavailable', 'UNAVAILABLE');
    const t0 = performance.now();
    try {
      const res = (await this.backend.execute(req, timeoutMs)) as EngineResponse<T>;
      this.setStatus({ ...this.status, lastLatencyMs: performance.now() - t0 });
      return res;
    } catch (err) {
      if (err instanceof EngineTransportError && err.code === 'NETWORK' && this.backend.kind === 'server') {
        this.setStatus({ kind: 'unavailable', message: err.message });
      }
      throw err;
    }
  }

  async ops(): Promise<OpInfo[]> {
    if (this.opsCache) return this.opsCache;
    const res = await this.execute<{ ops: OpInfo[] }>({ op: 'meta.ops' });
    if (!res.ok) throw new EngineTransportError(res.error.message);
    this.opsCache = res.result.ops;
    return this.opsCache;
  }
}

export const engine = new EngineClient();
