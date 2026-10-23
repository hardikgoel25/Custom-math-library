import { useEffect, useMemo } from 'react';
import { useLocation } from 'react-router-dom';
import { BarChart } from '../components/BarChart';
import { IconPlay } from '../components/Icons';
import { Plot, type Series } from '../components/Plot';
import { Card, DataTable, ErrorBanner, Examples, LoadingOverlay, NumberField, PageHeader, SelectField, Stat, TextArea, numberOf, useAutoRun, useToast } from '../components/ui';
import { useCompute } from '../engine/useEngine';
import { fmt, fmtMs } from '../lib/format';
import { pushHistory } from '../lib/storage';
import { useUrlState } from '../lib/urlState';

const METHODS = [
  { value: 'spline', label: 'Natural cubic spline' },
  { value: 'spline_clamped', label: 'Clamped cubic spline' },
  { value: 'lagrange', label: 'Lagrange polynomial' },
  { value: 'newton', label: 'Newton divided differences' },
  { value: 'linear', label: 'Piecewise linear' },
  { value: 'polyfit', label: 'Least-squares polynomial fit' },
];
const DEFAULTS = { pts: '0 1\n1 2.7\n2 7.4\n3 20\n4 54.6', method: 'spline', degree: 2, x: '2.5', d0: '0', dn: '0' };

interface Result {
  method: string;
  count: number;
  polynomial?: { text: string; unicode: string; coefficients: number[]; degree: number };
  coefficients?: number[];
  table?: number[][];
  segments?: { x0: number; x1: number; a: number; b: number; c: number; d: number }[];
  rSquared?: number;
  rmse?: number;
  residuals?: number[];
  x?: number;
  value?: number | null;
  curve: { x: number[]; y: (number | null)[] };
}

function parsePoints(text: string): { xs: number[]; ys: number[] } | string {
  const xs: number[] = [];
  const ys: number[] = [];
  const lines = text.split(/\n|;/).map((l) => l.trim()).filter(Boolean);
  for (const line of lines) {
    const parts = line.split(/[\s,]+/).filter(Boolean).map(Number);
    if (parts.length !== 2 || parts.some((v) => !Number.isFinite(v))) return `cannot read "${line}" as "x y"`;
    xs.push(parts[0]);
    ys.push(parts[1]);
  }
  if (xs.length === 0) return 'enter at least one point';
  return { xs, ys };
}

