// Builds the structured diagnostic report a user can export. The report is
// honest by construction: it contains facts the app actually observed and
// excludes everything a support chat must never receive — no credentials, no
// Wi-Fi passwords, and no network addresses (the "no secrets" rule from the
// web review is enforced here by never including the fields at all).

import type { AppInfo, ControllerInfo, StatusSnapshot, HardwareCheckReport } from "../domain/bridge";
import type { LogEvent } from "../log/log";

export interface ReportInput {
  app: AppInfo;
  controller: ControllerInfo;
  status: StatusSnapshot;
  check: HardwareCheckReport | null;
  log: LogEvent[];
}

export function buildReport(input: ReportInput): string {
  const doc = {
    app: {
      name: "Resonux Control Center",
      version: input.app.version,
      platform: input.app.platform,
      packaged: input.app.isPackaged,
      generatedAt: new Date().toISOString(),
    },
    controller: {
      id: input.controller.id,
      name: input.controller.name,
      kind: input.controller.kind,
      firmware: input.controller.fw,
      role: input.controller.role,
      health: input.controller.health,
      online: input.controller.online,
      uptimeMs: input.status.uptimeMs,
      heapBytes: input.status.heap,
      fps: input.status.fps,
      stripCount: input.status.stripCount,
      systemState: input.status.system.state,
      systemAction: input.status.system.action,
      display: input.status.display,
      // Deliberately absent: network address, Wi-Fi config, any credential.
    },
    hardwareCheck: input.check,
    sessionLog: input.log,
    note: "Reported facts only. No credentials, passwords, or network addresses are included.",
  };
  return JSON.stringify(doc, null, 2);
}

export function suggestedReportName(controllerName: string): string {
  const safe = controllerName.replace(/[^a-zA-Z0-9_-]+/g, "-").replace(/^-+|-+$/g, "");
  return `resonux-report-${safe || "controller"}-${new Date().toISOString().slice(0, 10)}.json`;
}