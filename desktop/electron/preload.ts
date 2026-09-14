import { contextBridge, ipcRenderer } from "electron";
import type { ResonuxApi, Snapshot, ThemeMode } from "../src/domain/bridge";

const api: ResonuxApi = {
  getInfo: () => ipcRenderer.invoke("app:info"),
  getSnapshot: () => ipcRenderer.invoke("app:snapshot"),
  setTheme: (mode: ThemeMode) => ipcRenderer.invoke("settings:setTheme", mode),
  selectController: (id: string | null) => ipcRenderer.invoke("controllers:select", id),
  rescan: () => ipcRenderer.invoke("controllers:rescan"),
  setSimulator: (enabled: boolean) => ipcRenderer.invoke("simulator:set", enabled),
  setBrightness: (id: string, value: number) => ipcRenderer.invoke("device:setBrightness", id, value),
  setCinematic: (id: string, enabled: boolean) => ipcRenderer.invoke("device:setCinematic", id, enabled),
  getStatus: (id: string) => ipcRenderer.invoke("device:getStatus", id),
  getCinematic: (id: string) => ipcRenderer.invoke("device:getCinematic", id),
  getDevices: (id: string) => ipcRenderer.invoke("device:getDevices", id),
  getAudioSources: (id: string) => ipcRenderer.invoke("device:getAudioSources", id),
  renameController: (id: string, name: string) => ipcRenderer.invoke("device:rename", id, name),
  setWifi: (id: string, ssid: string, password: string) =>
    ipcRenderer.invoke("device:setWifi", id, ssid, password),
  waitOnline: (id: string, timeoutMs: number) => ipcRenderer.invoke("controllers:waitOnline", id, timeoutMs),
  finishWizard: () => ipcRenderer.invoke("settings:finishWizard"),
  getThemes: (id: string) => ipcRenderer.invoke("device:getThemes", id),
  getState: (id: string) => ipcRenderer.invoke("device:getState", id),
  selectTheme: (id: string, themeId: string, strip?: number) =>
    ipcRenderer.invoke("device:selectTheme", id, themeId, strip),
  setStripEffect: (id: string, strip: number, effectId: number) =>
    ipcRenderer.invoke("device:setStripEffect", id, strip, effectId),
  selectAudioSource: (id: string, source: number) => ipcRenderer.invoke("device:selectAudioSource", id, source),
  setAutoSelect: (id: string, autoSelect: boolean) => ipcRenderer.invoke("device:setAutoSelect", id, autoSelect),
  triggerCinematicTest: (id: string) => ipcRenderer.invoke("device:triggerCinematicTest", id),
  runHardwareCheck: (id: string) => ipcRenderer.invoke("device:runHardwareCheck", id),
  onChanged: (cb: (snapshot: Snapshot) => void) => {
    const listener = (_event: unknown, snapshot: Snapshot) => cb(snapshot);
    ipcRenderer.on("app:changed", listener);
    return () => {
      ipcRenderer.removeListener("app:changed", listener);
    };
  },
};

contextBridge.exposeInMainWorld("resonux", api);