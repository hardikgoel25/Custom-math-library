// Plot — dependency-free SVG chart for functions, iterations and data.
//
// Features: multiple series (line / scatter / area), markers, shaded x-regions,
// area-under-curve fills, vertical/horizontal guide lines, nice ticks, recessive
// grid, crosshair tooltip listing every series, wheel zoom + drag pan, keyboard
// crosshair, legend, table view, PNG and CSV export. Colors come from the
// categorical palette tokens in fixed slot order.
import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import type { KeyboardEvent, PointerEvent as RPointerEvent, WheelEvent as RWheelEvent } from 'react';
import { fmt, fmtTick } from '../lib/format';
import { extent, linearScale, niceTicks, robustDomain } from '../lib/scale';
import { downloadText, toCsv } from '../lib/urlState';
import { IconDownload, IconReset, IconTable } from './Icons';

export interface Series {
  id: string;
  label: string;
  x: number[];
  y: (number | null)[];
  color?: string;
  kind?: 'line' | 'scatter' | 'area';
  dash?: boolean;
  width?: number;
  emphasis?: 'normal' | 'muted';
}
export interface Marker {
  x: number;
  y: number;
  label?: string;
  color?: string;
  shape?: 'circle' | 'diamond' | 'cross';
}
export interface Region {
  from: number;
  to: number;
  color?: string;
  label?: string;
}
export interface AreaFill {
  seriesId: string;
  from: number;
  to: number;
  color?: string;
}
export interface GuideLine {
  value: number;
  label?: string;
  color?: string;
  dash?: boolean;
}

export interface PlotProps {
  series: Series[];
  markers?: Marker[];
  regions?: Region[];
  areas?: AreaFill[];
  vlines?: GuideLine[];
  hlines?: GuideLine[];
  xDomain?: [number, number];
  yDomain?: [number, number];
  height?: number;
  xLabel?: string;
  yLabel?: string;
  ariaLabel: string;
  fileName?: string;
  legend?: boolean;
  equalAspect?: boolean;
  onClick?: (x: number, y: number) => void;
  compact?: boolean;
}

const MARGIN = { top: 14, right: 18, bottom: 40, left: 56 };
const MAX_TABLE_ROWS = 2000;

export function seriesColor(i: number): string {
  return `var(--series-${(i % 8) + 1})`;
}

function nearestIndex(xs: number[], x: number): number {
  if (xs.length === 0) return -1;
  // assume mostly sorted; fall back to linear scan when not
  let lo = 0;
  let hi = xs.length - 1;
  if (xs[0] <= xs[hi]) {
    while (lo < hi) {
      const mid = (lo + hi) >> 1;
      if (xs[mid] < x) lo = mid + 1;
      else hi = mid;
    }
    if (lo > 0 && Math.abs(xs[lo - 1] - x) < Math.abs(xs[lo] - x)) return lo - 1;
    return lo;
  }
  let best = 0;
  for (let i = 1; i < xs.length; i++) if (Math.abs(xs[i] - x) < Math.abs(xs[best] - x)) best = i;
  return best;
}

