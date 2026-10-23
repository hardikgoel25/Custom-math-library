import { useEffect } from 'react';
import { useLocation } from 'react-router-dom';
import { BarChart } from '../components/BarChart';
import { IconPlay } from '../components/Icons';
import { Plot } from '../components/Plot';
import { Card, ConvergenceBadge, DataTable, ErrorBanner, Examples, ExpressionField, LoadingOverlay, NumberField, PageHeader, Segmented, SelectField, Stat, numberOf, useAutoRun } from '../components/ui';
import { useCompute } from '../engine/useEngine';
import { fmt, fmtMs } from '../lib/format';
import { pushHistory } from '../lib/storage';
import { useUrlState } from '../lib/urlState';

const METHODS = [
  { value: 'auto', label: 'Automatic (adaptive, handles ±∞)' },
  { value: 'adaptive_simpson', label: 'Adaptive Simpson' },
  { value: 'romberg', label: 'Romberg extrapolation' },
  { value: 'gauss_legendre', label: 'Gauss–Legendre' },
  { value: 'simpson', label: 'Simpson 1/3' },
  { value: 'simpson38', label: 'Simpson 3/8' },
  { value: 'boole', label: 'Boole' },
  { value: 'trapezoid', label: 'Trapezoid' },
  { value: 'midpoint', label: 'Midpoint' },
  { value: 'monte_carlo', label: 'Monte Carlo' },
];
const DEFAULTS = { f: 'exp(-x^2)', a: '-inf', b: 'inf', method: 'auto', n: 100, points: 10, tol: '1e-10', samples: 100000, mode: 'single', cmethod: 'simpson' };

interface Integral {
  method: string;
  value: number | null;
  errorEstimate: number | null;
  evaluations: number;
  subdivisions: number;
  converged: boolean;
  message: string;
  table?: number[][];
  expression: string;
  var: string;
  a: number | null;
  b: number | null;
}

