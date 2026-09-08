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
  sync: { enabled: boolean; role: string; seq?: number; offsetMs?: number; masterAlive?: boolean }
  system: { state: string; action: string }
  display: { enabled: boolean; backlightPct: number; timeoutS: number; awake: boolean; wakePin?: number }
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

type Theme = {
  id: string
  name: string
  description?: string
  builtin?: boolean
  brightness: number | { base: number; min?: number }
  saturation: number
  palette: string[]
  response?: { bass: number; lowMid?: number; mid: number; highMid?: number; treble: number; beat: number; amp: number }
  animation?: { movement: number; pulse: number; flash?: number; sparkle: number; smoothing: number; contrast?: number; density?: number }
  effects?: number[]
}

type ThemeDraft = {
  id: string
  name: string
  brightness: number
  minBrightness: number
  saturation: number
  palette: string[]
  bass: number
  lowMid: number
  mid: number
  highMid: number
  treble: number
  beat: number
  amp: number
  movement: number
  pulse: number
  flash: number
  sparkle: number
  smoothing: number
  contrast: number
  density: number
}

type ThemesData = {
  themes: Theme[]
  active: string
  strips?: string[]
}

type StateT = {
  theme: { global: string; strips: string[] }
  effect: number[]
  brightness: number
  stripCount: number
  themeCount: number
}

type Tab = 'live' | 'themes' | 'config' | 'system' | 'ota'

const baseBrightness = (t: Theme) =>
  typeof t.brightness === 'object' ? t.brightness.base : t.brightness
const minBrightness = (t: Theme) => {
  if (typeof t.brightness === 'object') return t.brightness.min ?? Math.round(t.brightness.base * 0.3)
  return Math.round(t.brightness * 0.3)
}

const DEFAULT_RESP = { bass: 1, lowMid: 1, mid: 1, highMid: 1, treble: 1, beat: 1, amp: 1 }
const DEFAULT_ANIM = { movement: 0.5, pulse: 0.5, flash: 0.25, sparkle: 0.3, smoothing: 0.3, contrast: 0.5, density: 0.5 }
const DEFAULT_DRAFT: ThemeDraft = {
  id: 'custom1',
  name: '',
  brightness: 100,
  minBrightness: 15,
  saturation: 255,
  palette: ['#FF0000', '#00FF00', '#0000FF', '#FFFFFF'],
  bass: 1, lowMid: 1, mid: 1, highMid: 1, treble: 1, beat: 1, amp: 1,
  movement: 0.5, pulse: 0.5, flash: 0.25, sparkle: 0.3, smoothing: 0.3, contrast: 0.5, density: 0.5,
}

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

function FieldSlider({ label, value, min, max, step, onChange }: {
  label: string
  value: number
  min: number
  max: number
  step: number
  onChange: (v: number) => void
}) {
  return (
    <div className="row">
      <label>{label}</label>
      <input
        type="range"
        min={min}
        max={max}
        step={step}
        value={value}
        onChange={(e) => onChange(Number(e.target.value))}
      />
      <span className="num">{value.toFixed(2)}</span>
    </div>
  )
}

function PersonaBar({ label, value }: { label: string; value: number }) {
  const pct = Math.min(100, Math.max(0, Math.round(value * 100)))
  return (
    <div className="persona">
      <span>{label}</span>
      <div className="bar"><i style={{ width: `${pct}%` }} /></div>
    </div>
  )
}

