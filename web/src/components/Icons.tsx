import type { SVGProps } from 'react';

type P = SVGProps<SVGSVGElement>;
const base = (props: P) => ({
  width: 18,
  height: 18,
  viewBox: '0 0 24 24',
  fill: 'none',
  stroke: 'currentColor',
  strokeWidth: 2,
  strokeLinecap: 'round' as const,
  strokeLinejoin: 'round' as const,
  'aria-hidden': true,
  ...props,
});

export const IconHome = (p: P) => (
  <svg {...base(p)}>
    <path d="M3 11 12 3l9 8" />
    <path d="M5 10v10h5v-6h4v6h5V10" />
  </svg>
);
export const IconFunction = (p: P) => (
  <svg {...base(p)}>
    <path d="M4 18c3 0 4-2 5-6s2-8 5-8" />
    <path d="M7 10h7" />
    <path d="M13 14l6 6M19 14l-6 6" />
  </svg>
);
export const IconRoots = (p: P) => (
  <svg {...base(p)}>
    <path d="M3 12h18" />
    <path d="M4 18C8 18 9 6 13 6s5 12 7 12" />
    <circle cx="9.5" cy="12" r="2" fill="currentColor" stroke="none" />
  </svg>
);
export const IconOptimize = (p: P) => (
  <svg {...base(p)}>
    <path d="M3 6c4 0 5 12 9 12s5-12 9-12" />
    <circle cx="12" cy="18" r="2" fill="currentColor" stroke="none" />
  </svg>
);
export const IconPolynomial = (p: P) => (
  <svg {...base(p)}>
    <path d="M5 19 10 5" />
    <path d="M10 5l5 14" />
    <path d="M16 5h4M18 3v4" />
    <path d="M3 19h18" />
  </svg>
);
export const IconMatrix = (p: P) => (
  <svg {...base(p)}>
    <path d="M7 4H4v16h3M17 4h3v16h-3" />
    <circle cx="9.5" cy="9" r="1" fill="currentColor" />
    <circle cx="14.5" cy="9" r="1" fill="currentColor" />
    <circle cx="9.5" cy="15" r="1" fill="currentColor" />
    <circle cx="14.5" cy="15" r="1" fill="currentColor" />
  </svg>
);
export const IconIntegrate = (p: P) => (
  <svg {...base(p)}>
    <path d="M14 4c-3 0-3 2-3 5v6c0 3 0 5-3 5" />
    <path d="M4 18c4 0 5-10 8-10s4 10 8 10" opacity="0.5" />
  </svg>
);
export const IconOde = (p: P) => (
  <svg {...base(p)}>
    <path d="M3 12c3-6 6-6 9 0s6 6 9 0" />
    <path d="M3 18c3-6 6-6 9 0s6 6 9 0" opacity="0.4" />
  </svg>
);
export const IconInterpolate = (p: P) => (
  <svg {...base(p)}>
    <path d="M3 16C7 16 8 6 12 6s5 10 9 10" />
    <circle cx="5" cy="15" r="2" fill="currentColor" stroke="none" />
    <circle cx="12" cy="6.5" r="2" fill="currentColor" stroke="none" />
    <circle cx="19" cy="15" r="2" fill="currentColor" stroke="none" />
  </svg>
);
export const IconStats = (p: P) => (
  <svg {...base(p)}>
    <path d="M4 20V10M10 20V4M16 20v-8M22 20H2" />
  </svg>
);
export const IconSpecial = (p: P) => (
  <svg {...base(p)}>
    <path d="M6 4h12v4" />
    <path d="M6 4v16" />
    <path d="M6 20h4" />
  </svg>
);
export const IconApi = (p: P) => (
  <svg {...base(p)}>
    <path d="M8 4c-2 0-3 1-3 3v3c0 1-1 2-2 2 1 0 2 1 2 2v3c0 2 1 3 3 3" />
    <path d="M16 4c2 0 3 1 3 3v3c0 1 1 2 2 2-1 0-2 1-2 2v3c0 2-1 3-3 3" />
  </svg>
);
export const IconSun = (p: P) => (
  <svg {...base(p)}>
    <circle cx="12" cy="12" r="4" />
    <path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4" />
  </svg>
);
export const IconMoon = (p: P) => (
  <svg {...base(p)}>
    <path d="M21 12.8A9 9 0 1 1 11.2 3a7 7 0 0 0 9.8 9.8z" />
  </svg>
);
export const IconMonitor = (p: P) => (
  <svg {...base(p)}>
    <rect x="3" y="4" width="18" height="12" rx="2" />
    <path d="M8 20h8M12 16v4" />
  </svg>
);
export const IconSearch = (p: P) => (
  <svg {...base(p)}>
    <circle cx="11" cy="11" r="7" />
    <path d="m20 20-3.5-3.5" />
  </svg>
);
export const IconMenu = (p: P) => (
  <svg {...base(p)}>
    <path d="M4 7h16M4 12h16M4 17h16" />
  </svg>
);
export const IconCopy = (p: P) => (
  <svg {...base(p)}>
    <rect x="9" y="9" width="11" height="11" rx="2" />
    <path d="M5 15V6a2 2 0 0 1 2-2h9" />
  </svg>
);
export const IconDownload = (p: P) => (
  <svg {...base(p)}>
    <path d="M12 4v11M7 11l5 5 5-5M4 20h16" />
  </svg>
);
export const IconTable = (p: P) => (
  <svg {...base(p)}>
    <rect x="3" y="5" width="18" height="14" rx="2" />
    <path d="M3 10h18M9 5v14" />
  </svg>
);
export const IconReset = (p: P) => (
  <svg {...base(p)}>
    <path d="M4 12a8 8 0 1 0 2.3-5.7" />
    <path d="M4 4v5h5" />
  </svg>
);
export const IconPlay = (p: P) => (
  <svg {...base({ ...p, fill: 'currentColor', stroke: 'none' })}>
    <path d="M7 4v16l13-8z" />
  </svg>
);
export const IconCheck = (p: P) => (
  <svg {...base(p)}>
    <path d="m5 12 4 4L19 6" />
  </svg>
);
export const IconAlert = (p: P) => (
  <svg {...base(p)}>
    <path d="M12 3 2 20h20z" />
    <path d="M12 9v5M12 17h.01" />
  </svg>
);
export const IconPlus = (p: P) => (
  <svg {...base(p)}>
    <path d="M12 5v14M5 12h14" />
  </svg>
);
export const IconTrash = (p: P) => (
  <svg {...base(p)}>
    <path d="M4 7h16M10 11v6M14 11v6M6 7l1 13h10l1-13M9 7V4h6v3" />
  </svg>
);
export const IconShare = (p: P) => (
  <svg {...base(p)}>
    <path d="M10 14 20 4M20 4h-6M20 4v6" />
    <path d="M19 13v6a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V6a1 1 0 0 1 1-1h6" />
  </svg>
);
export const IconClock = (p: P) => (
  <svg {...base(p)}>
    <circle cx="12" cy="12" r="9" />
    <path d="M12 7v5l3 2" />
  </svg>
);
export const IconZap = (p: P) => (
  <svg {...base({ ...p, fill: 'currentColor', stroke: 'none' })}>
    <path d="M13 2 3 14h8l-1 8 10-12h-8z" />
  </svg>
);
