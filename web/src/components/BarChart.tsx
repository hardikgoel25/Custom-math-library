// BarChart — histogram / convergence bars with per-bar hover tooltips.
import { useEffect, useMemo, useRef, useState } from 'react';
import { fmt, fmtTick } from '../lib/format';
import { linearScale, niceTicks } from '../lib/scale';

export interface Bar {
  label: string;
  value: number;
  from?: number; // numeric x-range for histograms
  to?: number;
}

export interface BarChartProps {
  bars: Bar[];
  height?: number;
  ariaLabel: string;
  yLabel?: string;
  xLabel?: string;
  valueFormatter?: (v: number) => string;
  color?: string;
  logY?: boolean;
}

const M = { top: 14, right: 16, bottom: 44, left: 56 };

export function BarChart({ bars, height = 260, ariaLabel, yLabel, xLabel, valueFormatter = (v) => fmt(v, 6), color = 'var(--series-1)', logY = false }: BarChartProps) {
  const ref = useRef<HTMLDivElement>(null);
  const [width, setWidth] = useState(640);
  const [hover, setHover] = useState<number | null>(null);
  useEffect(() => {
    const el = ref.current;
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
  const values = bars.map((b) => (logY ? Math.log10(Math.max(b.value, 1e-300)) : b.value));
  const vmax = Math.max(0, ...values.filter(Number.isFinite));
  const vmin = Math.min(0, ...values.filter(Number.isFinite));
  const sy = useMemo(() => linearScale([vmin, vmax === vmin ? vmin + 1 : vmax * 1.08], [M.top + innerH, M.top]), [vmin, vmax, innerH]);
  const yTicks = niceTicks(vmin, vmax === vmin ? vmin + 1 : vmax * 1.08, 5);
  const n = bars.length;
  const slot = innerW / Math.max(1, n);
  const barW = Math.min(24, Math.max(2, slot - 2));
  const baseline = sy(0);

  return (
    <div className="plot" ref={ref} role="img" aria-label={ariaLabel}>
      <svg viewBox={`0 0 ${width} ${height}`} onPointerLeave={() => setHover(null)}>
        {yTicks.map((t) => (
          <g key={t}>
            <line x1={M.left} x2={M.left + innerW} y1={sy(t)} y2={sy(t)} stroke="var(--grid)" />
            <text x={M.left - 8} y={sy(t) + 4} fontSize={11} textAnchor="end" fill="var(--muted)">
              {logY ? `1e${fmtTick(t)}` : fmtTick(t)}
            </text>
          </g>
        ))}
        <line x1={M.left} x2={M.left + innerW} y1={baseline} y2={baseline} stroke="var(--axis)" />
        {bars.map((_, i) => {
          const v = values[i];
          if (!Number.isFinite(v)) return null;
          const cx = M.left + slot * i + slot / 2;
          const top = Math.min(sy(v), baseline);
          const h = Math.abs(sy(v) - baseline);
          const r = Math.min(4, barW / 2, h);
          const up = v >= 0;
          const d = up
            ? `M${cx - barW / 2} ${baseline} V${top + r} a${r} ${r} 0 0 1 ${r} -${r} H${cx + barW / 2 - r} a${r} ${r} 0 0 1 ${r} ${r} V${baseline} Z`
            : `M${cx - barW / 2} ${baseline} V${baseline + h - r} a${r} ${r} 0 0 0 ${r} ${r} H${cx + barW / 2 - r} a${r} ${r} 0 0 0 ${r} -${r} V${baseline} Z`;
          return (
            <g key={i} onPointerEnter={() => setHover(i)} onFocus={() => setHover(i)} tabIndex={-1}>
              <rect x={M.left + slot * i} y={M.top} width={slot} height={innerH} fill="transparent" />
              <path d={d} fill={color} opacity={hover === null || hover === i ? 1 : 0.55} />
            </g>
          );
        })}
        {bars.map((b, i) =>
          n <= 16 || i % Math.ceil(n / 12) === 0 ? (
            <text key={'l' + i} x={M.left + slot * i + slot / 2} y={M.top + innerH + 16} fontSize={11} textAnchor="middle" fill="var(--muted)">
              {b.label}
            </text>
          ) : null,
        )}
        {yLabel && (
          <text x={12} y={M.top + innerH / 2} fontSize={11.5} textAnchor="middle" fill="var(--text-2)" transform={`rotate(-90 12 ${M.top + innerH / 2})`}>
            {yLabel}
          </text>
        )}
        {xLabel && (
          <text x={M.left + innerW / 2} y={height - 6} fontSize={11.5} textAnchor="middle" fill="var(--text-2)">
            {xLabel}
          </text>
        )}
      </svg>
      {hover !== null && bars[hover] && (
        <div className="tooltip" style={{ left: Math.min(M.left + slot * hover + slot / 2 + 10, width - 160), top: 8 }}>
          <div className="t-x">{bars[hover].from !== undefined ? `${fmt(bars[hover].from, 5)} – ${fmt(bars[hover].to, 5)}` : bars[hover].label}</div>
          <div className="t-row">
            <span className="key" style={{ background: color }} />
            <span>value</span>
            <b>{valueFormatter(bars[hover].value)}</b>
          </div>
        </div>
      )}
    </div>
  );
}