function ThemeEditor({ draft, onChange, onSave, onCancel }: {
  draft: ThemeDraft
  onChange: (d: ThemeDraft) => void
  onSave: () => void
  onCancel: () => void
}) {
  const set = <K extends keyof ThemeDraft>(k: K, v: ThemeDraft[K]) => onChange({ ...draft, [k]: v })
  const setPal = (i: number, v: string) => set('palette', draft.palette.map((h, j) => (j === i ? v : h)))
  const addPal = () => set('palette', [...draft.palette.slice(0, 7), '#FFFFFF'])
  const delPal = () => set('palette', draft.palette.slice(0, -1))
  const fld = (label: string, child: React.ReactNode) => (
    <label className="fld">
      {label}
      {child}
    </label>
  )
  return (
    <div className="card editor-card">
      <h3>{draft.id ? `Edit ${draft.id}` : 'New theme'}</h3>
      <div className="grid-2">
        {fld('Name', <input type="text" value={draft.name} onChange={(e) => set('name', e.target.value)} />)}
        {fld('Id', <input type="text" value={draft.id} onChange={(e) => set('id', e.target.value)} />)}
        {fld('Brightness base %', <input type="number" min={0} max={100} value={draft.brightness} onChange={(e) => set('brightness', Math.max(0, Math.min(100, Number(e.target.value))))} />)}
        {fld('Brightness min %', <input type="number" min={0} max={100} value={draft.minBrightness} onChange={(e) => set('minBrightness', Math.max(0, Math.min(100, Number(e.target.value))))} />)}
        {fld('Saturation 0–255', <input type="number" min={0} max={255} value={draft.saturation} onChange={(e) => set('saturation', Math.max(0, Math.min(255, Number(e.target.value))))} />)}
      </div>

      <fieldset>
        <legend>Palette (hex #RRGGBB)</legend>
        <div className="pal-editor">
          {draft.palette.slice(0, 8).map((h, i) => (
            <span key={i} className="pal-row">
              <input type="text" value={h} spellCheck={false} onChange={(e) => setPal(i, e.target.value)} />
              <input type="color" value={h} onChange={(e) => setPal(i, e.target.value)} />
            </span>
          ))}
        </div>
        <div className="actions" style={{ marginTop: 8 }}>
          {draft.palette.length < 8 && <button onClick={addPal}>＋ add colour</button>}
          {draft.palette.length > 1 && <button onClick={delPal}>− remove</button>}
        </div>
      </fieldset>

      <fieldset>
        <legend>Response (0–2)</legend>
        <FieldSlider label="Bass" value={draft.bass} min={0} max={2} step={0.05} onChange={(v) => set('bass', v)} />
        <FieldSlider label="Low-mid" value={draft.lowMid} min={0} max={2} step={0.05} onChange={(v) => set('lowMid', v)} />
        <FieldSlider label="Mid" value={draft.mid} min={0} max={2} step={0.05} onChange={(v) => set('mid', v)} />
        <FieldSlider label="High-mid" value={draft.highMid} min={0} max={2} step={0.05} onChange={(v) => set('highMid', v)} />
        <FieldSlider label="Treble" value={draft.treble} min={0} max={2} step={0.05} onChange={(v) => set('treble', v)} />
        <FieldSlider label="Beat" value={draft.beat} min={0} max={2} step={0.05} onChange={(v) => set('beat', v)} />
        <FieldSlider label="Amp" value={draft.amp} min={0} max={2} step={0.05} onChange={(v) => set('amp', v)} />
      </fieldset>

      <fieldset>
        <legend>Animation (0–1)</legend>
        <FieldSlider label="Movement" value={draft.movement} min={0} max={1} step={0.05} onChange={(v) => set('movement', v)} />
        <FieldSlider label="Pulse" value={draft.pulse} min={0} max={1} step={0.05} onChange={(v) => set('pulse', v)} />
        <FieldSlider label="Beat flash" value={draft.flash} min={0} max={1} step={0.05} onChange={(v) => set('flash', v)} />
        <FieldSlider label="Sparkle" value={draft.sparkle} min={0} max={1} step={0.05} onChange={(v) => set('sparkle', v)} />
        <FieldSlider label="Smoothing" value={draft.smoothing} min={0} max={1} step={0.05} onChange={(v) => set('smoothing', v)} />
        <FieldSlider label="Contrast" value={draft.contrast} min={0} max={1} step={0.05} onChange={(v) => set('contrast', v)} />
        <FieldSlider label="Density" value={draft.density} min={0} max={1} step={0.05} onChange={(v) => set('density', v)} />
      </fieldset>

      <div className="actions">
        <button className="primary" onClick={onSave}>Save theme</button>
        <button onClick={onCancel}>Cancel</button>
      </div>
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
  const [themes, setThemes] = useState<ThemesData | null>(null)
  const [themesErr, setThemesErr] = useState('')
  const [editing, setEditing] = useState<ThemeDraft | null>(null)
  const [master, setMaster] = useState(255)
  const masterDirtyAt = useRef(0)
  const sensitivityRef = useRef(sensitivity)
  sensitivityRef.current = sensitivity
  const [sysPhase, setSysPhase] = useState<'idle' | 'restarting' | 'reconnecting' | 'poweroff'>('idle')
  const sysPhaseRef = useRef(sysPhase)
  sysPhaseRef.current = sysPhase

  const poll = useCallback(async () => {
    try {
      const [sr, fr, str] = await Promise.all([
        fetch('/api/status'),
        fetch('/api/frame'),
        fetch('/api/state'),
      ])
      const sj = (await sr.json()) as Status
      const fj = (await fr.json()) as Frame
      const stj = (await str.json()) as StateT
      setStatus(sj)
      setFrame(fj)
      setOnline(true)
      if (sysPhaseRef.current === 'restarting' || sysPhaseRef.current === 'reconnecting') {
        setSysPhase('idle')
      }
      if (
        stj.brightness !== undefined &&
        Date.now() - masterDirtyAt.current > 3000 &&
        Math.abs(stj.brightness - master) > 2
      ) {
        setMaster(stj.brightness)
      }
    } catch {
      setOnline(false)
      if (sysPhaseRef.current === 'restarting') setSysPhase('reconnecting')
    }
  }, [master])

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
    let parsed: unknown
    try {
      parsed = JSON.parse(configText)
    } catch (e) {
      setOtaMsg('Config not saved — invalid JSON: ' + (e as Error).message)
      return
    }
    if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed)) {
      setOtaMsg('Config not saved — expected a JSON object')
      return
    }
    ask({
      title: 'Save config & reboot?',
      message: 'Writing config.json to flash, then Resonux reboots to apply it. A malformed value can reset settings to defaults.',
      confirmLabel: 'Save & reboot',
      onConfirm: async () => {
        try {
          const r = await fetch('/api/config', {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: configText,
          })
          const j = (await r.json()) as { ok: boolean; rebooting?: boolean }
          setOtaMsg(j.ok ? 'Config saved — rebooting…' : 'Save failed')
        } catch (e) {
          setOtaMsg('Config save failed: ' + (e as Error).message)
        }
      },
    })
  }

  const reboot = () =>
    ask({
      title: 'Reboot Resonux?',
      message: 'The controller restarts cleanly and reconnects automatically.',
      confirmLabel: 'Reboot',
      onConfirm: () => {
        setSysPhase('restarting')
        setOtaMsg('')
        fetch('/api/system/restart', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{}' }).catch(() => {})
      },
    })

  const loadThemes = useCallback(async () => {
    try {
      const r = await fetch('/api/themes')
      if (!r.ok) throw new Error('HTTP ' + r.status)
      setThemes((await r.json()) as ThemesData)
      setThemesErr('')
    } catch (e) {
      setThemesErr((e as Error).message)
    }
  }, [])

  const selectTheme = async (id: string) => {
    try {
      const r = await fetch('/api/themes/select', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id }),
      })
      if (!r.ok) throw new Error('HTTP ' + r.status)
      await loadThemes()
    } catch (e) {
      setThemesErr((e as Error).message)
    }
  }

  const deleteTheme = async (id: string) => {
    ask({
      title: 'Delete theme?',
      message: `"${id}" will be removed from the device. This cannot be undone.`,
      confirmLabel: 'Delete',
      onConfirm: async () => {
        try {
          const r = await fetch('/api/themes?id=' + encodeURIComponent(id), { method: 'DELETE' })
          if (!r.ok) throw new Error('HTTP ' + r.status)
          await loadThemes()
        } catch (e) {
          setThemesErr((e as Error).message)
        }
      },
    })
  }

  const resetThemes = async () => {
    ask({
      title: 'Reset themes to defaults?',
      message: 'All custom themes are replaced with the built-in defaults.',
      confirmLabel: 'Reset',
      onConfirm: async () => {
        try {
          const r = await fetch('/api/themes/reset', { method: 'POST' })
          if (!r.ok) throw new Error('HTTP ' + r.status)
          await loadThemes()
        } catch (e) {
          setThemesErr((e as Error).message)
        }
      },
    })
  }

  const saveTheme = async (d: ThemeDraft) => {
    const body = {
      id: d.id.trim(),
      name: d.name.trim() || d.id.trim(),
      brightness: { base: d.brightness, min: d.minBrightness },
      saturation: d.saturation,
      palette: d.palette.filter((h) => /^#[0-9a-fA-F]{6}$/.test(h)),
      response: { bass: d.bass, lowMid: d.lowMid, mid: d.mid, highMid: d.highMid, treble: d.treble, beat: d.beat, amp: d.amp },
      animation: { movement: d.movement, pulse: d.pulse, flash: d.flash, sparkle: d.sparkle, smoothing: d.smoothing, contrast: d.contrast, density: d.density },
    }
    try {
      const r = await fetch('/api/themes', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body),
      })
      if (!r.ok) {
        const j = await r.json().catch(() => null)
        throw new Error((j && j.error) || 'HTTP ' + r.status)
      }
      setEditing(null)
      await loadThemes()
    } catch (e) {
      setThemesErr((e as Error).message)
    }
  }

  const openTheme = (t: Theme) => {
    const r = t.response ?? DEFAULT_RESP
    const an = t.animation ?? DEFAULT_ANIM
    setEditing({
      id: t.id,
      name: t.name,
      brightness: baseBrightness(t),
      minBrightness: minBrightness(t),
      saturation: t.saturation,
      palette: t.palette && t.palette.length ? t.palette : ['#FF0000', '#00FF00', '#0000FF'],
      bass: r.bass,
      lowMid: r.lowMid ?? 1,
      mid: r.mid,
      highMid: r.highMid ?? 1,
      treble: r.treble,
      beat: r.beat,
      amp: r.amp,
      movement: an.movement,
      pulse: an.pulse,
      flash: an.flash ?? 0.25,
      sparkle: an.sparkle,
      smoothing: an.smoothing,
      contrast: an.contrast ?? 0.5,
      density: an.density ?? 0.5,
    })
  }

  const newTheme = () => {
    const n = themes
      ? themes.themes.reduce((mx, x) => (/^custom\d+$/.test(x.id) ? Math.max(mx, Number(x.id.slice(6)) || 0) : mx), 0) + 1
      : 1
    setEditing({ ...DEFAULT_DRAFT, id: 'custom' + n })
  }

  const setMasterLocal = (v: number) => {
    masterDirtyAt.current = Date.now()
    setMaster(v)
    fetch('/api/state/brightness', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ value: v }),
    }).catch(() => {})
  }

  const [confirm, setConfirm] = useState<null | {
    title: string
    message: string
    confirmLabel: string
    danger?: boolean
    onConfirm: () => void
  }>(null)

  const ask = (p: { title: string; message: string; confirmLabel?: string; danger?: boolean; onConfirm: () => void }) =>
    setConfirm({
      title: p.title,
      message: p.message,
      confirmLabel: p.confirmLabel ?? 'Confirm',
      danger: p.danger ?? true,
      onConfirm: p.onConfirm,
    })

  const sensitivityTimer = useRef<number | undefined>(undefined)
  const setSensitivityLive = (v: number) => {
    setSensitivity(v)
    if (sensitivityTimer.current !== undefined) window.clearTimeout(sensitivityTimer.current)
    sensitivityTimer.current = window.setTimeout(() => {
      // Live sensitivity tuning only — never a full config PUT (no flash, no reboot).
      fetch('/api/state/sensitivity', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ value: sensitivityRef.current / 100 }),
      }).catch(() => {})
    }, 250)
  }

  const applyBacklight = (v: number) => {
    fetch('/api/state/backlight', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ value: v }),
    }).catch(() => {})
  }

  const applyTimeout = (v: number) => {
    fetch('/api/state/timeout', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ value: v }),
    }).catch(() => {})
  }

  const reqRestart = () =>
    ask({
      title: 'Restart Resonux?',
      message: 'LEDs are silenced, then the controller restarts cleanly. The dashboard reconnects automatically.',
      confirmLabel: 'Restart',
      onConfirm: () => {
        setSysPhase('restarting')
        setOtaMsg('')
        fetch('/api/system/restart', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{}' }).catch(() => {})
      },
    })

  const reqPowerOff = () =>
    ask({
      title: 'Safe power off?',
      message: 'Resonux shuts down cleanly: LEDs off, Art-Net blacked out, config saved, then deep sleep. Power it back on with the reset button or the configured wake pin.',
      confirmLabel: 'Power off',
      onConfirm: () => {
        setSysPhase('poweroff')
        setOtaMsg('')
        fetch('/api/system/power-off', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{}' }).catch(() => {})
      },
    })

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

  const syncBadgeClass = () => {
    if (!online || !status) return 'badge offline'
    const s = status.sync
    if (!s.enabled) return 'badge'
    if (s.role === 'slave' && s.masterAlive === false) return 'badge sync-lost'
    return `badge sync-${s.role}`
  }

  const syncText = (s: Status) => {
    if (!s.sync.enabled) return 'sync off'
    if (s.sync.role === 'slave') {
      return s.sync.masterAlive === false
        ? 'sync slave · master lost'
        : `sync slave · +${s.sync.offsetMs ?? 0} ms`
    }
    return `sync ${s.sync.role}`
  }

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
          <span className={syncBadgeClass()}>{g(syncText)}</span>
        </div>
      </header>

      <nav className="tabs">
        {(['live', 'themes', 'config', 'system', 'ota'] as Tab[]).map((t) => (
          <button key={t} className={`tab ${tab === t ? 'active' : ''}`} onClick={() => setTab(t)}>
            {t === 'live' ? 'Live' : t === 'themes' ? 'Themes' : t === 'config' ? 'Configuration' : t === 'system' ? 'System' : 'Firmware Update'}
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
              <div className="stat"><div className="val">{g((s) => (s.sync.enabled ? (s.sync.role === 'slave' ? (s.sync.masterAlive === false ? 'lost' : `+${s.sync.offsetMs ?? 0} ms`) : s.sync.role) : 'off'))}</div><div className="lbl">Sync</div></div>
            </div>
          </div>

          <div className="card">
            <h3>Global Tuning (live)</h3>
            <div className="row">
              <label>Master brightness</label>
              <input type="range" min={0} max={255} value={master} onChange={(e) => setMasterLocal(Number(e.target.value))} />
              <span>{master}</span>
            </div>
            <div className="row">
              <label>Sensitivity</label>
              <input type="range" min={0} max={200} value={sensitivity} onChange={(e) => setSensitivityLive(Number(e.target.value))} />
              <span>{(sensitivity / 100).toFixed(2)}×</span>
            </div>
            <p className="muted">Applied instantly over live endpoints — no flash write, no reboot.</p>
          </div>
        </div>
      )}

      {tab === 'themes' && (
        <div className="grid">
          <div className="card" style={{ gridColumn: '1 / -1' }}>
            <div className="row">
              <h3 style={{ margin: 0 }}>Lighting Themes</h3>
              <div className="actions" style={{ margin: 0 }}>
                <button onClick={() => loadThemes()}>Reload</button>
                <button className="primary" onClick={newTheme}>＋ New theme</button>
                <button className="danger" onClick={resetThemes}>Reset defaults</button>
              </div>
            </div>
            <div className="row" style={{ marginTop: 8 }}>
              <label>Master brightness (live)</label>
              <input type="range" min={0} max={255} value={master} onChange={(e) => setMasterLocal(Number(e.target.value))} />
              <span>{master}</span>
              {!online && <span className="muted">controller offline</span>}
            </div>
            {themesErr && <div className="err">{themesErr}</div>}
            {!themes && !themesErr && <p className="muted">Loading…</p>}
            {themes && themes.themes.length === 0 && !themesErr && (
              <p className="muted">No themes on the device yet.</p>
            )}
            {themes && themes.themes.length > 0 && (
              <div className="theme-grid">
                {themes.themes.map((t) => {
                  const resp = t.response ?? DEFAULT_RESP
                  const anim = t.animation ?? DEFAULT_ANIM
                  return (
                    <div key={t.id} className={`theme-card ${t.id === themes.active ? 'active' : ''}`}>
                      <div className="theme-head">
                        <strong>{t.name}</strong>
                        {t.builtin && <span className="badge">builtin</span>}
                      </div>
                      <div className="swatches">
                        {(t.palette ?? []).slice(0, 8).map((h, i) => (
                          <span key={i} style={{ background: h }} />
                        ))}
                      </div>
                      {t.description && <p className="theme-desc">{t.description}</p>}
                      <div className="meta">
                        <span>bri {baseBrightness(t)}% / min {minBrightness(t)}%</span>
                        <span>sat {t.saturation}</span>
                        <span>{t.id}</span>
                      </div>
                      {(themes.strips ?? []).includes(t.id) && <span className="badge strip">strip override</span>}
                      <div className="personas">
                        <PersonaBar label="Bass" value={resp.bass} />
                        <PersonaBar label="Groove" value={resp.lowMid ?? 1} />
                        <PersonaBar label="Treble" value={resp.treble} />
                        <PersonaBar label="Movement" value={anim.movement} />
                      </div>
                      <div className="actions">
                        <button
                          className={t.id === themes.active ? 'primary' : undefined}
                          onClick={() => selectTheme(t.id)}
                        >
                          {t.id === themes.active ? '✓ Active' : 'Select'}
                        </button>
                        <button onClick={() => openTheme(t)}>Edit</button>
                        {!t.builtin && (
                          <button className="danger" onClick={() => deleteTheme(t.id)}>Delete</button>
                        )}
                      </div>
                    </div>
                  )
                })}
              </div>
            )}
          </div>
          {editing && (
            <ThemeEditor
              draft={editing}
              onChange={setEditing}
              onSave={() => saveTheme(editing)}
              onCancel={() => setEditing(null)}
            />
          )}
        </div>
      )}

      {(sysPhase === 'restarting' || sysPhase === 'reconnecting' || sysPhase === 'poweroff') && (
        <div className={`sys-banner ${sysPhase === 'poweroff' ? 'off' : ''}`}>
          <span>
            {sysPhase === 'restarting' && 'Restarting Resonux…'}
            {sysPhase === 'reconnecting' && 'Device offline — waiting for Resonux to come back…'}
            {sysPhase === 'poweroff' && 'Resonux is off'}
          </span>
          {(sysPhase === 'restarting' || sysPhase === 'reconnecting') && (
            <button onClick={() => setSysPhase('idle')}>Dismiss</button>
          )}
        </div>
      )}

      {tab === 'system' && (
        <div className="grid">
          <div className="card">
            <h3>Status</h3>
            <div className="stat-row">
              <div className="stat"><div className="val">{g((s) => s.system.state)}</div><div className="lbl">State</div></div>
              <div className="stat"><div className="val">{g((s) => fmt(s.uptimeMs))}</div><div className="lbl">Uptime</div></div>
              <div className="stat"><div className="val">{(online && status ? (status.heap / 1024).toFixed(0) : '—')}</div><div className="lbl">Heap KB</div></div>
              <div className="stat"><div className="val">{g((s) => String(s.stripCount))}</div><div className="lbl">Strips</div></div>
            </div>
            <div className="stat-row">
              <div className="stat"><div className="val">{g((s) => s.wifi.mode)}</div><div className="lbl">Wi-Fi</div></div>
              <div className="stat"><div className="val">{g((s) => s.wifi.ip)}</div><div className="lbl">IP</div></div>
              <div className="stat"><div className="val">{g((s) => s.artnet.status)}</div><div className="lbl">Art-Net</div></div>
              <div className="stat"><div className="val">{g((s) => (s.sync.enabled ? s.sync.role : 'off'))}</div><div className="lbl">Sync</div></div>
            </div>
            {status?.system && status.system.action && status.system.action !== 'none' && (
              <div className="stat-row" style={{ marginTop: 8 }}>
                <div className="stat" style={{ flex: '1 1 100%' }}><div className="val">{status.system.action}</div><div className="lbl">Action</div></div>
              </div>
            )}
          </div>

          <div className="card">
            <h3>Display</h3>
            <div className="row">
              <label>Backlight</label>
              <input type="range" min={0} max={100} value={status?.display?.backlightPct ?? 70} onChange={(e) => applyBacklight(Number(e.target.value))} />
              <span>{status?.display?.backlightPct ?? 70}%</span>
            </div>
            <div className="row">
              <label>Screen timeout</label>
              <select value={status?.display?.timeoutS ?? 60} onChange={(e) => applyTimeout(Number(e.target.value))}>
                {[0, 30, 60, 120, 300, 600].map((t) => (
                  <option key={t} value={t}>{t === 0 ? 'Never' : `${t} seconds`}</option>
                ))}
              </select>
            </div>
            <p className="muted">Timeout only dims the panel — audio, LEDs, Art-Net and Wi-Fi keep running.</p>
          </div>

          <div className="card">
            <h3>Power</h3>
            <p className="muted">
              Both actions silence LED/DMX outputs before changing state. Restart reboots in place; Safe power-off
              saves config and enters deep sleep — your next start uses the reset button or the configured wake pin.
            </p>
            <div className="actions">
              <button onClick={reqRestart} disabled={sysPhase !== 'idle'}>Restart</button>
              <button className="danger" onClick={reqPowerOff} disabled={sysPhase !== 'idle'}>Safe power off</button>
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

      {sysPhase === 'poweroff' && (
        <div className="modal-ov">
          <div className="modal">
            <h3>Resonux is off</h3>
            <p>Resonux shut down cleanly and is in deep sleep.</p>
            <p style={{ marginTop: 8 }}>Press the device's reset button (or the configured wake pin) to power it back on. This page reconnects automatically.</p>
            <div className="actions">
              <button className="primary" onClick={() => setSysPhase('idle')}>OK</button>
            </div>
          </div>
        </div>
      )}

      {confirm && (
        <div className="modal-ov" onClick={() => setConfirm(null)}>
          <div className="modal" onClick={(e) => e.stopPropagation()}>
            <h3>{confirm.title}</h3>
            <p>{confirm.message}</p>
            <div className="actions">
              <button onClick={() => setConfirm(null)}>Cancel</button>
              <button
                className={confirm.danger ? 'danger' : 'primary'}
                onClick={() => {
                  const fn = confirm.onConfirm
                  setConfirm(null)
                  fn()
                }}
              >
                {confirm.confirmLabel}
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  )
}
