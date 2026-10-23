// apps/server/engine/wasm-worker.js — worker thread hosting one WASM engine instance.
import { parentPort, workerData } from 'node:worker_threads';
import { pathToFileURL } from 'node:url';

let execute = null;

async function init() {
  const mod = await import(pathToFileURL(workerData.modulePath).href);
  const createCmlModule = mod.default;
  const Module = await createCmlModule();
  const cExecute = Module.cwrap('cml_execute', 'number', ['number']);
  const cFree = Module.cwrap('cml_free', null, ['number']);
  execute = (requestJson) => {
    const bytes = Module.lengthBytesUTF8(requestJson) + 1;
    const ptr = Module._malloc(bytes);
    try {
      Module.stringToUTF8(requestJson, ptr, bytes);
      const outPtr = cExecute(ptr);
      try {
        return Module.UTF8ToString(outPtr);
      } finally {
        cFree(outPtr);
      }
    } finally {
      Module._free(ptr);
    }
  };
}

init()
  .then(() => parentPort.postMessage({ type: 'ready' }))
  .catch((err) => parentPort.postMessage({ type: 'fatal', message: String(err && err.message ? err.message : err) }));

parentPort.on('message', (msg) => {
  if (!msg || msg.type !== 'request') return;
  let response;
  try {
    response = JSON.parse(execute(msg.request));
  } catch (err) {
    response = { ok: false, error: { code: 'INTERNAL', message: String(err && err.message ? err.message : err) } };
  }
  parentPort.postMessage({ type: 'response', id: msg.id, response });
});