export function Plot(props: PlotProps) {
  const { series, markers = [], regions = [], areas = [], vlines = [], hlines = [], height = 320, legend = true, compact = false } = props;
  const containerRef = useRef<HTMLDivElement>(null);
  const svgRef = useRef<SVGSVGElement>(null);
  const [width, setWidth] = useState(640);
  const [view, setView] = useState<{ x: [number, number]; y: [number, number] } | null>(null);
  const [hover, setHover] = useState<{ px: number; py: number; x: number } | null>(null);
  const [showTable, setShowTable] = useState(false);
  const drag = useRef<{ x0: number; y0: number; vx: [number, number]; vy: [number, number]; moved: boolean } | null>(null);

  useEffect(() => {
    const el = containerRef.current;
    if (!el) return;
    const ro = new ResizeObserver((entries) => {
      const w = entries[0]?.contentRect.width;
      if (w && w > 0) setWidth(w);
    });
    ro.observe(el);
    return () => ro.disconnect();
  }, []);

  const baseDomain = useMemo(() => {
    const allX: number[] = [];
    const allY: number[] = [];
    for (const s of series) {
      for (let i = 0; i < s.x.length; i++) {
        allX.push(s.x[i]);
        const y = s.y[i];
        if (y !== null && Number.isFinite(y)) allY.push(y);
      }
    }
    for (const m of markers) {
      allX.push(m.x);
      if (Number.isFinite(m.y)) allY.push(m.y);
    }
    for (const r of regions) allX.push(r.from, r.to);
    for (const v of vlines) allX.push(v.value);
    for (const h of hlines) if (Number.isFinite(h.value)) allY.push(h.value);
    let xd = props.xDomain ?? extent(allX);
    if (!props.xDomain && xd[0] !== xd[1]) {
      const p = (xd[1] - xd[0]) * (series.some((s) => s.kind === 'scatter') ? 0.06 : 0.0);
      xd = [xd[0] - p, xd[1] + p];
    }
    let yd = props.yDomain ?? robustDomain(allY);
    // make sure markers remain visible even if the bulk clipped them
    for (const m of markers) {
      if (Number.isFinite(m.y)) {
        const pad = (yd[1] - yd[0]) * 0.05;
        if (m.y < yd[0]) yd = [m.y - pad, yd[1]];
        if (m.y > yd[1]) yd = [yd[0], m.y + pad];
      }
    }
    if (props.equalAspect) {
      const innerW = Math.max(10, width - MARGIN.left - MARGIN.right);
      const innerH = Math.max(10, height - MARGIN.top - MARGIN.bottom);
      const xr = xd[1] - xd[0];
      const yr = yd[1] - yd[0];
      const ratio = innerW / innerH;
      if (xr / yr > ratio) {
        const ny = xr / ratio;
        const cy = (yd[0] + yd[1]) / 2;
        yd = [cy - ny / 2, cy + ny / 2];
      } else {
        const nx = yr * ratio;
        const cx = (xd[0] + xd[1]) / 2;
        xd = [cx - nx / 2, cx + nx / 2];
      }
    }
    return { x: xd, y: yd };
  }, [series, markers, regions, vlines, hlines, props.xDomain, props.yDomain, props.equalAspect, width, height]);

  const dom = view ?? baseDomain;
  const innerW = Math.max(10, width - MARGIN.left - MARGIN.right);
  const innerH = Math.max(10, height - MARGIN.top - MARGIN.bottom);
  const sx = useMemo(() => linearScale(dom.x, [MARGIN.left, MARGIN.left + innerW]), [dom.x, innerW]);
  const sy = useMemo(() => linearScale(dom.y, [MARGIN.top + innerH, MARGIN.top]), [dom.y, innerH]);
  const xTicks = useMemo(() => niceTicks(dom.x[0], dom.x[1], Math.max(3, Math.floor(innerW / 80))), [dom.x, innerW]);
  const yTicks = useMemo(() => niceTicks(dom.y[0], dom.y[1], Math.max(3, Math.floor(innerH / 48))), [dom.y, innerH]);

  // Build SVG paths; break lines at gaps and asymptotes.
  const paths = useMemo(() => {
    const yr = dom.y[1] - dom.y[0];
    return series.map((s) => {
      if (s.kind === 'scatter') return '';
      let d = '';
      let pen = false;
      let prevY: number | null = null;
      for (let i = 0; i < s.x.length; i++) {
        const y = s.y[i];
        if (y === null || !Number.isFinite(y)) {
          pen = false;
          prevY = null;
          continue;
        }
        const jump = prevY !== null && Math.abs(y - prevY) > 4 * yr && (y - dom.y[1]) * (prevY - dom.y[0]) < 0 && Math.sign(y - prevY) !== 0;
        const outside = y > dom.y[1] + 2 * yr || y < dom.y[0] - 2 * yr;
        const px = sx(s.x[i]);
        const py = sy(Math.max(dom.y[0] - 2 * yr, Math.min(dom.y[1] + 2 * yr, y)));
        if (jump) pen = false;
        d += (pen ? 'L' : 'M') + px.toFixed(2) + ' ' + py.toFixed(2);
        pen = !outside || pen;
        prevY = y;
      }
      return d;
    });
  }, [series, sx, sy, dom.y]);

  const areaPaths = useMemo(() => {
    return areas.map((a) => {
      const s = series.find((q) => q.id === a.seriesId);
      if (!s) return '';
      const y0 = sy(Math.max(dom.y[0], Math.min(dom.y[1], 0)));
      const lo = Math.min(a.from, a.to);
      const hi = Math.max(a.from, a.to);
      let d = '';
      let started = false;
      let lastX = 0;
      for (let i = 0; i < s.x.length; i++) {
        const x = s.x[i];
        const y = s.y[i];
        if (x < lo || x > hi || y === null || !Number.isFinite(y)) continue;
        const yc = Math.max(dom.y[0], Math.min(dom.y[1], y));
        if (!started) {
          d += `M${sx(x).toFixed(2)} ${y0.toFixed(2)} `;
          started = true;
        }
        d += `L${sx(x).toFixed(2)} ${sy(yc).toFixed(2)} `;
        lastX = x;
      }
      if (started) d += `L${sx(lastX).toFixed(2)} ${y0.toFixed(2)} Z`;
      return d;
    });
  }, [areas, series, sx, sy, dom.y]);

  const clipId = useMemo(() => 'clip-' + Math.random().toString(36).slice(2, 9), []);

  // ------------------------------------------------------------ interaction
  const toData = useCallback(
    (clientX: number, clientY: number) => {
      const rect = svgRef.current!.getBoundingClientRect();
      const px = ((clientX - rect.left) / rect.width) * width;
      const py = ((clientY - rect.top) / rect.height) * height;
      return { px, py, x: sx.invert(px), y: sy.invert(py) };
    },
    [sx, sy, width, height],
  );

  const onPointerMove = (e: RPointerEvent<SVGSVGElement>) => {
    const p = toData(e.clientX, e.clientY);
    if (drag.current) {
      const dxPix = p.px - drag.current.x0;
      const dyPix = p.py - drag.current.y0;
      if (Math.abs(dxPix) + Math.abs(dyPix) > 3) drag.current.moved = true;
      const kx = (drag.current.vx[1] - drag.current.vx[0]) / innerW;
      const ky = (drag.current.vy[1] - drag.current.vy[0]) / innerH;
      setView({
        x: [drag.current.vx[0] - dxPix * kx, drag.current.vx[1] - dxPix * kx],
        y: [drag.current.vy[0] + dyPix * ky, drag.current.vy[1] + dyPix * ky],
      });
      return;
    }
    if (p.px < MARGIN.left || p.px > MARGIN.left + innerW) {
      setHover(null);
      return;
    }
    setHover({ px: p.px, py: p.py, x: p.x });
  };
  const onPointerDown = (e: RPointerEvent<SVGSVGElement>) => {
    const p = toData(e.clientX, e.clientY);
    drag.current = { x0: p.px, y0: p.py, vx: dom.x, vy: dom.y, moved: false };
    (e.target as Element).setPointerCapture?.(e.pointerId);
  };
  const onPointerUp = (e: RPointerEvent<SVGSVGElement>) => {
    const d = drag.current;
    drag.current = null;
    if (d && !d.moved && props.onClick) {
      const p = toData(e.clientX, e.clientY);
      if (p.px >= MARGIN.left && p.px <= MARGIN.left + innerW && p.py >= MARGIN.top && p.py <= MARGIN.top + innerH) props.onClick(p.x, p.y);
    }
  };
  const onWheel = (e: RWheelEvent<SVGSVGElement>) => {
    if (!e.ctrlKey && !e.metaKey && !e.altKey && Math.abs(e.deltaY) < 1) return;
    e.preventDefault();
    const p = toData(e.clientX, e.clientY);
    const f = Math.exp(e.deltaY * 0.0015);
    const zx = e.shiftKey ? 1 : f;
    const zy = e.altKey ? 1 : f;
    setView({
      x: [p.x - (p.x - dom.x[0]) * zx, p.x + (dom.x[1] - p.x) * zx],
      y: [p.y - (p.y - dom.y[0]) * zy, p.y + (dom.y[1] - p.y) * zy],
    });
  };
  useEffect(() => {
    // non-passive wheel listener so preventDefault works
    const el = svgRef.current;
    if (!el) return;
    const h = (e: WheelEvent) => {
      if (e.ctrlKey || e.metaKey || e.shiftKey || e.altKey) e.preventDefault();
    };
    el.addEventListener('wheel', h, { passive: false });
    return () => el.removeEventListener('wheel', h);
  }, []);

  const onKeyDown = (e: KeyboardEvent<HTMLDivElement>) => {
    const first = series[0];
    if (!first || first.x.length === 0) return;
    if (e.key === 'ArrowLeft' || e.key === 'ArrowRight') {
      e.preventDefault();
      const cur = hover ? nearestIndex(first.x, hover.x) : 0;
      const next = Math.max(0, Math.min(first.x.length - 1, cur + (e.key === 'ArrowRight' ? 1 : -1)));
      const x = first.x[next];
      setHover({ px: sx(x), py: MARGIN.top, x });
    } else if (e.key === 'Escape') {
      setHover(null);
    } else if (e.key === '0') {
      setView(null);
    }
  };

  // ------------------------------------------------------------ tooltip data
  const tooltip = useMemo(() => {
    if (!hover) return null;
    const rows = series
      .map((s, i) => {
        const idx = nearestIndex(s.x, hover.x);
        if (idx < 0) return null;
        const y = s.y[idx];
        return { label: s.label, color: s.color ?? seriesColor(i), x: s.x[idx], y, idx };
      })
      .filter((r): r is NonNullable<typeof r> => r !== null);
    const snapX = rows.length ? rows[0].x : hover.x;
    return { rows, snapX };
  }, [hover, series]);

  // ------------------------------------------------------------ export
  const exportPng = () => {
    const svg = svgRef.current;
    const container = containerRef.current;
    if (!svg || !container) return;
    const clone = svg.cloneNode(true) as SVGSVGElement;
    const cs = getComputedStyle(container);
    const resolve = (v: string) => v.replace(/var\((--[a-z0-9-]+)\)/g, (_, name) => cs.getPropertyValue(name).trim() || '#000');
    clone.querySelectorAll('*').forEach((el) => {
      for (const attr of ['fill', 'stroke']) {
        const v = el.getAttribute(attr);
        if (v && v.includes('var(')) el.setAttribute(attr, resolve(v));
      }
      const st = el.getAttribute('style');
      if (st && st.includes('var(')) el.setAttribute('style', resolve(st));
    });
    clone.setAttribute('width', String(width * 2));
    clone.setAttribute('height', String(height * 2));
    const bg = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
    bg.setAttribute('width', '100%');
    bg.setAttribute('height', '100%');
    bg.setAttribute('fill', cs.getPropertyValue('--surface').trim() || '#fff');
    clone.insertBefore(bg, clone.firstChild);
    const xml = new XMLSerializer().serializeToString(clone);
    const blob = new Blob([xml], { type: 'image/svg+xml' });
    const url = URL.createObjectURL(blob);
    const img = new Image();
    img.onload = () => {
      const canvas = document.createElement('canvas');
      canvas.width = width * 2;
      canvas.height = height * 2;
      const ctx = canvas.getContext('2d')!;
      ctx.drawImage(img, 0, 0);
      URL.revokeObjectURL(url);
      const a = document.createElement('a');
      a.download = (props.fileName || 'plot') + '.png';
      a.href = canvas.toDataURL('image/png');
      a.click();
    };
    img.src = url;
  };
  const exportCsv = () => {
    const headers = ['x', ...series.map((s) => s.label)];
    const n = Math.max(...series.map((s) => s.x.length), 0);
    const rows: (number | null)[][] = [];
    for (let i = 0; i < n; i++) rows.push([series[0]?.x[i] ?? null, ...series.map((s) => (i < s.x.length ? s.y[i] : null))]);
    downloadText((props.fileName || 'plot') + '.csv', toCsv(headers, rows), 'text/csv');
  };

  const hasData = series.some((s) => s.x.length > 0) || markers.length > 0;
  const showLegend = legend && series.length >= 2;

  return (
    <div className="plot-shell">
      <div
        className="plot"
        ref={containerRef}
        tabIndex={0}
        role="img"
        aria-label={props.ariaLabel}
        onKeyDown={onKeyDown}
        onPointerLeave={() => {
          if (!drag.current) setHover(null);
        }}
      >
        <svg
          ref={svgRef}
          viewBox={`0 0 ${width} ${height}`}
          onPointerMove={onPointerMove}
          onPointerDown={onPointerDown}
          onPointerUp={onPointerUp}
          onPointerCancel={() => (drag.current = null)}
          onWheel={onWheel}
          onDoubleClick={() => setView(null)}
          style={{ cursor: drag.current ? 'grabbing' : props.onClick ? 'crosshair' : 'default' }}
        >
          <defs>
            <clipPath id={clipId}>
              <rect x={MARGIN.left} y={MARGIN.top} width={innerW} height={innerH} />
            </clipPath>
          </defs>
          {/* grid */}
          <g>
            {yTicks.map((t) => (
              <line key={'gy' + t} x1={MARGIN.left} x2={MARGIN.left + innerW} y1={sy(t)} y2={sy(t)} stroke="var(--grid)" strokeWidth={1} />
            ))}
            {xTicks.map((t) => (
              <line key={'gx' + t} y1={MARGIN.top} y2={MARGIN.top + innerH} x1={sx(t)} x2={sx(t)} stroke="var(--grid)" strokeWidth={1} />
            ))}
          </g>
          {/* zero axes (baseline) */}
          {dom.y[0] < 0 && dom.y[1] > 0 && <line x1={MARGIN.left} x2={MARGIN.left + innerW} y1={sy(0)} y2={sy(0)} stroke="var(--axis)" strokeWidth={1} />}
          {dom.x[0] < 0 && dom.x[1] > 0 && <line y1={MARGIN.top} y2={MARGIN.top + innerH} x1={sx(0)} x2={sx(0)} stroke="var(--axis)" strokeWidth={1} />}
          <g clipPath={`url(#${clipId})`}>
            {regions.map((r, i) => (
              <rect
                key={'r' + i}
                x={sx(Math.min(r.from, r.to))}
                y={MARGIN.top}
                width={Math.max(0, Math.abs(sx(r.to) - sx(r.from)))}
                height={innerH}
                fill={r.color ?? 'var(--accent)'}
                opacity={0.12}
              />
            ))}
            {areaPaths.map((d, i) => d && <path key={'a' + i} d={d} fill={areas[i].color ?? 'var(--series-1)'} opacity={0.14} />)}
            {series.map((s, i) => {
              const color = s.color ?? seriesColor(i);
              const muted = s.emphasis === 'muted';
              if (s.kind === 'scatter') {
                return (
                  <g key={s.id}>
                    {s.x.map((x, j) => {
                      const y = s.y[j];
                      if (y === null || !Number.isFinite(y)) return null;
                      return <circle key={j} cx={sx(x)} cy={sy(y)} r={4} fill={color} stroke="var(--surface)" strokeWidth={2} opacity={muted ? 0.45 : 1} />;
                    })}
                  </g>
                );
              }
              return (
                <path
                  key={s.id}
                  d={paths[i]}
                  fill="none"
                  stroke={muted ? 'var(--muted)' : color}
                  strokeWidth={s.width ?? 2}
                  strokeLinejoin="round"
                  strokeLinecap="round"
                  strokeDasharray={s.dash ? '6 4' : undefined}
                  opacity={muted ? 0.6 : 1}
                />
              );
            })}
            {vlines.map((v, i) => (
              <g key={'v' + i}>
                <line x1={sx(v.value)} x2={sx(v.value)} y1={MARGIN.top} y2={MARGIN.top + innerH} stroke={v.color ?? 'var(--muted)'} strokeWidth={1.5} strokeDasharray={v.dash === false ? undefined : '4 4'} />
                {v.label && (
                  <text x={sx(v.value) + 4} y={MARGIN.top + 12} fontSize={11} fill="var(--text-2)">
                    {v.label}
                  </text>
                )}
              </g>
            ))}
            {hlines.map((h, i) => (
              <g key={'h' + i}>
                <line y1={sy(h.value)} y2={sy(h.value)} x1={MARGIN.left} x2={MARGIN.left + innerW} stroke={h.color ?? 'var(--muted)'} strokeWidth={1.5} strokeDasharray={h.dash === false ? undefined : '4 4'} />
                {h.label && (
                  <text x={MARGIN.left + innerW - 4} y={sy(h.value) - 4} fontSize={11} fill="var(--text-2)" textAnchor="end">
                    {h.label}
                  </text>
                )}
              </g>
            ))}
            {markers.map((m, i) => {
              if (!Number.isFinite(m.x) || !Number.isFinite(m.y)) return null;
              const cx = sx(m.x);
              const cy = sy(m.y);
              const color = m.color ?? 'var(--series-8)';
              return (
                <g key={'m' + i}>
                  {m.shape === 'diamond' ? (
                    <path d={`M${cx} ${cy - 6}L${cx + 6} ${cy}L${cx} ${cy + 6}L${cx - 6} ${cy}Z`} fill={color} stroke="var(--surface)" strokeWidth={2} />
                  ) : m.shape === 'cross' ? (
                    <path d={`M${cx - 5} ${cy - 5}L${cx + 5} ${cy + 5}M${cx - 5} ${cy + 5}L${cx + 5} ${cy - 5}`} stroke={color} strokeWidth={2.5} />
                  ) : (
                    <circle cx={cx} cy={cy} r={5.5} fill={color} stroke="var(--surface)" strokeWidth={2} />
                  )}
                  {m.label && (
                    <text x={cx + 8} y={cy - 8} fontSize={11.5} fontWeight={600} fill="var(--text)" stroke="var(--surface)" strokeWidth={3} paintOrder="stroke">
                      {m.label}
                    </text>
                  )}
                </g>
              );
            })}
            {/* crosshair */}
            {tooltip && (
              <g pointerEvents="none">
                <line x1={sx(tooltip.snapX)} x2={sx(tooltip.snapX)} y1={MARGIN.top} y2={MARGIN.top + innerH} stroke="var(--text-2)" strokeWidth={1} opacity={0.6} />
                {tooltip.rows.map((r, i) =>
                  r.y !== null && Number.isFinite(r.y) ? <circle key={i} cx={sx(r.x)} cy={sy(r.y)} r={4.5} fill={r.color} stroke="var(--surface)" strokeWidth={2} /> : null,
                )}
              </g>
            )}
          </g>
          {/* axes */}
          <line x1={MARGIN.left} x2={MARGIN.left + innerW} y1={MARGIN.top + innerH} y2={MARGIN.top + innerH} stroke="var(--axis)" />
          <line x1={MARGIN.left} x2={MARGIN.left} y1={MARGIN.top} y2={MARGIN.top + innerH} stroke="var(--axis)" />
          {xTicks.map((t) => (
            <text key={'tx' + t} x={sx(t)} y={MARGIN.top + innerH + 16} fontSize={11} textAnchor="middle" fill="var(--muted)" style={{ fontVariantNumeric: 'tabular-nums' }}>
              {fmtTick(t)}
            </text>
          ))}
          {yTicks.map((t) => (
            <text key={'ty' + t} x={MARGIN.left - 8} y={sy(t) + 4} fontSize={11} textAnchor="end" fill="var(--muted)" style={{ fontVariantNumeric: 'tabular-nums' }}>
              {fmtTick(t)}
            </text>
          ))}
          {props.xLabel && (
            <text x={MARGIN.left + innerW / 2} y={height - 6} fontSize={11.5} textAnchor="middle" fill="var(--text-2)">
              {props.xLabel}
            </text>
          )}
          {props.yLabel && (
            <text x={12} y={MARGIN.top + innerH / 2} fontSize={11.5} textAnchor="middle" fill="var(--text-2)" transform={`rotate(-90 12 ${MARGIN.top + innerH / 2})`}>
              {props.yLabel}
            </text>
          )}
          {!hasData && (
            <text x={width / 2} y={height / 2} textAnchor="middle" fill="var(--muted)" fontSize={13}>
              No data yet
            </text>
          )}
          {/* direct end labels for 2–4 series */}
          {showLegend &&
            series.length <= 4 &&
            series.map((s) => {
              let last = -1;
              for (let j = s.x.length - 1; j >= 0; j--) {
                const y = s.y[j];
                if (y !== null && Number.isFinite(y) && s.x[j] <= dom.x[1] && s.x[j] >= dom.x[0] && y <= dom.y[1] && y >= dom.y[0]) {
                  last = j;
                  break;
                }
              }
              if (last < 0) return null;
              return (
                <text key={'el' + s.id} x={Math.min(sx(s.x[last]) + 6, width - 4)} y={sy(s.y[last] as number) + 4} fontSize={11} fill="var(--text-2)" textAnchor={sx(s.x[last]) + 60 > width ? 'end' : 'start'} stroke="var(--surface)" strokeWidth={3} paintOrder="stroke">
                  {s.label}
                </text>
              );
            })}
        </svg>
        {tooltip && tooltip.rows.length > 0 && (
          <div className="tooltip" style={{ left: Math.min(hover!.px + 14, width - 170), top: Math.max(4, Math.min(hover!.py - 10, height - 100)) }}>
            <div className="t-x">x = {fmt(tooltip.snapX, 8)}</div>
            {tooltip.rows.map((r, i) => (
              <div className="t-row" key={i}>
                <span className="key" style={{ background: r.color }} />
                <span>{r.label}</span>
                <b>{r.y === null || !Number.isFinite(r.y) ? '—' : fmt(r.y, 8)}</b>
              </div>
            ))}
          </div>
        )}
      </div>
      {!compact && (
        <div className="plot-toolbar">
          <div className="legend" aria-label="legend">
            {showLegend &&
              series.map((s, i) => (
                <span className="item" key={s.id}>
                  <span className={'key' + (s.kind === 'scatter' ? ' dot' : '') + (s.dash ? ' dash' : '')} style={{ background: s.dash ? undefined : s.color ?? seriesColor(i), color: s.color ?? seriesColor(i) }} />
                  {s.label}
                </span>
              ))}
          </div>
          <div className="row" style={{ gap: 4 }}>
            {view && (
              <button className="btn ghost sm" onClick={() => setView(null)} title="Reset zoom (double-click or 0)">
                <IconReset /> Reset view
              </button>
            )}
            <button className="btn ghost sm" onClick={() => setShowTable((v) => !v)} aria-pressed={showTable} title="Show data as a table">
              <IconTable /> Table
            </button>
            <button className="btn ghost sm" onClick={exportCsv} title="Download CSV">
              <IconDownload /> CSV
            </button>
            <button className="btn ghost sm" onClick={exportPng} title="Download PNG">
              <IconDownload /> PNG
            </button>
          </div>
        </div>
      )}
      {showTable && (
        <div className="table-wrap" style={{ marginTop: 8 }}>
          <table className="data">
            <thead>
              <tr>
                <th>x</th>
                {series.map((s) => (
                  <th key={s.id}>{s.label}</th>
                ))}
              </tr>
            </thead>
            <tbody>
              {Array.from({ length: Math.min(MAX_TABLE_ROWS, Math.max(...series.map((s) => s.x.length), 0)) }).map((_, i) => (
                <tr key={i}>
                  <td className="mono">{fmt(series[0]?.x[i], 8)}</td>
                  {series.map((s) => (
                    <td key={s.id} className="mono">
                      {fmt(s.y[i] ?? null, 8)}
                    </td>
                  ))}
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </div>
  );
}
