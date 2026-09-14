import React, { useEffect, useState } from "react";
import type {
  Snapshot,
  UpdateFirmwareResult,
  ConfigBackupResult,
  ConfigRestoreResult,
  CheckUpdatesResult,
} from "../../src/domain/bridge";
import { Card, Button, HealthChip, KindChip, ErrorDetail, ConfirmDialog } from "../components";
import { Icon } from "../icons";

interface SettingsProps {
  snapshot: Snapshot;
}

interface RestoreOutcome extends ConfigRestoreResult {
  cameBack?: boolean;
}

export function Settings({ snapshot }: SettingsProps) {
  const initial = snapshot.selectedId ?? snapshot.controllers[0]?.id ?? null;
  const [targetId, setTargetId] = useState<string | null>(initial);
  const target = snapshot.controllers.find((c) => c.id === targetId) ?? null;

  const [fwUpdating, setFwUpdating] = useState(false);
  const [fwResult, setFwResult] = useState<UpdateFirmwareResult | null>(null);

  const [backingUp, setBackingUp] = useState(false);
  const [backupResult, setBackupResult] = useState<ConfigBackupResult | null>(null);

  const [restoreConfirm, setRestoreConfirm] = useState(false);
  const [restoring, setRestoring] = useState(false);
  const [restoreResult, setRestoreResult] = useState<RestoreOutcome | null>(null);

  const [appVersion, setAppVersion] = useState<string>("");
  useEffect(() => {
    let alive = true;
    void window.resonux.getInfo().then((i) => {
      if (alive) setAppVersion(i.version);
    });
    return () => {
      alive = false;
    };
  }, []);

  const [checking, setChecking] = useState(false);
  const [updateResult, setUpdateResult] = useState<CheckUpdatesResult | null>(null);

  const err = (e: unknown) => (e instanceof Error ? e.message : String(e));

  const doCheckUpdates = async () => {
    setChecking(true);
    setUpdateResult(null);
    try {
      setUpdateResult(await window.resonux.checkForUpdates());
    } catch (e) {
      setUpdateResult({ ok: false, current: appVersion, latest: "", available: false, detail: err(e) });
    } finally {
      setChecking(false);
    }
  };

  const doUpdate = async () => {
    if (!targetId) return;
    setFwUpdating(true);
    setFwResult(null);
    try {
      setFwResult(await window.resonux.updateFirmware(targetId));
    } catch (e) {
      setFwResult({ ok: false, detail: err(e) });
    } finally {
      setFwUpdating(false);
    }
  };

  const doBackup = async () => {
    if (!targetId) return;
    setBackingUp(true);
    setBackupResult(null);
    try {
      setBackupResult(await window.resonux.backupConfig(targetId));
    } catch (e) {
      setBackupResult({ saved: false, detail: err(e) });
    } finally {
      setBackingUp(false);
    }
  };

  const doRestore = async () => {
    if (!targetId) return;
    setRestoreConfirm(false);
    setRestoring(true);
    setRestoreResult(null);
    try {
      const res = await window.resonux.restoreConfig(targetId);
      const cameBack = res.ok ? await window.resonux.waitOnline(targetId, 20000) : false;
      setRestoreResult({ ...res, cameBack });
    } catch (e) {
      setRestoreResult({ ok: false, rebootApplied: false, detail: err(e) });
    } finally {
      setRestoring(false);
    }
  };

  if (snapshot.controllers.length === 0) {
    return (
      <section className="page-head">
        <h1>Settings</h1>
        <div className="empty" style={{ marginTop: "var(--space-4)" }}>
          <h3>No controller to configure yet</h3>
          <p>Connect a controller in Setup first — the simulator works too.</p>
        </div>
      </section>
    );
  }

  return (
    <>
      <section className="page-head">
        <h1>Settings</h1>
        <p>
          Keep the controller healthy and up to date. These operations are safe to
          use, but both writes will briefly take the unit offline.
        </p>
      </section>

      <Card title="1 · Pick a controller" hint="Settings apply to whichever controller you select here.">
        <div style={{ marginTop: "var(--space-4)" }}>
          {snapshot.controllers.map((c) => (
            <button
              key={c.id}
              className={`controller-card${targetId === c.id ? " selected" : ""}`}
              style={{ width: "100%", marginBottom: "var(--space-2)", textAlign: "left" }}
              onClick={() => {
                setTargetId(c.id);
                setFwResult(null);
                setBackupResult(null);
                setRestoreResult(null);
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

      <Card title="2 · Update firmware" hint="Upload a firmware image you built for the ESP32. The controller applies it to its update partition and reboots — a well-tested OTA path, no cable needed.">
        <div className="row">
          <Button variant="primary" icon="upload" onClick={() => void doUpdate()} disabled={!target || fwUpdating}>
            {fwUpdating ? "Uploading…" : "Update firmware…"}
          </Button>
          {target && (
            <span className="sub grow" style={{ marginTop: 0 }}>
              Current firmware on <b>{target.name}</b>: {target.fw || "unknown"}
            </span>
          )}
        </div>
        {target?.kind === "simulator" && (
          <p className="hint" style={{ marginTop: "var(--space-3)" }}>
            The simulator accepts the same OTA calls as a real controller, so you
            can try the flow end-to-end without hardware.
          </p>
        )}
        {fwResult && !fwResult.ok && fwResult.detail !== "Cancelled" && (
          <div style={{ marginTop: "var(--space-3)" }}>
            <ErrorDetail
              what="The firmware upload didn't complete."
              hint="Check the controller is online and the .bin is a valid image, then try again."
              detail={fwResult.detail}
              onRetry={() => void doUpdate()}
            />
          </div>
        )}
        {fwResult?.ok && (
          <p className="hint" style={{ marginTop: "var(--space-3)" }}>
            The controller accepted the image and is restarting to apply it.
          </p>
        )}
      </Card>

      <Card title="3 · Backup config" hint="Save the controller's full settings to a file you choose. The document includes every setting — including the Wi-Fi password — so keep the file somewhere safe.">
        <div className="row">
          <Button icon="save" onClick={() => void doBackup()} disabled={!target || backingUp}>
            {backingUp ? "Reading config…" : "Save config…"}
          </Button>
          {backupResult?.saved && backupResult.filePath && (
            <span className="sub grow" style={{ marginTop: 0 }}>
              Saved to <code>{backupResult.filePath}</code>
            </span>
          )}
        </div>
        {backupResult && !backupResult.saved && (
          <div style={{ marginTop: "var(--space-3)" }}>
            <ErrorDetail
              what="The config couldn't be saved."
              hint="Choose a location that lets the app write, then try again."
              detail={backupResult.detail}
              onRetry={() => void doBackup()}
            />
          </div>
        )}
      </Card>

      <Card title="4 · Restore config" hint="Replay a saved config file onto the controller. This replaces the unit's live settings and reboots it to apply. An automatic backup of the current settings is parked before the write.">
        <div className="row">
          <Button variant="danger" icon="save" onClick={() => setRestoreConfirm(true)} disabled={!target || restoring}>
            {restoring ? "Restoring…" : "Restore config…"}
          </Button>
          {restoreResult && (
            <span className="sub grow" style={{ marginTop: 0 }}>
              {restoreResult.ok && restoreResult.cameBack
                ? "Config restored; the controller is back online."
                : restoreResult.ok
                  ? "Config accepted, but the controller didn't come back within 20 seconds."
                  : "The config couldn't be restored."}
              {restoreResult.detail && ` (${restoreResult.detail})`}
            </span>
          )}
        </div>
        {restoreResult && !restoreResult.ok && restoreResult.detail !== "Cancelled" && (
          <div style={{ marginTop: "var(--space-3)" }}>
            <ErrorDetail
              what="The config couldn't be restored."
              hint="Check the file is a JSON config backup from this app, then try again."
              detail={restoreResult.detail}
              onRetry={() => setRestoreConfirm(true)}
            />
          </div>
        )}

        <ConfirmDialog
          open={restoreConfirm}
          title="Replace the controller's settings?"
          body={
            <>
              <b>{target?.name}</b> will forget its current settings and load the
              saved file instead. This is immediate and irreversible (a backup of
              the current settings is parked in the app data folder first).
            </>
          }
          confirmLabel="Restore config"
          danger
          onConfirm={() => void doRestore()}
          onCancel={() => setRestoreConfirm(false)}
        />
      </Card>

      <Card
        title="5 · About & updates"
        hint="Compares this build against the newest published release. The app never installs anything silently — when a newer build exists it links you to it."
      >
        <div className="row">
          <span className="sub grow" style={{ marginTop: 0 }}>
            <Icon name="chip" size={14} /> Resonux Control Center
            <b> {appVersion || "…"}</b>
          </span>
          <Button variant="ghost" icon="search" onClick={() => void doCheckUpdates()} disabled={checking}>
            {checking ? "Checking…" : "Check for updates"}
          </Button>
        </div>

        {updateResult?.ok && updateResult.available && (
          <div className="row" style={{ marginTop: "var(--space-3)", justifyContent: "flex-start" }}>
            <span className="sub grow" style={{ marginTop: 0 }}>
              A newer build is available: <b>{updateResult.latest}</b> (you have {updateResult.current}).
            </span>
            {updateResult.url && (
              <Button icon="external" onClick={() => void window.resonux.openExternal(updateResult.url!)}>
                See release
              </Button>
            )}
          </div>
        )}
        {updateResult?.ok && !updateResult.available && (
          <p className="hint" style={{ marginTop: "var(--space-3)" }}>
            You&apos;re on {updateResult.current} — no newer release found.
          </p>
        )}
        {updateResult && !updateResult.ok && (
          <div style={{ marginTop: "var(--space-3)" }}>
            <ErrorDetail
              what="The update check didn't work."
              hint="Check your internet connection, then try again."
              detail={updateResult.detail}
              onRetry={() => void doCheckUpdates()}
            />
          </div>
        )}
      </Card>
    </>
  );
}