import React, { useCallback, useEffect, useState } from "react";
import type { Snapshot, AudioSourcesPayload, AudioSourceOption } from "../../src/domain/bridge";
import { Card, Button, Switch } from "../components";

// Music: what the controller listens to. The controller figures out the best
// source by itself (auto-select) or you pick one explicitly. Nothing here
// touches configuration — source selection is live.

export function Music({ snapshot }: { snapshot: Snapshot }) {
  const sel = snapshot.selected;
  if (!sel) return <MusicEmpty snapshot={snapshot} />;
  return <SelectedMusic key={sel.id} snapshot={snapshot} />;
}

function MusicEmpty({ snapshot }: { snapshot: Snapshot }) {
  return (
    <div style={{ marginTop: "var(--space-5)" }}>
      <div className="empty">
        <h3>No controller to listen to</h3>
        <p>Pick a controller from Setup (the simulator works) and come back here to choose a music source.</p>
        <div className="row" style={{ justifyContent: "center" }}>
          <Button variant="primary" icon="search" onClick={() => void window.resonux.rescan()}>
            Find my controller
          </Button>
        </div>
      </div>
    </div>
  );
}

function SelectedMusic({ snapshot }: { snapshot: Snapshot }) {
  const sel = snapshot.selected!;
  const [payload, setPayload] = useState<AudioSourcesPayload | null>(null);
  const [busy, setBusy] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);

  const refresh = useCallback(async () => {
    setError(null);
    try {
      setPayload(await window.resonux.getAudioSources(sel.id));
    } catch {
      setError("The controller did not answer in time. It may be asleep or unreachable.");
    }
  }, [sel.id]);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  const pickSource = async (sourceId: number) => {
    setBusy(`src-${sourceId}`);
    try {
      const ok = await window.resonux.selectAudioSource(sel.id, sourceId);
      if (!ok) setError("The controller did not accept that source.");
      else await refresh();
    } catch {
      setError("The controller did not answer the source change.");
    }
    setBusy(null);
  };

  const toggleAuto = async (on: boolean) => {
    setBusy("auto");
    try {
      const ok = await window.resonux.setAutoSelect(sel.id, on);
      if (!ok) setError("The controller did not accept the auto-select change.");
      else await refresh();
    } catch {
      setError("The controller did not answer the auto-select change.");
    }
    setBusy(null);
  };

  if (!payload) {
    return (
      <Card title="Music">
        {error ? (
          <>
            <p className="hint">{error}</p>
            <Button variant="primary" onClick={() => void refresh()}>
              Try again
            </Button>
          </>
        ) : (
          <p className="hint">Loading audio sources…</p>
        )}
      </Card>
    );
  }

  const activeLabel = payload.sources.find((s) => s.id === payload.active)?.label ?? "not detected";

  return (
    <>
      <section className="page-head">
        <h1>Music</h1>
        <p>
          {sel.name} is currently listening to the <b>{activeLabel}</b>.
        </p>
        {error && <div className="error-banner" style={{ marginTop: "var(--space-3)" }}>{error}</div>}
      </section>

      <Card title="Source" hint="Pick what the controller listens to, or let it pick automatically.">
        <div className="stack">
          {payload.sources.map((s) => (
            <SourceRowView
              key={s.id}
              row={s}
              busy={busy === `src-${s.id}`}
              active={s.id === payload.active}
              onPick={() => void pickSource(s.id)}
            />
          ))}
        </div>
      </Card>

      <Card title="Auto-select" hint="Let the controller move to the best available source when one appears or disappears.">
        <div className="row">
          <span className="grow">
            Follow the best source automatically
            <div className="sub">
              {payload.autoSelect ? "On — falls back to the test tone if nothing else is available." : "Off — stays on whatever is selected."}
            </div>
          </span>
          <Switch checked={payload.autoSelect} onChange={(v) => void toggleAuto(v)} disabled={busy === "auto"} />
        </div>
      </Card>
    </>
  );
}

function SourceRowView({
  row,
  busy,
  active,
  onPick,
}: {
  row: AudioSourceOption;
  busy: boolean;
  active: boolean;
  onPick: () => void;
}) {
  return (
    <div className="row" style={{ padding: "var(--space-2) 0" }}>
      <span className="grow">
        {row.label}
        <div className="sub">{row.available ? (active ? "Active now" : "Available") : "No input detected"}</div>
      </span>
      {active ? (
        <span className="chip ok">
          <span className="dot" />
          Listening
        </span>
      ) : (
        <Button
          size="small"
          disabled={!row.available || busy}
          onClick={onPick}
        >
          Use this
        </Button>
      )}
    </div>
  );
}