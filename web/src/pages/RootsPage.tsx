import { useEffect, useMemo, useState } from 'react';
import { useLocation } from 'react-router-dom';
import { IconPlay } from '../components/Icons';
import { Plot, type Marker, type Region } from '../components/Plot';
import { Card, ConvergenceBadge, DataTable, ErrorBanner, Examples, ExpressionField, LoadingOverlay, NumberField, PageHeader, Segmented, SelectField, Stat, numberOf, useAutoRun } from '../components/ui';
import { useCompute } from '../engine/useEngine';
import { fmt, fmtMs } from '../lib/format';
import { pushHistory } from '../lib/storage';
import { useUrlState } from '../lib/urlState';

const METHODS = [
  { value: 'brent', label: 'Brent (bracketing, recommended)', bracket: true, info: 'Inverse quadratic interpolation with a bisection safeguard. Superlinear and guaranteed.' },
  { value: 'bisection', label: 'Bisection', bracket: true, info: 'Halves the bracket each step. Slow but bulletproof: linear convergence.' },
  { value: 'regula_falsi', label: 'Regula falsi (Illinois)', bracket: true, info: 'Secant inside a bracket; the Illinois tweak avoids one-sided stalls.' },
  { value: 'ridders', label: 'Ridders', bracket: true, info: 'Exponential correction of the midpoint; quadratic convergence with a bracket.' },
  { value: 'newton', label: 'Newton–Raphson', bracket: false, info: 'Uses the symbolic derivative automatically. Quadratic convergence near a root.' },
  { value: 'halley', label: 'Halley', bracket: false, info: 'Uses first and second derivatives; cubic convergence.' },
  { value: 'secant', label: 'Secant', bracket: false, info: 'Newton with a finite-difference slope from the last two points.' },
  { value: 'steffensen', label: 'Steffensen', bracket: false, info: 'Derivative-free quadratic convergence using f(x + f(x)).' },
  { value: 'fixed_point', label: 'Fixed point x = g(x)', bracket: false, info: 'Iterates x ← g(x). Enter g, not f: converges when |g′| < 1.' },
  { value: 'linear_search', label: 'Linear step search', bracket: true, info: 'Walks the interval with a fixed step until the sign changes, then refines.' },
  { value: 'variable_step', label: 'Variable step search', bracket: true, info: 'Doubles the step while the sign holds, halves on a change.' },
];

const DEFAULTS = { f: 'x^3 - x - 2', method: 'brent', a: '1', b: '2', x0: '1.5', x1: '', tol: '1e-12', maxIter: 100, mode: 'single', samples: 1000 };

interface Step {
  iteration: number;
  a: number;
  b: number;
  x: number;
  fx: number | null;
  error: number | null;
}
interface RootResult {
  method: string;
  root: number | null;
  fRoot: number | null;
  iterations: number;
  evaluations: number;
  converged: boolean;
  message: string;
  history: Step[];
  expression: string;
  var: string;
  derivative?: string;
  secondDerivative?: string;
  derivativeNote?: string;
}
interface Table {
  x: number[];
  y: (number | null)[];
}

