import { useEffect } from 'react';
import { useLocation } from 'react-router-dom';
import { IconPlay } from '../components/Icons';
import { Plot, seriesColor, type Series } from '../components/Plot';
import { Card, ConvergenceBadge, DataTable, ErrorBanner, Examples, ExpressionField, LoadingOverlay, NumberField, PageHeader, Segmented, SelectField, Stat, TextArea, TextField, numberOf, useAutoRun } from '../components/ui';
import { useCompute } from '../engine/useEngine';
import { fmt, fmtMs, parseNumberList } from '../lib/format';
import { pushHistory } from '../lib/storage';
import { useUrlState } from '../lib/urlState';

const METHODS = [
  { value: 'rk45', label: 'Dormand–Prince RK45 (adaptive)' },
  { value: 'rk4', label: 'Classical Runge–Kutta 4' },
  { value: 'heun', label: 'Heun (improved Euler)' },
  { value: 'midpoint', label: 'Midpoint' },
  { value: 'euler', label: 'Euler' },
];
const DEFAULTS = { mode: 'scalar', f: 'y - t^2 + 1', y0: '0.5', eq: '1.1x - 0.4x y|0.1x y - 0.4y', state: 'x, y', y0s: '10, 10', t0: '0', t1: '2', h: '0.1', method: 'rk45', tol: '1e-8' };

interface OdeResult {
  method: string;
  state: string[];
  equations: string[];
  t: number[];
  y: (number | null)[][];
  final: number[];
  steps: number;
  rejected: number;
  evaluations: number;
  success: boolean;
  message: string;
  thinned: boolean;
}

