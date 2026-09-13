# Agent Instructions — Resonux

Commit etiquette and standing rules for any work done on this repository.

## Documentation must move with code

Every code, firmware, web, or tooling change must be accompanied by updates to
the relevant documentation in the **same commit**:

- **All READMEs** — every `README.md` in the repo (root, and any under firmware/,
  web/, tools/, etc.), wherever the touched feature/section appears.
- `docs/*.md` — the docs that describe the touched area (e.g. feature changes
  → `docs/CINEMATIC.md` / `docs/WEB_DASHBOARD.md` / `docs/TOUCHSCREEN.md`,
  test changes → `docs/TESTING.md`, architecture → `docs/ARCHITECTURE.md`,
  plan/status → `docs/ROADMAP.md`).

Whenever a host-test count changes, search the whole repo for the old count
(patterns like `128 tests`, `84 tests`, `128 pass`) and update *every*
occurrence in every README + all docs. Always re-verify with
`Select-String -Path **/README.md,docs/*.md` before committing.

## General rules

- Verify before committing: `pio test -e native` (host tests), the affected
  `pio run` firmware env(s), and `web` `tsc --noEmit` + `npm run build` when
  web files change.
- Never amend commits; fix forward or leave as-is (e.g. the typo in `f4b84f4`).
- Only commit/push when the user explicitly asks, unless the change was already
  in flight.
- Small, focused commits with clear messages following the existing style.