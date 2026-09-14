import React, { useEffect, useState } from "react";
import type { Snapshot, HardwareCheckReport, CheckStepResult } from "../../src/domain/bridge";
import type { CheckVerdict } from "../../src/domain/types";
import { Card, Button, HealthChip, KindChip, ErrorDetail } from "../components";
import { Icon } from "../icons";

interface DiagnosticsProps {
  snapshot: Snapshot;
}

const STEP_ORDER: CheckStepResult["id"][] = ["reachability", "rail", "leds", "audio", "write"];

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
        </Card>
      )}
    </>
  );
}