export default function OdePage() {
  const [s, set, fromUrl] = useUrlState(DEFAULTS);
  const location = useLocation();
  const c = useCompute<OdeResult>();
  const system = s.mode === 'system';

  const run = async () => {
    const params: Record<string, unknown> = { t0: numberOf(s.t0), t1: numberOf(s.t1), method: s.method, tol: numberOf(s.tol, 1e-8), h: numberOf(s.h, 0.1), maxPoints: 4000 };
    try {
      if (system) {
        params.equations = s.eq.split('|').map((e) => e.trim()).filter(Boolean);
        params.state = s.state.split(/[,\s]+/).filter(Boolean);
        params.y0 = parseNumberList(s.y0s);
      } else {
        params.f = s.f;
        params.y0 = [numberOf(s.y0)];
      }
    } catch {
      return;
    }
    if (await c.run({ op: 'ode.solve', params })) pushHistory({ page: '/ode', title: system ? `ODE system ${s.state}` : `y' = ${s.f}`, search: location.search });
  };
  useAutoRun(fromUrl, () => void run());
  useEffect(() => {
    if (!fromUrl) void run();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const r = c.data;
  const series: Series[] = r ? r.state.map((name, i) => ({ id: name, label: name, x: r.t, y: r.y[i], color: seriesColor(i) })) : [];
  const phase: Series[] = r && r.state.length >= 2 ? [{ id: 'phase', label: `${r.state[1]} vs ${r.state[0]}`, x: r.y[0].map((v) => v ?? NaN), y: r.y[1], color: seriesColor(2) }] : [];
  const rows = r ? r.t.map((t, i) => Object.fromEntries([['t', t], ...r.state.map((n, k) => [n, r.y[k][i]])])) : [];

  return (
    <div className="stack">
      <PageHeader title="Differential equations" description="Solve initial value problems y′ = f(t, y) for one equation or a system, with fixed-step Runge–Kutta methods or adaptive Dormand–Prince, and see trajectories and phase portraits." />
      <div className="workspace">
        <div className="controls stack">
          <Card title="Problem">
            <div className="stack" style={{ gap: 10 }}>
              <Segmented value={s.mode} onChange={(v) => set({ mode: v })} label="Kind" options={[{ value: 'scalar', label: 'Single equation' }, { value: 'system', label: 'System' }]} />
              {system ? (
                <>
                  <TextField label="State variables" value={s.state} onChange={(v) => set({ state: v })} mono hint="comma separated, e.g. x, y" />
                  <TextArea label="Equations (one derivative per line, in order)" value={s.eq.split('|').join('\n')} onChange={(v) => set({ eq: v.split('\n').join('|') })} rows={4} hint="may use t and every state variable" />
                  <TextField label="Initial values" value={s.y0s} onChange={(v) => set({ y0s: v })} mono />
                </>
              ) : (
                <>
                  <ExpressionField label="dy/dt = f(t, y)" value={s.f} onChange={(v) => set({ f: v })} onEnter={run} />
                  <NumberField label="y(t₀)" value={s.y0} onChange={(v) => set({ y0: v })} onEnter={run} />
                </>
              )}
              <div className="grid-2">
                <NumberField label="t₀" value={s.t0} onChange={(v) => set({ t0: v })} onEnter={run} />
                <NumberField label="t₁" value={s.t1} onChange={(v) => set({ t1: v })} onEnter={run} />
              </div>
              <SelectField label="Method" value={s.method} onChange={(v) => set({ method: v })} options={METHODS} />
              <div className="grid-2">
                {s.method === 'rk45' ? <NumberField label="Tolerance" value={s.tol} onChange={(v) => set({ tol: v })} /> : <NumberField label="Step size h" value={s.h} onChange={(v) => set({ h: v })} />}
                {s.method === 'rk45' && <NumberField label="Initial step (0 = auto)" value={s.h} onChange={(v) => set({ h: v })} required={false} />}
              </div>
              <button className="btn primary" onClick={run} disabled={c.loading}>
                <IconPlay /> Solve
              </button>
              <Examples
                items={[
                  { label: 'Textbook y′ = y − t² + 1', apply: () => set({ mode: 'scalar', f: 'y - t^2 + 1', y0: '0.5', t0: '0', t1: '2' }) },
                  { label: 'Logistic growth', apply: () => set({ mode: 'scalar', f: '0.8 y (1 - y/50)', y0: '2', t0: '0', t1: '15' }) },
                  { label: 'Stiff decay (Euler struggles)', apply: () => set({ mode: 'scalar', f: '-15 y', y0: '1', t0: '0', t1: '1', method: 'euler', h: '0.1' }) },
                  { label: 'Lotka–Volterra', apply: () => set({ mode: 'system', eq: '1.1x - 0.4x y|0.1x y - 0.4y', state: 'x, y', y0s: '10, 10', t0: '0', t1: '60', method: 'rk45' }) },
                  { label: 'Van der Pol', apply: () => set({ mode: 'system', eq: 'v|5(1 - x^2) v - x', state: 'x, v', y0s: '2, 0', t0: '0', t1: '30' }) },
                  { label: 'Lorenz (3 states)', apply: () => set({ mode: 'system', eq: '10(y - x)|x(28 - z) - y|x y - 8z/3', state: 'x, y, z', y0s: '1, 1, 1', t0: '0', t1: '30', tol: '1e-9' }) },
                  { label: 'Pendulum', apply: () => set({ mode: 'system', eq: 'w|-9.81 sin(theta)', state: 'theta, w', y0s: '2.5, 0', t0: '0', t1: '10' }) },
                ]}
                onPick={() => setTimeout(run, 0)}
              />
            </div>
          </Card>
        </div>
        <div className="stack">
          <ErrorBanner error={c.error} />
          {r && (
            <LoadingOverlay loading={c.loading}>
              <div className="stack">
                <Card
                  title={
                    <>
                      Solution <ConvergenceBadge converged={r.success} message={r.message} />
                    </>
                  }
                  actions={<span className="hint">{fmtMs(c.timeMs)}</span>}
                >
                  <div className="stat-row">
                    {r.state.map((n, i) => (
                      <Stat key={n} label={`${n}(t₁)`} value={fmt(r.final[i], 10)} />
                    ))}
                    <Stat label="Steps" value={r.steps} sub={r.rejected ? `${r.rejected} rejected` : undefined} />
                    <Stat label="Evaluations" value={r.evaluations} />
                  </div>
                  {!r.success && <p className="error-text" style={{ marginTop: 8 }}>{r.message}</p>}
                </Card>
                <Card title="Trajectories">
                  <Plot series={series} ariaLabel="Solution components over time" height={340} xLabel="t" fileName="ode" />
                </Card>
                {phase.length > 0 && (
                  <Card title="Phase portrait">
                    <Plot series={phase} ariaLabel={`Phase portrait of ${r.state[1]} against ${r.state[0]}`} height={320} xLabel={r.state[0]} yLabel={r.state[1]} fileName="phase" equalAspect />
                  </Card>
                )}
                <Card title="Values">
                  <DataTable columns={[{ key: 't', label: 't', digits: 8 }, ...r.state.map((n) => ({ key: n, label: n, digits: 10 }))]} rows={rows as Record<string, unknown>[]} fileName="ode" caption={r.thinned ? 'thinned for display' : undefined} />
                </Card>
              </div>
            </LoadingOverlay>
          )}
        </div>
      </div>
    </div>
  );
}
