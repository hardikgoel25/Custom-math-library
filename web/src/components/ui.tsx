// Small presentational building blocks: fields, tiles, tables, badges, JSON view.
import { createContext, useCallback, useContext, useEffect, useId, useMemo, useRef, useState } from 'react';
import type { ReactNode } from 'react';
import { fmt, parseNumberInput } from '../lib/format';
import { copyText, downloadText, toCsv } from '../lib/urlState';
import type { EngineErrorBody } from '../engine/types';
import { IconAlert, IconCheck, IconCopy, IconDownload } from './Icons';

// ---------------------------------------------------------------- toasts
interface Toast {
  id: number;
  message: string;
  kind: 'info' | 'error';
}
const ToastCtx = createContext<(message: string, kind?: 'info' | 'error') => void>(() => {});
export function ToastProvider({ children }: { children: ReactNode }) {
  const [toasts, setToasts] = useState<Toast[]>([]);
  const push = useCallback((message: string, kind: 'info' | 'error' = 'info') => {
    const id = Date.now() + Math.random();
    setToasts((t) => [...t, { id, message, kind }]);
    setTimeout(() => setToasts((t) => t.filter((x) => x.id !== id)), 3200);
  }, []);
  return (
    <ToastCtx.Provider value={push}>
      {children}
      <div className="toasts" aria-live="polite">
        {toasts.map((t) => (
          <div key={t.id} className={'toast fade-in' + (t.kind === 'error' ? ' error' : '')}>
            {t.message}
          </div>
        ))}
      </div>
    </ToastCtx.Provider>
  );
}
export const useToast = () => useContext(ToastCtx);

// ---------------------------------------------------------------- fields
interface FieldProps {
  label: string;
  hint?: string;
  error?: string | null;
  children: (id: string) => ReactNode;
  className?: string;
}
export function Field({ label, hint, error, children, className }: FieldProps) {
  const id = useId();
  return (
    <div className={'field ' + (className ?? '')}>
      <label htmlFor={id}>{label}</label>
      {children(id)}
      {error ? (
        <span className="error-text" role="alert">
          {error}
        </span>
      ) : hint ? (
        <span className="hint">{hint}</span>
      ) : null}
    </div>
  );
}

interface TextFieldProps {
  label: string;
  value: string;
  onChange: (v: string) => void;
  hint?: string;
  error?: string | null;
  mono?: boolean;
  placeholder?: string;
  onEnter?: () => void;
  autoFocus?: boolean;
  list?: string;
}
export function TextField({ label, value, onChange, hint, error, mono, placeholder, onEnter, autoFocus, list }: TextFieldProps) {
  return (
    <Field label={label} hint={hint} error={error}>
      {(id) => (
        <input
          id={id}
          className={'input' + (mono ? ' mono' : '')}
          value={value}
          placeholder={placeholder}
          autoFocus={autoFocus}
          list={list}
          spellCheck={false}
          autoComplete="off"
          aria-invalid={!!error}
          onChange={(e) => onChange(e.target.value)}
          onKeyDown={(e) => {
            if (e.key === 'Enter' && onEnter) onEnter();
          }}
        />
      )}
    </Field>
  );
}

/** Expression input: monospace, Enter submits. */
export function ExpressionField(props: Omit<TextFieldProps, 'mono'>) {
  return <TextField {...props} mono />;
}

interface NumberFieldProps {
  label: string;
  value: string;
  onChange: (v: string) => void;
  hint?: string;
  onEnter?: () => void;
  allowInfinite?: boolean;
  required?: boolean;
}
/** Numeric input accepting pi, 2pi, pi/4, e, inf. Reports a validation error itself. */
export function NumberField({ label, value, onChange, hint, onEnter, allowInfinite = false, required = true }: NumberFieldProps) {
  const parsed = parseNumberInput(value);
  let error: string | null = null;
  if (parsed === null) error = required ? 'required' : null;
  else if (Number.isNaN(parsed)) error = 'not a number';
  else if (!allowInfinite && !Number.isFinite(parsed)) error = 'must be finite';
  return <TextField label={label} value={value} onChange={onChange} hint={hint} error={value === '' && !required ? null : error} mono onEnter={onEnter} />;
}

export function numberOf(text: string, fallback = NaN): number {
  const v = parseNumberInput(text);
  return v === null ? fallback : v;
}

