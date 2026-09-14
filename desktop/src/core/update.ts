// Updater support — pure helpers that are unit-testable without any network.
// The actual release lookup is a thin wrapper over the GitHub API; the app
// never downloads or installs anything silently (an unsigned build must not
// pretend to auto-update — it points the user at the release page instead).

export interface GitHubRelease {
  tag_name: string;
  html_url: string;
  published_at?: string;
}

export interface FetchLatestReleaseOptions {
  fetchFn?: typeof fetch;
}

/** Query the GitHub API for the newest published release of a repo. */
export async function fetchLatestRelease(
  repo: string,
  opts: FetchLatestReleaseOptions = {}
): Promise<GitHubRelease> {
  const f = opts.fetchFn ?? fetch;
  const res = await f(`https://api.github.com/repos/${repo}/releases/latest`, {
    headers: {
      accept: "application/vnd.github+json",
      "user-agent": "resonux-control-center",
    },
  });
  if (!res.ok) {
    throw new Error(`GitHub replied ${res.status} ${res.statusText}`);
  }
  const raw: unknown = await res.json();
  const release = raw as Partial<GitHubRelease>;
  if (typeof release.tag_name !== "string" || typeof release.html_url !== "string") {
    throw new Error("GitHub did not return a release");
  }
  return { tag_name: release.tag_name, html_url: release.html_url, published_at: release.published_at };
}

function numericParts(version: string): number[] {
  return (
    version
      // Tolerate a v prefix ("v0.1.0") and ignore free-form suffixes
      // ("0.1.0-beta.2"): only the leading numeric triple counts, because
      // the app's own versions are plain major.minor.patch tags.
      .replace(/^v/i, "")
      .split(/[^0-9]+/)
      .filter(Boolean)
      .slice(0, 3)
      .map((s) => parseInt(s, 10))
  );
}

/** -1 when a < b, 0 when equal, 1 when a > b. Missing components count as 0. */
export function compareVersions(a: string, b: string): number {
  const pa = numericParts(a);
  const pb = numericParts(b);
  const len = Math.max(pa.length, pb.length);
  for (let i = 0; i < len; i++) {
    const x = pa[i] ?? 0;
    const y = pb[i] ?? 0;
    if (x !== y) return x < y ? -1 : 1;
  }
  return 0;
}