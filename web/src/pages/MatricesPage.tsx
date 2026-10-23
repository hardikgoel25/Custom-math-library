import { useEffect, useMemo, useState } from 'react';
import { useLocation } from 'react-router-dom';
import { IconPlay } from '../components/Icons';
import { MatrixEditor, gridToNumbers, makeGrid, parseMatrixText } from '../components/MatrixEditor';
import { Card, ConvergenceBadge, CopyButton, ErrorBanner, LoadingOverlay, MatrixView, PageHeader, SelectField, Stat, TextField, useAutoRun } from '../components/ui';
import { useCompute } from '../engine/useEngine';
import { fmt, fmtMs, parseNumberList } from '../lib/format';
import { pushHistory } from '../lib/storage';
import { toCsv } from '../lib/urlState';
import { useUrlState } from '../lib/urlState';

const OPS = [
  { value: 'info', label: 'Summary (det, rank, trace, norms)', group: 'Basics' },
  { value: 'determinant', label: 'Determinant', group: 'Basics' },
  { value: 'inverse', label: 'Inverse', group: 'Basics' },
  { value: 'transpose', label: 'Transpose', group: 'Basics' },
  { value: 'rank', label: 'Rank', group: 'Basics' },
  { value: 'trace', label: 'Trace', group: 'Basics' },
  { value: 'norms', label: 'Norms', group: 'Basics' },
  { value: 'condition', label: 'Condition number', group: 'Basics' },
  { value: 'power', label: 'Matrix power Aⁿ', group: 'Basics' },
  { value: 'rref', label: 'Reduced row echelon form', group: 'Decompositions' },
  { value: 'lu', label: 'LU with partial pivoting (PA = LU)', group: 'Decompositions' },
  { value: 'qr', label: 'QR (Householder)', group: 'Decompositions' },
  { value: 'cholesky', label: 'Cholesky (A = LLᵀ)', group: 'Decompositions' },
  { value: 'eigen', label: 'Eigenvalues & eigenvectors', group: 'Decompositions' },
  { value: 'cofactor', label: 'Cofactor matrix', group: 'Decompositions' },
  { value: 'adjugate', label: 'Adjugate', group: 'Decompositions' },
  { value: 'solve', label: 'Solve A x = b', group: 'Systems' },
  { value: 'least_squares', label: 'Least squares  min ‖A x − b‖', group: 'Systems' },
  { value: 'power_iteration', label: 'Dominant eigenpair (power iteration)', group: 'Systems' },
  { value: 'multiply', label: 'A × B', group: 'Two matrices' },
  { value: 'add', label: 'A + B', group: 'Two matrices' },
  { value: 'subtract', label: 'A − B', group: 'Two matrices' },
  { value: 'hadamard', label: 'Element-wise A ∘ B', group: 'Two matrices' },
];
const DEFAULTS = { A: '4 1 2; 1 3 0; 2 0 5', B: '1 0 0; 0 1 0; 0 0 1', b: '1, 2, 3', fn: 'eigen', n: '2' };

type Result = Record<string, unknown> & { fn: string; rows: number; cols: number };

function gridToText(g: string[][]) {
  return g.map((r) => r.join(' ')).join('; ');
}
function textToGrid(t: string, fallback: string[][]) {
  return parseMatrixText(t) ?? fallback;
}
function toLatex(rows: number[][]) {
  return '\\begin{bmatrix}\n' + rows.map((r) => r.map((v) => fmt(v, 6)).join(' & ')).join(' \\\\\n') + '\n\\end{bmatrix}';
}