export default function IntegratePage() {
  const [s, set, fromUrl] = useUrlState(DEFAULTS);
  const location = useLocation();
  const single = useCompute<Integral>();
  const compare = useCompute<{ reference: number; results: (Integral & { absError: number })[] }>();
  const conv = useCompute<{ reference: number; rows: { n: number; value: number; absError: number }[] }>();
  const curve = useCompute<{ x: number[]; y: (number | null)[] }>();

  const a = numberOf(s.a);
  const b = numberOf(s.b);
  const finite = Number.isFinite(a) && Number.isFinite(b);

  const run = async () => {
    const lo = Number.isFinite(a) ? a : Number.isFinite(b) ? b - 10 : -5;
    const hi = Number.isFinite(b) ? b : Number.isFinite(a) ? a + 10 : 5;
    const pad = finite ? (hi - lo) * 0.15 : 0;
    void curve.run({ op: 'expression.table', params: { expr: s.f, from: lo - pad, to: hi + pad, samples: 800 } });
    const base: Record<string, unknown> = { f: s.f, a: s.a, b: s.b, n: s.n, points: s.points, tol: numberOf(s.tol, 1e-10), samples: s.samples };
    let ok = false;
    if (s.mode === 'single') ok = !!(await single.run({ op: 'integrate.compute', params: { ...base, method: s.method } }));
    else if (s.mode === 'compare') ok = !!(await compare.run({ op: 'integrate.compare', params: base }));
    else ok = !!(await conv.run({ op: 'integrate.convergence', params: { f: s.f, a: s.a, b: s.b, method: s.cmethod } }));
    if (ok) pushHistory({ page: '/integrate', title: `∫ ${s.f} from ${s.a} to ${s.b}`, search: location.search });
  };
  useAutoRun(fromUrl, () => void run());
  useEffect(() => {
    if (!fromUrl) void run();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const r = single.data;
  const series = curve.data ? [{ id: 'f', label: s.f, x: curve.data.x, y: curve.data.y }] : [];
  const areas = finite ? [{ seriesId: 'f', from: a, to: b }] : curve.data ? [{ seriesId: 'f', from: curve.data.x[0], to: curve.data.x[curve.data.x.length - 1] }] : [];
  const needsN = ['simpson', 'simpson38', 'boole', 'trapezoid', 'midpoint'].includes(s.method);

  return (
    <div className="stack">
      <PageHeader title="Integration" description="Definite integrals with Newton–Cotes rules, Romberg extrapolation, Gauss–Legendre quadrature, adaptive Simpson and Monte Carlo. Infinite bounds are handled by substitution." />
      <div className="workspace">
        <div className="controls stack">
          <Card title="Integral">
            <div className="stack" style={{ gap: 10 }}>
              <Segmented value={s.mode} onChange={(v) => set({ mode: v })} label="Mode" options={[{ value: 'single', label: 'Compute' }, { value: 'compare', label: 'Compare rules' }, { value: 'convergence', label: 'Convergence' }]} />
              <ExpressionField label="f(x)" value={s.f} onChange={(v) => set({ f: v })} onEnter={run} />
              <div className="grid-2">
                <NumberField label="Lower bound a" value={s.a} onChange={(v) => set({ a: v })} allowInfinite onEnter={run} hint="inf and -inf allowed" />
                <NumberField label="Upper bound b" value={s.b} onChange={(v) => set({ b: v })} allowInfinite onEnter={run} />
              </div>
              {s.mode === 'single' && <SelectField label="Method" value={s.method} onChange={(v) => set({ method: v })} options={METHODS} />}
              {s.mode === 'convergence' && <SelectField label="Rule" value={s.cmethod} onChange={(v) => set({ cmethod: v })} options={METHODS.filter((m) => ['simpson', 'simpson38', 'boole', 'trapezoid', 'midpoint', 'gauss_legendre'].includes(m.value))} />}
              <div className="grid-2">
                {(needsN || s.mode === 'compare') && <NumberField label="Subintervals n" value={String(s.n)} onChange={(v) => set({ n: Math.round(numberOf(v, 100)) })} />}
                {(s.method === 'gauss_legendre' || s.mode === 'compare') && <NumberField label="Gauss points" value={String(s.points)} onChange={(v) => set({ points: Math.round(numberOf(v, 10)) })} hint="1 – 64" />}
                {(s.method === 'monte_carlo' || s.mode === 'compare') && <NumberField label="Monte Carlo samples" value={String(s.samples)} onChange={(v) => set({ samples: Math.round(numberOf(v, 100000)) })} />}
                {['auto', 'adaptive_simpson', 'romberg'].includes(s.method) && s.mode === 'single' && <NumberField label="Tolerance" value={s.tol} onChange={(v) => set({ tol: v })} />}
              </div>
              <button className="btn primary" onClick={run} disabled={single.loading || compare.loading || conv.loading}>
                <IconPlay /> Integrate
              </button>
              <Examples
                items={[
                  { label: 'Gaussian over ℝ', apply: () => set({ f: 'exp(-x^2)', a: '-inf', b: 'inf', method: 'auto' }) },
                  { label: '∫₀^π sin x', apply: () => set({ f: 'sin(x)', a: '0', b: 'pi', method: 'romberg' }) },
                  { label: 'Singular 1/√x', apply: () => set({ f: '1/sqrt(x)', a: '0', b: '1', method: 'auto' }) },
                  { label: 'Oscillatory', apply: () => set({ f: 'cos(20x) exp(-x)', a: '0', b: '3', method: 'gauss_legendre', points: 40 }) },
                  { label: 'Compare on x³', apply: () => set({ f: 'x^3', a: '0', b: '1', mode: 'compare', n: 10 }) },
                  { label: 'Trapezoid convergence', apply: () => set({ f: 'sin(x)', a: '0', b: '3', mode: 'convergence', cmethod: 'trapezoid' }) },
                ]}
                onPick={() => setTimeout(run, 0)}
              />
            </div>
          </Card>
        </div>
        <div className="stack">
          <ErrorBanner error={single.error ?? compare.error ?? conv.error} />
          {s.mode === 'single' && r && (
            <LoadingOverlay loading={single.loading}>
              <div className="stack">
                <Card
                  title={
                    <>
                      Result <ConvergenceBadge converged={r.converged} message={r.message} />
                    </>
                  }
                  actions={<span className="hint">{fmtMs(single.timeMs)}</span>}
                >
                  <div className="hero-number mono">{fmt(r.value, 15)}</div>
                  <p className="hint">
                    ∫ {r.expression} d{r.var} from {s.a} to {s.b} · {METHODS.find((m) => m.value === r.method)?.label ?? r.method}
                    {r.message && ` · ${r.message}`}
                  </p>
                  <div className="stat-row" style={{ marginTop: 10 }}>
                    <Stat label="Error estimate" value={r.errorEstimate ? fmt(r.errorEstimate, 3) : '—'} />
                    <Stat label="Function evaluations" value={r.evaluations} />
                    <Stat label={r.method === 'romberg' ? 'Levels' : r.method === 'adaptive_simpson' || r.method === 'integrate' ? 'Max depth' : 'Subdivisions'} value={r.subdivisions} />
                  </div>
                </Card>
                <Card title="Area under the curve">
                  <Plot series={series} areas={areas} vlines={finite ? [{ value: a, label: 'a' }, { value: b, label: 'b' }] : []} hlines={[{ value: 0, dash: false, color: 'var(--axis)' }]} ariaLabel={`Plot of ${s.f} with the integrated area shaded`} height={320} fileName="integral" />
                </Card>
                {r.table && (
                  <Card title="Romberg tableau">
                    <div className="table-wrap">
                      <table className="data">
                        <thead>
                          <tr>
                            <th>level</th>
                            {r.table[r.table.length - 1].map((_, j) => (
                              <th key={j}>R(k, {j})</th>
                            ))}
                          </tr>
                        </thead>
                        <tbody>
                          {r.table.map((row, i) => (
                            <tr key={i}>
                              <td>{i}</td>
                              {row.map((v, j) => (
                                <td key={j} className="mono">
                                  {fmt(v, 12)}
                                </td>
                              ))}
                            </tr>
                          ))}
                        </tbody>
                      </table>
                    </div>
                    <p className="hint" style={{ marginTop: 6 }}>
                      Each column extrapolates the previous one with Richardson's formula; the diagonal converges fastest.
                    </p>
                  </Card>
                )}
              </div>
            </LoadingOverlay>
          )}
          {s.mode === 'compare' && compare.data && (
            <Card title="Rule comparison" actions={<span className="hint">{fmtMs(compare.timeMs)}</span>}>
              <p className="hint" style={{ marginBottom: 8 }}>
                Reference value (adaptive, 1e-13): <code>{fmt(compare.data.reference, 15)}</code>
              </p>
              <DataTable
                columns={[
                  { key: 'method', label: 'rule', render: (row) => METHODS.find((m) => m.value === row.method)?.label ?? String(row.method) },
                  { key: 'value', label: 'value', digits: 15 },
                  { key: 'absError', label: 'abs error', digits: 3 },
                  { key: 'evaluations', label: 'evaluations' },
                  { key: 'converged', label: 'status', render: (row) => <ConvergenceBadge converged={!!row.converged} message={String(row.message ?? '')} /> },
                ]}
                rows={compare.data.results as unknown as Record<string, unknown>[]}
                fileName="compare"
              />
              <div style={{ marginTop: 12 }}>
                <BarChart bars={compare.data.results.filter((q) => Number.isFinite(q.absError) && q.absError > 0).map((q) => ({ label: q.method.replace('_', ' '), value: q.absError }))} ariaLabel="Absolute error per rule on a log scale" yLabel="abs error" logY valueFormatter={(v) => fmt(v, 3)} />
              </div>
            </Card>
          )}
          {s.mode === 'convergence' && conv.data && (
            <Card title="Error versus subdivisions" actions={<span className="hint">{fmtMs(conv.timeMs)}</span>}>
              <BarChart bars={conv.data.rows.map((q) => ({ label: String(q.n), value: Math.max(q.absError, 1e-17) }))} ariaLabel="Absolute error versus number of subintervals" xLabel="n" yLabel="abs error" logY valueFormatter={(v) => fmt(v, 3)} />
              <div style={{ marginTop: 12 }}>
                <DataTable columns={[{ key: 'n', label: 'n' }, { key: 'value', label: 'value', digits: 15 }, { key: 'absError', label: 'abs error', digits: 3 }]} rows={conv.data.rows as unknown as Record<string, unknown>[]} fileName="convergence" />
              </div>
            </Card>
          )}
        </div>
      </div>
    </div>
  );
}
