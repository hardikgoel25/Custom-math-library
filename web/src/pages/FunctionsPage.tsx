import { useEffect, useState } from 'react';
import { IconPlay } from '../components/Icons';
import { Plot } from '../components/Plot';
import { Card, DataTable, ErrorBanner, LoadingOverlay, NumberField, PageHeader, SelectField, Stat, Tabs, numberOf, useAutoRun } from '../components/ui';
import { engine } from '../engine/client';
import { useCompute } from '../engine/useEngine';
import { fmt, fmtMs } from '../lib/format';
import { useUrlState } from '../lib/urlState';

const DEFAULTS = { tab: 'evaluate', fn: 'sin', x: '1', from: '-6', to: '6', samples: 400, nt: 'primeFactors', n: '360', k: '3', a: '48', b: '18' };

const FN_GROUPS: { label: string; fns: string[] }[] = [
  { label: 'Trigonometric', fns: ['sin', 'cos', 'tan', 'cot', 'sec', 'csc'] },
  { label: 'Inverse trigonometric', fns: ['asin', 'acos', 'atan', 'acot', 'asec', 'acsc'] },
  { label: 'Hyperbolic', fns: ['sinh', 'cosh', 'tanh', 'coth', 'sech', 'csch', 'asinh', 'acosh', 'atanh', 'acoth', 'asech', 'acsch'] },
  { label: 'Exponential & logarithmic', fns: ['exp', 'exp2', 'expm1', 'ln', 'log2', 'log10', 'log1p', 'sqrt', 'cbrt'] },
  { label: 'Special', fns: ['gamma', 'lgamma', 'digamma', 'erf', 'erfc', 'factorial', 'normcdf', 'normpdf'] },
  { label: 'Rounding', fns: ['abs', 'floor', 'ceil', 'round', 'trunc', 'sign'] },
];
const DOMAINS: Record<string, [string, string]> = { asin: ['-1', '1'], acos: ['-1', '1'], atanh: ['-1', '1'], acosh: ['1', '10'], asech: ['0', '1'], acoth: ['1.01', '10'], ln: ['0.01', '10'], log2: ['0.01', '10'], log10: ['0.01', '10'], sqrt: ['0', '10'], lgamma: ['0.01', '10'], gamma: ['-4', '5'], factorial: ['0', '10'], cosh: ['-5', '5'], sinh: ['-5', '5'], exp: ['-4', '4'], exp2: ['-4', '4'], expm1: ['-4', '4'], erfc: ['-3', '3'], erf: ['-3', '3'], normcdf: ['-4', '4'], normpdf: ['-4', '4'] };

interface Compare {
  fn: string;
  rows: { x: number; cml: number | null; libm: number | null; ulps: number | null }[];
  worstUlps: number;
  worstAt: number;
  meanUlps: number | null;
  hasReference: boolean;
}

