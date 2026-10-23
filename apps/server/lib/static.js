// apps/server/lib/static.js — path-traversal-safe static file server with SPA fallback.
import { createReadStream } from 'node:fs';
import { stat } from 'node:fs/promises';
import path from 'node:path';

const TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.wasm': 'application/wasm',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.ico': 'image/x-icon',
  '.woff': 'font/woff',
  '.woff2': 'font/woff2',
  '.txt': 'text/plain; charset=utf-8',
  '.map': 'application/json; charset=utf-8',
  '.webmanifest': 'application/manifest+json',
};

export class StaticServer {
  constructor(root) {
    this.root = path.resolve(root);
  }

  /** Resolve a URL path inside the root, or null if it escapes or is invalid. */
  resolve(urlPath) {
    let decoded;
    try {
      decoded = decodeURIComponent(urlPath);
    } catch {
      return null;
    }
    if (decoded.includes('\0')) return null;
    const normalized = path.posix.normalize(decoded.replace(/\\/g, '/'));
    const full = path.resolve(this.root, '.' + (normalized.startsWith('/') ? normalized : '/' + normalized));
    if (full !== this.root && !full.startsWith(this.root + path.sep)) return null;
    return full;
  }

  async serve(req, res, urlPath) {
    if (req.method !== 'GET' && req.method !== 'HEAD') {
      res.statusCode = 405;
      res.end();
      return;
    }
    let file = this.resolve(urlPath);
    if (!file) {
      res.statusCode = 400;
      res.end('bad path');
      return;
    }
    let info = await stat(file).catch(() => null);
    if (info && info.isDirectory()) {
      file = path.join(file, 'index.html');
      info = await stat(file).catch(() => null);
    }
    if (!info) {
      // SPA fallback for client-side routes (never for asset-looking paths)
      if (path.extname(urlPath) === '') {
        file = path.join(this.root, 'index.html');
        info = await stat(file).catch(() => null);
      }
      if (!info) {
        res.statusCode = 404;
        res.end('not found');
        return;
      }
    }
    const ext = path.extname(file).toLowerCase();
    res.setHeader('Content-Type', TYPES[ext] || 'application/octet-stream');
    res.setHeader('Content-Length', String(info.size));
    const immutable = /[\\/]assets[\\/]/.test(file) || ext === '.wasm';
    res.setHeader('Cache-Control', immutable ? 'public, max-age=31536000, immutable' : 'no-cache');
    if (req.method === 'HEAD') {
      res.end();
      return;
    }
    createReadStream(file).pipe(res);
  }
}
