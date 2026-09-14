// Persisted app settings (userData/settings.json). Nothing sensitive is ever
// written here; the selected controller, theme, and simulator preference are
// small, casual state the app restores on launch.

import fs from "node:fs";
import path from "node:path";
import type { AppSettings, ThemeMode } from "../domain/bridge";

export const DEFAULT_SETTINGS: AppSettings = {
  theme: "system",
  selectedControllerId: null,
  simulatorRunning: true,
};

export class SettingsStore {
  private readonly file: string;
  private settings: AppSettings;

  constructor(dataDir: string) {
    this.file = path.join(dataDir, "settings.json");
    this.settings = this.load();
    // First run: no file, no controller — a simulator keeps the app alive and
    // lets the first-run flow explain what a controller is.
    if (!fs.existsSync(this.file) && !Object.hasOwn(this.settings, "simulatorRunning")) {
      this.settings = { ...DEFAULT_SETTINGS };
      this.save();
    }
  }

  private load(): AppSettings {
    try {
      const raw = JSON.parse(fs.readFileSync(this.file, "utf8")) as Partial<AppSettings>;
      return {
        theme: ["system", "dark", "light"].includes(raw.theme ?? "") ? (raw.theme as ThemeMode) : "system",
        selectedControllerId: typeof raw.selectedControllerId === "string" ? raw.selectedControllerId : null,
        simulatorRunning: typeof raw.simulatorRunning === "boolean" ? raw.simulatorRunning : true,
      };
    } catch {
      return { ...DEFAULT_SETTINGS };
    }
  }

  get(): AppSettings {
    return { ...this.settings };
  }

  patch(p: Partial<AppSettings>): AppSettings {
    this.settings = { ...this.settings, ...p };
    this.save();
    return this.get();
  }

  private save(): void {
    try {
      const tmp = this.file + ".tmp";
      fs.writeFileSync(tmp, JSON.stringify(this.settings, null, 2));
      fs.renameSync(tmp, this.file);
    } catch {
      // Non-fatal: the app keeps running with in-memory settings.
    }
  }
}