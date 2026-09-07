import { useCallback, useEffect, useRef, useState } from 'react'

type Status = {
  ok: boolean
  device: string
  uptimeMs: number
  heap: number
  fps: number
  stripCount: number
  wifi: { mode: string; ip: string; connected: boolean }
  artnet: { enabled: boolean; fixtures: number; status: string }
}

type Frame = {
  ok: boolean
  fps: number
  amp: number
  bass: number
  lowMid: number
  mid: number
  highMid: number
  treble: number
  beat: boolean
  beatStrength: number
  bandCount: number
  bands?: number[]
  peaks?: number[]
}

type AnyConfig = Record<string, unknown>

type Tab = 'live' | 'config' | 'ota'

const fmt = (ms: number) => {
  const s = Math.floor(ms / 1000)
  const d = Math.floor(s / 86400)
  const h = Math.floor((s % 86400) / 3600)
  const m = Math.floor((s % 3600) / 60)
  const sec = s % 60
  return `${d}d ${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}:${String(sec).padStart(2, '0')}`
}

function Spectrum({ frame }: { frame: Frame | null }) {
  const bars = frame?.bands ?? []
  const peaks = frame?.peaks ?? []
  return (
    <div className="meter-bar">
      {bars.map((v, i) => (
        <div
          key={i}
          style={{ height: `${Math.max(2, v * 100)}%`, opacity: 0.75 + v * 0.25 }}
          className={peaks[i] > 0.85 ? 'peak' : ''}
        />
      ))}
    </div>
  )
}

function ConfiguredJsonPreview({ config }: { config: AnyConfig }) {
  return (
    <details>
      <summary>View full config.json</summary>
      <pre className="pre">{JSON.stringify(config, null, 2)}</pre>
    </details>
  )
}

