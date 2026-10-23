import { describe, expect, it } from 'vitest';
import { fmt, fmtTick, parseNumberInput, parseNumberList, superscript } from './format';
import { niceTicks } from './scale';

describe('format', () => {
  it('formats numbers compactly', () => {
    expect(fmt(1.5)).toBe('1.5');
    expect(fmt(1 / 3, 6)).toBe('0.333333');
    expect(fmt(0)).toBe('0');
    expect(fmt(1e20)).toMatch(/e\+?20/);
    expect(fmt(NaN)).toBe('NaN');
    expect(fmt(Infinity)).toBe('∞');
    expect(fmt(null)).toBe('—');
  });
  it('formats ticks', () => {
    expect(fmtTick(0)).toBe('0');
    expect(fmtTick(1500)).toBe('1.5K');
    expect(fmtTick(0.25)).toBe('0.25');
    expect(fmtTick(2e7)).toMatch(/e/);
  });
  it('parses number inputs with constants', () => {
    expect(parseNumberInput('2')).toBe(2);
    expect(parseNumberInput('pi')).toBeCloseTo(Math.PI);
    expect(parseNumberInput('-2pi')).toBeCloseTo(-2 * Math.PI);
    expect(parseNumberInput('pi/2')).toBeCloseTo(Math.PI / 2);
    expect(parseNumberInput('inf')).toBe(Infinity);
    expect(parseNumberInput('')).toBeNull();
    expect(Number.isNaN(parseNumberInput('abc'))).toBe(true);
  });
  it('parses number lists', () => {
    expect(parseNumberList('1, 2;3\n4')).toEqual([1, 2, 3, 4]);
    expect(() => parseNumberList('1 x')).toThrow();
  });
  it('superscripts', () => {
    expect(superscript(12)).toBe('¹²');
  });
  it('nice ticks', () => {
    const t = niceTicks(0, 10, 5);
    expect(t[0]).toBe(0);
    expect(t[t.length - 1]).toBe(10);
    expect(niceTicks(-1.3, 1.3, 6).length).toBeGreaterThan(3);
    expect(niceTicks(5, 5, 5)).toEqual([5]);
  });
});
