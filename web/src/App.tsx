import { Suspense, lazy, useEffect, useState } from 'react';
import { NavLink, Route, Routes, useLocation } from 'react-router-dom';
import { CommandPalette } from './components/CommandPalette';
import { IconMenu, IconMonitor, IconMoon, IconSearch, IconSun } from './components/Icons';
import { ToastProvider } from './components/ui';
import { API_BASE, engine } from './engine/client';
import { useEngineStatus } from './engine/useEngine';
import type { EnginePreference } from './engine/types';
import { fmtMs } from './lib/format';
import { useTheme } from './lib/storage';
import { NAV } from './nav';

const Home = lazy(() => import('./pages/Home'));
const PlotPage = lazy(() => import('./pages/PlotPage'));
const RootsPage = lazy(() => import('./pages/RootsPage'));
const OptimizePage = lazy(() => import('./pages/OptimizePage'));
const IntegratePage = lazy(() => import('./pages/IntegratePage'));
const OdePage = lazy(() => import('./pages/OdePage'));
const InterpolatePage = lazy(() => import('./pages/InterpolatePage'));
const PolynomialsPage = lazy(() => import('./pages/PolynomialsPage'));
const MatricesPage = lazy(() => import('./pages/MatricesPage'));
const StatisticsPage = lazy(() => import('./pages/StatisticsPage'));
const FunctionsPage = lazy(() => import('./pages/FunctionsPage'));
const ApiPage = lazy(() => import('./pages/ApiPage'));

function EngineBadge() {
  const status = useEngineStatus();
  const [open, setOpen] = useState(false);
  const pref = engine.getPreference();
  const label = status.kind === 'wasm' ? 'WebAssembly' : status.kind === 'server' ? 'Server' : status.kind === 'loading' ? 'Starting…' : 'Unavailable';
  const dot = status.kind === 'wasm' || status.kind === 'server' ? 'ok' : status.kind === 'unavailable' ? 'bad' : '';
  return (
    <div style={{ position: 'relative' }}>
      <button className="btn ghost sm" onClick={() => setOpen((v) => !v)} aria-haspopup="dialog" aria-expanded={open} title="Compute engine">
        <span className={'engine-dot ' + dot} />
        {label}
        {status.lastLatencyMs !== undefined && <span className="hint">{fmtMs(status.lastLatencyMs)}</span>}
      </button>
      {open && (
        <div className="card fade-in" role="dialog" aria-label="Engine settings" style={{ position: 'absolute', right: 0, top: 40, width: 280, zIndex: 30 }}>
          <div className="stack" style={{ gap: 8 }}>
            <b>Compute engine</b>
            <p className="hint">
              The C++ library runs as WebAssembly inside a Web Worker in your browser. If a backend is deployed, it can run natively on the server instead.
            </p>
            {status.version && <span className="hint">engine v{status.version}</span>}
            <span className="hint">Server: {API_BASE || 'same origin (/api)'}</span>
            {status.message && <span className="error-text">{status.message}</span>}
            <div className="segmented" role="group" aria-label="Engine preference">
              {(['auto', 'wasm', 'server'] as EnginePreference[]).map((p) => (
                <button
                  key={p}
                  aria-pressed={pref === p}
                  onClick={() => {
                    engine.setPreference(p);
                    setOpen(false);
                  }}
                >
                  {p === 'auto' ? 'Auto' : p === 'wasm' ? 'WASM' : 'Server'}
                </button>
              ))}
            </div>
          </div>
        </div>
      )}
    </div>
  );
}

function ThemeToggle() {
  const [theme, setTheme] = useTheme();
  const next = theme === 'system' ? 'light' : theme === 'light' ? 'dark' : 'system';
  const Icon = theme === 'light' ? IconSun : theme === 'dark' ? IconMoon : IconMonitor;
  return (
    <button className="btn ghost icon" onClick={() => setTheme(next)} title={`Theme: ${theme} (click for ${next})`} aria-label={`Theme: ${theme}. Switch to ${next}`}>
      <Icon />
    </button>
  );
}

export default function App() {
  const [navOpen, setNavOpen] = useState(false);
  const [paletteOpen, setPaletteOpen] = useState(false);
  const location = useLocation();
  useEffect(() => setNavOpen(false), [location.pathname]);
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'k') {
        e.preventDefault();
        setPaletteOpen((v) => !v);
      }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, []);

  const groups: { key: NavItem['group']; label: string | null }[] = [
    { key: 'start', label: null },
    { key: 'tools', label: 'Tools' },
    { key: 'more', label: 'More' },
  ];
  type NavItem = (typeof NAV)[number];

  return (
    <ToastProvider>
      <div className="app">
        <div className={'backdrop' + (navOpen ? ' open' : '')} onClick={() => setNavOpen(false)} />
        <nav className={'sidebar' + (navOpen ? ' open' : '')} aria-label="Main">
          <NavLink to="/" className="brand">
            <img src={`${import.meta.env.BASE_URL}favicon.svg`} alt="" width={30} height={30} />
            <span>
              <b>CML</b>
              <small>Custom Math Library</small>
            </span>
          </NavLink>
          {groups.map((g) => (
            <div key={g.key}>
              {g.label && <div className="nav-group">{g.label}</div>}
              {NAV.filter((n) => n.group === g.key).map((n) => (
                <NavLink key={n.to} to={n.to} end={n.to === '/'} className={({ isActive }) => 'nav-link' + (isActive ? ' active' : '')}>
                  <n.icon />
                  {n.label}
                </NavLink>
              ))}
            </div>
          ))}
          <div style={{ marginTop: 'auto', padding: '12px 10px' }} className="hint">
            Every algorithm is implemented from scratch in C++ and compiled to WebAssembly.{' '}
            <a href="https://github.com/hardikgoel25/Custom-math-library" target="_blank" rel="noreferrer">
              Source
            </a>
          </div>
        </nav>
        <header className="topbar">
          <button className="btn ghost icon menu-btn" aria-label="Open navigation" onClick={() => setNavOpen(true)}>
            <IconMenu />
          </button>
          <button className="btn ghost sm" onClick={() => setPaletteOpen(true)} aria-label="Open command palette">
            <IconSearch /> Search tools <span className="kbd">Ctrl K</span>
          </button>
          <div className="spacer" />
          <EngineBadge />
          <ThemeToggle />
        </header>
        <main className="main" id="main">
          <Suspense fallback={<div className="skeleton" style={{ height: 240 }} />}>
            <Routes>
              <Route path="/" element={<Home />} />
              <Route path="/plot" element={<PlotPage />} />
              <Route path="/roots" element={<RootsPage />} />
              <Route path="/optimize" element={<OptimizePage />} />
              <Route path="/integrate" element={<IntegratePage />} />
              <Route path="/ode" element={<OdePage />} />
              <Route path="/interpolate" element={<InterpolatePage />} />
              <Route path="/polynomials" element={<PolynomialsPage />} />
              <Route path="/matrices" element={<MatricesPage />} />
              <Route path="/statistics" element={<StatisticsPage />} />
              <Route path="/functions" element={<FunctionsPage />} />
              <Route path="/api" element={<ApiPage />} />
              <Route
                path="*"
                element={
                  <div className="card">
                    <h2>Page not found</h2>
                    <p>
                      <NavLink to="/">Back to the overview</NavLink>
                    </p>
                  </div>
                }
              />
            </Routes>
          </Suspense>
        </main>
        <CommandPalette open={paletteOpen} onClose={() => setPaletteOpen(false)} />
      </div>
    </ToastProvider>
  );
}
