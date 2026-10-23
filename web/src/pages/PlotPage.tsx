import { useCallback, useEffect, useMemo, useState } from 'react';
import { useLocation } from 'react-router-dom';
import { IconPlay, IconPlus, IconTrash } from '../components/Icons';
import { Plot, seriesColor, type Series } from '../components/Plot';
import { Card, Checkbox, ErrorBanner, Examples, NumberField, PageHeader, Stat, numberOf } from '../components/ui';
import { engine } from '../engine/client';
import type { EngineErrorBody } from '../engine/types';
import { fmt } from '../lib/format';
import { pushHistory } from '../lib/storage';
import { useUrlState } from '../lib/urlState';

const DEFAULTS = { f: 'sin(x)|x^2/8 - 1', from: '-2pi', to: '2pi', samples: 600, deriv: false, at: '1' };

interface TableResult {
  x: number[];
  y: (number | null)[];
  dy?: (number | null)[];
  derivative?: string;
  expression: string;
}

export default function PlotPage() {
  const [s, set] = useUrlState(DEFAULTS);
  const location = useLocation();
  const exprs = useMemo(() => s.f.split('|').map((e) => e.trim()), [s.f]);
  const [results, setResults] = useState<(TableResult | null)[]>([]);
  const [errors, setErrors] = useState<(EngineErrorBody | null)[]>([]);
  const [values, setValues] = useState<(number | null)[]>([]);
  const [loading, setLoading] = useState(false);

  const run = useCallback(async () => {
    const from = numberOf(s.from);
    const to = numberOf(s.to);
    if (!Number.isFinite(from) || !Number.isFinite(to)) return;
    setLoading(true);
    const outs = await Promise.all(
      exprs.map(async (expr) => {
        if (!expr) return { r: null, e: null, v: null };
        try {
          const r = await engine.execute<TableResult>({ op: 'expression.table', params: { expr, from, to, samples: s.samples, derivative: s.deriv } });
          let v: number | null = null;
          const at = numberOf(s.at);
          if (Number.isFinite(at)) {
            const ev = await engine.execute<{ value: number | null }>({ op: 'expression.evaluate', params: { expr, x: at } });
            v = ev.ok ? ev.result.value : null;
          }
          return r.ok ? { r: r.result, e: null, v } : { r: null, e: r.error, v: null };
        } catch (err) {
          return { r: null, e: { code: 'TRANSPORT', message: (err as Error).message }, v: null };
        }
      }),
    );
    setResults(outs.map((o) => o.r));
    setErrors(outs.map((o) => o.e));
    setValues(outs.map((o) => o.v));
    setLoading(false);
    if (outs.some((o) => o.r)) pushHistory({ page: '/plot', title: `Plot ${exprs.filter(Boolean).join(', ')}`, search: location.search });
  }, [exprs, s.from, s.to, s.samples, s.deriv, s.at, location.search]);

  useEffect(() => {
    void run();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const series: Series[] = [];
  results.forEach((r, i) => {
    if (!r) return;
    series.push({ id: 'f' + i, label: r.expression, x: r.x, y: r.y, color: seriesColor(i) });
    if (s.deriv && r.dy) series.push({ id: 'd' + i, label: `d/dx ${r.expression}`, x: r.x, y: r.dy, color: seriesColor(i), dash: true });
  });
  const at = numberOf(s.at);
  const markers = Number.isFinite(at)
    ? values.map((v, i) => (v !== null && Number.isFinite(v) ? { x: at, y: v, color: seriesColor(i), label: `${fmt(v, 6)}` } : null)).filter((m): m is NonNullable<typeof m> => m !== null)
    : [];

  const setExpr = (i: number, v: string) => set({ f: exprs.map((e, j) => (j === i ? v : e)).join('|') });

  return (
    <div className="stack">
      <PageHeader title="Plot & evaluate" description="Plot several functions at once, overlay symbolic derivatives, and read values off the curve. Implicit multiplication (2x, 3sin(x)) and all constants work." />
      <div className="workspace">
        <div className="controls stack">
          <Card title="Functions">
            <div className="stack" style={{ gap: 10 }}>
              {exprs.map((e, i) => (
                <div className="row" key={i} style={{ flexWrap: 'nowrap' }}>
                  <span className="engine-dot" style={{ background: seriesColor(i), flex: 'none' }} />
                  <input
                    className="input mono"
                    aria-label={`function ${i + 1}`}
                    value={e}
                    placeholder="f(x) ="
                    spellCheck={false}
                    onChange={(ev) => setExpr(i, ev.target.value)}
                    onKeyDown={(ev) => ev.key === 'Enter' && run()}
                    aria-invalid={!!errors[i]}
                  />
                  {exprs.length > 1 && (
                    <button className="btn ghost icon" aria-label="remove" onClick={() => set({ f: exprs.filter((_, j) => j !== i).join('|') })}>
                      <IconTrash />
                    </button>
                  )}
                </div>
              ))}
              {exprs.length < 6 && (
                <button className="btn ghost sm" onClick={() => set({ f: [...exprs, ''].join('|') })}>
                  <IconPlus /> Add function
                </button>
              )}
              {errors.map((e, i) => e && <ErrorBanner key={i} error={{ ...e, message: `${exprs[i]}: ${e.message}` }} />)}
              <div className="grid-3">
                <NumberField label="From" value={s.from} onChange={(v) => set({ from: v })} onEnter={run} />
                <NumberField label="To" value={s.to} onChange={(v) => set({ to: v })} onEnter={run} />
                <NumberField label="Samples" value={String(s.samples)} onChange={(v) => set({ samples: Math.max(2, Math.min(20000, Math.round(numberOf(v, 600)))) })} />
              </div>
              <div className="grid-2">
                <NumberField label="Evaluate at x" value={s.at} onChange={(v) => set({ at: v })} onEnter={run} required={false} />
                <div className="field" style={{ justifyContent: 'end' }}>
                  <Checkbox label="Overlay derivative" checked={s.deriv} onChange={(v) => set({ deriv: v })} />
                </div>
              </div>
              <button className="btn primary" onClick={run} disabled={loading}>
                <IconPlay /> Plot
              </button>
              <Examples
                items={[
                  { label: 'sin, cos, tan', apply: () => set({ f: 'sin(x)|cos(x)|tan(x)', from: '-2pi', to: '2pi' }) },
                  { label: 'Gaussian & its derivative', apply: () => set({ f: 'exp(-x^2/2)/sqrt(2pi)', from: '-4', to: '4', deriv: true }) },
                  { label: 'Gamma function', apply: () => set({ f: 'gamma(x)', from: '-4', to: '5', samples: 2000 }) },
                  { label: 'Damped oscillation', apply: () => set({ f: 'exp(-x/5) cos(3x)|exp(-x/5)|-exp(-x/5)', from: '0', to: '20' }) },
                  { label: 'Sinc and Si', apply: () => set({ f: 'sin(x)/x|erf(x)', from: '-15', to: '15' }) },
                ]}
                onPick={() => setTimeout(run, 0)}
              />
            </div>
          </Card>
        </div>
        <div className="stack">
          <Card>
            <Plot series={series} markers={markers} ariaLabel={`Plot of ${exprs.join(', ')}`} height={400} xLabel="x" fileName="plot" />
          </Card>
          {markers.length > 0 && (
            <div className="stat-row">
              {values.map((v, i) => results[i] && <Stat key={i} label={`${results[i]!.expression} at x = ${fmt(at, 6)}`} value={fmt(v, 15)} />)}
            </div>
          )}
          {s.deriv && results.some((r) => r?.derivative) && (
            <Card title="Symbolic derivatives">
              <ul style={{ margin: 0, paddingLeft: 18 }}>
                {results.map((r, i) => r?.derivative && (
                  <li key={i} className="mono">
                    d/dx [{r.expression}] = {r.derivative}
                  </li>
                ))}
              </ul>
            </Card>
          )}
        </div>
      </div>
    </div>
  );
}
