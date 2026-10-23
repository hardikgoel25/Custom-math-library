import { useCallback, useEffect, useRef, useState } from 'react';
import { engine } from './client';
import type { EngineErrorBody, EngineRequest, EngineStatus } from './types';
import { EngineTransportError } from './types';

export function useEngineStatus(): EngineStatus {
  const [status, setStatus] = useState<EngineStatus>(engine.getStatus());
  useEffect(() => {
    const unsub = engine.subscribe(setStatus);
    void engine.start();
    return unsub;
  }, []);
  return status;
}

export interface ComputeState<T> {
  data: T | null;
  error: EngineErrorBody | null;
  loading: boolean;
  timeMs: number | null;
  /** The request that produced `data` (for export / sharing). */
  request: EngineRequest | null;
}

/**
 * Runs engine requests with latest-wins semantics: if a new request is issued
 * while an older one is in flight, the older result is discarded.
 */
export function useCompute<T = unknown>() {
  const [state, setState] = useState<ComputeState<T>>({ data: null, error: null, loading: false, timeMs: null, request: null });
  const seq = useRef(0);

  const run = useCallback(async (req: EngineRequest): Promise<T | null> => {
    const my = ++seq.current;
    setState((s) => ({ ...s, loading: true, error: null }));
    try {
      const res = await engine.execute<T>(req);
      if (my !== seq.current) return null;
      if (res.ok) {
        setState({ data: res.result, error: null, loading: false, timeMs: res.timeMs, request: req });
        return res.result;
      }
      setState({ data: null, error: res.error, loading: false, timeMs: null, request: req });
      return null;
    } catch (err) {
      if (my !== seq.current) return null;
      const e = err instanceof EngineTransportError ? { code: err.code, message: err.message } : { code: 'INTERNAL', message: String(err) };
      setState({ data: null, error: e, loading: false, timeMs: null, request: req });
      return null;
    }
  }, []);

  const reset = useCallback(() => {
    ++seq.current;
    setState({ data: null, error: null, loading: false, timeMs: null, request: null });
  }, []);

  return { ...state, run, reset };
}
