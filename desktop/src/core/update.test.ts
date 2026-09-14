import { describe, it, expect } from "vitest";
import { compareVersions, fetchLatestRelease } from "./update";

describe("compareVersions", () => {
  it("recognises equal versions", () => {
    expect(compareVersions("0.1.0", "0.1.0")).toBe(0);
    expect(compareVersions("v0.1.0", "0.1.0")).toBe(0);
    expect(compareVersions("0.1.0", "0.1.0.0")).toBe(0);
  });

  it("orders major, minor and patch bumps correctly", () => {
    expect(compareVersions("0.1.0", "0.2.0")).toBe(-1);
    expect(compareVersions("1.0.0", "0.9.9")).toBe(1);
    expect(compareVersions("0.1.1", "0.1.0")).toBe(1);
    expect(compareVersions("0.9.9", "0.10.0")).toBe(-1);
  });

  it("tracks pre-release tags against their plain version", () => {
    expect(compareVersions("0.1.0-beta.2", "0.1.0")).toBe(0);
    expect(compareVersions("0.2.0-rc1", "0.2.0")).toBe(0);
  });

  it("treats missing components as zero", () => {
    expect(compareVersions("0.1", "0.1.0")).toBe(0);
    expect(compareVersions("0.1.1", "0.1")).toBe(1);
  });
});

describe("fetchLatestRelease", () => {
  it("parses a release from the GitHub API shape", async () => {
    const fetchFn = (async () =>
      ({
        ok: true,
        status: 200,
        json: async () => ({ tag_name: "v0.9.0", html_url: "https://github.com/acme/resonux/releases/tag/v0.9.0" }),
      }) as unknown as Response) as typeof fetch;

    const release = await fetchLatestRelease("acme/resonux", { fetchFn });
    expect(release.tag_name).toBe("v0.9.0");
    expect(release.html_url).toContain("releases/tag/v0.9.0");
  });

  it("fails honestly when GitHub errors", async () => {
    const fetchFn = (async () => ({ ok: false, status: 404, statusText: "Not Found" }) as unknown as Response) as typeof fetch;
    await expect(fetchLatestRelease("acme/resonux", { fetchFn })).rejects.toThrow(/404/);
  });

  it("fails honestly when the payload is not a release", async () => {
    const fetchFn = (async () => ({ ok: true, status: 200, json: async () => ({ message: "nope" }) }) as unknown as Response) as typeof fetch;
    await expect(fetchLatestRelease("acme/resonux", { fetchFn })).rejects.toThrow(/release/);
  });
});