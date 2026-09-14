import React, { useEffect, useMemo, useState } from "react";
import type { AppInfo, Snapshot, ThemeMode } from "../src/domain/bridge";
import { Icon, Logo, type IconName } from "./icons";
import { HealthChip, SimulatorChip } from "./components";
import { Home } from "./areas/Home";
import { Setup } from "./areas/Setup";
import { ComingSoon } from "./areas/ComingSoon";
import { Diagnostics } from "./areas/Diagnostics";
import { Wizard } from "./areas/Wizard";
import { Lights } from "./areas/Lights";
import { Music } from "./areas/Music";
import { Themes } from "./areas/Themes";
import { Cinematic } from "./areas/Cinematic";
import { Settings } from "./areas/Settings";

type Area = "home" | "setup" | "lights" | "music" | "themes" | "cinematic" | "devices" | "diagnostics" | "settings";

const NAV: { id: Area; label: string; icon: IconName }[] = [
  { id: "home", label: "Home", icon: "home" },
  { id: "setup", label: "Setup", icon: "plug" },
  { id: "lights", label: "Lights", icon: "lamp" },
  { id: "music", label: "Music", icon: "music" },
  { id: "themes", label: "Themes", icon: "palette" },
  { id: "cinematic", label: "Cinematic", icon: "film" },
  { id: "devices", label: "Devices", icon: "chip" },
  { id: "diagnostics", label: "Diagnostics", icon: "activity" },
  { id: "settings", label: "Settings", icon: "gear" },
];

function systemTheme(): "dark" | "light" {
  return window.matchMedia?.("(prefers-color-scheme: dark)").matches ? "dark" : "light";
}

function resolve(theme: ThemeMode): "dark" | "light" {
  return theme === "system" ? systemTheme() : theme;
}

export default function App() {
  const [info, setInfo] = useState<AppInfo | null>(null);
  const [snapshot, setSnapshot] = useState<Snapshot | null>(null);
  const [area, setArea] = useState<Area>("home");
  const [themeMode, setThemeMode] = useState<ThemeMode>("system");
  const [manualWizard, setManualWizard] = useState(false);

  useEffect(() => {
    void window.resonux.getInfo().then(setInfo);

    const applyTheme = (m: ThemeMode) => {
      const scheme = resolve(m);
      document.documentElement.dataset.theme = scheme;
      document.documentElement.style.colorScheme = scheme;
    };

    // Push updates from the main process; main sends 'system' until settings load.
    applyTheme("system");
    const unsub = window.resonux.onChanged((s) => {
      setSnapshot(s);
      setThemeMode(s.settings.theme);
      applyTheme(s.settings.theme);
    });
    void window.resonux.getSnapshot().then((s) => {
      setSnapshot(s);
      setThemeMode(s.settings.theme);
      applyTheme(s.settings.theme);
    });
    return unsub;
  }, []);

  const themeIcon: IconName = themeMode === "dark" ? "moon" : themeMode === "light" ? "sun" : "monitor";

  const cycleTheme = () => {
    const next: ThemeMode = themeMode === "system" ? "dark" : themeMode === "dark" ? "light" : "system";
    setThemeMode(next);
    document.documentElement.dataset.theme = resolve(next);
    void window.resonux.setTheme(next);
  };

  const selected = snapshot?.selected;

  const areaTitle = useMemo(() => NAV.find((n) => n.id === area)?.label ?? "", [area]);

  const wizardOpen = manualWizard || Boolean(snapshot?.wizardNeeded);
  const closeWizard = () => {
    setManualWizard(false);
    void window.resonux.finishWizard();
  };

  return (
    <div className="shell">
      <aside className="navrail">
        <div className="brand">
          <Logo size={30} className="brand-mark" />
          Resonux
          <span className="sub">Control Center</span>
        </div>
        {NAV.map((n) => (
          <button
            key={n.id}
            className={`nav-item${area === n.id ? " active" : ""}`}
            onClick={() => setArea(n.id)}
          >
            <Icon name={n.icon} className="nav-icon" />
            {n.label}
          </button>
        ))}
        {info && (
          <div className="nav-spacer" />
        )}
        {info && (
          <div className="chip mute" style={{ border: "none", justifyContent: "center" }}>
            v{info.version}
          </div>
        )}
      </aside>

      <header className="topbar">
        <div className="topbar-title">{areaTitle}</div>
        <div className="chips">
          {snapshot?.simulatorRunning && <SimulatorChip />}
          {selected?.kind === "simulator" ? (
            <span className="chip ok">
              <span className="dot" />
              Simulator online
            </span>
          ) : selected ? (
            <span className="chip ok">
              <span className="dot" />
              {selected.name} · {selected.address}
            </span>
          ) : (
            <span className="chip">
              <span className="dot" />
              No controller selected
            </span>
          )}
          <button className="icon-btn" title={`Theme: ${themeMode}`} onClick={cycleTheme}>
            <Icon name={themeIcon} size={16} />
          </button>
        </div>
      </header>

      <main className="content">
        <div className="page">
          {!snapshot ? (
            <div className="empty">
              <h3>Starting…</h3>
              <p>The Control Center is waking up and scanning for controllers.</p>
            </div>
          ) : area === "home" ? (
            <Home snapshot={snapshot} />
          ) : area === "setup" ? (
            <Setup snapshot={snapshot} onRunWizard={() => setManualWizard(true)} />
          ) : area === "lights" ? (
            <Lights snapshot={snapshot} />
          ) : area === "music" ? (
            <Music snapshot={snapshot} />
          ) : area === "themes" ? (
            <Themes snapshot={snapshot} />
          ) : area === "cinematic" ? (
            <Cinematic snapshot={snapshot} />
          ) : area === "diagnostics" ? (
            <Diagnostics snapshot={snapshot} />
          ) : area === "settings" ? (
            <Settings snapshot={snapshot} />
          ) : (
            <ComingSoon area={area} />
          )}
        </div>
      </main>

      {wizardOpen && snapshot && <Wizard snapshot={snapshot} onClose={closeWizard} />}
    </div>
  );
}