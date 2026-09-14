import React, { useState } from "react";
import type { Snapshot } from "../../src/domain/bridge";
import { Icon, Logo } from "../icons";
import { Button, HealthChip, KindChip, ErrorDetail } from "../components";

// First-run setup guide. Plain-language, no terminal, no jargon — every step
// either advances or explains honestly why it can't. The wizard never touches
// a config directly: rename and Wi-Fi go through AppCore, which backs up the
// current config byte-exact before the write and waits for the controller to
// come back online (a real config change reboots the unit).
//
// The simulator can stand in for a controller throughout — it is labelled as
// a simulator at every step, never disguised as hardware.

type Step = "welcome" | "find" | "connect" | "name" | "wifi" | "done";

const STEP_ORDER: Step[] = ["welcome", "find", "connect", "name", "wifi", "done"];

const STEP_TITLES: Record<Step, string> = {
  welcome: "Welcome",
  find: "Find your controller",
  connect: "Connect",
  name: "Name it",
  wifi: "Wi-Fi",
  done: "All set",
};

interface Stick {
  what: string;
  hint: string;
  detail?: string;
}

export function Wizard({ snapshot, onClose }: { snapshot: Snapshot; onClose: () => void }) {
  const [step, setStep] = useState<Step>("welcome");
  const [busy, setBusy] = useState<string | null>(null);
  const [stick, setStick] = useState<Stick | null>(null);
  const [name, setName] = useState("");

  const sel = snapshot.selected;
  const discovered = snapshot.controllers.filter((c) => c.kind === "discover");
  const stepIndex = STEP_ORDER.indexOf(step);

  const go = (to: Step) => {
    setStep(to);
    setStick(null);
    setBusy(null);
  };
  const back = () => {
    if (stepIndex > 0) go(STEP_ORDER[stepIndex - 1]);
  };

  const verify = async () => {
    if (!sel) return;
    setBusy("Verifying");
    setStick(null);
    try {
      const status = await window.resonux.getStatus(sel.id);
      if (!status.ok) throw new Error("controller reports not ok");
      go("name");
      setName(status.device || sel.name);
    } catch {
      setBusy(null);
      setStick({
        what: "Couldn't reach this controller.",
        hint:
          "Make sure it's powered, on the same Wi-Fi as this computer, and still listed as online. Discovery keeps listening, so a controller that comes back appears by itself.",
      });
    }
  };

  const saveName = async () => {
    if (!sel || !name.trim()) return;
    setBusy("Saving");
    setStick(null);
    const res = await window.resonux.renameController(sel.id, name);
    if (!res.ok) {
      setBusy(null);
      setStick({
        what: "The name wasn't saved.",
        hint: "The controller didn't accept the change. The app kept a backup of the last working config, so nothing is lost.",
        detail: res.detail,
      });
      return;
    }
    setBusy("Restarting the controller");
    const online = await window.resonux.waitOnline(sel.id, 20000);
    setBusy(null);
    if (!online) {
      setStick({
        what: "The controller hasn't come back yet.",
        hint: "Renaming restarts the unit, which normally takes a few seconds. If it stays away, power-cycle it — the new name is saved. You can re-run this guide from Setup.",
      });
      return;
    }
    go("wifi");
  };

  return (
    <div className="wizard-overlay">
      <div className="wizard" role="dialog" aria-modal="true" aria-label="Setup guide">
        <header className="wizard-head">
          <div className="wizard-brand">
            <Logo size={26} className="brand-mark" />
            <div>
              <div className="wizard-title">Set up a controller</div>
              <div className="sub">{STEP_TITLES[step]}</div>
            </div>
          </div>
          <div className="wizard-steps" aria-label="Progress">
            {STEP_ORDER.map((s, i) => (
              <span key={s} className={`wstep${i <= stepIndex ? " on" : ""}${s === step ? " cur" : ""}`} />
            ))}
          </div>
          {busy && (
            <span className="chip busy">
              <span className="spin" />
              {busy}
            </span>
          )}
        </header>

        <div className="wizard-body">
          {step === "welcome" && <Welcome onStart={() => go("find")} />}

          {step === "find" && (
            <section>
              <h2>Find your controller</h2>
              <p className="hint">
                Your controller and this computer need to be on the same Wi-Fi. The app finds controllers by
                itself — you pick one.
              </p>
              <div className="row">
                <Button variant="primary" icon="search" onClick={() => void window.resonux.rescan()}>
                  Scan now
                </Button>
                <span className="sub grow">
                  {discovered.length === 0
                    ? "Nothing heard yet — discovery keeps listening."
                    : `${discovered.length} controller${discovered.length === 1 ? "" : "s"} on this network.`}
                </span>
              </div>

              {discovered.map((c) => (
                <button
                  key={c.id}
                  className={`controller-card${snapshot.selectedId === c.id ? " selected" : ""}`}
                  onClick={() => void window.resonux.selectController(c.id)}
                >
                  <div className="name-row">
                    <span className="name">{c.name}</span>
                    <HealthChip health={c.health} offline={!c.online} />
                  </div>
                  <div className="addr">
                    {c.address} · {c.fw ? `firmware ${c.fw}` : "firmware unknown"}
                  </div>
                  <div className="meta">
                    <KindChip kind={c.kind} />
                    {c.role && <span>{c.role}</span>}
                  </div>
                </button>
              ))}

              {discovered.length === 0 && (
                <>
                  <div className="empty" style={{ marginTop: "var(--space-4)" }}>
                    <h3>Nothing here yet</h3>
                    <p>
                      Is the controller powered? Is it on the same Wi-Fi as this computer? If the unit shows a
                      “Resonux” hotspot instead, it's not on your Wi-Fi yet — the Wi-Fi step handles that.
                    </p>
                  </div>
                  <div className="row" style={{ marginTop: "var(--space-4)" }}>
                    <span className="sub grow">No controller in the box yet?</span>
                    <Button icon="activity" onClick={() => void window.resonux.setSimulator(true)}>
                      Try the simulator
                    </Button>
                  </div>
                </>
              )}

              {snapshot.selectedId && (
                <Button variant="primary" onClick={() => go("connect")}>
                  Continue →
                </Button>
              )}
            </section>
          )}

          {step === "connect" && (
            <section>
              <h2>Connect</h2>
              {sel ? (
                <>
                  <p className="hint">
                    “{sel.name}” — one honest check before we carry on: is it really reachable and answering?
                  </p>
                  <div className="controller-card selected">
                    <div className="name-row">
                      <span className="name">{sel.name}</span>
                      <HealthChip health={sel.health} offline={!sel.online} />
                    </div>
                    <div className="addr">
                      {sel.address}:{sel.port} · {sel.kind === "simulator" ? "in-process simulator" : `firmware ${sel.fw ?? "?"}`}
                    </div>
                  </div>
                  <Button variant="primary" icon="check" onClick={() => void verify()} disabled={!sel.online || Boolean(busy)}>
                    {busy ? "Working…" : "Verify connection"}
                  </Button>
                  {!sel.online && (
                    <div className="sub">This controller isn't answering right now — scan again or pick another.</div>
                  )}
                  {stick && <ErrorDetail what={stick.what} hint={stick.hint} onRetry={() => void verify()} />}
                </>
              ) : (
                <p className="hint">Pick a controller on the Find screen first.</p>
              )}
            </section>
          )}

          {step === "name" && sel && (
            <section>
              <h2>Give it a name</h2>
              <p className="hint">
                Something that means something to you — “Living room”, “Workshop”. Saving restarts the
                controller to apply it; the app reconnects automatically.
              </p>
              <label className="field">
                <span>Name</span>
                <input
                  className="input"
                  value={name}
                  maxLength={32}
                  placeholder="Living room"
                  onChange={(e) => setName(e.target.value)}
                />
              </label>
              <Button variant="primary" onClick={() => void saveName()} disabled={!name.trim() || Boolean(busy)}>
                {busy ? "Working…" : "Save name"}
              </Button>
              {stick && <ErrorDetail what={stick.what} hint={stick.hint} detail={stick.detail} onRetry={() => void saveName()} />}
            </section>
          )}

          {step === "wifi" &&
            (sel ? (
              <WifiStep
                wifi={sel.status.wifi}
                busy={busy}
                error={stick}
                onNext={() => go("done")}
                onSubmit={async (ssid, password) => {
                  setBusy("Connecting");
                  setStick(null);
                  const res = await window.resonux.setWifi(sel.id, ssid, password);
                  if (!res.ok) {
                    setBusy(null);
                    setStick({
                      what: "The controller couldn't join that Wi-Fi.",
                      hint:
                        "Mostly this is a detail in the network name or password. ESP32 controllers use 2.4 GHz Wi-Fi only — a 5 GHz-only network won't show up for them.",
                      detail: res.detail,
                    });
                    return;
                  }
                  setBusy("Waiting for the controller to rejoin");
                  const online = await window.resonux.waitOnline(sel.id, 60000);
                  setBusy(null);
                  if (!online) {
                    setStick({
                      what: "The controller hasn't reappeared on the new network yet.",
                      hint:
                        "Give it half a minute; the wizard keeps watching. Check the network name against your router, the password (any extra space counts), and that your router allows 2.4 GHz devices. If it can't join, the controller falls back to its own hotspot — the old setup is untouched.",
                    });
                    return;
                  }
                  go("done");
                }}
              />
            ) : (
              <p className="hint">Pick a controller on the Find screen first.</p>
            ))}

          {step === "done" && sel && <Done sel={sel} onFinish={onClose} />}
        </div>

        <footer className="wizard-foot">
          <Button variant="ghost" onClick={onClose}>
            Skip for now
          </Button>
          <span className="sub grow" style={{ textAlign: "right" }}>
            You can run the full guide any time from Setup.
          </span>
          {stepIndex > 1 && !busy && (
            <Button variant="ghost" onClick={back}>
              ← Back
            </Button>
          )}
        </footer>
      </div>
    </div>
  );
}

