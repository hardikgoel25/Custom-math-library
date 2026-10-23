// apps/server/lib/ratelimit.js — per-key token bucket with bounded memory.

export class RateLimiter {
  /**
   * @param {{perMinute: number, burst: number, maxKeys?: number, now?: () => number}} opts
   */
  constructor({ perMinute, burst, maxKeys = 10000, now = Date.now }) {
    this.ratePerMs = perMinute / 60000;
    this.burst = Math.max(1, burst);
    this.maxKeys = maxKeys;
    this.now = now;
    this.buckets = new Map();
  }

  allow(key) {
    const t = this.now();
    let b = this.buckets.get(key);
    if (!b) {
      if (this.buckets.size >= this.maxKeys) this.evict(t);
      b = { tokens: this.burst, updated: t };
      this.buckets.set(key, b);
    }
    const elapsed = t - b.updated;
    b.tokens = Math.min(this.burst, b.tokens + elapsed * this.ratePerMs);
    b.updated = t;
    if (b.tokens >= 1) {
      b.tokens -= 1;
      return true;
    }
    return false;
  }

  /** Drop buckets that are full again (idle clients) and, if still too many, the oldest. */
  evict(t) {
    for (const [k, b] of this.buckets) {
      if (b.tokens + (t - b.updated) * this.ratePerMs >= this.burst) this.buckets.delete(k);
    }
    if (this.buckets.size >= this.maxKeys) {
      const oldest = [...this.buckets.entries()].sort((a, b) => a[1].updated - b[1].updated).slice(0, Math.ceil(this.maxKeys / 10));
      for (const [k] of oldest) this.buckets.delete(k);
    }
  }
}
