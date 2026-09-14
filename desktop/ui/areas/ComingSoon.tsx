import React from "react";

const AREA_BLURBS: Record<string, { title: string; p: string }> = {
  lights: {
    title: "Lights",
    p: "Per-strip control, zones, colours, and running effects — tuned by name, not by JSON.",
  },
  music: {
    title: "Music",
    p: "Choose what the controller listens to — microphone, aux input, or another controller's sync — and watch the source pick automatically.",
  },
  themes: {
    title: "Themes",
    p: "Browse themes as swatches, apply one or assign per strip, and shape palettes visually.",
  },
  cinematic: {
    title: "Cinematic",
    p: "Scenes, moods, companion source, and the QA test bursts — tasteful controls for the reactive engine.",
  },
  devices: {
    title: "Devices",
    p: "See what the controller detected — microphones, amps, fixtures — with the honest connection status the firmware reports.",
  },
  diagnostics: {
    title: "Diagnostics",
    p: "Health, logs, and structured reports you can export — no secrets, just facts.",
  },
  settings: {
    title: "Settings",
    p: "Appearance, updates, backup & restore, and advanced preferences for technicians.",
  },
};

export function ComingSoon({ area }: { area: string }) {
  const b = AREA_BLURBS[area];
  return (
    <div style={{ marginTop: "var(--space-5)" }}>
      <div className="empty">
        <h3>{b?.title ?? area} — coming in this build</h3>
        <p>{b?.p ?? "This surface is being built in phases."}</p>
      </div>
    </div>
  );
}