import { useEffect } from 'react';
import { useLocation } from 'react-router-dom';
import { IconPlay } from '../components/Icons';
import { Plot } from '../components/Plot';
import { Card, CopyButton, DataTable, ErrorBanner, Examples, ExpressionField, LoadingOverlay, NumberField, PageHeader, SelectField, Stat, Tabs, TextField, numberOf, useAutoRun } from '../components/ui';
import { engine } from '../engine/client';
import { useCompute } from '../engine/useEngine';
import { fmt, fmtMs, parseNumberList } from '../lib/format';
import { pushHistory } from '../lib/storage';
import { useUrlState } from '../lib/urlState';

const DEFAULTS = { tab: 'analyze', p: 'x^5 - 3x^3 + x - 1', a: 'x^2 - 1', b: 'x - 1', op: 'divide', roots: '1, 2, 3', x: '1.5' };

interface Poly {
  coefficients: number[];
  degree: number;
  text: string;
  unicode: string;
}
interface RootsResult extends Poly {
  roots: { re: number; im: number; residual: number }[];
  realRoots: number[];
  iterations: number;
  converged: boolean;
}
interface Arith {
  a: Poly;
  b?: Poly;
  result: Poly;
  quotient?: Poly;
  remainder?: Poly;
}

export default function PolynomialsPage() {
  const [s, set, fromUrl] = useUrlState(DEFAULTS);
  const location = useLocation();
  const roots = useCompute<RootsResult>();
  const calc = useCompute<{ derivative: Poly; integral: Poly; value: number | null; derivativeValue: number | null }>();
  const curve = useCompute<{ x: number[]; y: (number | null)[] }>();
  const arith = useCompute<Arith>();
  const built = useCompute<Poly>();

  const runAnalyze = async () => {
    const r = await roots.run({ op: 'polynomial.roots', params: { p: s.p } });
    if (!r) return;
    const [d, i, ev] = await Promise.all([
      engine.execute<{ result: Poly }>({ op: 'polynomial.calculus', params: { p: s.p, fn: 'derivative' } }),
      engine.execute<{ result: Poly }>({ op: 'polynomial.calculus', params: { p: s.p, fn: 'integral' } }),
      engine.execute<{ value: number | null; derivativeValue: number | null }>({ op: 'polynomial.evaluate', params: { p: s.p, x: numberOf(s.x, 0) } }),
    ]);
    if (d.ok && i.ok && ev.ok) {
      // store via a synthetic compute state
      void calc.run({ op: 'polynomial.evaluate', params: { p: s.p, x: numberOf(s.x, 0) } }).then(() => {});
      calcData.current = { derivative: d.result.result, integral: i.result.result, value: ev.result.value, derivativeValue: ev.result.derivativeValue };
    }
    const reals = r.realRoots;
    const lo = reals.length ? Math.min(...reals) : -3;
    const hi = reals.length ? Math.max(...reals) : 3;
    const pad = Math.max(1, (hi - lo) * 0.3);
    void curve.run({ op: 'polynomial.table', params: { p: s.p, from: lo - pad, to: hi + pad, samples: 600 } });
    pushHistory({ page: '/polynomials', title: `Polynomial ${r.text}`, search: location.search });
  };
  const calcData = { current: null as null | { derivative: Poly; integral: Poly; value: number | null; derivativeValue: number | null } };
  const runArith = async () => {
    if (await arith.run({ op: 'polynomial.arithmetic', params: { a: s.a, b: s.b, fn: s.op } })) pushHistory({ page: '/polynomials', title: `(${s.a}) ${s.op} (${s.b})`, search: location.search });
  };
  const runBuild = async () => {
    try {
      await built.run({ op: 'polynomial.fromRoots', params: { roots: parseNumberList(s.roots) } });
    } catch {
      /* invalid list */
    }
  };
  const run = () => (s.tab === 'analyze' ? runAnalyze() : s.tab === 'arith' ? runArith() : runBuild());
  useAutoRun(fromUrl, () => void run());
  useEffect(() => {
    if (!fromUrl) void run();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [s.tab]);

  const r = roots.data;
  const cd = calc.data ? calcData.current ?? null : null;
  const complexSeries = r ? [{ id: 'roots', label: 'roots', x: r.roots.map((z) => z.re), y: r.roots.map((z) => z.im), kind: 'scatter' as const, color: 'var(--series-8)' }] : [];
  const unitCircle = (() => {
    const xs: number[] = [];
    const ys: number[] = [];
    for (let i = 0; i <= 120; i++) {
      xs.push(Math.cos((i / 120) * 2 * Math.PI));
      ys.push(Math.sin((i / 120) * 2 * Math.PI));
    }
    return { id: 'unit', label: 'unit circle', x: xs, y: ys, color: 'var(--muted)', dash: true, emphasis: 'muted' as const };
  })();

  return (
    <div className="stack">
      <PageHeader title="Polynomials" description="Parse, evaluate, differentiate and integrate polynomials, find all real and complex roots with the Aberth–Ehrlich method, and do exact polynomial arithmetic." />
      <Tabs value={s.tab} onChange={(v) => set({ tab: v })} tabs={[{ value: 'analyze', label: 'Analyse & roots' }, { value: 'arith', label: 'Arithmetic' }, { value: 'build', label: 'From roots' }]} />
      <div className="workspace">
        <div className="controls stack">
          <Card title="Input">
            <div className="stack" style={{ gap: 10 }}>
              {s.tab === 'analyze' && (
                <>
                  <ExpressionField label="p(x)" value={s.p} onChange={(v) => set({ p: v })} onEnter={run} hint="e.g. 3x^2 - 2x + 1, x**4 - 1, or 2x³ + x" />
                  <NumberField label="Evaluate at x" value={s.x} onChange={(v) => set({ x: v })} onEnter={run} />
                </>
              )}
              {s.tab === 'arith' && (
                <>
                  <ExpressionField label="a(x)" value={s.a} onChange={(v) => set({ a: v })} onEnter={run} />
                  <SelectField label="Operation" value={s.op} onChange={(v) => set({ op: v })} options={[{ value: 'add', label: 'a + b' }, { value: 'subtract', label: 'a − b' }, { value: 'multiply', label: 'a × b' }, { value: 'divide', label: 'a ÷ b (quotient and remainder)' }, { value: 'modulo', label: 'a mod b' }, { value: 'gcd', label: 'gcd(a, b)' }, { value: 'compose', label: 'a(b(x))' }]} />
                  <ExpressionField label="b(x)" value={s.b} onChange={(v) => set({ b: v })} onEnter={run} />
                </>
              )}
              {s.tab === 'build' && <TextField label="Roots" value={s.roots} onChange={(v) => set({ roots: v })} mono hint="comma separated real roots" onEnter={run} />}
              <button className="btn primary" onClick={run} disabled={roots.loading || arith.loading}>
                <IconPlay /> Compute
              </button>
              <Examples
                items={
                  s.tab === 'analyze'
                    ? [
                        { label: 'x⁵ − 1 (unit circle)', apply: () => set({ p: 'x^5 - 1' }) },
                        { label: 'Wilkinson-ish', apply: () => set({ p: '(x-1)(x-2)(x-3)(x-4)'.length ? 'x^4 - 10x^3 + 35x^2 - 50x + 24' : '' }) },
                        { label: 'Complex pair', apply: () => set({ p: 'x^4 + 2x^2 + 1' }) },
                        { label: 'Repeated root', apply: () => set({ p: 'x^3 - 3x^2 + 3x - 1' }) },
                        { label: 'Degree 10', apply: () => set({ p: 'x^10 - 2x^7 + x^3 - 5x + 1' }) },
                      ]
                    : s.tab === 'arith'
                      ? [
                          { label: '(x²−1)/(x−1)', apply: () => set({ a: 'x^2 - 1', b: 'x - 1', op: 'divide' }) },
                          { label: 'gcd', apply: () => set({ a: 'x^3 - 6x^2 + 11x - 6', b: 'x^2 - 5x + 6', op: 'gcd' }) },
                          { label: 'Compose', apply: () => set({ a: 'x^2 + 1', b: '2x - 3', op: 'compose' }) },
                        ]
                      : [{ label: '1, 2, 3', apply: () => set({ roots: '1, 2, 3' }) }, { label: '±1, ±2', apply: () => set({ roots: '-2, -1, 1, 2' }) }]
                }
                onPick={() => setTimeout(run, 0)}
              />
            </div>
          </Card>
        </div>
        <div className="stack">
          <ErrorBanner error={roots.error ?? arith.error ?? built.error} />
          {s.tab === 'analyze' && r && (
            <LoadingOverlay loading={roots.loading}>
              <div className="stack">
                <Card title="Polynomial" actions={<CopyButton text={r.text} />}>
                  <div className="hero-number" style={{ fontSize: '1.5rem' }}>
                    {r.unicode}
                  </div>
                  <div className="stat-row" style={{ marginTop: 10 }}>
                    <Stat label="Degree" value={r.degree} />
                    <Stat label="Coefficients (low → high)" value={<span style={{ fontSize: '0.85rem' }}>{r.coefficients.map((c) => fmt(c, 6)).join(', ')}</span>} />
                    {cd && <Stat label={`p(${s.x})`} value={fmt(cd.value, 12)} sub={`p′(${s.x}) = ${fmt(cd.derivativeValue, 8)}`} />}
                  </div>
                  {cd && (
                    <div className="grid-2" style={{ marginTop: 10 }}>
                      <Stat label="Derivative" value={<span style={{ fontSize: '0.95rem' }}>{cd.derivative.unicode}</span>} />
                      <Stat label="Antiderivative" value={<span style={{ fontSize: '0.95rem' }}>{cd.integral.unicode} + C</span>} />
                    </div>
                  )}
                </Card>
                <Card title={`${r.roots.length} roots (${r.realRoots.length} real)`} actions={<span className="hint">{r.iterations ? `${r.iterations} Aberth iterations` : 'closed form'} · {fmtMs(roots.timeMs)}</span>}>
                  <DataTable
                    columns={[
                      { key: 're', label: 'real part', digits: 15 },
                      { key: 'im', label: 'imaginary part', digits: 15 },
                      { key: 'residual', label: '|p(z)|', digits: 3 },
                    ]}
                    rows={r.roots as unknown as Record<string, unknown>[]}
                    fileName="roots"
                  />
                </Card>
                <div className="grid-2 stack-sm" style={{ alignItems: 'start' }}>
                  <Card title="Real line">
                    <Plot series={curve.data ? [{ id: 'p', label: r.text, x: curve.data.x, y: curve.data.y }] : []} markers={r.realRoots.map((x) => ({ x, y: 0, label: fmt(x, 5), color: 'var(--series-8)' }))} hlines={[{ value: 0, dash: false, color: 'var(--axis)' }]} ariaLabel="Polynomial plotted with its real roots marked" height={280} compact />
                  </Card>
                  <Card title="Complex plane">
                    <Plot series={[unitCircle, ...complexSeries]} ariaLabel="Roots on the complex plane" height={280} xLabel="Re" yLabel="Im" equalAspect compact />
                  </Card>
                </div>
              </div>
            </LoadingOverlay>
          )}
          {s.tab === 'arith' && arith.data && (
            <Card title="Result" actions={<CopyButton text={arith.data.result.text} />}>
              <div className="hero-number" style={{ fontSize: '1.5rem' }}>
                {arith.data.result.unicode}
              </div>
              {arith.data.remainder && (
                <div className="stat-row" style={{ marginTop: 10 }}>
                  <Stat label="Quotient" value={<span style={{ fontSize: '0.95rem' }}>{arith.data.quotient?.unicode}</span>} />
                  <Stat label="Remainder" value={<span style={{ fontSize: '0.95rem' }}>{arith.data.remainder.unicode}</span>} />
                </div>
              )}
              <p className="hint" style={{ marginTop: 8 }}>
                a = {arith.data.a.unicode}
                {arith.data.b && <> · b = {arith.data.b.unicode}</>}
              </p>
            </Card>
          )}
          {s.tab === 'build' && built.data && (
            <Card title="Polynomial with those roots" actions={<CopyButton text={built.data.text} />}>
              <div className="hero-number" style={{ fontSize: '1.5rem' }}>
                {built.data.unicode}
              </div>
              <p className="hint" style={{ marginTop: 8 }}>
                coefficients (low → high): {built.data.coefficients.map((c) => fmt(c, 8)).join(', ')}
              </p>
            </Card>
          )}
        </div>
      </div>
    </div>
  );
}
