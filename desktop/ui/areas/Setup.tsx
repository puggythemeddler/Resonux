import React, { useState } from "react";
import type { Snapshot } from "../../src/domain/bridge";
import { Card, Button, HealthChip, KindChip } from "../components";

interface SetupProps {
  snapshot: Snapshot;
}

export function Setup({ snapshot }: SetupProps) {
  const [scanning, setScanning] = useState(false);

  const rescan = async () => {
    setScanning(true);
    await window.resonux.rescan();
    setTimeout(() => setScanning(false), 800);
  };

  const discovered = snapshot.controllers.filter((c) => c.kind === "discover");
  const simulators = snapshot.controllers.filter((c) => c.kind === "simulator");

  return (
    <>
      <section className="page-head">
        <h1>Connect a controller</h1>
        <p>
          A few guided steps to link Control Center to your Resonux controller.
          No terminal, no cables into your laptop — just Wi-Fi and two buttons.
        </p>
      </section>

      <Card
        title="1 · Find your controller"
        hint="Your controller and this computer need to be on the same Wi-Fi network. If the controller shows a Resonux access point instead (grey unit with a hotspot), connect to it in Windows Wi-Fi settings first."
      >
        <div className="row">
          <Button variant="primary" icon="search" onClick={() => void rescan()} disabled={scanning}>
            {scanning ? "Scanning…" : "Scan now"}
          </Button>
          <span className="sub grow" style={{ marginTop: 0 }}>
            {discovered.length === 0
              ? "No controllers found yet."
              : `${discovered.length} controller${discovered.length === 1 ? "" : "s"} on this network.`}
          </span>
        </div>

        {discovered.length > 0 && (
          <div style={{ marginTop: "var(--space-4)" }}>
            {discovered.map((c) => (
              <button
                key={c.id}
                className={`controller-card${snapshot.selectedId === c.id ? " selected" : ""}`}
                style={{ width: "100%", marginBottom: "var(--space-2)", textAlign: "left" }}
                onClick={() => void window.resonux.selectController(c.id)}
              >
                <div className="name-row">
                  <span className="name">{c.name}</span>
                  <HealthChip health={c.health} offline={!c.online} />
                </div>
                <div className="addr">
                  {c.address}:{c.port}
                </div>
                <div className="meta">
                  <KindChip kind={c.kind} />
                  {c.fw && <span>firmware {c.fw}</span>}
                  {c.role && <span>{c.role}</span>}
                </div>
              </button>
            ))}
          </div>
        )}

        {discovered.length === 0 && !scanning && (
          <div className="empty" style={{ marginTop: "var(--space-4)" }}>
            <h3>Nothing here yet</h3>
            <p>
              Stay on this screen: discovery keeps listening, so a controller
              that comes online appears here automatically. Common fixes — is
              the controller powered? Is it on the same Wi-Fi? Did it fall back
              to hotspot mode?
            </p>
          </div>
        )}
      </Card>

      <Card
        title="2 · Not your controller yet?"
        hint="If your controller isn't here because it's in route, offline, or still in the box — practise with the built-in simulator. It behaves like a real controller (same REST surface) and is always labelled as a simulator."
      >
        <div className="row">
          <span className="grow">
            Simulator
            <div className="sub">{simulators.length > 0 ? "Running on this computer" : "Not running"}</div>
          </span>
          <Button
            icon="activity"
            onClick={() => void window.resonux.setSimulator(!snapshot.simulatorRunning)}
          >
            {snapshot.simulatorRunning ? "Stop" : "Start"}
          </Button>
        </div>
      </Card>

      <Card title="3 · Verified" hint="An honest connection check before you move on.">
        {snapshot.selected ? (
          <div className="row">
            <span className="grow">
              <b>{snapshot.selected.name}</b>
              <div className="sub">
                {snapshot.selected.address}:{snapshot.selected.port} · reachable and answering
              </div>
            </span>
            <HealthChip health="ok" />
          </div>
        ) : (
          <div className="sub">
            Pick a controller above. If it's a simulator, the check is against the
            in-process server — not a physical unit.
          </div>
        )}
      </Card>
    </>
  );
}