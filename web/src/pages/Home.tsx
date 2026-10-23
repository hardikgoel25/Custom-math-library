import { useEffect, useState } from 'react';
import { Link } from 'react-router-dom';
import { IconClock, IconZap } from '../components/Icons';
import { Card, PageHeader } from '../components/ui';
import { engine } from '../engine/client';
import { useEngineStatus } from '../engine/useEngine';
import { fmt } from '../lib/format';
import { useHistory } from '../lib/storage';
import { NAV } from '../nav';

export default function Home() {
  const status = useEngineStatus();
  const [expr, setExpr] = useState('sin(pi/4)^2 + 3!/ln(e^2)');
  const [out, setOut] = useState<{ text: string; error?: string } | null>(null);
  const [history, clearHistory] = useHistory();

  useEffect(() => {
    let cancelled = false;
    const t = setTimeout(async () => {
      if (!expr.trim()) return setOut(null);
      try {
        const r = await engine.execute<{ text: string; value: number | null }>({ op: 'expression.evaluate', params: { expr } });
        if (cancelled) return;
        setOut(r.ok ? { text: r.result.text } : { text: '', error: r.error.message });
      } catch (e) {
        if (!cancelled) setOut({ text: '', error: (e as Error).message });
      }
    }, 150);
    return () => {
      cancelled = true;
      clearTimeout(t);
    };
  }, [expr]);

  return (
    <div className="stack" style={{ gap: 20 }}>
      <PageHeader title="Overview" />
      <section className="hero">
        <div>
          <h1>Numerical computing, from scratch, in your browser.</h1>
          <p style={{ marginTop: 8, color: 'var(--text-2)', maxWidth: '68ch' }}>
            Every function here, from <code>sin</code> to eigenvalues, is implemented by hand in C++ without the standard math library, tested against
            high-precision references, and compiled to WebAssembly. Pick a tool, type a formula, and watch the algorithm work.
          </p>
        </div>
        <div className="row" style={{ alignItems: 'stretch' }}>
          <div className="field" style={{ flex: '1 1 320px' }}>
            <label htmlFor="quick">Quick calculator</label>
            <input id="quick" className="input mono" value={expr} onChange={(e) => setExpr(e.target.value)} spellCheck={false} placeholder="e.g. gamma(5.5) / sqrt(2pi)" />
          </div>
          <div className="stat" style={{ flex: '0 1 320px', display: 'flex', flexDirection: 'column', justifyContent: 'center' }}>
            <div className="label">Result</div>
            {out?.error ? <span className="error-text">{out.error}</span> : <div className="hero-number mono" style={{ fontSize: '1.6rem' }}>{out?.text ?? '…'}</div>}
          </div>
        </div>
        <div className="row hint">
          <IconZap width={14} height={14} />
          {status.kind === 'wasm' && `Engine v${status.version} running as WebAssembly in a worker thread`}
          {status.kind === 'server' && `Engine v${status.version} running natively on the server`}
          {status.kind === 'loading' && 'Starting the engine…'}
          {status.kind === 'unavailable' && `Engine unavailable: ${status.message}`}
        </div>
      </section>

      <div>
        <h2 style={{ marginBottom: 10 }}>Tools</h2>
        <div className="module-grid">
          {NAV.filter((n) => n.group !== 'start').map((n) => (
            <Link key={n.to} to={n.to} className="card module-card">
              <span className="icon">
                <n.icon />
              </span>
              <b>{n.label}</b>
              <p>{n.hint}</p>
            </Link>
          ))}
        </div>
      </div>

      <div className="grid-2" style={{ alignItems: 'start' }}>
        <Card
          title={
            <>
              <IconClock /> Recent computations
            </>
          }
          actions={
            history.length > 0 && (
              <button className="btn ghost sm" onClick={clearHistory}>
                Clear
              </button>
            )
          }
        >
          {history.length === 0 ? (
            <p className="hint">Results you compute will appear here (stored only in this browser).</p>
          ) : (
            <ul style={{ margin: 0, paddingLeft: 0, listStyle: 'none', display: 'grid', gap: 6 }}>
              {history.slice(0, 10).map((h) => (
                <li key={h.id}>
                  <Link to={h.page + h.search}>{h.title}</Link>
                  <span className="hint"> · {new Date(h.at).toLocaleTimeString()}</span>
                </li>
              ))}
            </ul>
          )}
        </Card>
        <Card title="What makes this different">
          <ul style={{ margin: 0, paddingLeft: 18, color: 'var(--text-2)', display: 'grid', gap: 6 }}>
            <li>
              <b>No math library.</b> exp, log, trig, gamma, erf and friends use their own range reduction and series, accurate to a few ULP (see{' '}
              <Link to="/functions?tab=accuracy">accuracy</Link>).
            </li>
            <li>
              <b>Every step is visible.</b> Root finders, minimisers and ODE solvers return their full iteration history for tables and plots.
            </li>
            <li>
              <b>Safe by construction.</b> Inputs are validated, every routine has resource caps, and a runaway computation is cut off by a watchdog.
            </li>
            <li>
              <b>One engine, many hosts.</b> The same C++ runs as WASM here, as a native CLI, behind a Node API, or from any language via the C ABI.
            </li>
          </ul>
          <p className="hint" style={{ marginTop: 10 }}>
            Precision shown: {fmt(0.1 + 0.2, 17)} is what IEEE doubles do with 0.1 + 0.2.
          </p>
        </Card>
      </div>
    </div>
  );
}
