// Post-build: make every client-side route a real file so static hosts
// (GitHub Pages, S3, ...) answer 200 instead of relying on a 404 fallback.
import { copyFileSync, mkdirSync } from 'node:fs';
import { join } from 'node:path';

const dist = new URL('../dist/', import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, '$1');
const routes = ['plot', 'roots', 'optimize', 'integrate', 'ode', 'interpolate', 'polynomials', 'matrices', 'statistics', 'functions', 'api'];
const index = join(dist, 'index.html');
copyFileSync(index, join(dist, '404.html'));
for (const r of routes) {
  mkdirSync(join(dist, r), { recursive: true });
  copyFileSync(index, join(dist, r, 'index.html'));
}
console.log(`postbuild: 404.html + ${routes.length} route pages written`);