export default function FunctionsPage() {
  const [s, set, fromUrl] = useUrlState(DEFAULTS);
  const ev = useCompute<{ value: number | null; text: string }>();
  const curve = useCompute<{ x: number[]; y: (number | null)[] }>();
  const cmp = useCompute<Compare>();
  const nt = useCompute<Record<string, unknown>>();
  const [constants, setConstants] = useState<Record<string, number> | null>(null);

  const run = async () => {
    if (s.tab === 'evaluate') {
      const [lo, hi] = DOMAINS[s.fn] ? DOMAINS[s.fn] : [s.from, s.to];
      void ev.run({ op: 'core.evaluate', params: { fn: s.fn, x: numberOf(s.x) } });
      void curve.run({ op: 'expression.table', params: { expr: `${s.fn === 'ln' ? 'ln' : s.fn === 'factorial' ? 'fact' : s.fn}(x)`, from: numberOf(lo), to: numberOf(hi), samples: s.samples } });
    } else if (s.tab === 'accuracy') {
      const [lo, hi] = DOMAINS[s.fn] ? DOMAINS[s.fn] : [s.from, s.to];
      void cmp.run({ op: 'core.compare', params: { fn: s.fn, from: numberOf(lo), to: numberOf(hi), samples: Math.min(2000, s.samples) } });
    } else if (s.tab === 'number') {
      const params: Record<string, unknown> = { fn: s.nt, n: numberOf(s.n), k: numberOf(s.k), a: numberOf(s.a), b: numberOf(s.b) };
      void nt.run({ op: 'special.numberTheory', params });
    }
  };
  useAutoRun(fromUrl, () => void run());
  useEffect(() => {
    if (!fromUrl) void run();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [s.tab]);
  useEffect(() => {
    if (s.tab === 'constants' && !constants) {
      void engine.execute<Record<string, number>>({ op: 'core.constants' }).then((r) => r.ok && setConstants(r.result));
    }
  }, [s.tab, constants]);

  const fnOptions = FN_GROUPS.flatMap((g) => g.fns.map((f) => ({ value: f, label: `${f}  (${g.label.toLowerCase()})` })));
  const c = cmp.data;

  return (
    <div className="stack">
      <PageHeader title="Functions & accuracy" description="Every elementary and special function is implemented from scratch. Evaluate them, plot them, and measure their error in ULPs against the platform's own math library." />
      <Tabs value={s.tab} onChange={(v) => set({ tab: v })} tabs={[{ value: 'evaluate', label: 'Evaluate & plot' }, { value: 'accuracy', label: 'Accuracy vs libm' }, { value: 'number', label: 'Number theory' }, { value: 'constants', label: 'Constants' }]} />
      <div className="workspace">
        <div className="controls stack">
          <Card title="Input">
            <div className="stack" style={{ gap: 10 }}>
              {(s.tab === 'evaluate' || s.tab === 'accuracy') && <SelectField label="Function" value={s.fn} onChange={(v) => set({ fn: v })} options={fnOptions} />}
              {s.tab === 'evaluate' && <NumberField label="x" value={s.x} onChange={(v) => set({ x: v })} onEnter={run} />}
              {(s.tab === 'evaluate' || s.tab === 'accuracy') && !DOMAINS[s.fn] && (
                <div className="grid-2">
                  <NumberField label="Plot from" value={s.from} onChange={(v) => set({ from: v })} />
                  <NumberField label="to" value={s.to} onChange={(v) => set({ to: v })} />
                </div>
              )}
              {s.tab === 'number' && (
                <>
                  <SelectField label="Tool" value={s.nt} onChange={(v) => set({ nt: v })} options={[{ value: 'primeFactors', label: 'Prime factorisation of n' }, { value: 'isPrime', label: 'Is n prime? (deterministic Miller–Rabin)' }, { value: 'primes', label: 'Primes up to n' }, { value: 'factorial', label: 'n!' }, { value: 'fibonacci', label: 'Fibonacci F(n)' }, { value: 'binomial', label: 'n choose k' }, { value: 'permutations', label: 'n P k' }, { value: 'gcd', label: 'gcd(a, b)' }, { value: 'lcm', label: 'lcm(a, b)' }, { value: 'beta', label: 'Beta(a, b)' }]} />
                  {['primeFactors', 'isPrime', 'primes', 'factorial', 'fibonacci', 'binomial', 'permutations'].includes(s.nt) && <NumberField label="n" value={s.n} onChange={(v) => set({ n: v })} onEnter={run} />}
                  {['binomial', 'permutations'].includes(s.nt) && <NumberField label="k" value={s.k} onChange={(v) => set({ k: v })} onEnter={run} />}
                  {['gcd', 'lcm', 'beta'].includes(s.nt) && (
                    <div className="grid-2">
                      <NumberField label="a" value={s.a} onChange={(v) => set({ a: v })} onEnter={run} />
                      <NumberField label="b" value={s.b} onChange={(v) => set({ b: v })} onEnter={run} />
                    </div>
                  )}
                </>
              )}
              {s.tab !== 'constants' && (
                <button className="btn primary" onClick={run}>
                  <IconPlay /> Compute
                </button>
              )}
            </div>
          </Card>
        </div>
        <div className="stack">
          <ErrorBanner error={ev.error ?? cmp.error ?? nt.error} />
          {s.tab === 'evaluate' && (
            <LoadingOverlay loading={ev.loading}>
              <div className="stack">
                {ev.data && (
                  <Card title={`${s.fn}(${s.x})`} actions={<span className="hint">{fmtMs(ev.timeMs)}</span>}>
                    <div className="hero-number mono">{ev.data.text}</div>
                  </Card>
                )}
                <Card title={`${s.fn}(x)`}>
                  <Plot series={curve.data ? [{ id: 'f', label: `${s.fn}(x)`, x: curve.data.x, y: curve.data.y }] : []} markers={ev.data && ev.data.value !== null ? [{ x: numberOf(s.x), y: ev.data.value, label: fmt(ev.data.value, 6) }] : []} ariaLabel={`Plot of ${s.fn}`} height={320} fileName={s.fn} />
                </Card>
              </div>
            </LoadingOverlay>
          )}
          {s.tab === 'accuracy' && c && (
            <LoadingOverlay loading={cmp.loading}>
              <div className="stack">
                <Card title={`${c.fn}: cml versus the platform libm`} actions={<span className="hint">{fmtMs(cmp.timeMs)}</span>}>
                  {c.hasReference ? (
                    <>
                      <div className="stat-row">
                        <Stat label="Worst error" value={`${fmt(c.worstUlps, 3)} ulp`} sub={`at x = ${fmt(c.worstAt, 6)}`} />
                        <Stat label="Mean error" value={`${fmt(c.meanUlps, 3)} ulp`} />
                        <Stat label="Samples" value={c.rows.length} />
                      </div>
                      <p className="hint" style={{ marginTop: 8 }}>
                        One ULP (unit in the last place) is the spacing between adjacent doubles at that magnitude. A correctly rounded result is within 0.5 ulp; libm itself is usually within 1 ulp, so differences of a few ulp mean both agree to ~16 significant digits.
                      </p>
                    </>
                  ) : (
                    <p className="hint">The platform library has no direct equivalent of {c.fn}; values are shown without a reference.</p>
                  )}
                </Card>
                {c.hasReference && (
                  <Card title="Error across the interval">
                    <Plot series={[{ id: 'ulp', label: 'ulp error', x: c.rows.map((row) => row.x), y: c.rows.map((row) => row.ulps), kind: 'scatter' }]} yDomain={[-0.2, Math.max(1.5, c.worstUlps * 1.2)]} ariaLabel="ULP error at each sample point" yLabel="ulp" xLabel="x" height={240} compact />
                  </Card>
                )}
                <Card title="Values">
                  <DataTable columns={[{ key: 'x', label: 'x', digits: 8 }, { key: 'cml', label: 'cml', digits: 17 }, { key: 'libm', label: 'libm', digits: 17 }, { key: 'ulps', label: 'ulp', digits: 3 }]} rows={c.rows as unknown as Record<string, unknown>[]} fileName={`accuracy-${c.fn}`} />
                </Card>
              </div>
            </LoadingOverlay>
          )}
          {s.tab === 'number' && nt.data && (
            <Card title="Result" actions={<span className="hint">{fmtMs(nt.timeMs)}</span>}>
              {'exact' in nt.data && <div className="hero-number mono">{String(nt.data.exact)}</div>}
              {!('exact' in nt.data) && typeof nt.data.value === 'number' && <div className="hero-number mono">{fmt(nt.data.value as number, 17)}</div>}
              {typeof nt.data.value === 'boolean' && <div className="hero-number">{nt.data.value ? 'prime' : 'composite'}</div>}
              {Array.isArray(nt.data.factors) && <div className="hero-number mono" style={{ fontSize: '1.5rem' }}>{(nt.data.factors as number[]).join(' × ')}</div>}
              {Array.isArray(nt.data.primes) && (
                <>
                  <p className="hint">{String(nt.data.count)} primes</p>
                  <p className="mono" style={{ marginTop: 6, wordBreak: 'break-word', maxHeight: 300, overflow: 'auto' }}>
                    {(nt.data.primes as number[]).join(', ')}
                  </p>
                </>
              )}
            </Card>
          )}
          {s.tab === 'constants' && constants && (
            <Card title="Constants (double precision)">
              <DataTable columns={[{ key: 'name', label: 'name' }, { key: 'value', label: 'value', digits: 17 }]} rows={Object.entries(constants).map(([name, value]) => ({ name, value }))} fileName="constants" />
            </Card>
          )}
        </div>
      </div>
    </div>
  );
}
