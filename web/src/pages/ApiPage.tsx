import { useEffect, useMemo, useState } from 'react';
import { IconPlay } from '../components/Icons';
import { Card, CopyButton, ErrorBanner, JsonView, PageHeader, SelectField, TextArea } from '../components/ui';
import { engine } from '../engine/client';
import { useCompute } from '../engine/useEngine';
import type { OpInfo } from '../engine/types';
import { fmtMs } from '../lib/format';
import { useUrlState } from '../lib/urlState';

const EXAMPLES: Record<string, Record<string, unknown>> = {
  'meta.ops': {},
  'meta.version': {},
  'expression.evaluate': { expr: '2x^2 + sin(y)', x: 3, y: 'pi/2' },
  'expression.parse': { expr: '3x^2 + 2x y - sin(x)' },
  'expression.derivative': { expr: 'x^3 sin(x)', var: 'x', order: 2, at: 1 },
  'expression.table': { expr: 'sin(x)/x', from: -10, to: 10, samples: 11 },
  'expression.functions': {},
  'core.evaluate': { fn: 'gamma', x: 5.5 },
  'core.compare': { fn: 'exp', from: -5, to: 5, samples: 11 },
  'core.constants': {},
  'core.functions': {},
  'special.numberTheory': { fn: 'primeFactors', n: 9007199254740991 },
  'roots.find': { f: 'x^3 - x - 2', method: 'newton', x0: 1.5, tol: 1e-12 },
  'roots.compare': { f: 'cos(x) - x', a: 0, b: 1, x0: 0.5 },
  'roots.all': { f: 'sin(x)', a: 0.5, b: 20, samples: 2000 },
  'polynomial.parse': { p: '3x^2 - 2x + 1' },
  'polynomial.evaluate': { p: [1, -2, 3], x: 2 },
  'polynomial.roots': { p: 'x^5 - 1' },
  'polynomial.calculus': { p: 'x^3 + x', fn: 'definite_integral', a: 0, b: 2 },
  'polynomial.arithmetic': { a: 'x^3 - 1', b: 'x - 1', fn: 'divide' },
  'polynomial.table': { p: 'x^2 - 1', from: -2, to: 2, samples: 5 },
  'polynomial.fromRoots': { roots: [1, 2, 3] },
  'matrix.compute': { A: [[4, 1, 2], [1, 3, 0], [2, 0, 5]], fn: 'eigen' },
  'optimize.minimize': { f: 'x^2 + 4cos(x)', method: 'brent', a: 0, b: 4 },
  'optimize.bracket': { f: '(x-3)^2', x0: 0, step: 0.5 },
  'optimize.minimizeNd': { f: '(1-x)^2 + 100(y-x^2)^2', method: 'nelder_mead', x0: [-1.2, 1] },
  'optimize.surface': { f: 'x^2 + y^2', xFrom: -2, xTo: 2, yFrom: -2, yTo: 2, xSamples: 5, ySamples: 5 },
  'integrate.compute': { f: 'exp(-x^2)', a: '-inf', b: 'inf' },
  'integrate.compare': { f: 'x^3', a: 0, b: 1, n: 10 },
  'integrate.convergence': { f: 'sin(x)', a: 0, b: 3, method: 'trapezoid', ns: [2, 4, 8, 16, 32] },
  'ode.solve': { equations: ['v', '-x'], state: ['x', 'v'], y0: [1, 0], t0: 0, t1: 6.283185307179586, method: 'rk45', tol: 1e-10, maxPoints: 20 },
  'interpolate.compute': { xs: [0, 1, 2, 3], ys: [1, 2, 5, 10], method: 'lagrange', x: 1.5, samples: 5 },
  'stats.describe': { data: [2, 4, 4, 4, 5, 5, 7, 9] },
  'stats.regression': { x: [1, 2, 3, 4], y: [2.1, 3.9, 6.2, 7.8] },
  'stats.transform': { data: [1, 2, 3, 4, 5], fn: 'moving_average', window: 3 },
};

const DEFAULTS = { op: 'roots.find' };

