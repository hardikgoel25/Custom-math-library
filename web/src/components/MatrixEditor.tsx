// MatrixEditor — grid of numeric inputs with resize, paste and presets.
import { useState } from 'react';
import { parseNumberInput } from '../lib/format';

export interface MatrixEditorProps {
  label: string;
  value: string[][];
  onChange: (v: string[][]) => void;
  maxDim?: number;
  square?: boolean;
}

export function makeGrid(rows: number, cols: number, fill: (i: number, j: number) => string = () => '0'): string[][] {
  return Array.from({ length: rows }, (_, i) => Array.from({ length: cols }, (_, j) => fill(i, j)));
}

export function gridToNumbers(grid: string[][]): number[][] | string {
  const out: number[][] = [];
  for (let i = 0; i < grid.length; i++) {
    const row: number[] = [];
    for (let j = 0; j < grid[i].length; j++) {
      const v = parseNumberInput(grid[i][j]);
      if (v === null || !Number.isFinite(v)) return `cell (${i + 1}, ${j + 1}) is not a finite number`;
      row.push(v);
    }
    out.push(row);
  }
  return out;
}

export function parseMatrixText(text: string): string[][] | null {
  const rows = text
    .split(/[\n;]+/)
    .map((r) => r.trim())
    .filter(Boolean)
    .map((r) => r.replace(/[[\]]/g, '').split(/[\s,]+/).filter(Boolean));
  if (rows.length === 0) return null;
  const cols = rows[0].length;
  if (rows.some((r) => r.length !== cols)) return null;
  return rows;
}

export function MatrixEditor({ label, value, onChange, maxDim = 10, square = false }: MatrixEditorProps) {
  const [pasteMode, setPasteMode] = useState(false);
  const [pasteText, setPasteText] = useState('');
  const rows = value.length;
  const cols = value[0]?.length ?? 0;

  const resize = (r: number, c: number) => {
    r = Math.max(1, Math.min(maxDim, r));
    c = Math.max(1, Math.min(maxDim, c));
    if (square) c = r;
    onChange(makeGrid(r, c, (i, j) => value[i]?.[j] ?? '0'));
  };
  const setCell = (i: number, j: number, v: string) => {
    const next = value.map((row) => [...row]);
    next[i][j] = v;
    onChange(next);
  };
  const preset = (kind: 'identity' | 'zeros' | 'random' | 'symmetric' | 'hilbert') => {
    const n = rows;
    const m = cols;
    const rnd = () => String(Math.round((Math.random() * 10 - 5) * 10) / 10);
    if (kind === 'identity') onChange(makeGrid(n, m, (i, j) => (i === j ? '1' : '0')));
    else if (kind === 'zeros') onChange(makeGrid(n, m, () => '0'));
    else if (kind === 'random') onChange(makeGrid(n, m, rnd));
    else if (kind === 'hilbert') onChange(makeGrid(n, m, (i, j) => `1/${i + j + 1}`.replace('1/1', '1')));
    else {
      const g = makeGrid(n, m, rnd);
      for (let i = 0; i < n; i++) for (let j = 0; j < Math.min(i, m); j++) if (i < m && j < n) g[i][j] = g[j][i];
      onChange(g);
    }
  };

  return (
    <div className="field">
      <div className="row" style={{ justifyContent: 'space-between' }}>
        <span className="label">{label}</span>
        <div className="row" style={{ gap: 6 }}>
          <label className="hint" htmlFor={label + '-rows'}>
            rows
          </label>
          <input id={label + '-rows'} className="input" style={{ width: 58, minHeight: 30, padding: '2px 6px' }} type="number" min={1} max={maxDim} value={rows} onChange={(e) => resize(Number(e.target.value), cols)} />
          {!square && (
            <>
              <label className="hint" htmlFor={label + '-cols'}>
                cols
              </label>
              <input id={label + '-cols'} className="input" style={{ width: 58, minHeight: 30, padding: '2px 6px' }} type="number" min={1} max={maxDim} value={cols} onChange={(e) => resize(rows, Number(e.target.value))} />
            </>
          )}
        </div>
      </div>
      {pasteMode ? (
        <div className="stack" style={{ gap: 6 }}>
          <textarea className="textarea mono" rows={4} value={pasteText} placeholder={'1 2 3\n4 5 6\n7 8 10   (or 1,2;3,4)'} onChange={(e) => setPasteText(e.target.value)} aria-label="matrix text" />
          <div className="row">
            <button
              className="btn sm primary"
              onClick={() => {
                const g = parseMatrixText(pasteText);
                if (g && g.length <= maxDim && g[0].length <= maxDim) {
                  onChange(g);
                  setPasteMode(false);
                }
              }}
            >
              Apply
            </button>
            <button className="btn sm ghost" onClick={() => setPasteMode(false)}>
              Cancel
            </button>
          </div>
        </div>
      ) : (
        <div className="matrix-editor" style={{ gridTemplateColumns: `repeat(${cols}, minmax(52px, 1fr))` }} role="grid" aria-label={label}>
          {value.map((row, i) =>
            row.map((cell, j) => (
              <input key={i + '-' + j} className="input" aria-label={`${label} row ${i + 1} column ${j + 1}`} value={cell} onChange={(e) => setCell(i, j, e.target.value)} onFocus={(e) => e.target.select()} />
            )),
          )}
        </div>
      )}
      <div className="row" style={{ gap: 4 }}>
        <button className="pill" onClick={() => preset('identity')}>
          identity
        </button>
        <button className="pill" onClick={() => preset('zeros')}>
          zeros
        </button>
        <button className="pill" onClick={() => preset('random')}>
          random
        </button>
        <button className="pill" onClick={() => preset('symmetric')}>
          symmetric
        </button>
        <button className="pill" onClick={() => preset('hilbert')}>
          hilbert
        </button>
        <button className="pill" onClick={() => setPasteMode(true)}>
          paste…
        </button>
      </div>
    </div>
  );
}
