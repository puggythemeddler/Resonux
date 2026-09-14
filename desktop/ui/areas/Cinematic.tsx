import React, { useCallback, useEffect, useState } from "react";
import type { Snapshot, CinematicPayload } from "../../src/domain/bridge";
import { Card, Button, Switch, HealthChip, Stat } from "../components";
import { fmtLatency } from "../format";

// Cinematic: the reactive scene engine. This surface reads live scene state
// and lets the user toggle the mode and fire a QA test burst. Deep scene
// shaping stays in the firmware/companion — here we keep it tasteful.

export function Cinematic({ snapshot }: { snapshot: Snapshot }) {
  const sel = snapshot.selected;
  if (!sel) return <CinematicEmpty snapshot={snapshot} />;
  return <SelectedCinematic key={sel.id} snapshot={snapshot} />;
}

function CinematicEmpty({ snapshot }: { snapshot: Snapshot }) {
  return (
    <div style={{ marginTop: "var(--space-5)" }}>
      <div className="empty">
        <h3>No controller to run scenes on</h3>
        <p>Pick a controller from Setup (the simulator works) and the cinematic engine shows up here.</p>
        <div className="row" style={{ justifyContent: "center" }}>
          <Button variant="primary" icon="search" onClick={() => void window.resonux.rescan()}>
            Find my controller
          </Button>
        </div>
      </div>
    </div>
  );
}

function SelectedCinematic({ snapshot }: { snapshot: Snapshot }) {
  const sel = snapshot.selected!;
  const [payload, setPayload] = useState<CinematicPayload | null>(null);
  const [busy, setBusy] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);

  const refresh = useCallback(async () => {
    setError(null);
    try {
      setPayload(await window.resonux.getCinematic(sel.id));
    } catch {
      setError("The controller did not answer in time. It may be asleep or unreachable.");
    }
  }, [sel.id]);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  const toggleActive = async (on: boolean) => {
    setBusy("toggle");
    try {
      const ok = await window.resonux.setCinematic(sel.id, on);
      if (!ok) setError("The controller did not accept the cinematic change.");
      else await refresh();
    } catch {
      setError("The controller did not answer the cinematic change.");
    }
    setBusy(null);
  };

  const fireTest = async () => {
    setBusy("test");
    try {
      const ok = await window.resonux.triggerCinematicTest(sel.id);
      if (!ok) setError("The controller did not accept the test burst.");
    } catch {
      setError("The controller did not answer the test request.");
    }
    setBusy(null);
  };

  if (!payload) {
    return (
      <Card title="Cinematic">
        {error ? (
          <>
            <p className="hint">{error}</p>
            <Button variant="primary" onClick={() => void refresh()}>
              Try again
            </Button>
          </>
        ) : (
          <p className="hint">Loading cinematic state…</p>
        )}
      </Card>
    );
  }

  const st = payload.status;

  return (
    <>
      <section className="page-head">
        <h1>Cinematic</h1>
        <p>{sel.name} — scene reaction engine.</p>
        {error && <div className="error-banner" style={{ marginTop: "var(--space-3)" }}>{error}</div>}
      </section>

      <Card title="Engine" hint="Reactive scenes driven by the companion over the local network.">
        <div className="row">
          <span className="grow">
            Cinematic mode
            <div className="sub">
              {payload.active ? "Running — reacting to what it hears and sees." : "Paused — lights stay on their theme."}
            </div>
          </span>
          <Switch checked={payload.active} onChange={(v) => void toggleActive(v)} disabled={busy === "toggle"} />
        </div>

        <div className="stat-grid" style={{ marginTop: "var(--space-4)" }}>
          <Stat k="Scene" v={st.sceneId || formatId(st.scene)} />
          <Stat k="Mood" v={st.moodLabel || formatId(st.mood)} />
          <Stat k="Energy" v={bar(st.moodEnergy)} />
          <Stat k="Tension" v={bar(st.tension)} />
          <Stat k="Audio" v={bar(st.audioLevel)} />
          <Stat k="Boom" v={st.boom ? "yes" : "no"} highlight={st.boom} />
          <Stat k="Events/min" v={st.recentEvents} />
          <Stat k="Link" v={fmtLatency(st.linkLatencyMs)} />
        </div>

        <div className="row" style={{ marginTop: "var(--space-3)" }}>
          <span className="grow">
            Companion source
            <div className="sub">
              {payload.companionSource || "none"}
              {payload.companionAlive ? " · alive" : " · not answering"}
            </div>
          </span>
          {payload.companionAlive ? (
            <HealthChip health="ok" />
          ) : (
            <HealthChip health="offline" offline />
          )}
        </div>
      </Card>

      <Card title="Test burst" hint="Fires a short QA pattern the firmware runs for you — a safe way to see how it reacts.">
        <Button icon="activity" disabled={busy === "test"} onClick={() => void fireTest()}>
          Run a test burst
        </Button>
        <p className="sub" style={{ marginTop: "var(--space-2)" }}>
          The lights will flash through a canned scene so you can verify the rig and the reactive path end to end.
        </p>
      </Card>
    </>
  );
}

function bar(v: number): string {
  const n = Math.round(Math.min(1, Math.max(0, v)) * 10);
  return `${n}/10`;
}

function formatId(n: number): string {
  return `#${n}`;
}