export default function RootsPage() {
  const [s, set, fromUrl] = useUrlState(DEFAULTS);
  const location = useLocation();
  const method = METHODS.find((m) => m.value === s.method) ?? METHODS[0];
  const single = useCompute<RootResult>();
  const compare = useCompute<{ results: RootResult[] }>();
  const all = useCompute<{ roots: { root: number; fRoot: number; iterations: number; converged: boolean }[]; count: number }>();
  const curve = useCompute<Table>();
  const [step, setStep] = useState<number | null>(null);

  const bracket = method.bracket;
  const a = numberOf(s.a);
  const b = numberOf(s.b);
  const x0 = numberOf(s.x0);

  const plotRange = (): [number, number] => {
    if (s.mode === 'all' || bracket) return [Math.min(a, b), Math.max(a, b)];
    const w = Math.max(2, Math.abs(x0) * 0.5);
    return [x0 - w, x0 + w];
  };

  const run = async () => {
    const [lo, hi] = plotRange();
    const params: Record<string, unknown> = { f: s.f, tol: numberOf(s.tol, 1e-12), maxIter: s.maxIter };
    if (bracket || s.mode !== 'single') Object.assign(params, { a, b });
    if (!bracket || s.mode === 'compare') Object.assign(params, { x0 });
    if (s.x1) params.x1 = numberOf(s.x1);
    setStep(null);
    void curve.run({ op: 'expression.table', params: { expr: s.f, from: lo, to: hi, samples: 800 } });
    let ok = false;
    if (s.mode === 'single') ok = !!(await single.run({ op: 'roots.find', params: { ...params, method: s.method } }));
    else if (s.mode === 'compare') ok = !!(await compare.run({ op: 'roots.compare', params }));
    else ok = !!(await all.run({ op: 'roots.all', params: { f: s.f, a, b, samples: s.samples, tol: numberOf(s.tol, 1e-12) } }));
    if (ok) pushHistory({ page: '/roots', title: `Roots of ${s.f} (${s.mode === 'single' ? method.label.split(' ')[0] : s.mode})`, search: location.search });
  };
  useAutoRun(fromUrl, () => void run());
  useEffect(() => {
    if (!fromUrl) void run();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const r = single.data;
  const curveSeries = curve.data ? [{ id: 'f', label: r?.expression ?? s.f, x: curve.data.x, y: curve.data.y }] : [];
  const history = r?.history ?? [];
  const shown = step === null ? history.length - 1 : step;
  const current = history[shown];
  const markers: Marker[] = [];
  const regions: Region[] = [];
  if (s.mode === 'single' && r) {
    if (current) {
      if (bracket && Number.isFinite(current.a) && Number.isFinite(current.b)) regions.push({ from: current.a, to: current.b, label: 'bracket' });
      if (Number.isFinite(current.x) && current.fx !== null) markers.push({ x: current.x, y: current.fx, label: `x${shown + 1}`, color: 'var(--series-2)' });
    }
    if (r.root !== null && r.fRoot !== null && Number.isFinite(r.root)) markers.push({ x: r.root, y: r.fRoot, label: `root ≈ ${fmt(r.root, 8)}`, color: 'var(--series-8)' });
  }
  if (s.mode === 'all' && all.data) for (const rt of all.data.roots) markers.push({ x: rt.root, y: rt.fRoot, label: fmt(rt.root, 6), color: 'var(--series-8)' });

  const errorSeries = useMemo(() => {
    if (!r) return [];
    const xs: number[] = [];
    const ys: (number | null)[] = [];
    for (const h of r.history) {
      xs.push(h.iteration);
      ys.push(h.error !== null && h.error > 0 ? Math.log10(h.error) : null);
    }
    return [{ id: 'err', label: 'log10 |error|', x: xs, y: ys, kind: 'line' as const }];
  }, [r]);

  const loading = single.loading || compare.loading || all.loading;

  return (
    <div className="stack">
      <PageHeader title="Root finding" description="Solve f(x) = 0 with eleven methods and inspect every iteration: the bracket shrinking, the Newton tangent steps, the error decaying." />
      <div className="workspace">
        <div className="controls stack">
          <Card title="Problem">
            <div className="stack" style={{ gap: 10 }}>
              <Segmented value={s.mode} onChange={(v) => set({ mode: v })} label="Mode" options={[{ value: 'single', label: 'One method' }, { value: 'compare', label: 'Compare all' }, { value: 'all', label: 'All roots in [a, b]' }]} />
              <ExpressionField label={s.method === 'fixed_point' && s.mode === 'single' ? 'g(x)  (iterate x = g(x))' : 'f(x)'} value={s.f} onChange={(v) => set({ f: v })} onEnter={run} placeholder="x^3 - x - 2" />
              {s.mode === 'single' && <SelectField label="Method" value={s.method} onChange={(v) => set({ method: v })} options={METHODS.map((m) => ({ value: m.value, label: m.label }))} hint={method.info} />}
              {(bracket || s.mode !== 'single') && (
                <div className="grid-2">
                  <NumberField label="a (interval start)" value={s.a} onChange={(v) => set({ a: v })} onEnter={run} />
                  <NumberField label="b (interval end)" value={s.b} onChange={(v) => set({ b: v })} onEnter={run} />
                </div>
              )}
              {(!bracket || s.mode === 'compare') && s.mode !== 'all' && (
                <div className="grid-2">
                  <NumberField label="x₀ (initial guess)" value={s.x0} onChange={(v) => set({ x0: v })} onEnter={run} />
                  {s.method === 'secant' && <NumberField label="x₁ (second guess)" value={s.x1} onChange={(v) => set({ x1: v })} required={false} hint="defaults to x₀ + 10%" />}
                </div>
              )}
              <div className="grid-2">
                <NumberField label="Tolerance" value={s.tol} onChange={(v) => set({ tol: v })} />
                {s.mode === 'all' ? (
                  <NumberField label="Scan samples" value={String(s.samples)} onChange={(v) => set({ samples: Math.round(numberOf(v, 1000)) })} />
                ) : (
                  <NumberField label="Max iterations" value={String(s.maxIter)} onChange={(v) => set({ maxIter: Math.round(numberOf(v, 100)) })} />
                )}
              </div>
              <button className="btn primary" onClick={run} disabled={loading}>
                <IconPlay /> {s.mode === 'single' ? 'Find root' : s.mode === 'compare' ? 'Compare methods' : 'Find all roots'}
              </button>
              <Examples
                items={[
                  { label: '√2 by Newton', apply: () => set({ f: 'x^2 - 2', method: 'newton', x0: '1', mode: 'single' }) },
                  { label: 'cos x = x', apply: () => set({ f: 'cos(x) - x', method: 'brent', a: '0', b: '1', mode: 'single' }) },
                  { label: 'Kepler: E − 0.5 sin E = 1', apply: () => set({ f: 'x - 0.5 sin(x) - 1', method: 'halley', x0: '1', mode: 'single' }) },
                  { label: 'Fixed point √2', apply: () => set({ f: '(x + 2/x)/2', method: 'fixed_point', x0: '1', mode: 'single' }) },
                  { label: 'All zeros of Bessel-like', apply: () => set({ f: 'sin(x)/x - 0.1', a: '0.1', b: '30', mode: 'all' }) },
                  { label: 'Compare on x e^x = 1', apply: () => set({ f: 'x exp(x) - 1', a: '0', b: '1', x0: '0.5', mode: 'compare' }) },
                ]}
                onPick={() => setTimeout(run, 0)}
              />
            </div>
          </Card>
        </div>
        <div className="stack">
          <ErrorBanner error={single.error ?? compare.error ?? all.error} />
          {s.mode === 'single' && r && (
            <LoadingOverlay loading={loading}>
              <div className="stack">
                <Card
                  title={
                    <>
                      Result <ConvergenceBadge converged={r.converged} message={r.message} />
                    </>
                  }
                  actions={<span className="hint">{fmtMs(single.timeMs)}</span>}
                >
                  <div className="stack" style={{ gap: 10 }}>
                    <div className="hero-number mono">{r.root === null ? '—' : fmt(r.root, 15)}</div>
                    <p className="hint">{r.message}</p>
                    <div className="stat-row">
                      <Stat label="f(root)" value={fmt(r.fRoot, 6)} />
                      <Stat label="Iterations" value={r.iterations} />
                      <Stat label="Function evaluations" value={r.evaluations} />
                      {r.derivative && <Stat label="Derivative used" value={<span style={{ fontSize: '0.9rem' }}>{r.derivative}</span>} />}
                    </div>
                    {r.derivativeNote && <div className="callout warn">{r.derivativeNote}</div>}
                  </div>
                </Card>
                <Card title="Function and iterations">
                  <Plot series={curveSeries} markers={markers} regions={regions} hlines={[{ value: 0, dash: false, color: 'var(--axis)' }]} ariaLabel={`Plot of ${s.f} with the root marked`} height={340} xLabel={r.var} fileName="roots" />
                  {history.length > 1 && (
                    <div className="row" style={{ marginTop: 10 }}>
                      <label htmlFor="iter" className="label">
                        Iteration {shown + 1} / {history.length}
                      </label>
                      <input id="iter" className="range" type="range" min={0} max={history.length - 1} value={shown} onChange={(e) => setStep(Number(e.target.value))} style={{ flex: 1 }} />
                      {current && (
                        <span className="hint mono">
                          x = {fmt(current.x, 10)}, f = {fmt(current.fx, 4)}
                        </span>
                      )}
                    </div>
                  )}
                </Card>
                {history.length > 1 && (
                  <div className="grid-2" style={{ alignItems: 'start' }}>
                    <Card title="Error per iteration">
                      <Plot series={errorSeries} ariaLabel="Error per iteration on a log scale" height={220} xLabel="iteration" yLabel="log10 error" compact />
                    </Card>
                    <Card title="Iteration table">
                      <DataTable
                        columns={[
                          { key: 'iteration', label: '#' },
                          { key: 'a', label: bracket ? 'a' : 'x prev', digits: 10 },
                          { key: 'b', label: bracket ? 'b' : 'x', digits: 10 },
                          { key: 'x', label: 'estimate', digits: 12 },
                          { key: 'fx', label: 'f(x)', digits: 4 },
                          { key: 'error', label: 'error', digits: 3 },
                        ]}
                        rows={history as unknown as Record<string, unknown>[]}
                        fileName="iterations"
                      />
                    </Card>
                  </div>
                )}
              </div>
            </LoadingOverlay>
          )}
          {s.mode === 'compare' && compare.data && (
            <Card title="Method comparison" actions={<span className="hint">{fmtMs(compare.timeMs)}</span>}>
              <DataTable
                columns={[
                  { key: 'method', label: 'method' },
                  { key: 'root', label: 'root', digits: 15 },
                  { key: 'fRoot', label: 'f(root)', digits: 3 },
                  { key: 'iterations', label: 'iterations' },
                  { key: 'evaluations', label: 'evaluations' },
                  { key: 'converged', label: 'status', render: (row) => <ConvergenceBadge converged={!!row.converged} message={String(row.message)} /> },
                ]}
                rows={compare.data.results as unknown as Record<string, unknown>[]}
                fileName="compare"
              />
              <div style={{ marginTop: 12 }}>
                <Plot series={curveSeries} markers={compare.data.results.filter((q) => q.converged && q.root !== null).map((q) => ({ x: q.root as number, y: q.fRoot ?? 0 }))} hlines={[{ value: 0, dash: false, color: 'var(--axis)' }]} ariaLabel="Function with the roots found by each method" height={260} compact />
              </div>
            </Card>
          )}
          {s.mode === 'all' && all.data && (
            <Card title={`${all.data.count} root${all.data.count === 1 ? '' : 's'} found`} actions={<span className="hint">{fmtMs(all.timeMs)}</span>}>
              <Plot series={curveSeries} markers={markers} hlines={[{ value: 0, dash: false, color: 'var(--axis)' }]} ariaLabel="Function with all roots marked" height={320} fileName="all-roots" />
              <div style={{ marginTop: 12 }}>
                <DataTable columns={[{ key: 'root', label: 'root', digits: 15 }, { key: 'fRoot', label: 'f(root)', digits: 3 }, { key: 'iterations', label: 'refinement iterations' }]} rows={all.data.roots as unknown as Record<string, unknown>[]} fileName="roots" />
              </div>
            </Card>
          )}
          {!r && !compare.data && !all.data && !loading && (
            <Card subtle>
              <p className="hint">Enter a function and press Find root. Bracketing methods need f(a) and f(b) with opposite signs; open methods start from x₀.</p>
            </Card>
          )}
        </div>
      </div>
    </div>
  );
}
