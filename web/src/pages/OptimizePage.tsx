import { useEffect, useState } from 'react';
import { useLocation } from 'react-router-dom';
import { Heatmap } from '../components/Heatmap';
import { IconPlay } from '../components/Icons';
import { Plot, type Marker, type Region } from '../components/Plot';
import { Card, Checkbox, ConvergenceBadge, DataTable, ErrorBanner, Examples, ExpressionField, LoadingOverlay, NumberField, PageHeader, SelectField, Stat, Tabs, TextField, numberOf, useAutoRun } from '../components/ui';
import { useCompute } from '../engine/useEngine';
import { fmt, fmtMs, parseNumberList } from '../lib/format';
import { pushHistory } from '../lib/storage';
import { useUrlState } from '../lib/urlState';

const METHODS_1D = [
  { value: 'brent', label: 'Brent (parabolic + golden)', bracket: true },
  { value: 'golden', label: 'Golden section', bracket: true },
  { value: 'fibonacci', label: 'Fibonacci search', bracket: true },
  { value: 'binary', label: 'Binary search on f′', bracket: true },
  { value: 'newton', label: 'Newton (uses f′, f″)', bracket: false },
  { value: 'linear_search', label: 'Linear step search', bracket: true },
  { value: 'variable_step', label: 'Variable step search', bracket: true },
];

const DEFAULTS = { tab: '1d', f: 'x^2 + 4cos(x)', method: 'brent', a: '0', b: '4', x0: '1', tol: '1e-8', maxIter: 500, maximize: false, fnd: '(1-x)^2 + 100(y-x^2)^2', nd: 'nelder_mead', x0nd: '-1.2, 1', step: '1', lr: '0.01' };

interface Min1D {
  x: number | null;
  fx: number | null;
  iterations: number;
  evaluations: number;
  converged: boolean;
  message: string;
  history: { iteration: number; a: number; b: number; x: number; fx: number | null }[];
  expression: string;
  var: string;
  derivative?: string;
}
interface MinND {
  x: number[];
  fx: number | null;
  iterations: number;
  evaluations: number;
  converged: boolean;
  message: string;
  path: number[][];
  pathFx: (number | null)[];
  vars: string[];
}
interface Surface {
  x: number[];
  y: number[];
  z: (number | null)[][];
}