interface SelectFieldProps {
  label: string;
  value: string;
  onChange: (v: string) => void;
  options: { value: string; label: string }[];
  hint?: string;
}
export function SelectField({ label, value, onChange, options, hint }: SelectFieldProps) {
  return (
    <Field label={label} hint={hint}>
      {(id) => (
        <select id={id} className="select" value={value} onChange={(e) => onChange(e.target.value)}>
          {options.map((o) => (
            <option key={o.value} value={o.value}>
              {o.label}
            </option>
          ))}
        </select>
      )}
    </Field>
  );
}

export function Segmented<T extends string>({ value, onChange, options, label }: { value: T; onChange: (v: T) => void; options: { value: T; label: string }[]; label: string }) {
  return (
    <div className="segmented" role="group" aria-label={label}>
      {options.map((o) => (
        <button key={o.value} type="button" aria-pressed={value === o.value} onClick={() => onChange(o.value)}>
          {o.label}
        </button>
      ))}
    </div>
  );
}

export function Checkbox({ label, checked, onChange }: { label: string; checked: boolean; onChange: (v: boolean) => void }) {
  return (
    <label className="check">
      <input type="checkbox" checked={checked} onChange={(e) => onChange(e.target.checked)} />
      {label}
    </label>
  );
}

export function TextArea({ label, value, onChange, hint, rows = 5, placeholder, error }: { label: string; value: string; onChange: (v: string) => void; hint?: string; rows?: number; placeholder?: string; error?: string | null }) {
  return (
    <Field label={label} hint={hint} error={error}>
      {(id) => <textarea id={id} className="textarea mono" rows={rows} value={value} placeholder={placeholder} spellCheck={false} aria-invalid={!!error} onChange={(e) => onChange(e.target.value)} />}
    </Field>
  );
}

// ---------------------------------------------------------------- display
export function Stat({ label, value, sub, mono = true }: { label: string; value: ReactNode; sub?: ReactNode; mono?: boolean }) {
  return (
    <div className="stat">
      <div className="label">{label}</div>
      <div className={'value' + (mono ? ' mono' : '')}>{value}</div>
      {sub && <div className="sub">{sub}</div>}
    </div>
  );
}

export function ConvergenceBadge({ converged, message }: { converged: boolean; message?: string }) {
  return (
    <span className={'badge ' + (converged ? 'good' : 'bad')} title={message}>
      {converged ? <IconCheck width={14} height={14} /> : <IconAlert width={14} height={14} />}
      {converged ? 'Converged' : 'Not converged'}
    </span>
  );
}

export function ErrorBanner({ error }: { error: EngineErrorBody | null }) {
  if (!error) return null;
  return (
    <div className="callout error" role="alert">
      <b>{error.code.replace(/_/g, ' ').toLowerCase()}</b>: {error.message}
      {error.position !== undefined && <span className="hint"> (position {error.position})</span>}
    </div>
  );
}

export function Card({ title, children, actions, subtle, className }: { title?: ReactNode; children: ReactNode; actions?: ReactNode; subtle?: boolean; className?: string }) {
  return (
    <section className={'card' + (subtle ? ' subtle' : '') + (className ? ' ' + className : '')}>
      {(title || actions) && (
        <div className="card-head">
          {title && <h3>{title}</h3>}
          {actions && <div className="row">{actions}</div>}
        </div>
      )}
      {children}
    </section>
  );
}

