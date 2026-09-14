import { app, BrowserWindow, ipcMain, dialog } from "electron";
import path from "node:path";
import fs from "node:fs";
import { AppCore } from "../src/core/app";
import { suggestedReportName } from "../src/core/report";
import type { HardwareCheckReport } from "../src/domain/bridge";

let core: AppCore | null = null;
let win: BrowserWindow | null = null;

function createWindow(): void {
  const smoke = Boolean(process.env.RESONUX_SMOKE);
  win = new BrowserWindow({
    width: 1280,
    height: 820,
    minWidth: 1024,
    minHeight: 640,
    title: "Resonux Control Center",
    backgroundColor: "#14161a",
    show: !smoke,
    webPreferences: {
      preload: path.join(__dirname, "preload.js"),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
    },
  });

  win.once("ready-to-show", () => {
    if (!smoke) win?.show();
  });
  win.on("closed", () => {
    win = null;
  });

  if (smoke) {
    win.webContents.once("did-finish-load", () => {
      console.log("[smoke] renderer loaded; preload + IPC handlers live");
      app.exit(0);
    });
    win.webContents.once("did-fail-load", (_e, code, desc) => {
      console.error(`[smoke] renderer failed to load (${code}) ${desc}`);
      app.exit(3);
    });
    setTimeout(() => {
      console.error("[smoke] timed out");
      app.exit(2);
    }, 15000).unref();
  }

  const devUrl = process.env.RESONUX_DEV_URL;
  if (devUrl) {
    void win.loadURL(devUrl);
  } else {
    void win.loadFile(path.join(__dirname, "..", "..", "renderer", "index.html"));
  }
}