export default function App() {
  const [tab, setTab] = useState<Tab>('live')
  const [status, setStatus] = useState<Status | null>(null)
  const [frame, setFrame] = useState<Frame | null>(null)
  const [config, setConfig] = useState<AnyConfig | null>(null)
  const [configText, setConfigText] = useState('')
  const [online, setOnline] = useState(false)

  const [otaProg, setOtaProg] = useState(0)
  const [otaMsg, setOtaMsg] = useState('')
  const [uploading, setUploading] = useState(false)
  const fileRef = useRef<HTMLInputElement>(null)

  const [sensitivity, setSensitivity] = useState(50)
  const [brightness, setBrightness] = useState(100)
  const sensitivityRef = useRef(sensitivity)
  const brightnessRef = useRef(brightness)
  sensitivityRef.current = sensitivity
  brightnessRef.current = brightness

  const poll = useCallback(async () => {
    try {
      const [sr, fr] = await Promise.all([
        fetch('/api/status'),
        fetch('/api/frame'),
      ])
      const sj = (await sr.json()) as Status
      const fj = (await fr.json()) as Frame
      setStatus(sj)
      setFrame(fj)
      setOnline(true)
    } catch {
      setOnline(false)
    }
  }, [])

  useEffect(() => {
    poll()
    const id = setInterval(poll, 250)
    return () => clearInterval(id)
  }, [poll])



  const loadConfig = async () => {
    const r = await fetch('/api/config')
    const j = (await r.json()) as AnyConfig
    setConfig(j)
    setConfigText(JSON.stringify(j, null, 2))
  }

  const saveConfig = async () => {
    const r = await fetch('/api/config', {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: configText,
    })
    const j = (await r.json()) as { ok: boolean; rebooting?: boolean }
    setOtaMsg(j.ok ? 'Config saved' : 'Save failed')
  }

  const reboot = async () => {
    await fetch('/api/reboot', { method: 'POST' })
    setOtaMsg('Rebooting…')
  }

  const applyGlobals = useCallback(async (rev: number, bright: number) => {
    try {
      const r = await fetch('/api/config')
      const j = (await r.json()) as AnyConfig
      const strips = Array.isArray(j.strips) ? j.strips : []
      strips.forEach((s) => {
        if (typeof s === 'object' && s !== null) {
          const o = s as Record<string, unknown>
          o.sensitivity = rev / 100
          o.maxBrightness = bright
        }
      })
      await fetch('/api/config', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(j),
      })
    } catch {
      /* ignore transient */
    }
  }, [])

  const applyTimers = useRef(false)
  useEffect(() => {
    if (applyTimers.current) return
    applyTimers.current = true
    const id = setInterval(() => {
      applyGlobals(sensitivityRef.current, brightnessRef.current).catch(() => {})
    }, 3000)
    return () => clearInterval(id)
  }, [applyGlobals])

  const uploadFirmware = () => fileRef.current?.click()

  const onFile = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0]
    if (!file) return
    setUploading(true)
    setOtaProg(0)
    setOtaMsg(`Uploading ${file.name}`)
    const xhr = new XMLHttpRequest()
    xhr.open('POST', '/api/ota')
    xhr.upload.onprogress = (ev) => {
      if (ev.lengthComputable) setOtaProg(Math.round((ev.loaded / ev.total) * 100))
    }
    xhr.onload = () => {
      setOtaProg(100)
      setOtaMsg(xhr.status === 200 ? 'Update complete — device rebooting…' : `Update failed (${xhr.status})`)
      setUploading(false)
    }
    xhr.onerror = () => {
      setOtaMsg('Upload network error')
      setUploading(false)
    }
    xhr.send(file)
    e.target.value = ''
  }

  const g = (accessor: (s: Status) => string) => (online && status ? accessor(status) : '—')

  return (
    <div className="app">
      <header>
        <div className="brand">
          <div className="logo">R</div>
          <div>
            <h1>Resonux</h1>
            <p>{online && status ? status.device : 'connecting…'}</p>
          </div>
        </div>
        <div className="badges">
          <span className={`badge ${online ? 'online' : 'offline'}`}>{online ? 'ONLINE' : 'OFFLINE'}</span>
          <span className="badge">{g((s) => s.wifi.mode)}</span>
          <span className="badge">{g((s) => s.wifi.ip)}</span>
          <span className="badge">{g((s) => `art-net ${s.artnet.status}`)}</span>
        </div>
      </header>

      <nav className="tabs">
        {(['live', 'config', 'ota'] as Tab[]).map((t) => (
          <button key={t} className={`tab ${tab === t ? 'active' : ''}`} onClick={() => setTab(t)}>
            {t === 'live' ? 'Live' : t === 'config' ? 'Configuration' : 'Firmware Update'}
          </button>
        ))}
      </nav>

      {tab === 'live' && (
        <div className="grid">
          <div className="card">
            <h3>Live Spectrum</h3>
            <Spectrum frame={frame} />
            <div className="status-line">
              <span className={`dot ${frame?.beat ? 'beat' : ''}`} />
              <span>{frame ? `beat ${frame.beat ? 'yes' : 'no'} (${(frame.beatStrength ?? 0).toFixed(2)})` : '…'}</span>
              <span style={{ marginLeft: 'auto' }}>{frame ? `${(frame.fps ?? 0).toFixed(0)} fps` : ''}</span>
            </div>
          </div>

          <div className="card">
            <h3>Levels</h3>
            <div className="stat-row">
              <div className="stat"><div className="val">{frame ? (frame.amp ?? 0).toFixed(2) : '—'}</div><div className="lbl">Amplitude</div></div>
              <div className="stat"><div className="val">{frame ? (frame.bass ?? 0).toFixed(2) : '—'}</div><div className="lbl">Bass</div></div>
              <div className="stat"><div className="val">{frame ? (frame.mid ?? 0).toFixed(2) : '—'}</div><div className="lbl">Mid</div></div>
              <div className="stat"><div className="val">{frame ? (frame.treble ?? 0).toFixed(2) : '—'}</div><div className="lbl">Treble</div></div>
            </div>
          </div>

          <div className="card">
            <h3>System</h3>
            <div className="stat-row">
              <div className="stat"><div className="val">{g((s) => fmt(s.uptimeMs))}</div><div className="lbl">Uptime</div></div>
              <div className="stat"><div className="val">{(online && status ? (status.heap / 1024).toFixed(0) : '—')}</div><div className="lbl">Heap KB</div></div>
              <div className="stat"><div className="val">{g((s) => String(s.stripCount))}</div><div className="lbl">Strips</div></div>
            </div>
          </div>

          <div className="card">
            <h3>Global Tuning</h3>
            <div className="row">
              <label>Sensitivity</label>
              <input type="range" min={0} max={200} value={sensitivity} onChange={(e) => setSensitivity(Number(e.target.value))} />
              <span>{(sensitivity / 100).toFixed(2)}</span>
            </div>
            <div className="row">
              <label>Max brightness</label>
              <input type="range" min={0} max={255} value={brightness} onChange={(e) => setBrightness(Number(e.target.value))} />
              <span>{brightness}</span>
            </div>
          </div>
        </div>
      )}

      {tab === 'config' && (
        <div className="card">
          <div className="row">
            <h3 style={{ margin: 0 }}>Configuration (config.json on LittleFS)</h3>
            <div className="actions">
              <button onClick={loadConfig}>Reload</button>
              <button className="danger" onClick={reboot}>Reboot</button>
            </div>
          </div>
          <p className="muted">Editing the JSON updates every strip &amp; effect. Saving writes to flash and reboots.</p>
          <textarea
            value={configText}
            onChange={(e) => setConfigText(e.target.value)}
            spellCheck={false}
            rows={18}
            style={{ width: '100%', background: 'var(--panel-2)', color: 'var(--text)', border: '1px solid var(--border)', borderRadius: 8, padding: 10, fontFamily: 'ui-monospace, monospace', fontSize: 12 }}
          />
          <div className="actions">
            <button className="primary" onClick={saveConfig}>Save &amp; reboot</button>
          </div>
          {config && <ConfiguredJsonPreview config={config} />}
          {otaMsg && <div className="ok">{otaMsg}</div>}
        </div>
      )}

      {tab === 'ota' && (
        <div className="card">
          <h3>Firmware Update (OTA)</h3>
          <p className="muted">
            Upload a compiled <code>firmware.bin</code> to flash the inactive partition; the device
            reboots into it and rolls back automatically on failure.
          </p>
          <input ref={fileRef} type="file" accept=".bin" hidden onChange={onFile} />
          <div className="actions">
            <button className="primary ota" onClick={uploadFirmware} disabled={uploading}>
              {uploading ? 'Uploading…' : 'Choose firmware.bin &amp; update'}
            </button>
          </div>
          {uploading && <progress value={otaProg} max={100} />}
          {otaMsg && <div className="ok">{otaMsg}</div>}
          <p className="muted" style={{ marginTop: 16 }}>
            Tip: build locally with <code>pio run</code> then upload{' '}
            <code>firmware/.pio/build/esp32-s3/firmware.bin</code>.
          </p>
        </div>
      )}
    </div>
  )
}