export default function OptimizePage() {
  const [s, set, fromUrl] = useUrlState(DEFAULTS);
  const location = useLocation();
  const one = useCompute<Min1D>();
  const curve = useCompute<{ x: number[]; y: (number | null)[] }>();
  const nd = useCompute<MinND>();
  const surf = useCompute<Surface>();
  const [step, setStep] = useState<number | null>(null);
  const method = METHODS_1D.find((m) => m.value === s.method) ?? METHODS_1D[0];

  const run1d = async () => {
    const a = numberOf(s.a);
    const b = numberOf(s.b);
    const x0 = numberOf(s.x0);
    const params: Record<string, unknown> = { f: s.f, method: s.method, tol: numberOf(s.tol, 1e-8), maxIter: s.maxIter, maximize: s.maximize };
    if (method.bracket) Object.assign(params, { a, b });
    else params.x0 = x0;
    const [lo, hi] = method.bracket ? [Math.min(a, b), Math.max(a, b)] : [x0 - 3, x0 + 3];
    setStep(null);
    void curve.run({ op: 'expression.table', params: { expr: s.f, from: lo, to: hi, samples: 600 } });
    if (await one.run({ op: 'optimize.minimize', params })) pushHistory({ page: '/optimize', title: `${s.maximize ? 'Maximise' : 'Minimise'} ${s.f}`, search: location.search });
  };
  const runNd = async () => {
    let x0: number[];
    try {
      x0 = parseNumberList(s.x0nd);
    } catch {
      return;
    }
    const params: Record<string, unknown> = { f: s.fnd, method: s.nd, x0, tol: numberOf(s.tol, 1e-8), maxIter: Math.max(s.maxIter, 2000), maximize: s.maximize };
    if (s.nd === 'nelder_mead') params.step = numberOf(s.step, 1);
    else params.learningRate = numberOf(s.lr, 0.01);
    const r = await nd.run({ op: 'optimize.minimizeNd', params });
    if (r && r.vars.length === 2) {
      const xs = r.path.map((p) => p[0]).concat(x0[0], r.x[0]);
      const ys = r.path.map((p) => p[1]).concat(x0[1], r.x[1]);
      const pad = (v: number[]) => {
        const lo = Math.min(...v);
        const hi = Math.max(...v);
        const w = Math.max(hi - lo, 1) * 0.3;
        return [lo - w, hi + w];
      };
      const [xf, xt] = pad(xs);
      const [yf, yt] = pad(ys);
      void surf.run({ op: 'optimize.surface', params: { f: s.fnd, vars: r.vars, xFrom: xf, xTo: xt, yFrom: yf, yTo: yt, xSamples: 80, ySamples: 80 } });
    }
    if (r) pushHistory({ page: '/optimize', title: `${s.nd === 'nelder_mead' ? 'Nelder–Mead' : 'Gradient descent'} on ${s.fnd}`, search: location.search });
  };
  const run = () => (s.tab === '1d' ? run1d() : runNd());
  useAutoRun(fromUrl, () => void run());
  useEffect(() => {
    if (!fromUrl) void run();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const r = one.data;
  const history = r?.history ?? [];
  const shown = step === null ? history.length - 1 : step;
  const cur = history[shown];
  const markers: Marker[] = [];
  const regions: Region[] = [];
  if (r && r.x !== null && r.fx !== null) markers.push({ x: r.x, y: r.fx, label: `${s.maximize ? 'max' : 'min'} at ${fmt(r.x, 8)}`, color: 'var(--series-8)' });
  if (cur) {
    if (method.bracket && Number.isFinite(cur.a) && Number.isFinite(cur.b)) regions.push({ from: cur.a, to: cur.b });
    if (cur.fx !== null) markers.push({ x: cur.x, y: cur.fx, color: 'var(--series-2)', shape: 'diamond' });
  }
  const ndr = nd.data;

  return (
    <div className="stack">
      <PageHeader title="Optimisation" description="Minimise (or maximise) a function of one variable with bracketing and derivative methods, or explore a 2-D surface with Nelder–Mead and gradient descent." />
      <Tabs value={s.tab} onChange={(v) => set({ tab: v })} tabs={[{ value: '1d', label: 'One variable' }, { value: 'nd', label: 'Several variables' }]} />
      <div className="workspace">
        <div className="controls stack">
          <Card title={s.tab === '1d' ? 'Problem' : 'Problem (x, y, …)'}>
            <div className="stack" style={{ gap: 10 }}>
              {s.tab === '1d' ? (
                <>
                  <ExpressionField label="f(x)" value={s.f} onChange={(v) => set({ f: v })} onEnter={run} />
                  <SelectField label="Method" value={s.method} onChange={(v) => set({ method: v })} options={METHODS_1D.map((m) => ({ value: m.value, label: m.label }))} />
                  {method.bracket ? (
                    <div className="grid-2">
                      <NumberField label="a" value={s.a} onChange={(v) => set({ a: v })} onEnter={run} />
                      <NumberField label="b" value={s.b} onChange={(v) => set({ b: v })} onEnter={run} />
                    </div>
                  ) : (
                    <NumberField label="x₀" value={s.x0} onChange={(v) => set({ x0: v })} onEnter={run} />
                  )}
                </>
              ) : (
                <>
                  <ExpressionField label="f(x, y, …)" value={s.fnd} onChange={(v) => set({ fnd: v })} onEnter={run} hint="variables are detected automatically" />
                  <SelectField label="Method" value={s.nd} onChange={(v) => set({ nd: v })} options={[{ value: 'nelder_mead', label: 'Nelder–Mead simplex (derivative-free)' }, { value: 'gradient_descent', label: 'Gradient descent (numeric gradient, Armijo line search)' }]} />
                  <TextField label="Starting point" value={s.x0nd} onChange={(v) => set({ x0nd: v })} mono hint="one value per variable, e.g. -1.2, 1" onEnter={run} />
                  {s.nd === 'nelder_mead' ? <NumberField label="Initial simplex step" value={s.step} onChange={(v) => set({ step: v })} /> : <NumberField label="Learning rate" value={s.lr} onChange={(v) => set({ lr: v })} />}
                </>
              )}
              <div className="grid-2">
                <NumberField label="Tolerance" value={s.tol} onChange={(v) => set({ tol: v })} />
                <NumberField label="Max iterations" value={String(s.maxIter)} onChange={(v) => set({ maxIter: Math.round(numberOf(v, 500)) })} />
              </div>
              <Checkbox label="Maximise instead" checked={s.maximize} onChange={(v) => set({ maximize: v })} />
              <button className="btn primary" onClick={run} disabled={one.loading || nd.loading}>
                <IconPlay /> {s.maximize ? 'Maximise' : 'Minimise'}
              </button>
              <Examples
                items={
                  s.tab === '1d'
                    ? [
                        { label: 'x² + 4cos x', apply: () => set({ f: 'x^2 + 4cos(x)', method: 'brent', a: '0', b: '4' }) },
                        { label: 'Multimodal', apply: () => set({ f: 'sin(x) + sin(10x/3)', method: 'golden', a: '2.7', b: '7.5' }) },
                        { label: 'Newton from 1', apply: () => set({ f: 'x^4 - 3x^3 + 2', method: 'newton', x0: '1' }) },
                        { label: 'Maximise x e^-x', apply: () => set({ f: 'x exp(-x)', method: 'brent', a: '0', b: '5', maximize: true }) },
                      ]
                    : [
                        { label: 'Rosenbrock', apply: () => set({ fnd: '(1-x)^2 + 100(y-x^2)^2', nd: 'nelder_mead', x0nd: '-1.2, 1' }) },
                        { label: 'Himmelblau', apply: () => set({ fnd: '(x^2 + y - 11)^2 + (x + y^2 - 7)^2', nd: 'nelder_mead', x0nd: '0, 0' }) },
                        { label: 'Bowl by gradient descent', apply: () => set({ fnd: '(x-3)^2 + 2(y+1)^2 + x y/4', nd: 'gradient_descent', x0nd: '0, 0', lr: '0.1' }) },
                        { label: '3 variables', apply: () => set({ fnd: '(a-1)^2 + (b-2)^2 + (c+1)^2 + a b c', nd: 'nelder_mead', x0nd: '0, 0, 0' }) },
                      ]
                }
                onPick={() => setTimeout(run, 0)}
              />
            </div>
          </Card>
        </div>
        <div className="stack">
          <ErrorBanner error={s.tab === '1d' ? one.error : nd.error} />
          {s.tab === '1d' && r && (
            <LoadingOverlay loading={one.loading}>
              <div className="stack">
                <Card
                  title={
                    <>
                      Result <ConvergenceBadge converged={r.converged} message={r.message} />
                    </>
                  }
                  actions={<span className="hint">{fmtMs(one.timeMs)}</span>}
                >
                  <div className="stat-row">
                    <Stat label={`${r.var} at ${s.maximize ? 'maximum' : 'minimum'}`} value={fmt(r.x, 12)} />
                    <Stat label={`f(${r.var})`} value={fmt(r.fx, 12)} />
                    <Stat label="Iterations" value={r.iterations} />
                    <Stat label="Evaluations" value={r.evaluations} />
                  </div>
                  <p className="hint" style={{ marginTop: 8 }}>
                    {r.message}
                    {r.derivative && <> · f′ = <code>{r.derivative}</code></>}
                  </p>
                </Card>
                <Card title="Function and search">
                  <Plot series={curve.data ? [{ id: 'f', label: r.expression, x: curve.data.x, y: curve.data.y }] : []} markers={markers} regions={regions} ariaLabel={`Plot of ${s.f} with the optimum marked`} height={340} fileName="optimize" />
                  {history.length > 1 && (
                    <div className="row" style={{ marginTop: 10 }}>
                      <label htmlFor="iter" className="label">
                        Iteration {shown + 1} / {history.length}
                      </label>
                      <input id="iter" className="range" type="range" min={0} max={history.length - 1} value={shown} onChange={(e) => setStep(Number(e.target.value))} style={{ flex: 1 }} />
                    </div>
                  )}
                </Card>
                {history.length > 1 && (
                  <Card title="Iteration table">
                    <DataTable columns={[{ key: 'iteration', label: '#' }, { key: 'a', label: 'a', digits: 10 }, { key: 'b', label: 'b', digits: 10 }, { key: 'x', label: 'best x', digits: 12 }, { key: 'fx', label: 'f(x)', digits: 10 }]} rows={history as unknown as Record<string, unknown>[]} fileName="iterations" />
                  </Card>
                )}
              </div>
            </LoadingOverlay>
          )}
          {s.tab === 'nd' && ndr && (
            <LoadingOverlay loading={nd.loading}>
              <div className="stack">
                <Card
                  title={
                    <>
                      Result <ConvergenceBadge converged={ndr.converged} message={ndr.message} />
                    </>
                  }
                  actions={<span className="hint">{fmtMs(nd.timeMs)}</span>}
                >
                  <div className="stat-row">
                    {ndr.vars.map((v, i) => (
                      <Stat key={v} label={v} value={fmt(ndr.x[i], 10)} />
                    ))}
                    <Stat label="f" value={fmt(ndr.fx, 12)} />
                    <Stat label="Iterations" value={ndr.iterations} />
                    <Stat label="Evaluations" value={ndr.evaluations} />
                  </div>
                  <p className="hint" style={{ marginTop: 8 }}>
                    {ndr.message}
                  </p>
                </Card>
                {ndr.vars.length === 2 && surf.data && (
                  <Card title="Surface and search path">
                    <Heatmap x={surf.data.x} y={surf.data.y} z={surf.data.z} path={ndr.path.map((p) => [p[0], p[1]] as [number, number])} markers={[{ x: ndr.x[0], y: ndr.x[1], label: 'optimum' }]} ariaLabel="Heatmap of the function with the optimisation path" xLabel={ndr.vars[0]} yLabel={ndr.vars[1]} height={420} />
                    <p className="hint" style={{ marginTop: 6 }}>
                      Darker is larger. The orange path starts at the first point and ends at the red optimum.
                    </p>
                  </Card>
                )}
                {ndr.pathFx.length > 1 && (
                  <Card title="Objective along the path">
                    <Plot series={[{ id: 'fx', label: 'f', x: ndr.pathFx.map((_, i) => i), y: ndr.pathFx }]} ariaLabel="Objective value per iteration" height={220} xLabel="iteration" compact />
                  </Card>
                )}
              </div>
            </LoadingOverlay>
          )}
        </div>
      </div>
    </div>
  );
}
