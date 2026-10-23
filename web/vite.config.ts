import { defineConfig } from 'vite';
import type { Plugin } from 'vite';
import react from '@vitejs/plugin-react';

// Optional compute server (e.g. a Render deployment). When set, the UI can
// fall back to it and the Content-Security-Policy allows connecting to it.
const apiUrl = String(process.env.VITE_API_URL || '').replace(/\/$/, '');
function cspPlugin(): Plugin {
  return {
    name: 'cml-csp-connect-src',
    transformIndexHtml(html) {
      return apiUrl ? html.replace("connect-src 'self'", `connect-src 'self' ${apiUrl}`) : html;
    },
  };
}

// BASE_PATH lets the same build deploy at a sub-path (e.g. GitHub Pages
// https://user.github.io/Custom-math-library/) — set it in CI.
export default defineConfig({
  plugins: [react(), cspPlugin()],
  base: process.env.BASE_PATH || '/',
  build: {
    target: 'es2020',
    sourcemap: false,
    chunkSizeWarningLimit: 900,
  },
  server: {
    port: 5173,
    proxy: {
      '^/api/': 'http://localhost:8080',
    },
  },
  worker: {
    format: 'es',
  },
  test: {
    environment: 'node',
    include: ['src/**/*.test.ts'],
  },
});
