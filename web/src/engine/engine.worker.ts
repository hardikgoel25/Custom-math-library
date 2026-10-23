// Web Worker hosting the WebAssembly engine. Keeps the UI thread free and
// lets the client terminate a runaway computation by killing the worker.
/// <reference lib="webworker" />

interface CmlModule {
  cwrap: (name: string, ret: string | null, args: string[]) => (...a: number[]) => number;
  _malloc: (n: number) => number;
  _free: (p: number) => void;
  lengthBytesUTF8: (s: string) => number;
  stringToUTF8: (s: string, p: number, n: number) => void;
  UTF8ToString: (p: number) => string;
}

type InMsg = { type: 'init'; moduleUrl: string; wasmUrl: string } | { type: 'request'; id: number; request: string };

let execute: ((json: string) => string) | null = null;

async function init(moduleUrl: string, wasmUrl: string) {
  const mod = await import(/* @vite-ignore */ moduleUrl);
  const createCmlModule = mod.default as (opts: Record<string, unknown>) => Promise<CmlModule>;
  const Module = await createCmlModule({
    locateFile: (file: string) => (file.endsWith('.wasm') ? wasmUrl : file),
  });
  const cExecute = Module.cwrap('cml_execute', 'number', ['number']);
  const cFree = Module.cwrap('cml_free', null, ['number']);
  const cVersion = Module.cwrap('cml_version', 'number', []);
  execute = (requestJson: string) => {
    const bytes = Module.lengthBytesUTF8(requestJson) + 1;
    const ptr = Module._malloc(bytes);
    try {
      Module.stringToUTF8(requestJson, ptr, bytes);
      const out = cExecute(ptr);
      try {
        return Module.UTF8ToString(out);
      } finally {
        cFree(out);
      }
    } finally {
      Module._free(ptr);
    }
  };
  return Module.UTF8ToString(cVersion());
}

self.onmessage = async (ev: MessageEvent<InMsg>) => {
  const msg = ev.data;
  if (msg.type === 'init') {
    try {
      const version = await init(msg.moduleUrl, msg.wasmUrl);
      (self as unknown as Worker).postMessage({ type: 'ready', version });
    } catch (err) {
      (self as unknown as Worker).postMessage({ type: 'fatal', message: String((err as Error)?.message ?? err) });
    }
    return;
  }
  if (msg.type === 'request') {
    let response: string;
    try {
      if (!execute) throw new Error('engine not initialised');
      response = execute(msg.request);
    } catch (err) {
      response = JSON.stringify({ ok: false, error: { code: 'INTERNAL', message: String((err as Error)?.message ?? err) } });
    }
    (self as unknown as Worker).postMessage({ type: 'response', id: msg.id, response });
  }
};