function registerIpc(): void {
  ipcMain.handle("app:info", () => core?.info());
  ipcMain.handle("app:snapshot", () => core?.snapshot());
  ipcMain.handle("settings:setTheme", (_e, mode: string) => {
    core?.setTheme(mode as Parameters<AppCore["setTheme"]>[0]);
  });
  ipcMain.handle("controllers:select", (_e, id: string | null) => core?.selectController(id));
  ipcMain.handle("controllers:rescan", () => core?.rescan());
  ipcMain.handle("simulator:set", (_e, enabled: boolean) => core?.setSimulator(Boolean(enabled)));
  ipcMain.handle("device:setBrightness", (_e, id: string, value: number) =>
    core?.setBrightness(id, value)
  );
  ipcMain.handle("device:setCinematic", (_e, id: string, enabled: boolean) =>
    core?.setCinematic(id, enabled)
  );
  ipcMain.handle("device:getStatus", (_e, id: string) => core?.getStatus(id));
  ipcMain.handle("device:getCinematic", (_e, id: string) => core?.getCinematic(id));
  ipcMain.handle("device:getDevices", (_e, id: string) => core?.getDevices(id));
  ipcMain.handle("device:getAudioSources", (_e, id: string) => core?.getAudioSources(id));
  ipcMain.handle("device:rename", (_e, id: string, name: string) => core?.renameController(id, name));
  ipcMain.handle("device:setWifi", (_e, id: string, ssid: string, password: string) =>
    core?.setWifi(id, ssid, password)
  );
  ipcMain.handle("controllers:waitOnline", (_e, id: string, timeoutMs: number) =>
    core?.waitOnline(id, timeoutMs)
  );
  ipcMain.handle("settings:finishWizard", () => core?.finishWizard());
  ipcMain.handle("device:getThemes", (_e, id: string) => core?.getThemes(id));
  ipcMain.handle("device:getState", (_e, id: string) => core?.getState(id));
  ipcMain.handle("device:selectTheme", (_e, id: string, themeId: string, strip?: number) =>
    core?.selectTheme(id, themeId, strip)
  );
  ipcMain.handle("device:setStripEffect", (_e, id: string, strip: number, effectId: number) =>
    core?.setStripEffect(id, strip, effectId)
  );
  ipcMain.handle("device:selectAudioSource", (_e, id: string, source: number) =>
    core?.selectAudioSource(id, source)
  );
  ipcMain.handle("device:setAutoSelect", (_e, id: string, autoSelect: boolean) =>
    core?.setAutoSelect(id, Boolean(autoSelect))
  );
  ipcMain.handle("device:triggerCinematicTest", (_e, id: string) => core?.triggerCinematicTest(id));
  ipcMain.handle("device:runHardwareCheck", (_e, id: string) => core?.runHardwareCheck(id));
  ipcMain.handle("device:restartController", (_e, id: string) => core?.requestRestart(id));
  ipcMain.handle("device:powerOff", (_e, id: string) => core?.requestPowerOff(id));
  ipcMain.handle("device:exportReport", async (event, id: string, check: HardwareCheckReport | null) => {
    try {
      const text = await core?.exportReport(id, check);
      if (!text) return { saved: false, detail: "Control Center is not running." };
      const controller = core?.snapshot().controllers.find((c) => c.id === id);
      const options = {
        title: "Export diagnostic report",
        defaultPath: suggestedReportName(controller?.name ?? "controller"),
        filters: [{ name: "JSON report", extensions: ["json"] }],
      };
      const res =
        win && !win.isDestroyed()
          ? await dialog.showSaveDialog(win, options)
          : await dialog.showSaveDialog(options);
      if (res.canceled || !res.filePath) return { saved: false };
      fs.writeFileSync(res.filePath, text, "utf8");
      return { saved: true, filePath: res.filePath };
    } catch (err) {
      return { saved: false, detail: err instanceof Error ? err.message : String(err) };
    }
  });
  // ---- D5: firmware update + config backup/restore ----
  ipcMain.handle("device:updateFirmware", async (_e, id: string) => {
    const options = {
      title: "Choose firmware image",
      filters: [
        { name: "Firmware image", extensions: ["bin"] },
        { name: "All files", extensions: ["*"] },
      ],
    };
    const res =
      win && !win.isDestroyed()
        ? await dialog.showOpenDialog(win, options)
        : await dialog.showOpenDialog(options);
    if (res.canceled || res.filePaths.length === 0) return { ok: false, detail: "Cancelled" };
    try {
      return (await core?.updateFirmware(id, res.filePaths[0])) ?? { ok: false, detail: "Control Center is not running." };
    } catch (err) {
      return { ok: false, detail: err instanceof Error ? err.message : String(err) };
    }
  });
  ipcMain.handle("device:backupConfig", async (_e, id: string) => {
    try {
      const text = await core?.backupConfig(id);
      if (text === undefined) return { saved: false, detail: "Control Center is not running." };
      const controller = core?.snapshot().controllers.find((c) => c.id === id);
      const safeName = (controller?.name ?? "controller").replace(/[^a-zA-Z0-9_-]+/g, "-");
      const options = {
        title: "Save config backup",
        defaultPath: `resonux-config-${safeName}-${new Date().toISOString().slice(0, 10)}.json`,
        filters: [{ name: "Config backup", extensions: ["json"] }],
      };
      const r =
        win && !win.isDestroyed()
          ? await dialog.showSaveDialog(win, options)
          : await dialog.showSaveDialog(options);
      if (r.canceled || !r.filePath) return { saved: false };
      fs.writeFileSync(r.filePath, text, "utf8");
      return { saved: true, filePath: r.filePath };
    } catch (err) {
      return { saved: false, detail: err instanceof Error ? err.message : String(err) };
    }
  });
  ipcMain.handle("device:restoreConfig", async (_e, id: string) => {
    const options = {
      title: "Choose a saved config backup",
      filters: [
        { name: "Config backup", extensions: ["json"] },
        { name: "All files", extensions: ["*"] },
      ],
    };
    const res =
      win && !win.isDestroyed()
        ? await dialog.showOpenDialog(win, options)
        : await dialog.showOpenDialog(options);
    if (res.canceled || res.filePaths.length === 0) return { ok: false, rebootApplied: false, detail: "Cancelled" };
    try {
      const text = fs.readFileSync(res.filePaths[0], "utf8");
      return (await core?.restoreConfig(id, text)) ?? { ok: false, rebootApplied: false, detail: "Control Center is not running." };
    } catch (err) {
      return { ok: false, rebootApplied: false, detail: err instanceof Error ? err.message : String(err) };
    }
  });
}

app.whenReady().then(() => {
  core = new AppCore({
    dataDir: app.getPath("userData"),
    appInfo: { version: app.getVersion() },
  });
  core.onChange(() => {
    if (win && !win.isDestroyed()) {
      win.webContents.send("app:changed", core?.snapshot());
    }
  });

  registerIpc();
  createWindow();

  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on("window-all-closed", () => {
  if (process.platform !== "darwin") app.quit();
});

app.on("will-quit", () => {
  core?.stop();
  core = null;
});