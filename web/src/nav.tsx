import type { ComponentType, SVGProps } from 'react';
import { IconApi, IconFunction, IconHome, IconIntegrate, IconInterpolate, IconMatrix, IconOde, IconOptimize, IconPolynomial, IconRoots, IconSpecial, IconStats } from './components/Icons';

export interface NavItem {
  to: string;
  label: string;
  hint: string;
  icon: ComponentType<SVGProps<SVGSVGElement>>;
  group: 'start' | 'tools' | 'more';
}

export const NAV: NavItem[] = [
  { to: '/', label: 'Overview', hint: 'home', icon: IconHome, group: 'start' },
  { to: '/plot', label: 'Plot & evaluate', hint: 'functions, derivatives', icon: IconFunction, group: 'tools' },
  { to: '/roots', label: 'Root finding', hint: 'bisection, Newton, Brent…', icon: IconRoots, group: 'tools' },
  { to: '/optimize', label: 'Optimisation', hint: 'golden, Brent, Nelder–Mead', icon: IconOptimize, group: 'tools' },
  { to: '/integrate', label: 'Integration', hint: 'Simpson, Romberg, Gauss', icon: IconIntegrate, group: 'tools' },
  { to: '/ode', label: 'Differential equations', hint: 'Euler, RK4, RK45', icon: IconOde, group: 'tools' },
  { to: '/interpolate', label: 'Interpolation & fitting', hint: 'Lagrange, splines, least squares', icon: IconInterpolate, group: 'tools' },
  { to: '/polynomials', label: 'Polynomials', hint: 'roots, arithmetic, calculus', icon: IconPolynomial, group: 'tools' },
  { to: '/matrices', label: 'Matrices', hint: 'det, inverse, LU, QR, eigen', icon: IconMatrix, group: 'tools' },
  { to: '/statistics', label: 'Statistics', hint: 'describe, regression, histogram', icon: IconStats, group: 'tools' },
  { to: '/functions', label: 'Functions & accuracy', hint: 'elementary, special, number theory', icon: IconSpecial, group: 'more' },
  { to: '/api', label: 'API playground', hint: 'raw JSON requests', icon: IconApi, group: 'more' },
];