function Welcome({ onStart }: { onStart: () => void }) {
  return (
    <section>
      <h2>Welcome to Resonux Control Center</h2>
      <p>
        This app controls your Resonux lights, music, and cinematic scenes. First you connect the small board
        that powers them — no terminal, no cables into your laptop.
      </p>
      <ul className="plain">
        <li>Your controller talks over your Wi-Fi — same network as this computer.</li>
        <li>A few steps down, the app will also set your Wi-Fi details on the controller itself.</li>
      </ul>
      <Button variant="primary" onClick={onStart}>
        Get started
      </Button>
    </section>
  );
}

const WITH_LABEL: Record<string, string> = { sta: "your Wi-Fi", ap: "its hotspot", ap_sta: "Wi-Fi + hotspot", off: "off" };

function WifiStep({
  wifi,
  busy,
  error,
  onNext,
  onSubmit,
}: {
  wifi: { mode: string; ip: string; connected: boolean };
  busy: string | null;
  error: Stick | null;
  onNext: () => void;
  onSubmit: (ssid: string, password: string) => Promise<void>;
}) {
  const [ssid, setSsid] = useState("");
  const [password, setPassword] = useState("");
  const alreadyConnected = wifi.mode === "sta" && wifi.connected;

  if (alreadyConnected) {
    return (
      <section>
        <h2>Wi-Fi — already connected</h2>
        <p className="hint">Your controller is already on this Wi-Fi network, so there's nothing to change here.</p>
        <div className="row">
          <Icon name="check" size={18} style={{ color: "var(--ok)" }} />
          <span className="grow">
            On <b>{WITH_LABEL[wifi.mode] ?? wifi.mode}</b>
            <div className="sub">{wifi.ip}</div>
          </span>
          <Button variant="primary" onClick={onNext}>
            Continue →
          </Button>
        </div>
      </section>
    );
  }

  return (
    <section>
      <h2>Connect it to your Wi-Fi</h2>
      <p className="hint">
        Your controller is currently running its own “Resonux” hotspot. Give it your home Wi-Fi name and
        password and it will join your network — the hotspot stays on as a fallback if the Wi-Fi ever drops.
        ESP32 boards use 2.4 GHz Wi-Fi only.
      </p>
      <label className="field">
        <span>Wi-Fi name</span>
        <input
          className="input"
          value={ssid}
          maxLength={32}
          placeholder="MyHomeNetwork"
          onChange={(e) => setSsid(e.target.value)}
        />
      </label>
      <label className="field">
        <span>Password</span>
        <input
          className="input"
          type="password"
          value={password}
          maxLength={64}
          autoComplete="off"
          placeholder="••••••••"
          onChange={(e) => setPassword(e.target.value)}
        />
      </label>
      <div className="sub" style={{ marginBottom: "var(--space-3)" }}>
        The password is sent once to the controller — it's never stored by this app.
      </div>
      <Button
        variant="primary"
        onClick={() => void onSubmit(ssid, password)}
        disabled={!ssid.trim() || Boolean(busy)}
      >
        {busy ? "Working…" : "Join this Wi-Fi"}
      </Button>
      {error && <ErrorDetail what={error.what} hint={error.hint} detail={error.detail} onRetry={() => void onSubmit(ssid, password)} />}
    </section>
  );
}

function Done({ sel, onFinish }: { sel: NonNullable<Snapshot["selected"]>; onFinish: () => void }) {
  return (
    <section>
      <h2>You're set.</h2>
      <div className="controller-card selected">
        <div className="name-row">
          <span className="name">{sel.name}</span>
          <span className="chip ok">
            <Icon name="check" size={12} />
            Connected
          </span>
        </div>
        <div className="addr">
          {sel.address}:{sel.port} · on {WITH_LABEL[sel.status.wifi.mode] ?? sel.status.wifi.mode}
        </div>
      </div>
      <p className="hint">
        Next, head to <b>Lights</b> to pick a scene and brightness, and <b>Music</b> to point it at a sound
        source. This guide is always available under Setup.
      </p>
      <Button variant="primary" onClick={onFinish}>
        Finish
      </Button>
    </section>
  );
}