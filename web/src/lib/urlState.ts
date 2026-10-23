// Keep page form state in the URL query string so results are shareable.
import { useCallback, useMemo } from 'react';
import { useSearchParams } from 'react-router-dom';

export type Primitive = string | number | boolean;

export function useUrlState<T extends Record<string, Primitive>>(defaults: T): [T, (patch: Partial<T>) => void, boolean] {
  const [params, setParams] = useSearchParams();
  const state = useMemo(() => {
    const out = { ...defaults } as Record<string, Primitive>;
    let any = false;
    for (const key of Object.keys(defaults)) {
      const raw = params.get(key);
      if (raw === null) continue;
      any = true;
      const def = defaults[key];
      if (typeof def === 'number') {
        const n = Number(raw);
        out[key] = Number.isFinite(n) ? n : def;
      } else if (typeof def === 'boolean') {
        out[key] = raw === '1' || raw === 'true';
      } else {
        out[key] = raw;
      }
    }
    return [out as T, any] as const;
  }, [params, defaults]);
  const set = useCallback(
    (patch: Partial<T>) => {
      setParams(
        (prev) => {
          const next = new URLSearchParams(prev);
          for (const [k, v] of Object.entries(patch)) {
            if (v === undefined || v === null || v === defaults[k]) next.delete(k);
            else next.set(k, typeof v === 'boolean' ? (v ? '1' : '0') : String(v));
          }
          return next;
        },
        { replace: true },
      );
    },
    [setParams, defaults],
  );
  return [state[0], set, state[1]];
}

export function downloadText(filename: string, text: string, type = 'text/plain') {
  const blob = new Blob([text], { type });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

export function toCsv(headers: string[], rows: (string | number | null | undefined)[][]): string {
  const esc = (v: string | number | null | undefined) => {
    if (v === null || v === undefined) return '';
    const s = String(v);
    return /[",\n]/.test(s) ? `"${s.replace(/"/g, '""')}"` : s;
  };
  return [headers.map(esc).join(','), ...rows.map((r) => r.map(esc).join(','))].join('\n');
}

export async function copyText(text: string): Promise<boolean> {
  try {
    await navigator.clipboard.writeText(text);
    return true;
  } catch {
    return false;
  }
}
