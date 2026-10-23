// Scale and tick helpers for the SVG plots.

export interface Scale {
  (v: number): number;
  invert(px: number): number;
  domain: [number, number];
  range: [number, number];
}

export function linearScale(domain: [number, number], range: [number, number]): Scale {
  const [d0, d1] = domain;
  const [r0, r1] = range;
  const k = d1 === d0 ? 0 : (r1 - r0) / (d1 - d0);
  const fn = ((v: number) => r0 + (v - d0) * k) as Scale;
  fn.invert = (px: number) => (k === 0 ? d0 : d0 + (px - r0) / k);
  fn.domain = domain;
  fn.range = range;
  return fn;
}

/** "Nice" tick values covering [min, max] (d3-style). */
export function niceTicks(min: number, max: number, count = 6): number[] {
  if (!Number.isFinite(min) || !Number.isFinite(max)) return [];
  if (min === max) return [min];
  if (min > max) [min, max] = [max, min];
  const span = max - min;
  const step0 = Math.pow(10, Math.floor(Math.log10(span / count)));
  const err = (span / count) / step0;
  const step = step0 * (err >= 7.5 ? 10 : err >= 3.5 ? 5 : err >= 1.5 ? 2 : 1);
  const start = Math.ceil(min / step) * step;
  const ticks: number[] = [];
  for (let v = start; v <= max + step * 1e-9; v += step) ticks.push(parseFloat(v.toFixed(12)));
  return ticks;
}

/** Robust domain for y values: ignores non-finite values and clips extreme outliers (asymptotes). */
export function robustDomain(values: number[], pad = 0.08): [number, number] {
  const finite = values.filter((v) => Number.isFinite(v));
  if (finite.length === 0) return [-1, 1];
  const sorted = [...finite].sort((a, b) => a - b);
  const q = (p: number) => sorted[Math.min(sorted.length - 1, Math.max(0, Math.floor(p * (sorted.length - 1))))];
  const lo = sorted[0];
  const hi = sorted[sorted.length - 1];
  const q05 = q(0.05);
  const q95 = q(0.95);
  const iqr = q95 - q05;
  // If the tails are wildly outside the bulk, clip them (keeps 1/x readable).
  let dmin = lo;
  let dmax = hi;
  if (iqr > 0 && (hi - q95 > 6 * iqr || q05 - lo > 6 * iqr)) {
    dmin = Math.max(lo, q05 - 1.5 * iqr);
    dmax = Math.min(hi, q95 + 1.5 * iqr);
  }
  if (dmin === dmax) {
    const d = Math.abs(dmin) || 1;
    return [dmin - d, dmax + d];
  }
  const p = (dmax - dmin) * pad;
  return [dmin - p, dmax + p];
}

export function extent(values: number[]): [number, number] {
  let lo = Infinity;
  let hi = -Infinity;
  for (const v of values) {
    if (!Number.isFinite(v)) continue;
    if (v < lo) lo = v;
    if (v > hi) hi = v;
  }
  if (lo === Infinity) return [0, 1];
  if (lo === hi) return [lo - 1, hi + 1];
  return [lo, hi];
}