export interface Column<T> {
  key: string;
  label: string;
  render?: (row: T, i: number) => ReactNode;
  digits?: number;
}
export function DataTable<T extends Record<string, unknown>>({ columns, rows, maxRows = 500, fileName = 'table', caption }: { columns: Column<T>[]; rows: T[]; maxRows?: number; fileName?: string; caption?: string }) {
  const shown = rows.slice(0, maxRows);
  const exportCsv = () => {
    downloadText(
      fileName + '.csv',
      toCsv(
        columns.map((c) => c.label),
        rows.map((r) => columns.map((c) => (typeof r[c.key] === 'number' ? (r[c.key] as number) : String(r[c.key] ?? '')))),
      ),
      'text/csv',
    );
  };
  return (
    <div>
      <div className="row" style={{ justifyContent: 'space-between', marginBottom: 6 }}>
        <span className="hint">
          {caption ? caption + ' · ' : ''}
          {rows.length} rows{rows.length > maxRows ? ` (showing ${maxRows})` : ''}
        </span>
        <button className="btn ghost sm" onClick={exportCsv}>
          <IconDownload /> CSV
        </button>
      </div>
      <div className="table-wrap">
        <table className="data">
          <thead>
            <tr>
              {columns.map((c) => (
                <th key={c.key}>{c.label}</th>
              ))}
            </tr>
          </thead>
          <tbody>
            {shown.map((r, i) => (
              <tr key={i}>
                {columns.map((c) => (
                  <td key={c.key} className={typeof r[c.key] === 'number' ? 'mono' : undefined}>
                    {c.render ? c.render(r, i) : typeof r[c.key] === 'number' ? fmt(r[c.key] as number, c.digits ?? 10) : String(r[c.key] ?? '')}
                  </td>
                ))}
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
}

export function CopyButton({ text, label = 'Copy' }: { text: string; label?: string }) {
  const toast = useToast();
  return (
    <button
      className="btn ghost sm"
      onClick={async () => {
        toast((await copyText(text)) ? 'Copied to clipboard' : 'Copy failed', 'info');
      }}
    >
      <IconCopy /> {label}
    </button>
  );
}

export function JsonView({ value, fileName = 'result' }: { value: unknown; fileName?: string }) {
  const text = useMemo(() => JSON.stringify(value, null, 2), [value]);
  return (
    <div>
      <div className="row" style={{ justifyContent: 'flex-end', marginBottom: 6 }}>
        <CopyButton text={text} label="Copy JSON" />
        <button className="btn ghost sm" onClick={() => downloadText(fileName + '.json', text, 'application/json')}>
          <IconDownload /> JSON
        </button>
      </div>
      <pre className="json-view mono">{text}</pre>
    </div>
  );
}

export function MatrixView({ rows, shade = true, digits = 6 }: { rows: number[][]; shade?: boolean; digits?: number }) {
  const cols = rows[0]?.length ?? 0;
  let maxAbs = 0;
  for (const r of rows) for (const v of r) if (Number.isFinite(v)) maxAbs = Math.max(maxAbs, Math.abs(v));
  return (
    <div className="matrix-view" style={{ gridTemplateColumns: `repeat(${cols}, auto)` }} role="table">
      {rows.map((r, i) =>
        r.map((v, j) => {
          const t = shade && maxAbs > 0 ? Math.abs(v) / maxAbs : 0;
          const bg = v === 0 || t === 0 ? 'transparent' : `color-mix(in srgb, ${v > 0 ? 'var(--div-pos)' : 'var(--div-neg)'} ${Math.round(8 + 32 * t)}%, transparent)`;
          return (
            <span key={i + '-' + j} className="cell" role="cell" style={{ background: bg }}>
              {fmt(v, digits)}
            </span>
          );
        }),
      )}
    </div>
  );
}

export function Tabs<T extends string>({ value, onChange, tabs }: { value: T; onChange: (v: T) => void; tabs: { value: T; label: string }[] }) {
  return (
    <div className="tabs" role="tablist">
      {tabs.map((t) => (
        <button key={t.value} role="tab" aria-selected={value === t.value} onClick={() => onChange(t.value)}>
          {t.label}
        </button>
      ))}
    </div>
  );
}

export function PageHeader({ title, description, children }: { title: string; description?: string; children?: ReactNode }) {
  useEffect(() => {
    document.title = `${title} · CML`;
  }, [title]);
  return (
    <div className="page-head">
      <div>
        <h1>{title}</h1>
        {description && <p>{description}</p>}
      </div>
      {children}
    </div>
  );
}

export function Examples({ items, onPick }: { items: { label: string; apply: () => void }[]; onPick?: () => void }) {
  return (
    <div className="pill-list" aria-label="examples">
      {items.map((it) => (
        <button
          key={it.label}
          className="pill"
          onClick={() => {
            it.apply();
            onPick?.();
          }}
        >
          {it.label}
        </button>
      ))}
    </div>
  );
}

export function LoadingOverlay({ loading, children }: { loading: boolean; children: ReactNode }) {
  return <div style={{ opacity: loading ? 0.55 : 1, transition: 'opacity 120ms' }}>{children}</div>;
}

/** Runs a callback once after mount if a condition holds (used for URL-driven auto-run). */
export function useAutoRun(enabled: boolean, run: () => void) {
  const done = useRef(false);
  useEffect(() => {
    if (enabled && !done.current) {
      done.current = true;
      run();
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [enabled]);
}