export default function InterpolatePage() {
  const [s, set, fromUrl] = useUrlState(DEFAULTS);
  const location = useLocation();
  const c = useCompute<Result>();
  const toast = useToast();
  const parsed = useMemo(() => parsePoints(s.pts), [s.pts]);
  const parseError = typeof parsed === 'string' ? parsed : null;

  const run = async () => {
    if (typeof parsed === 'string') return;
    const params: Record<string, unknown> = { xs: parsed.xs, ys: parsed.ys, method: s.method, samples: 500 };
    const xq = numberOf(s.x);
    if (Number.isFinite(xq)) params.x = xq;
    if (s.method === 'polyfit') params.degree = s.degree;
    if (s.method === 'spline_clamped') Object.assign(params, { d0: numberOf(s.d0, 0), dn: numberOf(s.dn, 0) });
    if (await c.run({ op: 'interpolate.compute', params })) pushHistory({ page: '/interpolate', title: `${METHODS.find((m) => m.value === s.method)?.label} through ${parsed.xs.length} points`, search: location.search });
  };
  useAutoRun(fromUrl, () => void run());
  useEffect(() => {
    if (!fromUrl) void run();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const r = c.data;
  const pts = typeof parsed === 'string' ? null : parsed;
  const series: Series[] = [];
  if (r) series.push({ id: 'curve', label: METHODS.find((m) => m.value === r.method)?.label ?? r.method, x: r.curve.x, y: r.curve.y });
  if (pts) series.push({ id: 'pts', label: 'data points', x: pts.xs, y: pts.ys, kind: 'scatter', color: 'var(--series-2)' });
  const markers = r && r.x !== undefined && r.value !== null && r.value !== undefined ? [{ x: r.x, y: r.value, label: `${fmt(r.value, 6)} at ${fmt(r.x, 6)}`, color: 'var(--series-8)' }] : [];

  const addPoint = (x: number, y: number) => {
    set({ pts: s.pts.trim() + `\n${fmt(x, 4)} ${fmt(y, 4)}` });
    toast(`Added point (${fmt(x, 4)}, ${fmt(y, 4)}) — press Compute`);
  };

  return (
    <div className="stack">
      <PageHeader title="Interpolation & fitting" description="Pass a curve through points with Lagrange or Newton polynomials and cubic splines, or fit a least-squares polynomial. Click on the plot to add points." />
      <div className="workspace">
        <div className="controls stack">
          <Card title="Data">
            <div className="stack" style={{ gap: 10 }}>
              <TextArea label="Points (x y per line)" value={s.pts} onChange={(v) => set({ pts: v })} rows={7} error={parseError} placeholder={'0 1\n1 2\n2 5'} />
              <SelectField label="Method" value={s.method} onChange={(v) => set({ method: v })} options={METHODS} />
              {s.method === 'polyfit' && <NumberField label="Degree" value={String(s.degree)} onChange={(v) => set({ degree: Math.max(0, Math.min(20, Math.round(numberOf(v, 2)))) })} />}
              {s.method === 'spline_clamped' && (
                <div className="grid-2">
                  <NumberField label="Slope at start" value={s.d0} onChange={(v) => set({ d0: v })} />
                  <NumberField label="Slope at end" value={s.dn} onChange={(v) => set({ dn: v })} />
                </div>
              )}
              <NumberField label="Evaluate at x" value={s.x} onChange={(v) => set({ x: v })} required={false} onEnter={run} />
              <button className="btn primary" onClick={run} disabled={c.loading || !!parseError}>
                <IconPlay /> Compute
              </button>
              <Examples
                items={[
                  { label: 'Exponential-ish data', apply: () => set({ pts: DEFAULTS.pts, method: 'spline' }) },
                  { label: 'Runge phenomenon', apply: () => set({ pts: Array.from({ length: 11 }, (_, i) => { const x = -5 + i; return `${x} ${(1 / (1 + x * x)).toFixed(5)}`; }).join('\n'), method: 'lagrange' }) },
                  { label: 'Noisy line, fit degree 1', apply: () => set({ pts: '1 2.1\n2 3.9\n3 6.2\n4 7.8\n5 10.1\n6 11.8', method: 'polyfit', degree: 1 }) },
                  { label: 'Sine samples, spline', apply: () => set({ pts: Array.from({ length: 9 }, (_, i) => `${i} ${Math.sin(i).toFixed(4)}`).join('\n'), method: 'spline' }) },
                ]}
                onPick={() => setTimeout(run, 0)}
              />
            </div>
          </Card>
        </div>
        <div className="stack">
          <ErrorBanner error={c.error} />
          <LoadingOverlay loading={c.loading}>
            <div className="stack">
              <Card title="Curve" actions={<span className="hint">{fmtMs(c.timeMs)}</span>}>
                <Plot series={series} markers={markers} ariaLabel="Interpolated curve through the data points" height={360} onClick={addPoint} fileName="interpolation" />
                <p className="hint" style={{ marginTop: 6 }}>
                  Click anywhere on the plot to add a point.
                </p>
              </Card>
              {r && (
                <>
                  {(r.polynomial || r.value !== undefined || r.rSquared !== undefined) && (
                    <div className="stat-row">
                      {r.polynomial && <Stat label="Polynomial" value={<span style={{ fontSize: '0.95rem' }}>{r.polynomial.unicode}</span>} sub={`degree ${r.polynomial.degree}`} />}
                      {r.value !== undefined && r.value !== null && <Stat label={`value at x = ${fmt(r.x, 6)}`} value={fmt(r.value, 12)} />}
                      {r.rSquared !== undefined && <Stat label="R²" value={fmt(r.rSquared, 8)} sub={`RMSE ${fmt(r.rmse, 4)}`} />}
                    </div>
                  )}
                  {r.residuals && (
                    <Card title="Residuals">
                      <BarChart bars={r.residuals.map((v, i) => ({ label: fmt(pts?.xs[i], 4), value: v }))} ariaLabel="Residual of each data point" height={200} color="var(--series-2)" />
                    </Card>
                  )}
                  {r.table && (
                    <Card title="Divided-difference table">
                      <div className="table-wrap">
                        <table className="data">
                          <thead>
                            <tr>
                              <th>x</th>
                              {r.table.map((_, k) => (
                                <th key={k}>f[{k === 0 ? 'x' : `x…x+${k}`}]</th>
                              ))}
                            </tr>
                          </thead>
                          <tbody>
                            {pts?.xs.map((x, i) => (
                              <tr key={i}>
                                <td className="mono">{fmt(x, 6)}</td>
                                {r.table!.map((col, k) => (
                                  <td key={k} className="mono">
                                    {i < col.length ? fmt(col[i], 8) : ''}
                                  </td>
                                ))}
                              </tr>
                            ))}
                          </tbody>
                        </table>
                      </div>
                      <p className="hint" style={{ marginTop: 6 }}>
                        Newton coefficients are the first entry of each column: {r.coefficients?.map((v) => fmt(v, 6)).join(', ')}
                      </p>
                    </Card>
                  )}
                  {r.segments && (
                    <Card title="Spline segments  S(x) = a + b(x−x₀) + c(x−x₀)² + d(x−x₀)³">
                      <DataTable columns={[{ key: 'x0', label: 'x₀', digits: 6 }, { key: 'x1', label: 'x₁', digits: 6 }, { key: 'a', label: 'a', digits: 8 }, { key: 'b', label: 'b', digits: 8 }, { key: 'c', label: 'c', digits: 8 }, { key: 'd', label: 'd', digits: 8 }]} rows={r.segments as unknown as Record<string, unknown>[]} fileName="spline" />
                    </Card>
                  )}
                </>
              )}
            </div>
          </LoadingOverlay>
        </div>
      </div>
    </div>
  );
}
