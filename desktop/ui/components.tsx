import React from "react";
import type { HealthKind } from "../src/domain/bridge";
import { Icon, type IconName } from "./icons";

export function HealthChip({ health, offline }: { health: HealthKind | string; offline?: boolean }) {
  const cls =
    health === "ok" ? "ok" : health === "degraded" ? "degraded" : "offline";
  const label =
    health === "ok" ? "Online" : health === "degraded" ? "Weak signal" : offline ? "Offline" : "Offline";
  return (
    <span className={`chip ${cls}`}>
      <span className="dot" />
      {label}
    </span>
  );
}

export function SimulatorChip() {
  return (
    <span className="chip simulator">
      <Icon name="activity" size={12} />
      Simulator
    </span>
  );
}

export function KindChip({ kind }: { kind: string }) {
  if (kind === "simulator") return <SimulatorChip />;
  return (
    <span className="chip">
      <Icon name="wifi" size={12} />
      {kind === "discover" ? "Discovered" : kind}
    </span>
  );
}

export function Card({
  title,
  hint,
  children,
  aside,
}: {
  title?: string;
  hint?: string;
  aside?: React.ReactNode;
  children: React.ReactNode;
}) {
  return (
    <section className="card">
      {title && (
        <h2>
          {title}
          {aside && <span style={{ float: "right" }}>{aside}</span>}
        </h2>
      )}
      {hint && <p className="hint">{hint}</p>}
      {children}
    </section>
  );
}

export function Button({
  children,
  variant,
  icon,
  onClick,
  disabled,
}: {
  children: React.ReactNode;
  variant?: "primary" | "ghost";
  icon?: IconName;
  onClick?: () => void;
  disabled?: boolean;
}) {
  return (
    <button
      className={`btn ${variant ?? ""}`}
      onClick={onClick}
      disabled={disabled}
    >
      {icon && <Icon name={icon} size={16} />}
      {children}
    </button>
  );
}

export function Switch({
  checked,
  onChange,
  disabled,
  danger,
}: {
  checked: boolean;
  onChange: (v: boolean) => void;
  disabled?: boolean;
  danger?: boolean;
}) {
  return (
    <label className="switch">
      <input
        type="checkbox"
        checked={checked}
        disabled={disabled}
        onChange={(e) => onChange(e.target.checked)}
      />
      <span className={`track${danger ? " danger-thumb" : ""}`} />
    </label>
  );
}

export function Stat({
  k,
  v,
  unit,
  alert,
  highlight,
}: {
  k: string;
  v: React.ReactNode;
  unit?: string;
  alert?: boolean;
  highlight?: boolean;
}) {
  return (
    <div className={`stat${alert ? " alert" : ""}${highlight ? " highlight" : ""}`}>
      <div className="k">{k}</div>
      <div className="v">
        {v}
        {unit && <small> {unit}</small>}
      </div>
    </div>
  );
}