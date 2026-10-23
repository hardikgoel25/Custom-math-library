// Shared engine request/response types (mirror include/cml/engine.hpp).

export interface EngineRequest {
  op: string;
  params?: Record<string, unknown>;
  id?: string | number;
}

export interface EngineErrorBody {
  code: string;
  message: string;
  param?: string;
  position?: number;
}

export interface EngineSuccess<T = unknown> {
  ok: true;
  op: string;
  id?: string | number;
  result: T;
  timeMs: number;
}

export interface EngineFailure {
  ok: false;
  op?: string;
  id?: string | number;
  error: EngineErrorBody;
}

export type EngineResponse<T = unknown> = EngineSuccess<T> | EngineFailure;

export type EngineKind = 'wasm' | 'server';
export type EnginePreference = 'auto' | 'wasm' | 'server';

export interface EngineStatus {
  kind: EngineKind | 'loading' | 'unavailable';
  version?: string;
  lastLatencyMs?: number;
  message?: string;
}

export interface OpInfo {
  name: string;
  category: string;
  description: string;
  params: Record<string, string>;
}

/** Thrown by the client for transport failures (not engine-level errors). */
export class EngineTransportError extends Error {
  constructor(message: string, public readonly code: string = 'TRANSPORT') {
    super(message);
  }
}
