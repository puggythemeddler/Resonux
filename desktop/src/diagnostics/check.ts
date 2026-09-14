// Guided Hardware Check (desktop D4): a host-side, step-by-step health check
// that talks ONLY to the firmware's existing REST surface — no firmware
// changes needed. Every step is honest: it can prove what it proves (the
// controller answers, its rail is healthy, a strip is configured, sound is
// reaching it, and it accepts + confirms a live write) and says so when it
// cannot (a computer cannot see LEDs with its eyes — that final confirmation
// is the human's, phrased in the UI).
//
// The engine is pure: callers inject the small set of REST calls it needs, so
// host tests run it against the same simulator the app uses.

import type {
  CheckStepResult,
  CheckVerdict,
  FrameSample,
  HardwareCheckReport,
  StatePayload,
  StatusSnapshot,
} from "../domain/types";

export interface CheckTarget {
  id: string;
  name: string;
  isSimulator: boolean;
}

export interface CheckCalls {
  fetchStatus(): Promise<StatusSnapshot>;
  fetchFrame(): Promise<FrameSample>;
  fetchState(): Promise<StatePayload>;
  setBrightness(value: number): Promise<boolean>;
}

export interface CheckOptions {
  /** Milliseconds between audio samples (tests pass 0). */
  audioSpacingMs?: number;
  /** How long the brightness blip stays up (tests pass 0). */
  blipHoldMs?: number;
}

const MIN_HEAP_BYTES = 8192;
const AUDIO_SAMPLES = 6;
const AUDIO_SPACING_MS = 400;
const AUDIO_SPREAD = 0.03;
const BLIP_HOLD_MS = 900;

/** Where the brightness "blip" travels to, so a human can actually see it.
 *  Never to 0 (that looks broken) and never a brutal full blast. */
export function blipTarget(current: number): number {
  if (current <= 0) return 40;
  if (current >= 200) return 140;
  return current + 40;
}

export function summarize(steps: CheckStepResult[]): HardwareCheckReport["summary"] {
  const s = { pass: 0, warn: 0, fail: 0 };
  for (const st of steps) s[st.verdict]++;
  return s;
}

function errText(e: unknown): string {
  return e instanceof Error ? e.message : String(e);
}

export async function runHardwareCheck(
  target: CheckTarget,
  calls: CheckCalls,
  opts: CheckOptions = {}
): Promise<HardwareCheckReport> {
  const audioSpacingMs = opts.audioSpacingMs ?? AUDIO_SPACING_MS;
  const blipHoldMs = opts.blipHoldMs ?? BLIP_HOLD_MS;
  const steps: CheckStepResult[] = [];
  let visual: { from: number; to: number } | null = null;

  const push = (id: CheckStepResult["id"], label: string, verdict: CheckVerdict, detail: string) =>
    steps.push({ id, label, verdict, detail });

  // 1 — reachability. Everything else depends on the controller answering.
  let status: StatusSnapshot;
  try {
    status = await calls.fetchStatus();
    if (status.ok) {
      push("reachability", "Controller answers", "pass", `${target.name} replied to a status request.`);
    } else {
      push("reachability", "Controller answers", "fail", `Replied, but reports itself not ok.`);
      return report();
    }
  } catch (e) {
    push("reachability", "Controller answers", "fail", `No reply: ${errText(e)}.`);
    return report();
  }

  // 2 — the rail: memory + uptime look sane.
  if (status.heap >= MIN_HEAP_BYTES) {
    push("rail", "Controller is healthy", "pass", `Free memory ${status.heap} B, up ${Math.round(status.uptimeMs / 60000)} min.`);
  } else {
    push("rail", "Controller is healthy", "warn", `Low free memory (${status.heap} B) — expect slowdowns.`);
  }

  // 3 — is anything actually configured to light up?
  if (status.stripCount >= 1) {
    push("leds", "Lights are configured", "pass", `${status.stripCount} strip${status.stripCount === 1 ? "" : "s"} on this controller.`);
  } else {
    push("leds", "Lights are configured", "warn", `No light strips configured on "${status.device}".`);
  }

  // 4 — is sound reaching it? Sample /api/frame a few times and look for life.
  try {
    let prevAmp: number | null = null;
    let transitions = 0;
    let max = -Infinity;
    let min = Infinity;
    for (let i = 0; i < AUDIO_SAMPLES; i++) {
      const f = await calls.fetchFrame();
      if (Number.isFinite(f.amp)) {
        max = Math.max(max, f.amp);
        min = Math.min(min, f.amp);
        if (prevAmp !== null && Math.abs(f.amp - prevAmp) > AUDIO_SPREAD) transitions++;
        prevAmp = f.amp;
      }
      if (i < AUDIO_SAMPLES - 1) await new Promise((r) => setTimeout(r, audioSpacingMs));
    }
    if (transitions >= 1 && max - min > AUDIO_SPREAD) {
      push("audio", "Sound reaches it", "pass", `The audio signal is alive (amp ${min.toFixed(2)}–${max.toFixed(2)} across ${AUDIO_SAMPLES} samples).`);
    } else {
      push("audio", "Sound reaches it", "warn", `No movement in the audio signal — is music playing? The mic may be silent or the source not set.`);
    }
  } catch (e) {
    push("audio", "Sound reaches it", "warn", `Couldn't read the audio signal: ${errText(e)}.`);
  }

  // 5 — a live, visible write round-trip. Brightness goes up for a beat, comes
  // back home, and the controller confirms each leg. The human confirms the
  // actual light output in the UI afterwards.
  try {
    const before = (await calls.fetchState()).brightness;
    const to = blipTarget(before);
    const sent = await calls.setBrightness(to);
    if (!sent) {
      push("write", "Controller accepts changes", "fail", `Brightness change was rejected.`);
      return report();
    }
    const mid = await calls.fetchState();
    if (mid.brightness !== to) {
      push("write", "Controller accepts changes", "fail", `Set to ${to}, but reads back ${mid.brightness}.`);
      return report();
    }
    // Give a human a moment to actually see the change.
    await new Promise((r) => setTimeout(r, blipHoldMs));
    const restored = await calls.setBrightness(before);
    const back = restored ? (await calls.fetchState()).brightness : undefined;
    visual = { from: before, to };
    if (restored && back === before) {
      push("write", "Controller accepts changes", "pass", `Brightness ${before} → ${to} → ${before}; every step confirmed.`);
    } else {
      push("write", "Controller accepts changes", "warn", `The change went through but we could not restore the original (${before}) — check the Lights tab.`);
    }
  } catch (e) {
    push("write", "Controller accepts changes", "fail", `Timed out mid-check: ${errText(e)}. Controller state may have changed.`);
  }

  function report(): HardwareCheckReport {
    return {
      targetId: target.id,
      targetName: target.name,
      isSimulator: target.isSimulator,
      steps,
      summary: summarize(steps),
      visual,
      note: target.isSimulator
        ? "This is the built-in simulator — there are no physical lights to see. The checks still run for real against its HTTP surface."
        : "A computer can verify the controller and its connections — only your eyes can confirm the LEDs are actually glowing. That is the last, human step.",
    };
  }

  return report();
}