// A small, honest event log for the Control Center session. It records what
// the app did and what the controllers said — discovery, selections, config
// writes, checks, restarts — so the user (or a report they export) can see the
// session's story. It holds the most recent events in memory only (nothing is
// written to disk) and, by design, never contains secrets: no passwords, no
// tokens, and no network addresses are ever passed into an event message.

export type LogLevel = "info" | "warn" | "error";

export interface LogEvent {
  /** Monotonic sequence number (not the wall clock) — events are ordered by this. */
  seq: number;
  /** ISO timestamp of when the event happened. */
  at: string;
  level: LogLevel;
  /** Machine-friendly facet, e.g. "simulator" | "config" | "system" | "check". */
  kind: string;
  /** Plain-language message, safe to show and safe to export. */
  message: string;
  controllerId?: string;
  controllerName?: string;
}

export class LogStore {
  private buf: LogEvent[] = [];
  private seq = 0;

  constructor(private readonly cap = 200) {}

  add(e: Omit<LogEvent, "seq" | "at">): LogEvent {
    const event: LogEvent = {
      seq: this.seq++,
      at: new Date().toISOString(),
      ...e,
    };
    this.buf.push(event);
    if (this.buf.length > this.cap) this.buf.splice(0, this.buf.length - this.cap);
    return event;
  }

  entries(): LogEvent[] {
    return this.buf.map((e) => ({ ...e }));
  }
}