import React, { useEffect, useState } from "react";
import type { Snapshot, HardwareCheckReport, CheckStepResult, ExportReportResult } from "../../src/domain/bridge";
import type { CheckVerdict } from "../../src/domain/types";
import { Card, Button, HealthChip, KindChip, ErrorDetail, ConfirmDialog } from "../components";
import { Icon } from "../icons";

interface DiagnosticsProps {
  snapshot: Snapshot;
}

const STEP_ORDER: CheckStepResult["id"][] = ["reachability", "rail", "leds", "audio", "write"];

type SystemAction = "restart" | "power_off";
interface SystemOutcome {
  kind: SystemAction;
  ok: boolean;
  cameBack?: boolean;
  detail?: string;
}

function StepIcon({ verdict }: { verdict: CheckVerdict | "asked" }) {
  if (verdict === "pass") return <Icon name="check" size={16} className="check-icon pass" />;
  if (verdict === "asked") return <Icon name="moon" size={16} className="check-icon asked" />;
  return <Icon name="alert" size={16} className={`check-icon ${verdict}`} />;
}

export function Diagnostics({ snapshot }: DiagnosticsProps) {
  const initial = snapshot.selectedId ?? snapshot.controllers[0]?.id ?? null;
  const [targetId, setTargetId] = useState<string | null>(initial);
  const [running, setRunning] = useState(false);
  const [report, setReport] = useState<HardwareCheckReport | null>(null);
  const [error, setError] = useState<{ what: string; hint?: string; detail?: string } | null>(null);
  const [sawLights, setSawLights] = useState<boolean | null>(null);
  const [confirm, setConfirm] = useState<SystemAction | null>(null);
  const [systemBusy, setSystemBusy] = useState(false);
  const [systemOutcome, setSystemOutcome] = useState<SystemOutcome | null>(null);
  const [exporting, setExporting] = useState(false);
  const [exportResult, setExportResult] = useState<ExportReportResult | null>(null);

  // Follow the selected controller when it changes elsewhere (e.g. setup).
  useEffect(() => {
    if (snapshot.selectedId) setTargetId(snapshot.selectedId);
  }, [snapshot.selectedId]);

  const target = snapshot.controllers.find((c) => c.id === targetId) ?? null;

  const run = async () => {
    if (!targetId) return;
    setRunning(true);
    setError(null);
    setReport(null);
    setSawLights(null);
    setExportResult(null);
    try {
      const rep = await window.resonux.runHardwareCheck(targetId);
      setReport(rep);
    } catch (err) {
      setError({
        what: "The check couldn't run.",
        hint: "Make sure the controller is powered and on the same Wi-Fi, then try again.",
        detail: err instanceof Error ? err.message : String(err),
      });
    } finally {
      setRunning(false);
    }
  };

  const doRestart = async () => {
    if (!targetId) return;
    setConfirm(null);
    setSystemBusy(true);
    setSystemOutcome(null);
    try {
      const res = await window.resonux.restartController(targetId);
      const cameBack = res.ok ? await window.resonux.waitOnline(targetId, 20000) : false;
      setSystemOutcome({ kind: "restart", ok: res.ok, cameBack, detail: res.detail });
    } catch (err) {
      setSystemOutcome({
        kind: "restart",
        ok: false,
        detail: err instanceof Error ? err.message : String(err),
      });
    } finally {
      setSystemBusy(false);
    }
  };

  const doPowerOff = async () => {
    if (!targetId) return;
    setConfirm(null);
    setSystemBusy(true);
    setSystemOutcome(null);
    try {
      const res = await window.resonux.powerOff(targetId);
      setSystemOutcome({ kind: "power_off", ok: res.ok, detail: res.detail });
    } catch (err) {
      setSystemOutcome({
        kind: "power_off",
        ok: false,
        detail: err instanceof Error ? err.message : String(err),
      });
    } finally {
      setSystemBusy(false);
    }
  };

  const doExport = async () => {
    if (!targetId || !report) return;
    setExporting(true);
    setExportResult(null);
    try {
      const res = await window.resonux.exportReport(targetId, report);
      setExportResult(res);
    } catch (err) {
      setExportResult({
        saved: false,
        detail: err instanceof Error ? err.message : String(err),
      });
    } finally {
      setExporting(false);
    }
  };

  const humanSteps: CheckStepResult[] =
    sawLights === null
      ? []
      : [
          {
            id: "write",
            label: "Your lights responded",
            verdict: sawLights ? "pass" : "warn",
            detail: sawLights
              ? "You saw the brightness change — the LEDs are really glowing."
              : "You didn't see anything. The controller accepted the command, so check the strip wiring and power.",
          },
        ];

  const steps = report ? [...report.steps, ...humanSteps] : [];
  const fails = report ? report.summary.fail + (sawLights === false ? 1 : 0) : 0;
  const warns = report ? report.summary.warn + (sawLights === false ? 0 : 0) : 0;
  const overall: CheckVerdict = fails > 0 ? "fail" : warns > 0 ? "warn" : "pass";
  const verdictTitle =
    overall === "pass" ? "All clear" : overall === "warn" ? "Mostly fine" : "Something's off";
  const verdictText =
    overall === "pass"
      ? "Every automated step passed — and with your eyes, so did the lights."
      : overall === "warn"
        ? "The controller answered everything, but a few things deserve a look before you trust the setup."
        : "One or more steps failed. Start with the red ones below.";

  if (snapshot.controllers.length === 0) {
    return (
      <section className="page-head">
        <h1>Diagnostics</h1>
        <div className="empty" style={{ marginTop: "var(--space-4)" }}>
          <h3>No controller to check yet</h3>
          <p>Connect a controller in Setup first — the simulator works too.</p>
        </div>
      </section>
    );
  }

  return (
    <>
      <section className="page-head">
        <h1>Guided hardware check</h1>
        <p>
          A handful of plain-language tests to confirm your Resonux controller is
          really alive, listening, and in control of its lights. It only uses the
          controller&apos;s normal answers — nothing destructive.
        </p>
      </section>

      <Card title="1 · Pick a controller" hint="The check runs against whichever controller you select here.">
        <div style={{ marginTop: "var(--space-4)" }}>
          {snapshot.controllers.map((c) => (
            <button
              key={c.id}
              className={`controller-card${targetId === c.id ? " selected" : ""}`}
              style={{ width: "100%", marginBottom: "var(--space-2)", textAlign: "left" }}
              onClick={() => {
                setTargetId(c.id);
                setReport(null);
                setSawLights(null);
              }}
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
              </div>
            </button>
          ))}
        </div>
      </Card>

      <Card
        title="2 · Run the check"
        hint="Takes a few seconds. The controller's brightness blips up for a moment near the end — that is the part where you watch the lights."
      >
        <div className="row">
          <Button variant="primary" icon="activity" onClick={() => void run()} disabled={!target || running}>
            {running ? "Checking…" : report ? "Check again" : "Run the check"}
          </Button>
          {target && (
            <span className="sub grow" style={{ marginTop: 0 }}>
              Against <b>{target.name}</b> at {target.address}:{target.port}
            </span>
          )}
        </div>
        {running && (
          <p className="hint" style={{ marginTop: "var(--space-3)" }}>
            Reachability → rail → lights → sound → live write. Keep an eye on the lights for the blip.
          </p>
        )}
        {error && (
          <div style={{ marginTop: "var(--space-3)" }}>
            <ErrorDetail what={error.what} hint={error.hint} detail={error.detail} onRetry={() => void run()} />
          </div>
        )}
      </Card>

      {report && !running && (
        <Card title="3 · Results">
          <div className={`verdict ${overall}`}>
            <div className="verdict-title">
              <Icon name={overall === "pass" ? "check" : "alert"} size={18} />
              {verdictTitle}
            </div>
            <div className="sub">{verdictText}</div>
          </div>

          <ul className="check-list">
            {steps.map((s, i) => (
              <li key={i} className={`check-step ${s.verdict}`}>
                <StepIcon verdict={s.verdict} />
                <div className="grow">
                  <div className="check-label">{s.label}</div>
                  <div className="sub">{s.detail}</div>
                </div>
              </li>
            ))}
          </ul>

          {sawLights === null && report && (
            <div className="ask-eyes">
              <div className="check-label">
                Your turn — did you see the lights change?
              </div>
              <div className="sub">
                {report.visual
                  ? `The controller ran brightness ${report.visual.from} → ${report.visual.to}, then back.`
                  : "The controller answered — but only your eyes can confirm the LEDs are actually glowing."}
              </div>
              <div className="row" style={{ marginTop: "var(--space-3)" }}>
                <Button icon="check" onClick={() => setSawLights(true)}>
                  Yes, I saw it
                </Button>
                <Button icon="alert" onClick={() => setSawLights(false)}>
                  Nothing changed
                </Button>
              </div>
            </div>
          )}

          {report.note && <p className="hint">{report.note}</p>}

          <div className="row" style={{ justifyContent: "flex-start", marginTop: "var(--space-4)" }}>
            <Button variant="ghost" icon="save" onClick={() => void doExport()} disabled={exporting}>
              {exporting ? "Exporting…" : "Export a report"}
            </Button>
            <span className="sub grow">
              Facts only — no credentials, passwords, or network addresses.
            </span>
          </div>
          {exportResult && !exportResult.saved && (
            <div style={{ marginTop: "var(--space-3)" }}>
              <ErrorDetail
                what="The report couldn't be saved."
                hint="Choose a location that lets the app write, then try again."
                detail={exportResult.detail}
                onRetry={() => void doExport()}
              />
            </div>
          )}
          {exportResult?.saved && exportResult.filePath && (
            <p className="hint" style={{ marginTop: "var(--space-3)" }}>
              Saved to <code>{exportResult.filePath}</code>
            </p>
          )}
        </Card>
      )}

      <Card
        title="4 · System controls"
        hint="Both commands ask the controller to shut down gracefully. Restart is safe anytime; power-off puts the unit to sleep until its wake pin or the reset button wakes it."
      >
        <div className="row">
          <Button icon="power" onClick={() => setConfirm("restart")} disabled={!target || systemBusy}>
            {systemOutcome?.kind === "restart" || systemBusy ? "Restarting…" : "Restart controller"}
          </Button>
          <Button
            variant="danger"
            icon="power"
            onClick={() => setConfirm("power_off")}
            disabled={!target || systemBusy}
          >
            Power off
          </Button>
          {systemBusy && (
            <span className="sub grow">
              Waiting for the controller to respond — this can take a moment.
            </span>
          )}
        </div>

        {systemOutcome && systemOutcome.kind === "restart" && (
          <p className="hint" style={{ marginTop: "var(--space-3)" }}>
            {!systemOutcome.ok
              ? `The controller didn't confirm the restart.`
              : systemOutcome.cameBack
                ? "The controller restarted and is back online."
                : "The restart was accepted, but the controller didn't come back within 20 seconds."}
            {systemOutcome.detail && ` (${systemOutcome.detail})`}
          </p>
        )}
        {systemOutcome && systemOutcome.kind === "power_off" && (
          <p className="hint" style={{ marginTop: "var(--space-3)" }}>
            {systemOutcome.ok
              ? "The controller is powering off. Wake it with the reset button or its configured wake pin."
              : `The controller didn't confirm power-off.${systemOutcome.detail ? ` (${systemOutcome.detail})` : ""}`}
          </p>
        )}

        <ConfirmDialog
          open={confirm === "restart"}
          title="Restart this controller?"
          body={
            <>
              <b>{target?.name}</b> will shut down and boot back up. Its lights
              will go out for a moment, then return.
            </>
          }
          confirmLabel="Restart"
          onConfirm={() => void doRestart()}
          onCancel={() => setConfirm(null)}
        />
        <ConfirmDialog
          open={confirm === "power_off"}
          title="Power this controller off?"
          body={
            <>
              <b>{target?.name}</b> will shut down into deep sleep. Its lights
              will stop until you wake it with the reset button or a configured
              wake pin.
            </>
          }
          confirmLabel="Power off"
          danger
          onConfirm={() => void doPowerOff()}
          onCancel={() => setConfirm(null)}
        />
      </Card>

      <Card
        title="Session log"
        hint="What the Control Center has done during this session. Kept in memory only — nothing here is a password, token, or network address."
      >
        {snapshot.log.length === 0 ? (
          <p className="sub">Nothing logged yet this session.</p>
        ) : (
          <ul className="log-list">
            {snapshot.log
              .slice(-30)
              .reverse()
              .map((e) => (
                <li key={e.seq} className={`log-item ${e.level}`}>
                  <span className="log-when">{new Date(e.at).toLocaleTimeString([], { hour12: false })}</span>
                  <span className={`log-chip ${e.level}`}>{e.level}</span>
                  <span className="grow">
                    {e.message}
                    {e.controllerName && <span className="sub"> — {e.controllerName}</span>}
                  </span>
                </li>
              ))}
          </ul>
        )}
      </Card>
    </>
  );
}