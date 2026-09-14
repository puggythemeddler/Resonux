import React, { useCallback, useEffect, useState } from "react";
import type { Snapshot } from "../../src/domain/bridge";
import type {
  CinematicPayload,
  DevicesPayload,
  DeviceRow,
  StatusSnapshot,
} from "../../src/domain/types";
import { Card, Button, Stat, Switch, HealthChip } from "../components";
import {
  fmtUptime,
  fmtBytes,
  brightnessToWire,
  fmtLatency,
  fmtClockOffset,
  fmtSystemState,
} from "../format";

interface HomeProps {
  snapshot: Snapshot;
}

export function Home({ snapshot }: HomeProps) {
  const selected = snapshot.selected;
  if (!selected) return <EmptyState snapshot={snapshot} />;
  return (
    <SelectedHome
      key={selected.id}
      id={selected.id}
      latencyMs={selected.latencyMs}
      status={selected.status}
    />
  );
}

function EmptyState({ snapshot }: { snapshot: Snapshot }) {
  return (
    <div style={{ marginTop: "var(--space-5)" }}>
      <div className="empty">
        <h3>No controller connected yet</h3>
        <p>
          Resonux Control Center talks to your Resonux controller over your
          local network. If you have one powered on and on the same Wi-Fi, find
          it — or practise with the built-in simulator while you get set up.
        </p>
        <div className="row" style={{ justifyContent: "center" }}>
          <Button variant="primary" icon="search" onClick={() => void window.resonux.rescan()}>
            Find my controller
          </Button>
          {!snapshot.simulatorRunning && (
            <Button icon="activity" onClick={() => void window.resonux.setSimulator(true)}>
              Start the simulator
            </Button>
          )}
        </div>
      </div>
      {snapshot.controllers.length > 0 && (
        <div className="controller-grid" style={{ marginTop: "var(--space-4)" }}>
          {snapshot.controllers.map((c) => (
            <button
              key={c.id}
              className="controller-card"
              onClick={() => void window.resonux.selectController(c.id)}
            >
              <div className="name-row">
                <span className="name">{c.name}</span>
                {c.kind === "simulator" && <HealthChip health="ok" />}
                {c.kind !== "simulator" && <HealthChip health={c.health} offline={!c.online} />}
              </div>
              <div className="addr">
                {c.address}:{c.port}
              </div>
              <div className="meta">
                {c.kind === "simulator" ? "Simulator" : "Discovered"}
                {c.fw && <span>fw {c.fw}</span>}
              </div>
            </button>
          ))}
        </div>
      )}
    </div>
  );
}

function SelectedHome({
  id,
  latencyMs,
  status,
}: {
  id: string;
  latencyMs: number | undefined;
  status: StatusSnapshot;
}) {
  const [brightness, setBrightness] = useState<number>(() => 100);
  const [cinematic, setCinematic] = useState<boolean>(true);
  const [busy, setBusy] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [devices, setDevices] = useState<DevicesPayload | null>(null);

  const refresh = useCallback(async () => {
    try {
      const [cin, devs] = await Promise.all([
        window.resonux.getCinematic(id) as Promise<CinematicPayload>,
        window.resonux.getDevices(id),
      ]);
      setCinematic(cin.active);
      setDevices(devs);
    } catch {
      setError("The controller did not answer in time. It may be asleep or unreachable.");
    }
  }, [id]);

  useEffect(() => {
    setError(null);
    void refresh();
  }, [refresh]);

  const onBrightnessCommit = async (pct: number) => {
    setBusy("brightness");
    try {
      const ok = await window.resonux.setBrightness(id, brightnessToWire(pct));
      if (!ok) setError("The controller rejected the brightness change.");
    } catch {
      setError("The controller did not answer the brightness change.");
    }
    setBusy(null);
  };

  const onCinematicToggle = async (on: boolean) => {
    setBusy("cinematic");
    try {
      const ok = await window.resonux.setCinematic(id, on);
      if (!ok) setError("The controller did not accept the cinematic change.");
      else setCinematic(on);
    } catch {
      setError("The controller did not answer the cinematic change.");
    }
    setBusy(null);
  };

  const sync = status.sync;
  const wifi = status.wifi;

  return (
    <>
      <section className="card">
        <div style={{ display: "flex", alignItems: "center", gap: "var(--space-3)", marginBottom: "var(--space-4)" }}>
          <h2 style={{ margin: 0 }}>{status.device || "Controller"}</h2>
          <HealthChip health={status.heap < 8192 ? "degraded" : "ok"} />
        </div>
        <div className="stat-grid">
          <Stat k="Frames / sec" v={status.fps} highlight />
          <Stat k="Up time" v={fmtUptime(status.uptimeMs)} />
          <Stat k="Free memory" v={fmtBytes(status.heap)} />
          <Stat k="Strips" v={status.stripCount} />
          <Stat k="Latency" v={fmtLatency(latencyMs)} />
          <Stat k="System" v={fmtSystemState(status.system.state)} />
        </div>
        <p className="sub">
          {wifi.connected ? "Connected to " : "Access point "}
          {wifi.ip}
          {sync.enabled && sync.role === "slave" && ` · ${fmtClockOffset(sync.offsetMs)} to master`}
        </p>
      </section>

      {error && <div className="error-banner">{error}</div>}

      <Card
        title="Master tuning"
        hint="Adjusts the whole controller. Changes apply live — no reboot needed."
      >
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
          onPointerUp={(e) =>
            void onBrightnessCommit(Number((e.currentTarget as HTMLInputElement).value))
          }
          onKeyUp={(e) => {
            if (
              e.key === "ArrowLeft" ||
              e.key === "ArrowRight" ||
              e.key === "Home" ||
              e.key === "End"
            ) {
              void onBrightnessCommit(Number((e.currentTarget as HTMLInputElement).value));
            }
          }}
        />
      </Card>

      <Card title="Cinematic mode" hint="Companion-driven scene reaction. Toggle drives the running mode.">
        <div className="row">
          <span className="grow">
            Cinematic reactive scene engine
            <div className="sub">Active: {cinematic ? "yes" : "no"}</div>
          </span>
          <Switch
            checked={cinematic}
            onChange={(v) => void onCinematicToggle(v)}
            disabled={busy === "cinematic"}
          />
        </div>
      </Card>

      <Card title="Detected inputs" hint="What the controller currently sees on its input side.">
        {devices && devices.devices.length > 0 ? (
          devices.devices.map((d) => <DeviceRowView key={d.id} row={d} />)
        ) : (
          <div className="sub">No inputs detected.</div>
        )}
      </Card>
    </>
  );
}

function DeviceRowView({ row }: { row: DeviceRow }) {
  return (
    <div className="row" style={{ marginTop: "var(--space-2)" }}>
      <span className="grow">
        {row.name}
        <div className="sub">
          {row.connectionLabel} · {row.statusLabel}
        </div>
      </span>
      {row.needsConditioning && (
        <span
          className="chip degraded"
          title="This input is still settling after being detected. It normally clears by itself within a moment."
        >
          <span className="dot" />
          Conditioning
        </span>
      )}
    </div>
  );
}