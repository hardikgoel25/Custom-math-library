import { useEffect, useMemo } from 'react';
import { useLocation } from 'react-router-dom';
import { BarChart } from '../components/BarChart';
import { IconPlay } from '../components/Icons';
import { Plot } from '../components/Plot';
import { Card, DataTable, ErrorBanner, Examples, LoadingOverlay, NumberField, PageHeader, Stat, Tabs, TextArea, numberOf, useAutoRun } from '../components/ui';
import { useCompute } from '../engine/useEngine';
import { fmt, fmtMs, parseNumberList } from '../lib/format';
import { pushHistory } from '../lib/storage';
import { useUrlState } from '../lib/urlState';

const DEFAULTS = { tab: 'describe', data: '2, 4, 4, 4, 5, 5, 7, 9, 12, 3.5, 6, 8, 4.2, 5.1, 7.7', bins: 0, x: '1, 2, 3, 4, 5, 6, 7, 8', y: '2.1, 3.9, 6.2, 7.8, 10.1, 11.8, 14.3, 15.9', degree: 1 };

interface Describe {
  count: number;
  sum: number;
  mean: number;
  median: number;
  mode: number | null;
  min: number;
  max: number;
  range: number;
  variance: number;
  variancePopulation: number;
  stddev: number;
  stddevPopulation: number;
  stdError: number;
  q1: number;
  q3: number;
  iqr: number;
  skewness: number | null;
  kurtosis: number | null;
  geometricMean: number | null;
  harmonicMean: number | null;
  meanAbsDev: number;
  coefficientOfVariation: number | null;
  histogram: { edges: number[]; counts: number[]; density: number[] };
  sorted?: number[];
  quantiles: Record<string, number>;
}
interface Regression {
  slope: number;
  intercept: number;
  r: number;
  rSquared: number;
  stdErrorSlope: number;
  stdErrorIntercept: number;
  rmse: number;
  residuals: number[];
  equation: string;
  pearson: number;
  spearman: number;
  covariance: number;
  polynomial?: { unicode: string; coefficients: number[] };
  polynomialRSquared?: number;
}

