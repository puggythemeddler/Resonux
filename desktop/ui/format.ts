// Small presentation helpers. Pure, testable, no DOM.

export function fmtUptime(ms: number): string {
  if (!Number.isFinite(ms) || ms < 0) return "—";
  const totalMin = Math.floor(ms / 60000);
  const h = Math.floor(totalMin / 60);
  const m = totalMin % 60;
  const s = Math.floor((ms % 60000) / 1000);
  if (h > 0) return `${h}h ${m}m`;
  if (m > 0) return `${m}m ${s}s`;
  return `${s}s`;
}

export function fmtBytes(n: number): string {
  if (!Number.isFinite(n) || n < 0) return "—";
  if (n < 1024) return `${n} B`;
  const kb = n / 1024;
  if (kb < 1024) return `${kb.toFixed(0)} KB`;
  return `${(kb / 1024).toFixed(1)} MB`;
}

export function fmtBrightnessPct(value0255: number): number {
  return Math.round(((Math.min(255, Math.max(0, value0255)) / 255) * 100));
}

export function brightnessToWire(pct: number): number {
  return Math.round((Math.min(100, Math.max(0, pct)) / 100) * 255);
}

export function fmtLatency(ms: number | undefined): string {
  if (typeof ms !== "number") return "—";
  return `${ms} ms`;
}

export function fmtClockOffset(offsetMs: number | undefined): string {
  if (typeof offsetMs !== "number") return "—";
  return offsetMs === 0 ? "0 ms" : `${offsetMs > 0 ? "+" : ""}${offsetMs} ms`;
}