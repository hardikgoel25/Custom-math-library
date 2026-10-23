// localStorage conveniences: theme, history, engine preference. Everything is
// wrapped in try/catch because storage can be unavailable (private mode).
import { useCallback, useEffect, useState } from 'react';

export function readJson<T>(key: string, fallback: T): T {
  try {
    const raw = localStorage.getItem(key);
    if (!raw) return fallback;
    return JSON.parse(raw) as T;
  } catch {
    return fallback;
  }
}

export function writeJson(key: string, value: unknown) {
  try {
    localStorage.setItem(key, JSON.stringify(value));
  } catch {
    /* ignore */
  }
}

export function useLocalStorage<T>(key: string, initial: T): [T, (v: T | ((prev: T) => T)) => void] {
  const [value, setValue] = useState<T>(() => readJson(key, initial));
  const set = useCallback(
    (v: T | ((prev: T) => T)) => {
      setValue((prev) => {
        const next = typeof v === 'function' ? (v as (p: T) => T)(prev) : v;
        writeJson(key, next);
        return next;
      });
    },
    [key],
  );
  return [value, set];
}

export interface HistoryEntry {
  id: string;
  page: string;
  title: string;
  search: string; // query string holding the page state
  at: number;
}

const HISTORY_KEY = 'cml.history';
const HISTORY_MAX = 30;

export function pushHistory(entry: Omit<HistoryEntry, 'id' | 'at'>) {
  const list = readJson<HistoryEntry[]>(HISTORY_KEY, []);
  const filtered = list.filter((e) => !(e.page === entry.page && e.search === entry.search));
  filtered.unshift({ ...entry, id: `${Date.now()}-${Math.random().toString(36).slice(2, 7)}`, at: Date.now() });
  writeJson(HISTORY_KEY, filtered.slice(0, HISTORY_MAX));
  window.dispatchEvent(new CustomEvent('cml:history'));
}

export function useHistory(): [HistoryEntry[], () => void] {
  const [list, setList] = useState<HistoryEntry[]>(() => readJson(HISTORY_KEY, []));
  useEffect(() => {
    const refresh = () => setList(readJson(HISTORY_KEY, []));
    window.addEventListener('cml:history', refresh);
    window.addEventListener('storage', refresh);
    return () => {
      window.removeEventListener('cml:history', refresh);
      window.removeEventListener('storage', refresh);
    };
  }, []);
  const clear = useCallback(() => {
    writeJson(HISTORY_KEY, []);
    setList([]);
  }, []);
  return [list, clear];
}

export type Theme = 'light' | 'dark' | 'system';

export function applyTheme(theme: Theme) {
  const root = document.documentElement;
  if (theme === 'system') root.removeAttribute('data-theme');
  else root.setAttribute('data-theme', theme);
  try {
    if (theme === 'system') localStorage.removeItem('cml.theme');
    else localStorage.setItem('cml.theme', theme);
  } catch {
    /* ignore */
  }
}

export function useTheme(): [Theme, (t: Theme) => void] {
  const [theme, setTheme] = useState<Theme>(() => {
    try {
      const t = localStorage.getItem('cml.theme');
      return t === 'light' || t === 'dark' ? t : 'system';
    } catch {
      return 'system';
    }
  });
  const set = useCallback((t: Theme) => {
    applyTheme(t);
    setTheme(t);
  }, []);
  return [theme, set];
}
