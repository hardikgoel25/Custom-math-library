// Command palette (Ctrl/⌘ K): jump to a tool or run a preset example.
import { useEffect, useMemo, useRef, useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { NAV } from '../nav';

export interface Command {
  id: string;
  title: string;
  hint?: string;
  to: string;
}

const PRESETS: Command[] = [
  { id: 'p1', title: 'Solve x³ − x − 2 = 0 with Newton', hint: 'roots', to: '/roots?f=x%5E3+-+x+-+2&method=newton&x0=1.5' },
  { id: 'p2', title: 'All roots of sin(x) on [0, 20]', hint: 'roots', to: '/roots?f=sin(x)&mode=all&a=0&b=20' },
  { id: 'p3', title: 'Integrate e^(−x²) from −∞ to ∞', hint: 'integrate', to: '/integrate?f=exp(-x%5E2)&a=-inf&b=inf&method=auto' },
  { id: 'p4', title: 'Minimise the Rosenbrock function', hint: 'optimise', to: '/optimize?tab=nd&f=(1-x)%5E2+%2B+100(y-x%5E2)%5E2&x0=-1.2%2C1' },
  { id: 'p5', title: 'Lotka–Volterra predator–prey', hint: 'ode', to: '/ode?mode=system&eq=1.1x+-+0.4x+y%7C0.1x+y+-+0.4y&state=x%2Cy&y0=10%2C10&t1=60' },
  { id: 'p6', title: 'Eigenvalues of a symmetric 3×3', hint: 'matrices', to: '/matrices?A=4+1+2%3B1+3+0%3B2+0+5&fn=eigen' },
  { id: 'p7', title: 'Roots of x⁵ − 1 on the complex plane', hint: 'polynomials', to: '/polynomials?p=x%5E5+-+1' },
  { id: 'p8', title: 'Compare sin(x) against libm', hint: 'functions', to: '/functions?tab=accuracy&fn=sin' },
];

export function CommandPalette({ open, onClose }: { open: boolean; onClose: () => void }) {
  const [q, setQ] = useState('');
  const [idx, setIdx] = useState(0);
  const inputRef = useRef<HTMLInputElement>(null);
  const nav = useNavigate();

  const commands = useMemo<Command[]>(() => [...NAV.map((n) => ({ id: n.to, title: n.label, hint: n.hint, to: n.to })), ...PRESETS], []);
  const filtered = useMemo(() => {
    const t = q.trim().toLowerCase();
    if (!t) return commands;
    return commands
      .map((c) => {
        const hay = (c.title + ' ' + (c.hint ?? '')).toLowerCase();
        let score = 0;
        if (hay.includes(t)) score = 10 - hay.indexOf(t) / 100;
        else {
          let i = 0;
          for (const ch of hay) if (ch === t[i]) i++;
          if (i === t.length) score = 1;
        }
        return { c, score };
      })
      .filter((x) => x.score > 0)
      .sort((a, b) => b.score - a.score)
      .map((x) => x.c);
  }, [q, commands]);

  useEffect(() => {
    if (open) {
      setQ('');
      setIdx(0);
      setTimeout(() => inputRef.current?.focus(), 0);
    }
  }, [open]);
  useEffect(() => setIdx(0), [q]);

  if (!open) return null;
  const go = (c: Command) => {
    onClose();
    nav(c.to);
  };
  return (
    <div className="palette-backdrop" onClick={onClose}>
      <div className="palette" role="dialog" aria-modal="true" aria-label="Command palette" onClick={(e) => e.stopPropagation()}>
        <input
          ref={inputRef}
          value={q}
          placeholder="Jump to a tool or try an example…"
          aria-label="Search commands"
          onChange={(e) => setQ(e.target.value)}
          onKeyDown={(e) => {
            if (e.key === 'ArrowDown') {
              e.preventDefault();
              setIdx((i) => Math.min(filtered.length - 1, i + 1));
            } else if (e.key === 'ArrowUp') {
              e.preventDefault();
              setIdx((i) => Math.max(0, i - 1));
            } else if (e.key === 'Enter' && filtered[idx]) {
              go(filtered[idx]);
            } else if (e.key === 'Escape') {
              onClose();
            }
          }}
        />
        <ul role="listbox">
          {filtered.map((c, i) => (
            <li key={c.id} role="option" aria-selected={i === idx} onMouseEnter={() => setIdx(i)} onClick={() => go(c)}>
              <span>{c.title}</span>
              {c.hint && <small>{c.hint}</small>}
            </li>
          ))}
          {filtered.length === 0 && <li aria-disabled>No matches</li>}
        </ul>
      </div>
    </div>
  );
}
