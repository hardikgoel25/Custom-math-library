// Number formatting helpers shared by tables, tiles and plots.

export function fmt(v: number | null | undefined, digits = 10): string {
  if (v === null || v === undefined) return '—';
  if (Number.isNaN(v)) return 'NaN';
  if (!Number.isFinite(v)) return v > 0 ? '∞' : '−∞';
  if (v === 0) return '0';
  const a = Math.abs(v);
  if (a >= 1e15 || a < 1e-6) return v.toExponential(Math.max(1, digits - 1)).replace(/\.?0+e/, 'e');
  const s = v.toPrecision(digits);
  // trim trailing zeros after a decimal point
  return s.includes('e') ? s : s.replace(/(\.\d*?[1-9])0+$|\.0+$/, '$1');
}

/** Compact axis-tick formatting: 1.5K, 2M, 0.001, 1e-7 */
export function fmtTick(v: number): string {
  if (!Number.isFinite(v)) return '';
  if (v === 0) return '0';
  const a = Math.abs(v);
  if (a >= 1e6 || a < 1e-4) return v.toExponential(1).replace('e+', 'e').replace('.0e', 'e');
  if (a >= 1000) return (v / 1000).toFixed(a >= 10000 ? 0 : 1).replace(/\.0$/, '') + 'K';
  const digits = a >= 100 ? 0 : a >= 10 ? 1 : a >= 1 ? 2 : 4;
  return parseFloat(v.toFixed(digits)).toString();
}

export function fmtMs(ms: number | null | undefined): string {
  if (ms === null || ms === undefined) return '';
  if (ms < 1) return `${(ms * 1000).toFixed(0)} µs`;
  if (ms < 1000) return `${ms.toFixed(1)} ms`;
  return `${(ms / 1000).toFixed(2)} s`;
}

export function fmtInt(v: number): string {
  return Math.round(v).toLocaleString();
}

/** Unicode superscripts for polynomial display. */
export function superscript(n: number): string {
  const map: Record<string, string> = { '0': '⁰', '1': '¹', '2': '²', '3': '³', '4': '⁴', '5': '⁵', '6': '⁶', '7': '⁷', '8': '⁸', '9': '⁹', '-': '⁻' };
  return String(n)
    .split('')
    .map((c) => map[c] ?? c)
    .join('');
}

/** Parse a free-text list of numbers (commas, spaces, newlines, semicolons). */
export function parseNumberList(text: string): number[] {
  return text
    .split(/[\s,;]+/)
    .map((t) => t.trim())
    .filter(Boolean)
    .map((t) => {
      const v = Number(t);
      if (!Number.isFinite(v)) throw new Error(`"${t}" is not a number`);
      return v;
    });
}

/** Accept "pi", "2pi", "-inf", "1e-3" etc. for numeric fields. Returns null for empty. */
export function parseNumberInput(text: string): number | null {
  const t = text.trim();
  if (!t) return null;
  const lower = t.toLowerCase().replace(/\s+/g, '');
  if (lower === 'inf' || lower === '+inf' || lower === 'infinity') return Infinity;
  if (lower === '-inf' || lower === '-infinity') return -Infinity;
  const n = Number(t);
  if (Number.isFinite(n)) return n;
  const m = lower.match(/^(-?)(\d*\.?\d*)\*?(pi|e|tau|phi)(?:\/(\d+\.?\d*))?$/);
  if (m) {
    const consts: Record<string, number> = { pi: Math.PI, e: Math.E, tau: 2 * Math.PI, phi: (1 + Math.sqrt(5)) / 2 };
    const coeff = m[2] === '' ? 1 : Number(m[2]);
    const div = m[4] ? Number(m[4]) : 1;
    const v = ((m[1] === '-' ? -1 : 1) * coeff * consts[m[3]]) / div;
    if (Number.isFinite(v)) return v;
  }
  return NaN;
}

export function clamp(v: number, lo: number, hi: number) {
  return Math.min(hi, Math.max(lo, v));
}