export default function ApiPage() {
  const [s, set] = useUrlState(DEFAULTS);
  const [ops, setOps] = useState<OpInfo[]>([]);
  const [params, setParams] = useState(() => JSON.stringify(EXAMPLES[DEFAULTS.op] ?? {}, null, 2));
  const [parseError, setParseError] = useState<string | null>(null);
  const c = useCompute<unknown>();

  useEffect(() => {
    engine.ops().then(setOps).catch(() => {});
  }, []);
  useEffect(() => {
    setParams(JSON.stringify(EXAMPLES[s.op] ?? {}, null, 2));
  }, [s.op]);

  const info = useMemo(() => ops.find((o) => o.name === s.op), [ops, s.op]);
  const run = () => {
    try {
      const p = JSON.parse(params);
      setParseError(null);
      void c.run({ op: s.op, params: p });
    } catch (e) {
      setParseError((e as Error).message);
    }
  };
  const requestJson = JSON.stringify({ op: s.op, params: (() => { try { return JSON.parse(params); } catch { return {}; } })() });
  const curl = `curl -s -X POST ${location.origin}/api/compute -H 'Content-Type: application/json' -d '${requestJson.replace(/'/g, "'\\''")}'`;
  const cli = `cml-cli --pretty '${requestJson.replace(/'/g, "'\\''")}'`;

  return (
    <div className="stack">
      <PageHeader title="API playground" description="Every tool in this app is one JSON request to the same engine. Compose requests here, inspect raw responses, and copy the equivalent curl or CLI command." />
      <div className="workspace">
        <div className="controls stack">
          <Card title="Request">
            <div className="stack" style={{ gap: 10 }}>
              <SelectField label="Operation" value={s.op} onChange={(v) => set({ op: v })} options={(ops.length ? ops.map((o) => o.name) : Object.keys(EXAMPLES)).map((n) => ({ value: n, label: n }))} />
              {info && <p className="hint">{info.description}</p>}
              <TextArea label="params (JSON)" value={params} onChange={setParams} rows={12} error={parseError} />
              <button className="btn primary" onClick={run} disabled={c.loading}>
                <IconPlay /> Send
              </button>
              {info && (
                <details className="disclosure">
                  <summary>Parameter schema ( * = required )</summary>
                  <div className="table-wrap">
                    <table className="data">
                      <tbody>
                        {Object.entries(info.params).map(([k, v]) => (
                          <tr key={k}>
                            <td className="mono">{k}</td>
                            <td style={{ textAlign: 'left', whiteSpace: 'normal' }}>{v}</td>
                          </tr>
                        ))}
                        {Object.keys(info.params).length === 0 && (
                          <tr>
                            <td colSpan={2}>no parameters</td>
                          </tr>
                        )}
                      </tbody>
                    </table>
                  </div>
                </details>
              )}
            </div>
          </Card>
          <Card title="Equivalent commands" subtle>
            <div className="stack" style={{ gap: 8 }}>
              <div className="row" style={{ justifyContent: 'space-between' }}>
                <span className="label">HTTP (when a server is deployed)</span>
                <CopyButton text={curl} />
              </div>
              <pre className="json-view mono">{curl}</pre>
              <div className="row" style={{ justifyContent: 'space-between' }}>
                <span className="label">Native CLI</span>
                <CopyButton text={cli} />
              </div>
              <pre className="json-view mono">{cli}</pre>
            </div>
          </Card>
        </div>
        <div className="stack">
          <ErrorBanner error={c.error} />
          <Card title="Response" actions={<span className="hint">{fmtMs(c.timeMs)}</span>}>
            {c.data !== null ? <JsonView value={{ ok: true, op: s.op, result: c.data, timeMs: c.timeMs }} fileName={s.op} /> : <p className="hint">Send a request to see the raw JSON response.</p>}
          </Card>
          <Card title="Operations" subtle>
            <div className="table-wrap" style={{ maxHeight: 420 }}>
              <table className="data">
                <thead>
                  <tr>
                    <th>op</th>
                    <th style={{ textAlign: 'left' }}>description</th>
                  </tr>
                </thead>
                <tbody>
                  {ops.map((o) => (
                    <tr key={o.name} onClick={() => set({ op: o.name })} style={{ cursor: 'pointer' }}>
                      <td className="mono">{o.name}</td>
                      <td style={{ textAlign: 'left', whiteSpace: 'normal' }}>{o.description}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </Card>
        </div>
      </div>
    </div>
  );
}
