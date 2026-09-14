import React, { useCallback, useEffect, useState } from "react";
import type { Snapshot, StatePayload, ThemesPayload, ThemeDefWire } from "../../src/domain/bridge";
import { Card, Button } from "../components";
import { EFFECT_LABELS } from "../../src/domain/types";
import { brightnessToWire, fmtBrightnessPct } from "../format";

// Lights: the per-strip control surface. Master brightness is the one master
// control (firmware keeps a single master value); each strip gets its own
// theme and its own preferred effect. Everything here is live — no config
// writes, no reboots.

export function Lights({ snapshot }: { snapshot: Snapshot }) {
  const sel = snapshot.selected;
  if (!sel) return <LightsEmpty snapshot={snapshot} />;
  return <SelectedLights key={sel.id} snapshot={snapshot} />;
}

function LightsEmpty({ snapshot }: { snapshot: Snapshot }) {
  return (
    <div style={{ marginTop: "var(--space-5)" }}>
      <div className="empty">
        <h3>No controller to light up yet</h3>
        <p>Pick a controller from Setup (the simulator works perfectly for this) to control lights.</p>
        <div className="row" style={{ justifyContent: "center" }}>
          <Button variant="primary" icon="search" onClick={() => void window.resonux.rescan()}>
            Find my controller
          </Button>
        </div>
      </div>
    </div>
  );
}

function SelectedLights({ snapshot }: { snapshot: Snapshot }) {
  const sel = snapshot.selected!;
  const [state, setState] = useState<StatePayload | null>(null);
  const [themes, setThemes] = useState<ThemesPayload | null>(null);
  const [brightness, setBrightness] = useState<number>(100);
  const [busy, setBusy] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);

  const refresh = useCallback(async () => {
    setError(null);
    try {
      const [st, th] = await Promise.all([
        window.resonux.getState(sel.id),
        window.resonux.getThemes(sel.id),
      ]);
      setState(st);
      setThemes(th);
      setBrightness(fmtBrightnessPct(st.brightness));
    } catch {
      setError("The controller did not answer in time. It may be asleep or unreachable.");
    }
  }, [sel.id]);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  const commitBrightness = async (pct: number) => {
    setBusy("brightness");
    try {
      const ok = await window.resonux.setBrightness(sel.id, brightnessToWire(pct));
      if (!ok) setError("The controller rejected the brightness change.");
    } catch {
      setError("The controller did not answer the brightness change.");
    }
    setBusy(null);
  };

  const changeStripTheme = async (strip: number, themeId: string) => {
    setBusy(`strip-${strip}`);
    try {
      const ok = await window.resonux.selectTheme(sel.id, themeId, strip);
      if (!ok) setError("The controller did not accept that theme for this strip.");
      else await refresh();
    } catch {
      setError("The controller did not answer the theme change.");
    }
    setBusy(null);
  };

  const changeStripEffect = async (strip: number, effectId: number) => {
    setBusy(`strip-${strip}`);
    try {
      const ok = await window.resonux.setStripEffect(sel.id, strip, effectId);
      if (!ok) setError("The controller did not accept that effect for this strip.");
      else await refresh();
    } catch {
      setError("The controller did not answer the effect change.");
    }
    setBusy(null);
  };

  if (!state || !themes) {
    return (
      <Card title="Lights">
        {error ? (
          <>
            <p className="hint">{error}</p>
            <Button variant="primary" onClick={() => void refresh()}>
              Try again
            </Button>
          </>
        ) : (
          <p className="hint">Loading light state…</p>
        )}
      </Card>
    );
  }

  const stripCount = state.stripCount || sel.status.stripCount;
  const themeById = new Map(themes.themes.map((t) => [t.id, t]));

  return (
    <>
      <section className="page-head">
        <h1>Lights</h1>
        <p>Controls for {sel.name} — strip by strip.</p>
        {error && <div className="error-banner" style={{ marginTop: "var(--space-3)" }}>{error}</div>}
      </section>

      <Card title="Master brightness" hint="One master control for the whole controller. Applies live.">
        <div className="slider-label">
          <span>Brightness</span>
          <b>{brightness}%</b>
        </div>
        <input
          type="range"
          min={0}
          max={100}
          value={brightness}
          disabled={busy === "brightness"}
          onChange={(e) => setBrightness(Number(e.target.value))}
          onPointerUp={(e) => void commitBrightness(Number((e.currentTarget as HTMLInputElement).value))}
          onKeyUp={(e) => {
            if (["ArrowLeft", "ArrowRight", "Home", "End"].includes(e.key)) {
              void commitBrightness(Number((e.currentTarget as HTMLInputElement).value));
            }
          }}
        />
      </Card>

      <div className="stat-grid">
        {Array.from({ length: Math.max(1, stripCount) }, (_, i) => {
          const stripThemeId = themes.strips[i] || themes.active;
          const themeDef = themeById.get(stripThemeId);
          const effectId = state.effect[i] ?? 1;
          return (
            <StripCard
              key={i}
              index={i}
              theme={themeDef}
              themeId={stripThemeId}
              themes={themes.themes}
              effectId={effectId}
              busy={busy === `strip-${i}`}
              onTheme={(tid) => void changeStripTheme(i, tid)}
              onEffect={(eid) => void changeStripEffect(i, eid)}
            />
          );
        })}
      </div>

      <p className="sub">Theme and effect changes take effect immediately. They're remembered by the controller across restarts.</p>
    </>
  );
}

function StripCard({
  index,
  theme,
  themeId,
  themes,
  effectId,
  busy,
  onTheme,
  onEffect,
}: {
  index: number;
  theme: ThemeDefWire | undefined;
  themeId: string;
  themes: ThemeDefWire[];
  effectId: number;
  busy: boolean;
  onTheme: (themeId: string) => void;
  onEffect: (effectId: number) => void;
}) {
  const effectLabel = EFFECT_LABELS[effectId] ?? `Effect ${effectId}`;
  const preferred = theme?.effects ?? [];

  return (
    <section className="card">
      <h2>Strip {index + 1}</h2>
      <div className="swatch-row">
        {(theme?.palette ?? ["#444"]).map((c) => (
          <span key={c} className="swatch" style={{ background: c }} />
        ))}
      </div>
      <label className="field">
        <span>Theme</span>
        <select
          className="input"
          value={themeId}
          disabled={busy}
          onChange={(e) => onTheme(e.target.value)}
        >
          {themes.map((t) => (
            <option key={t.id} value={t.id}>
              {t.name}
              {t.id === themeId ? " · now" : ""}
            </option>
          ))}
        </select>
      </label>
      <label className="field">
        <span>Effect · {effectLabel}</span>
        <select
          className="input"
          value={effectId}
          disabled={busy}
          onChange={(e) => onEffect(Number(e.target.value))}
        >
          {[effectId, ...preferred].filter((v, i, a) => a.indexOf(v) === i).map((eid) => (
            <option key={eid} value={eid}>
              {EFFECT_LABELS[eid] ?? `Effect ${eid}`}
            </option>
          ))}
        </select>
      </label>
    </section>
  );
}