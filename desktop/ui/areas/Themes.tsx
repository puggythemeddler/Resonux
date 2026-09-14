import React, { useCallback, useEffect, useState } from "react";
import type { Snapshot, ThemesPayload, ThemeDefWire } from "../../src/domain/bridge";
import { Card, Button } from "../components";

// Themes: a visual, swatch-first browser. Apply a theme across the whole
// controller or pin one to an individual strip. Themes live on the controller
// itself — the desktop app is just browsing what the firmware already has.

export function Themes({ snapshot }: { snapshot: Snapshot }) {
  const sel = snapshot.selected;
  if (!sel) return <ThemesEmpty snapshot={snapshot} />;
  return <SelectedThemes key={sel.id} snapshot={snapshot} />;
}

function ThemesEmpty({ snapshot }: { snapshot: Snapshot }) {
  return (
    <div style={{ marginTop: "var(--space-5)" }}>
      <div className="empty">
        <h3>No controller to theme</h3>
        <p>Pick a controller from Setup (the simulator ships its own themes) and you can browse them here.</p>
        <div className="row" style={{ justifyContent: "center" }}>
          <Button variant="primary" icon="search" onClick={() => void window.resonux.rescan()}>
            Find my controller
          </Button>
        </div>
      </div>
    </div>
  );
}

function SelectedThemes({ snapshot }: { snapshot: Snapshot }) {
  const sel = snapshot.selected!;
  const [payload, setPayload] = useState<ThemesPayload | null>(null);
  const [busy, setBusy] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);

  const refresh = useCallback(async () => {
    setError(null);
    try {
      setPayload(await window.resonux.getThemes(sel.id));
    } catch {
      setError("The controller did not answer in time. It may be asleep or unreachable.");
    }
  }, [sel.id]);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  const applyGlobally = async (id: string) => {
    setBusy(`global-${id}`);
    try {
      const ok = await window.resonux.selectTheme(sel.id, id);
      if (!ok) setError("The controller did not accept the theme.");
      else await refresh();
    } catch {
      setError("The controller did not answer the theme change.");
    }
    setBusy(null);
  };

  if (!payload) {
    return (
      <Card title="Themes">
        {error ? (
          <>
            <p className="hint">{error}</p>
            <Button variant="primary" onClick={() => void refresh()}>
              Try again
            </Button>
          </>
        ) : (
          <p className="hint">Loading themes…</p>
        )}
      </Card>
    );
  }

  const stripCount = sel.status.stripCount;
  const stripTheme = (i: number) => payload.strips[i] || payload.active;

  return (
    <>
      <section className="page-head">
        <h1>Themes</h1>
        <p>
          Currently active across {sel.name}: <b>{payload.active}</b>.
        </p>
        {error && <div className="error-banner" style={{ marginTop: "var(--space-3)" }}>{error}</div>}
      </section>

      {Array.from({ length: Math.max(1, stripCount) }, (_, i) => (
        <PinRow
          key={i}
          index={i}
          themeId={payload.strips[i]}
          activeThemeId={payload.active}
        />
      ))}

      <div className="theme-grid">
        {payload.themes.map((t) => (
          <ThemeCard
            key={t.id}
            theme={t}
            active={t.id === payload.active}
            busy={busy === `global-${t.id}`}
            onApply={() => void applyGlobally(t.id)}
            stripCount={stripCount}
            stripTheme={(i) => stripTheme(i)}
          />
        ))}
      </div>
    </>
  );
}

function PinRow({
  index,
  themeId,
  activeThemeId,
}: {
  index: number;
  themeId: string;
  activeThemeId: string;
}) {
  return (
    <div className="card" style={{ display: "flex", alignItems: "center", gap: "var(--space-3)", paddingTop: "var(--space-3)", paddingBottom: "var(--space-3)" }}>
      <span className="grow">
        Strip {index + 1}
        <div className="sub">
          {themeId ? `Pinned to its own theme: ${themeId}` : "Follows the controller-wide theme"}
        </div>
      </span>
      <span className="sub" style={{ maxWidth: "28ch", whiteSpace: "nowrap", overflow: "hidden", textOverflow: "ellipsis" }}>
        global: {activeThemeId}
      </span>
    </div>
  );
}

function ThemeCard({
  theme,
  active,
  busy,
  onApply,
  stripCount,
  stripTheme,
}: {
  theme: ThemeDefWire;
  active: boolean;
  busy: boolean;
  onApply: () => void;
  stripCount: number;
  stripTheme: (i: number) => string;
}) {
  const pinnedStrips = Array.from({ length: stripCount }, (_, i) => i).filter((i) => stripTheme(i) === theme.id);
  return (
    <section className={"card theme-card" + (active ? " active" : "")}>
      <div className="swatch-row">
        {theme.palette.map((c) => (
          <span key={c} className="swatch grow-swatch" style={{ background: c }} />
        ))}
      </div>
      <h2 style={{ margin: "var(--space-2) 0 0" }}>{theme.name}</h2>
      <p className="sub">
        {theme.builtin ? "Built-in" : "Your theme"} · base brightness {theme.brightness.base}%
        {theme.description && <span> — {theme.description}</span>}
      </p>
      {active ? (
        <span className="chip ok">
          <span className="dot" />
          Active
        </span>
      ) : (
        <Button variant="primary" size="small" disabled={busy} onClick={onApply}>
          Use everywhere
        </Button>
      )}
      {pinnedStrips.length > 0 && (
        <div className="sub" style={{ marginTop: "var(--space-2)" }}>
          Pinned to: {pinnedStrips.map((i) => `strip ${i + 1}`).join(", ")}
        </div>
      )}
    </section>
  );
}