export default function StatisticsPage() {
  const [s, set, fromUrl] = useUrlState(DEFAULTS);
  const location = useLocation();
  const d = useCompute<Describe>();
  const reg = useCompute<Regression>();

  const parsedData = useMemo(() => {
    try {
      return parseNumberList(s.data);
    } catch (e) {
      return (e as Error).message;
    }
  }, [s.data]);
  const parsedX = useMemo(() => {
    try {
      return parseNumberList(s.x);
    } catch (e) {
      return (e as Error).message;
    }
  }, [s.x]);
  const parsedY = useMemo(() => {
    try {
      return parseNumberList(s.y);
    } catch (e) {
      return (e as Error).message;
    }
  }, [s.y]);

  const run = async () => {
    if (s.tab === 'describe') {
      if (typeof parsedData === 'string') return;
      if (await d.run({ op: 'stats.describe', params: { data: parsedData, bins: s.bins } })) pushHistory({ page: '/statistics', title: `Describe ${parsedData.length} values`, search: location.search });
    } else {
      if (typeof parsedX === 'string' || typeof parsedY === 'string') return;
      if (await reg.run({ op: 'stats.regression', params: { x: parsedX, y: parsedY, degree: s.degree } })) pushHistory({ page: '/statistics', title: `Regression on ${parsedX.length} points`, search: location.search });
    }
  };
  useAutoRun(fromUrl, () => void run());
  useEffect(() => {
    if (!fromUrl) void run();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [s.tab]);

  const r = d.data;
  const g = reg.data;
  const xs = typeof parsedX === 'string' ? [] : parsedX;
  const ys = typeof parsedY === 'string' ? [] : parsedY;
  const lineX = xs.length ? [Math.min(...xs), Math.max(...xs)] : [];

  return (
    <div className="stack">
      <PageHeader title="Statistics" description="Descriptive statistics with quantiles and a histogram, plus linear regression with correlation coefficients and an optional polynomial fit." />
      <Tabs value={s.tab} onChange={(v) => set({ tab: v })} tabs={[{ value: 'describe', label: 'Describe a sample' }, { value: 'regression', label: 'Regression' }]} />
      <div className="workspace">
        <div className="controls stack">
          <Card title="Data">
            <div className="stack" style={{ gap: 10 }}>
              {s.tab === 'describe' ? (
                <>
                  <TextArea label="Values" value={s.data} onChange={(v) => set({ data: v })} rows={6} error={typeof parsedData === 'string' ? parsedData : null} hint="separate with commas, spaces or new lines" />
                  <NumberField label="Histogram bins (0 = auto)" value={String(s.bins)} onChange={(v) => set({ bins: Math.max(0, Math.round(numberOf(v, 0))) })} />
                </>
              ) : (
                <>
                  <TextArea label="x values" value={s.x} onChange={(v) => set({ x: v })} rows={3} error={typeof parsedX === 'string' ? parsedX : null} />
                  <TextArea label="y values" value={s.y} onChange={(v) => set({ y: v })} rows={3} error={typeof parsedY === 'string' ? parsedY : null} />
                  <NumberField label="Also fit polynomial of degree" value={String(s.degree)} onChange={(v) => set({ degree: Math.max(1, Math.min(10, Math.round(numberOf(v, 1)))) })} hint="1 = line only" />
                </>
              )}
              <button className="btn primary" onClick={run} disabled={d.loading || reg.loading}>
                <IconPlay /> Compute
              </button>
              <Examples
                items={
                  s.tab === 'describe'
                    ? [
                        { label: 'Textbook sample', apply: () => set({ data: '2, 4, 4, 4, 5, 5, 7, 9' }) },
                        { label: 'Normal-ish (50)', apply: () => set({ data: Array.from({ length: 50 }, (_, i) => (10 + 2 * Math.sin(i * 1.7) * Math.cos(i * 0.3) * 2 + ((i * 7919) % 13) / 6).toFixed(2)).join(', ') }) },
                        { label: 'Skewed', apply: () => set({ data: Array.from({ length: 40 }, (_, i) => Math.exp(((i * 37) % 41) / 10).toFixed(2)).join(', ') }) },
                      ]
                    : [
                        { label: 'Nearly linear', apply: () => set({ x: DEFAULTS.x, y: DEFAULTS.y, degree: 1 }) },
                        { label: 'Quadratic trend', apply: () => set({ x: '0, 1, 2, 3, 4, 5, 6', y: '1.1, 1.9, 5.2, 9.8, 17.1, 26.2, 36.9', degree: 2 }) },
                        { label: 'Anscombe I', apply: () => set({ x: '10, 8, 13, 9, 11, 14, 6, 4, 12, 7, 5', y: '8.04, 6.95, 7.58, 8.81, 8.33, 9.96, 7.24, 4.26, 10.84, 4.82, 5.68', degree: 1 }) },
                      ]
                }
                onPick={() => setTimeout(run, 0)}
              />
            </div>
          </Card>
        </div>
        <div className="stack">
          <ErrorBanner error={s.tab === 'describe' ? d.error : reg.error} />
          {s.tab === 'describe' && r && (
            <LoadingOverlay loading={d.loading}>
              <div className="stack">
                <Card title="Summary" actions={<span className="hint">{fmtMs(d.timeMs)}</span>}>
                  <div className="stat-row">
                    <Stat label="Count" value={r.count} />
                    <Stat label="Mean" value={fmt(r.mean, 8)} sub={`± ${fmt(r.stdError, 4)} std error`} />
                    <Stat label="Median" value={fmt(r.median, 8)} />
                    <Stat label="Mode" value={fmt(r.mode, 8)} />
                    <Stat label="Std dev (sample)" value={fmt(r.stddev, 8)} sub={`population ${fmt(r.stddevPopulation, 6)}`} />
                    <Stat label="Variance (sample)" value={fmt(r.variance, 8)} />
                    <Stat label="Min / max" value={`${fmt(r.min, 6)} / ${fmt(r.max, 6)}`} sub={`range ${fmt(r.range, 6)}`} />
                    <Stat label="Quartiles" value={`${fmt(r.q1, 6)} · ${fmt(r.q3, 6)}`} sub={`IQR ${fmt(r.iqr, 6)}`} />
                    <Stat label="Skewness" value={fmt(r.skewness, 6)} />
                    <Stat label="Excess kurtosis" value={fmt(r.kurtosis, 6)} />
                    <Stat label="Geometric mean" value={fmt(r.geometricMean, 8)} />
                    <Stat label="Harmonic mean" value={fmt(r.harmonicMean, 8)} />
                    <Stat label="Mean abs deviation" value={fmt(r.meanAbsDev, 8)} />
                    <Stat label="Coefficient of variation" value={fmt(r.coefficientOfVariation, 6)} />
                    <Stat label="Sum" value={fmt(r.sum, 10)} />
                  </div>
                </Card>
                <Card title="Histogram">
                  <BarChart bars={r.histogram.counts.map((c, i) => ({ label: fmt(r.histogram.edges[i], 3), value: c, from: r.histogram.edges[i], to: r.histogram.edges[i + 1] }))} ariaLabel="Histogram of the sample" yLabel="count" valueFormatter={(v) => String(v)} />
                </Card>
                <Card title="Quantiles">
                  <DataTable columns={[{ key: 'p', label: 'quantile' }, { key: 'value', label: 'value', digits: 10 }]} rows={Object.entries(r.quantiles).map(([p, value]) => ({ p, value }))} fileName="quantiles" />
                </Card>
              </div>
            </LoadingOverlay>
          )}
          {s.tab === 'regression' && g && (
            <LoadingOverlay loading={reg.loading}>
              <div className="stack">
                <Card title="Linear fit" actions={<span className="hint">{fmtMs(reg.timeMs)}</span>}>
                  <div className="hero-number mono" style={{ fontSize: '1.4rem' }}>
                    {g.equation}
                  </div>
                  <div className="stat-row" style={{ marginTop: 10 }}>
                    <Stat label="Slope" value={fmt(g.slope, 10)} sub={`± ${fmt(g.stdErrorSlope, 4)}`} />
                    <Stat label="Intercept" value={fmt(g.intercept, 10)} sub={`± ${fmt(g.stdErrorIntercept, 4)}`} />
                    <Stat label="R²" value={fmt(g.rSquared, 8)} />
                    <Stat label="Pearson r" value={fmt(g.pearson, 8)} />
                    <Stat label="Spearman ρ" value={fmt(g.spearman, 8)} />
                    <Stat label="RMSE" value={fmt(g.rmse, 6)} />
                    <Stat label="Covariance" value={fmt(g.covariance, 8)} />
                    {g.polynomial && <Stat label={`Degree ${s.degree} fit`} value={<span style={{ fontSize: '0.9rem' }}>{g.polynomial.unicode}</span>} sub={`R² ${fmt(g.polynomialRSquared, 6)}`} />}
                  </div>
                </Card>
                <Card title="Scatter and fitted line">
                  <Plot
                    series={[
                      { id: 'line', label: 'fitted line', x: lineX, y: lineX.map((x) => g.intercept + g.slope * x) },
                      { id: 'pts', label: 'data', x: xs, y: ys, kind: 'scatter', color: 'var(--series-2)' },
                    ]}
                    ariaLabel="Scatter plot of the data with the fitted regression line"
                    height={320}
                    fileName="regression"
                  />
                </Card>
                <Card title="Residuals">
                  <BarChart bars={g.residuals.map((v, i) => ({ label: fmt(xs[i], 4), value: v }))} ariaLabel="Residual of each observation" height={200} color="var(--series-2)" />
                </Card>
              </div>
            </LoadingOverlay>
          )}
        </div>
      </div>
    </div>
  );
}
