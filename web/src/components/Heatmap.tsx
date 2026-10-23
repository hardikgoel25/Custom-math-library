// Heatmap — canvas cells (sequential one-hue ramp) with an SVG overlay for
// axes, an optimisation path and markers. Used for f(x, y) surfaces.
import { useEffect, useMemo, useRef, useState } from 'react';
import { fmt, fmtTick } from '../lib/format';
import { linearScale, niceTicks } from '../lib/scale';

export interface HeatmapProps {
  x: number[];
  y: number[];
  z: (number | null)[][]; // z[j][i] for y[j], x[i]
  path?: [number, number][];
  markers?: { x: number; y: number; label?: string }[];
  height?: number;
  ariaLabel: string;
  xLabel?: string;
  yLabel?: string;
}

const M = { top: 12, right: 18, bottom: 40, left: 56 };

function hexToRgb(hex: string): [number, number, number] {
  const h = hex.replace('#', '');
  const v = parseInt(h.length === 3 ? h.split('').map((c) => c + c).join('') : h, 16);
  return [(v >> 16) & 255, (v >> 8) & 255, v & 255];
}

export function Heatmap({ x, y, z, path = [], markers = [], height = 360, ariaLabel, xLabel, yLabel }: HeatmapProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [width, setWidth] = useState(640);
  const [hover, setHover] = useState<{ i: number; j: number; px: number; py: number } | null>(null);

  useEffect(() => {
    const el = containerRef.current;
    if (!el) return;
    const ro = new ResizeObserver((entries) => {
      const w = entries[0]?.contentRect.width;
      if (w) setWidth(w);
    });
    ro.observe(el);
    return () => ro.disconnect();
  }, []);

  const innerW = Math.max(10, width - M.left - M.right);
  const innerH = Math.max(10, height - M.top - M.bottom);
  const xd: [number, number] = [x[0], x[x.length - 1]];
  const yd: [number, number] = [y[0], y[y.length - 1]];
  const sx = useMemo(() => linearScale(xd, [M.left, M.left + innerW]), [xd[0], xd[1], innerW]);
  const sy = useMemo(() => linearScale(yd, [M.top + innerH, M.top]), [yd[0], yd[1], innerH]);

  const { zmin, zmax } = useMemo(() => {
    let lo = Infinity;
    let hi = -Infinity;
    for (const row of z) for (const v of row) if (v !== null && Number.isFinite(v)) { lo = Math.min(lo, v); hi = Math.max(hi, v); }
    if (lo === Infinity) return { zmin: 0, zmax: 1 };
    // compress huge dynamic ranges so the structure stays visible
    return { zmin: lo, zmax: hi === lo ? lo + 1 : hi };
  }, [z]);

  useEffect(() => {
    const canvas = canvasRef.current;
    const container = containerRef.current;
    if (!canvas || !container || x.length < 2 || y.length < 2) return;
    const cs = getComputedStyle(container);
    const stops = ['--seq-100', '--seq-250', '--seq-400', '--seq-550', '--seq-700'].map((n) => hexToRgb(cs.getPropertyValue(n).trim() || '#3987e5'));
    const dpr = window.devicePixelRatio || 1;
    canvas.width = Math.round(width * dpr);
    canvas.height = Math.round(height * dpr);
    const ctx = canvas.getContext('2d')!;
    ctx.scale(dpr, dpr);
    ctx.clearRect(0, 0, width, height);
    const cw = innerW / (x.length - 1);
    const ch = innerH / (y.length - 1);
    const useLog = zmax - zmin > 1000 && zmin >= 0;
    for (let j = 0; j < y.length; j++) {
      for (let i = 0; i < x.length; i++) {
        const v = z[j]?.[i];
        if (v === null || v === undefined || !Number.isFinite(v)) continue;
        let t = useLog ? Math.log1p(v - zmin) / Math.log1p(zmax - zmin) : (v - zmin) / (zmax - zmin);
        t = Math.max(0, Math.min(1, t));
        const pos = t * (stops.length - 1);
        const k = Math.min(stops.length - 2, Math.floor(pos));
        const f = pos - k;
        const c = stops[k].map((a, idx) => Math.round(a + (stops[k + 1][idx] - a) * f));
        ctx.fillStyle = `rgb(${c[0]},${c[1]},${c[2]})`;
        ctx.fillRect(sx(x[i]) - cw / 2, sy(y[j]) - ch / 2, cw + 0.5, ch + 0.5);
      }
    }
  }, [x, y, z, width, height, innerW, innerH, sx, sy, zmin, zmax]);

  const xTicks = niceTicks(xd[0], xd[1], 6);
  const yTicks = niceTicks(yd[0], yd[1], 6);

  const onMove = (e: React.PointerEvent<SVGSVGElement>) => {
    const rect = (e.currentTarget as SVGSVGElement).getBoundingClientRect();
    const px = ((e.clientX - rect.left) / rect.width) * width;
    const py = ((e.clientY - rect.top) / rect.height) * height;
    const xv = sx.invert(px);
    const yv = sy.invert(py);
    if (xv < xd[0] || xv > xd[1] || yv < yd[0] || yv > yd[1]) {
      setHover(null);
      return;
    }
    const i = Math.round(((xv - xd[0]) / (xd[1] - xd[0])) * (x.length - 1));
    const j = Math.round(((yv - yd[0]) / (yd[1] - yd[0])) * (y.length - 1));
    setHover({ i, j, px, py });
  };

  return (
    <div className="plot" ref={containerRef} role="img" aria-label={ariaLabel} style={{ position: 'relative' }}>
      <canvas ref={canvasRef} style={{ position: 'absolute', inset: 0, width: '100%', height: '100%' }} />
      <svg viewBox={`0 0 ${width} ${height}`} style={{ position: 'relative' }} onPointerMove={onMove} onPointerLeave={() => setHover(null)}>
        {path.length > 1 && (
          <path d={path.map(([px, py], i) => `${i ? 'L' : 'M'}${sx(px).toFixed(1)} ${sy(py).toFixed(1)}`).join(' ')} fill="none" stroke="var(--series-2)" strokeWidth={2} strokeLinejoin="round" />
        )}
        {path.map(([px, py], i) => (i === 0 || i === path.length - 1 ? <circle key={i} cx={sx(px)} cy={sy(py)} r={5} fill={i === 0 ? 'var(--series-2)' : 'var(--series-8)'} stroke="var(--surface)" strokeWidth={2} /> : null))}
        {markers.map((m, i) => (
          <g key={i}>
            <circle cx={sx(m.x)} cy={sy(m.y)} r={6} fill="var(--series-8)" stroke="var(--surface)" strokeWidth={2} />
            {m.label && (
              <text x={sx(m.x) + 9} y={sy(m.y) - 8} fontSize={11.5} fontWeight={600} fill="var(--text)" stroke="var(--surface)" strokeWidth={3} paintOrder="stroke">
                {m.label}
              </text>
            )}
          </g>
        ))}
        <line x1={M.left} x2={M.left + innerW} y1={M.top + innerH} y2={M.top + innerH} stroke="var(--axis)" />
        <line x1={M.left} x2={M.left} y1={M.top} y2={M.top + innerH} stroke="var(--axis)" />
        {xTicks.map((t) => (
          <text key={'x' + t} x={sx(t)} y={M.top + innerH + 16} fontSize={11} textAnchor="middle" fill="var(--muted)">
            {fmtTick(t)}
          </text>
        ))}
        {yTicks.map((t) => (
          <text key={'y' + t} x={M.left - 8} y={sy(t) + 4} fontSize={11} textAnchor="end" fill="var(--muted)">
            {fmtTick(t)}
          </text>
        ))}
        {xLabel && (
          <text x={M.left + innerW / 2} y={height - 6} fontSize={11.5} textAnchor="middle" fill="var(--text-2)">
            {xLabel}
          </text>
        )}
        {yLabel && (
          <text x={12} y={M.top + innerH / 2} fontSize={11.5} textAnchor="middle" fill="var(--text-2)" transform={`rotate(-90 12 ${M.top + innerH / 2})`}>
            {yLabel}
          </text>
        )}
        {hover && <rect x={sx(x[hover.i]) - innerW / (x.length - 1) / 2} y={sy(y[hover.j]) - innerH / (y.length - 1) / 2} width={innerW / (x.length - 1)} height={innerH / (y.length - 1)} fill="none" stroke="var(--text)" strokeWidth={1.5} />}
      </svg>
      {hover && (
        <div className="tooltip" style={{ left: Math.min(hover.px + 14, width - 150), top: Math.max(4, hover.py - 40) }}>
          <div className="t-x">
            ({fmt(x[hover.i], 5)}, {fmt(y[hover.j], 5)})
          </div>
          <div className="t-row">
            <span>f</span>
            <b>{fmt(z[hover.j]?.[hover.i] ?? null, 8)}</b>
          </div>
        </div>
      )}
      <div className="ramp-legend" style={{ position: 'absolute', right: 18, top: 10 }}>
        <span>{fmtTick(zmin)}</span>
        <span className="bar" style={{ background: 'linear-gradient(90deg, var(--seq-100), var(--seq-400), var(--seq-700))' }} />
        <span>{fmtTick(zmax)}</span>
      </div>
    </div>
  );
}
