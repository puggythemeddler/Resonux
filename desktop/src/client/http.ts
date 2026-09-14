// Minimal JSON-over-HTTP transport for the Resonux REST surface. Node fetch
// with a hard timeout; throws HttpError with status + body so the UI can show
// friendly what/when/try-again messages instead of a bare network exception.

export class HttpError extends Error {
  constructor(
    readonly status: number,
    readonly statusText: string,
    readonly body: string
  ) {
    super(`HTTP ${status} ${statusText}`);
    this.name = "HttpError";
  }
}

export interface HttpEndpoint {
  address: string; // host or IP (no scheme)
  port: number;
}

export function baseUrl(e: HttpEndpoint): string {
  return `http://${e.address}:${e.port}`;
}

export interface HttpOptions {
  timeoutMs?: number;
}

export async function httpJson<T>(
  endpoint: HttpEndpoint,
  method: string,
  path: string,
  body?: unknown,
  opts: HttpOptions = {}
): Promise<T> {
  const timeoutMs = opts.timeoutMs ?? 3000;
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), timeoutMs);
  try {
    const res = await fetch(baseUrl(endpoint) + path, {
      method,
      headers: body === undefined ? undefined : { "content-type": "application/json" },
      body: body === undefined ? undefined : JSON.stringify(body),
      signal: ctrl.signal,
    });
    const text = await res.text();
    if (!res.ok) {
      throw new HttpError(res.status, res.statusText, text);
    }
    if (text === "") return undefined as T;
    try {
      return JSON.parse(text) as T;
    } catch {
      throw new HttpError(res.status, res.statusText, `invalid json: ${text}`);
    }
  } catch (err) {
    if (err instanceof HttpError) throw err;
    if (err instanceof Error && err.name === "AbortError") {
      throw new HttpError(0, "timeout", `no reply within ${timeoutMs} ms`);
    }
    throw err;
  } finally {
    clearTimeout(timer);
  }
}

export const get = <T>(e: HttpEndpoint, path: string, o?: HttpOptions) =>
  httpJson<T>(e, "GET", path, undefined, o);
export const post = <T>(e: HttpEndpoint, path: string, body?: unknown, o?: HttpOptions) =>
  httpJson<T>(e, "POST", path, body, o);
export const put = <T>(e: HttpEndpoint, path: string, body?: unknown, o?: HttpOptions) =>
  httpJson<T>(e, "PUT", path, body, o);

// Raw body (no JSON parse) — used to back up a config byte-exact before a
// write, so a restore later replays exactly what the controller returned.
export async function getText(e: HttpEndpoint, path: string, opts: HttpOptions = {}): Promise<string> {
  const timeoutMs = opts.timeoutMs ?? 3000;
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), timeoutMs);
  try {
    const res = await fetch(baseUrl(e) + path, {
      method: "GET",
      signal: ctrl.signal,
    });
    const text = await res.text();
    if (!res.ok) throw new HttpError(res.status, res.statusText, text);
    return text;
  } catch (err) {
    if (err instanceof HttpError) throw err;
    if (err instanceof Error && err.name === "AbortError") {
      throw new HttpError(0, "timeout", `no reply within ${timeoutMs} ms`);
    }
    throw err;
  } finally {
    clearTimeout(timer);
  }
}

// One-line technical description for the "advanced details" disclosure. Never
// a stack trace — just enough for an engineer to recognise the failure.
export function describeError(err: unknown): string {
  if (err instanceof HttpError) return `${err.message} (${err.body || err.statusText})`;
  if (err instanceof Error) return err.message;
  return String(err);
}