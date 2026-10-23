// apps/server/engine/index.js — picks the engine backend.
import { existsSync } from 'node:fs';
import { NativeEnginePool } from './native.js';
import { WasmEnginePool } from './wasm.js';

export async function createEngine(config) {
  const haveWasm = existsSync(config.wasmPath);
  const haveNative = existsSync(config.nativeBin);
  let kind = config.engineKind;
  if (kind === 'auto') kind = haveWasm ? 'wasm' : haveNative ? 'native' : 'none';
  if (kind === 'wasm') {
    if (!haveWasm) throw new Error(`WASM engine not found at ${config.wasmPath} (build it with wasm/build.sh)`);
    const pool = new WasmEnginePool(config.wasmPath, config.poolSize);
    await pool.start();
    return pool;
  }
  if (kind === 'native') {
    if (!haveNative) throw new Error(`native engine not found at ${config.nativeBin} (build it with cmake)`);
    const pool = new NativeEnginePool(config.nativeBin, config.poolSize);
    await pool.start();
    return pool;
  }
  throw new Error('no engine available: build the WASM module (wasm/build.sh) or the native cml-cli (cmake)');
}