export default function MatricesPage() {
  const [s, set, fromUrl] = useUrlState(DEFAULTS);
  const location = useLocation();
  const c = useCompute<Result>();
  const [A, setA] = useState<string[][]>(() => textToGrid(s.A, makeGrid(3, 3, (i, j) => (i === j ? '1' : '0'))));
  const [B, setB] = useState<string[][]>(() => textToGrid(s.B, makeGrid(3, 3, (i, j) => (i === j ? '1' : '0'))));
  const op = OPS.find((o) => o.value === s.fn) ?? OPS[0];
  const needsB = op.group === 'Two matrices';
  const needsVector = s.fn === 'solve' || s.fn === 'least_squares';
  const aErr = useMemo(() => {
    const g = gridToNumbers(A);
    return typeof g === 'string' ? g : null;
  }, [A]);

  const run = async () => {
    const An = gridToNumbers(A);
    if (typeof An === 'string') return;
    const params: Record<string, unknown> = { A: An, fn: s.fn };
    if (needsB) {
      const Bn = gridToNumbers(B);
      if (typeof Bn === 'string') return;
      params.B = Bn;
    }
    if (needsVector) {
      try {
        params.b = parseNumberList(s.b);
      } catch {
        return;
      }
    }
    if (s.fn === 'power') params.n = Math.round(Number(s.n) || 2);
    set({ A: gridToText(A), B: gridToText(B) });
    if (await c.run({ op: 'matrix.compute', params })) pushHistory({ page: '/matrices', title: `${op.label} of ${An.length}×${An[0].length}`, search: location.search });
  };
  useAutoRun(fromUrl, () => void run());
  useEffect(() => {
    if (!fromUrl) void run();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const r = c.data;
  const matrixBlocks: { key: string; label: string }[] = [
    { key: 'matrix', label: 'Result' },
    { key: 'L', label: 'L' },
    { key: 'U', label: 'U' },
    { key: 'P', label: 'P' },
    { key: 'Q', label: 'Q' },
    { key: 'R', label: 'R' },
    { key: 'vectors', label: 'Eigenvectors (columns)' },
  ];
  const groups = Array.from(new Set(OPS.map((o) => o.group)));

  return (
    <div className="stack">
      <PageHeader title="Matrices" description="Determinants, inverses and linear systems via LU with partial pivoting; QR, Cholesky, RREF and rank; symmetric eigen-decomposition by Jacobi rotations and general eigenvalues by shifted QR." />
      <div className="workspace">
        <div className="controls stack">
          <Card title="Matrix">
            <div className="stack" style={{ gap: 12 }}>
              <MatrixEditor label="A" value={A} onChange={setA} />
              {aErr && <span className="error-text">{aErr}</span>}
              <div className="field">
                <label htmlFor="op">Operation</label>
                <select id="op" className="select" value={s.fn} onChange={(e) => set({ fn: e.target.value })}>
                  {groups.map((g) => (
                    <optgroup key={g} label={g}>
                      {OPS.filter((o) => o.group === g).map((o) => (
                        <option key={o.value} value={o.value}>
                          {o.label}
                        </option>
                      ))}
                    </optgroup>
                  ))}
                </select>
              </div>
              {s.fn === 'power' && <TextField label="Exponent n" value={s.n} onChange={(v) => set({ n: v })} mono />}
              {needsVector && <TextField label="Right-hand side b" value={s.b} onChange={(v) => set({ b: v })} mono hint="one value per row of A" onEnter={run} />}
              {needsB && <MatrixEditor label="B" value={B} onChange={setB} />}
              <button className="btn primary" onClick={run} disabled={c.loading || !!aErr}>
                <IconPlay /> Compute
              </button>
              <SelectField
                label="Example matrices"
                value=""
                onChange={(v) => {
                  const g = parseMatrixText(v);
                  if (g) setA(g);
                }}
                options={[
                  { value: '', label: 'Pick an example…' },
                  { value: '4 1 2; 1 3 0; 2 0 5', label: 'Symmetric positive definite 3×3' },
                  { value: '1 2 3; 4 5 6; 7 8 9', label: 'Singular 3×3 (rank 2)' },
                  { value: '0 -1; 1 0', label: 'Rotation (complex eigenvalues)' },
                  { value: '2 0 0; 1 1 0; 3 4 3', label: 'Lower triangular' },
                  { value: '1 1/2 1/3 1/4; 1/2 1/3 1/4 1/5; 1/3 1/4 1/5 1/6; 1/4 1/5 1/6 1/7', label: 'Hilbert 4×4 (ill-conditioned)' },
                  { value: '2 -1 0 0; -1 2 -1 0; 0 -1 2 -1; 0 0 -1 2', label: 'Tridiagonal Laplacian' },
                ]}
              />
            </div>
          </Card>
        </div>
        <div className="stack">
          <ErrorBanner error={c.error} />
          {r && (
            <LoadingOverlay loading={c.loading}>
              <div className="stack">
                <Card title={op.label} actions={<span className="hint">{fmtMs(c.timeMs)}</span>}>
                  <div className="stat-row">
                    {typeof r.value === 'number' && <Stat label="Value" value={fmt(r.value, 15)} />}
                    {r.value === null && 'value' in r && <Stat label="Value" value="∞" />}
                    {typeof r.rank === 'number' && <Stat label="Rank" value={r.rank} />}
                    {typeof r.determinant === 'number' && <Stat label="Determinant" value={fmt(r.determinant, 12)} />}
                    {typeof r.trace === 'number' && <Stat label="Trace" value={fmt(r.trace, 12)} />}
                    {typeof r.frobenius === 'number' && <Stat label="Frobenius norm" value={fmt(r.frobenius, 10)} />}
                    {typeof r.norm1 === 'number' && <Stat label="1-norm" value={fmt(r.norm1, 10)} />}
                    {typeof r.normInf === 'number' && <Stat label="∞-norm" value={fmt(r.normInf, 10)} />}
                    {typeof r.sign === 'number' && <Stat label="Permutation sign" value={r.sign as number} />}
                    {typeof r.symmetric === 'boolean' && <Stat label="Symmetric" value={r.symmetric ? 'yes' : 'no'} mono={false} />}
                    {typeof r.square === 'boolean' && <Stat label="Square" value={r.square ? 'yes' : 'no'} mono={false} />}
                    {typeof r.residual === 'number' && <Stat label="Residual ‖Ax − b‖" value={fmt(r.residual, 3)} />}
                    {typeof r.iterations === 'number' && <Stat label="Iterations" value={r.iterations} sub={typeof r.converged === 'boolean' ? <ConvergenceBadge converged={r.converged} /> : undefined} />}
                    {typeof r.singular === 'boolean' && r.singular && <Stat label="Singular" value="yes" mono={false} />}
                  </div>
                  {Array.isArray(r.values) && (
                    <div style={{ marginTop: 12 }}>
                      <div className="label">Eigenvalues</div>
                      <div className="table-wrap" style={{ marginTop: 6 }}>
                        <table className="data">
                          <thead>
                            <tr>
                              <th>#</th>
                              <th>real</th>
                              <th>imaginary</th>
                            </tr>
                          </thead>
                          <tbody>
                            {(r.values as number[]).map((v, i) => (
                              <tr key={i}>
                                <td>{i + 1}</td>
                                <td className="mono">{fmt(v, 12)}</td>
                                <td className="mono">{fmt((r.imag as number[])?.[i] ?? 0, 12)}</td>
                              </tr>
                            ))}
                          </tbody>
                        </table>
                      </div>
                    </div>
                  )}
                  {Array.isArray(r.x) && (
                    <div style={{ marginTop: 12 }}>
                      <div className="label">Solution x</div>
                      <MatrixView rows={(r.x as number[]).map((v) => [v])} shade={false} digits={12} />
                    </div>
                  )}
                  {Array.isArray(r.vector) && (
                    <div style={{ marginTop: 12 }}>
                      <div className="label">Dominant eigenvector</div>
                      <MatrixView rows={(r.vector as number[]).map((v) => [v])} shade={false} digits={10} />
                    </div>
                  )}
                  {Array.isArray(r.pivots) && (
                    <p className="hint" style={{ marginTop: 8 }}>
                      Pivot columns: {(r.pivots as number[]).map((p) => p + 1).join(', ') || 'none'}
                    </p>
                  )}
                </Card>
                {matrixBlocks.map(({ key, label }) => {
                  const m = r[key];
                  if (!Array.isArray(m)) return null;
                  const rows = m as number[][];
                  return (
                    <Card key={key} title={label} actions={<><CopyButton text={toCsv([], rows)} label="CSV" /><CopyButton text={toLatex(rows)} label="LaTeX" /></>}>
                      <MatrixView rows={rows} />
                    </Card>
                  );
                })}
              </div>
            </LoadingOverlay>
          )}
        </div>
      </div>
    </div>
  );
}
