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

type IntegrationRec = {
  kind: string
  title: string
  route: string
  safe: boolean
}

type Device = {
  id: string
  name: string
  profileId?: string
  capabilities?: number
  connection?: string
  connectionLabel?: string
  status?: string
  statusLabel?: string
  source?: number
  persisted?: boolean
  needsConditioning?: boolean
  note?: string
  lastSeen?: number
  caps?: string[]
  fw?: string
  role?: string
  protocols?: string[]
  safetyNotes?: string[]
  confidence?: { identity: number; capability: number; integration: number }
  integrations?: IntegrationRec[]
}

type DeviceProfileRow = {
  id: string
  name: string
  manufacturer?: string
  deviceType?: string
  capabilities?: number
  needsConditioning?: boolean
}

type CatalogRow = { id: string; label: string }

type DevicesData = {
  ok: boolean
  devices: Device[]
  profiles: DeviceProfileRow[]
  capCatalog?: CatalogRow[]
  connCatalog?: CatalogRow[]
}

type AudioSourceRow = {
  id: number
  ident: string
  label: string
  available: boolean
  current: boolean
  active?: boolean
}

type AudioSourcesData = {
  ok: boolean
  active: number
  reason?: string
  current: number
  autoSelect: boolean
  preferred: number
  fallback: number
  sources: AudioSourceRow[]
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

type Tab = 'live' | 'themes' | 'devices' | 'config' | 'system' | 'ota' | 'cinema'

type CinematicConfig = {
  enabled: boolean
  mode: number
  modeId?: string
  modeLabel?: string
  genre: number
  genreId?: string
  genreLabel?: string
  sensitivity: number
  reaction: number
  visualInfluence: number
  audioInfluence: number
  colorInfluence: number
  speed: number
  smoothing: number
  flashIntensity: number
  flashDurationMs: number
  flashMinGapMs: number
  boomCooldownMs: number
  whisperDim: number
  maxBrightness: number
  ambientFloor: number
  roomMapping: boolean
  waveSpeed: number
  waveDecay: number
  waveWidth: number
  maxWaves: number
  receiveUdp: boolean
  group: string
  port: number
  staleMs: number
}

type CinematicStatus = {
  source: number
  scene: number
  sceneId: string
  event: number
  eventId: string
  eventConfidence: number
  sceneConfidence: number
  luminance: number
  motion: number
  progAudio: number
  hue: number
  sat: number
  val: number
  audioLevel: number
  boom: number
  tension: number
  lastFrameMs: number
  mood: number
  moodId: string
  moodLabel: string
  moodEnergy: number
  recentEvents: number
  spatialActive?: boolean
}

type CinematicData = {
  ok: boolean
  config: CinematicConfig
  active: boolean
  companionAlive: boolean
  status: CinematicStatus
}

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

const SOURCE_REASON_LABELS: Record<string, string> = {
  configured: 'Configured source',
  auto_preferred: 'Preferred source available',
  auto_fallback: 'Preferred unavailable — using fallback',
  auto_none: 'No source available — idle',
  test_tone: 'Test tone selected',
  sync_slave: 'Sync slave — frames arrive over the network',
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

const CINE_PRESETS: Array<Partial<CinematicConfig>> = [
  { reaction: 0.45, visualInfluence: 0.5, audioInfluence: 0.3, colorInfluence: 0.8, speed: 0.3, flashIntensity: 0.35, whisperDim: 0.35, smoothing: 0.6, flashDurationMs: 200, flashMinGapMs: 120, boomCooldownMs: 600 },
  { reaction: 0.8, visualInfluence: 0.7, audioInfluence: 0.5, colorInfluence: 1, speed: 0.5, flashIntensity: 0.6, whisperDim: 0.55, smoothing: 0.4, flashDurationMs: 180, flashMinGapMs: 90, boomCooldownMs: 450 },
  { reaction: 0.95, visualInfluence: 0.85, audioInfluence: 0.65, colorInfluence: 1.1, speed: 0.65, flashIntensity: 0.75, whisperDim: 0.7, smoothing: 0.45, flashDurationMs: 220, flashMinGapMs: 110, boomCooldownMs: 500 },
  { reaction: 1, visualInfluence: 0.8, audioInfluence: 0.85, colorInfluence: 1.2, speed: 0.85, flashIntensity: 0.9, whisperDim: 0.8, smoothing: 0.3, flashDurationMs: 200, flashMinGapMs: 80, boomCooldownMs: 400 },
  { reaction: 1.25, visualInfluence: 0.9, audioInfluence: 1, colorInfluence: 1.3, speed: 1, flashIntensity: 1, whisperDim: 0.9, smoothing: 0.25, flashDurationMs: 240, flashMinGapMs: 60, boomCooldownMs: 320 },
]
const CINE_MODE_LABELS = ['Gentle', 'Balanced', 'Immersive', 'Dynamic', 'Extreme']
const CINE_MODE_DESCRIPTIONS = [
  'Soft, low-key lighting for quiet nights and dialogue.',
  'Balanced across dialogue, action and music.',
  'Stronger motion, colour and beats for films and series.',
  'Big flash/pulse response tuned for gaming.',
  'Maximum flash, pulse and motion — brightest, most reactive.',
]
const CINE_GENRES = [
  { label: 'None', value: 0, hint: 'No genre tuning' },
  { label: 'Horror', value: 1, hint: 'Tension-heavy, muted flashes, cold tones' },
  { label: 'Anime', value: 2, hint: 'Snappy, bright, warm action' },
  { label: 'Automatic', value: 3, hint: 'Director derives the feel from the content' },
]

function CinematicPanel({ data, err, onPatch, onReload }: {
  data: CinematicData
  err: string
  onPatch: (patch: Record<string, unknown>) => void
  onReload: () => void
}) {
  const [draft, setDraft] = useState<CinematicConfig>(() => data.config)
  const timers = useRef<Record<string, number>>({})

  const debounce = (key: string, value: unknown, ms: number) => {
    if (timers.current[key] !== undefined) window.clearTimeout(timers.current[key])
    timers.current[key] = window.setTimeout(() => onPatch({ [key]: value }), ms)
  }
  const knob = (key: string, value: number, match: keyof CinematicConfig) => {
    setDraft((d) => ({ ...d, [match]: value }))
    debounce(key, value, 220)
  }
  const setInt = (key: keyof CinematicConfig, raw: string | number) => {
    const v = Math.round(Number(raw))
    if (Number.isNaN(v)) return
    setDraft((d) => ({ ...d, [key]: v }))
    debounce(String(key), v, 400)
  }
  const toggle = (key: keyof CinematicConfig, value: boolean) => {
    setDraft((d) => ({ ...d, [key]: value }))
    onPatch({ [key]: value })
  }
  const setField = (key: keyof CinematicConfig, value: unknown) => {
    setDraft((d) => ({ ...d, [key]: value }))
    onPatch({ [key]: value })
  }
  const pickMode = (m: number) => {
    setDraft((d) => ({ ...d, ...CINE_PRESETS[m], mode: m, modeLabel: CINE_MODE_LABELS[m] }))
    onPatch({ applyPreset: true, mode: m })
  }

  const c = draft
  const s = data.status
  const fields: Array<{ key: keyof CinematicConfig; label: string; min: number; max: number; step: number; hint?: string }> = [
    { key: 'sensitivity', label: 'Audio sensitivity', min: 0.25, max: 3, step: 0.05 },
    { key: 'reaction', label: 'Reaction strength', min: 0, max: 1.5, step: 0.05 },
    { key: 'visualInfluence', label: 'Scene (video) influence', min: 0, max: 1, step: 0.05 },
    { key: 'audioInfluence', label: 'On-device audio influence', min: 0, max: 1, step: 0.05 },
    { key: 'colorInfluence', label: 'Dominant-colour pull', min: 0, max: 2, step: 0.05 },
    { key: 'speed', label: 'Motion speed', min: 0, max: 1, step: 0.05 },
    { key: 'smoothing', label: 'Smoothing (1 = sticky)', min: 0, max: 1, step: 0.05 },
    { key: 'flashIntensity', label: 'Flash intensity', min: 0, max: 1, step: 0.05 },
    { key: 'whisperDim', label: 'Whisper dim', min: 0, max: 1, step: 0.05 },
    { key: 'maxBrightness', label: 'Max brightness', min: 0, max: 1, step: 0.05 },
    { key: 'ambientFloor', label: 'Ambient floor', min: 0, max: 0.5, step: 0.01, hint: 'Minimum brightness in near-black scenes.' },
    { key: 'waveSpeed', label: 'Wave speed', min: 0.5, max: 5, step: 0.1, hint: 'How fast an event focus travels across the room (units = room widths per second).' },
    { key: 'waveDecay', label: 'Wave decay', min: 0.1, max: 2, step: 0.05, hint: 'How quickly wave energy fades as it moves.' },
    { key: 'waveWidth', label: 'Wave width', min: 0.1, max: 1, step: 0.05, hint: 'Breadth of the bright wavefront.' },
  ]

  return (
    <div className="grid">
      <div className="card">
        <div className="row">
          <h3 style={{ margin: 0 }}>Cinematic Mode</h3>
          <div className="actions" style={{ margin: 0 }}>
            <button onClick={onReload}>Reload</button>
            <button className={c.enabled ? 'primary' : undefined} onClick={() => toggle('enabled', !c.enabled)}>
              {c.enabled ? '✓ On' : 'Off'}
            </button>
          </div>
        </div>
        <p className="muted">
          Movie &amp; TV scene-reactive lighting. The companion app feeds live scene/event frames
          (UDP multicast); the controller blends them with its own on-device audio analysis. With no
          companion present it runs on device audio alone.
        </p>
        {err && <div className="err">{err}</div>}

        <h4>Reaction preset</h4>
        <div className="caps-cell">
          {CINE_MODE_LABELS.map((l, i) => (
            <button key={l} className={`chip-btn ${c.mode === i ? 'on' : ''}`} onClick={() => pickMode(i)}>
              {l}
            </button>
          ))}
        </div>
        <p className="muted" style={{ marginTop: 6 }}>
          {CINE_MODE_DESCRIPTIONS[c.mode] ?? ''}
        </p>
        <div className="row" style={{ marginTop: 8 }}>
          <label>Genre overlay</label>
          <select value={c.genre} onChange={(e) => setField('genre', Number(e.target.value))}>
            {CINE_GENRES.map((g) => (
              <option key={g.value} value={g.value}>{g.label}</option>
            ))}
          </select>
        </div>
        <p className="muted" style={{ marginTop: 4 }}>
          {(CINE_GENRES.find((g) => g.value === c.genre)?.hint ?? '')}
        </p>

        <h4 style={{ marginTop: 12 }}>Reaction tuning</h4>
        {fields.map((f) => (
          <FieldSlider
            key={f.key}
            label={f.label}
            value={Number(c[f.key])}
            min={f.min}
            max={f.max}
            step={f.step}
            onChange={(v) => knob(String(f.key), v, f.key)}
          />
        ))}
        {fields.filter((f) => f.hint).map((f) => (
          <p key={f.key} className="muted">{f.hint}</p>
        ))}
        <p className="muted">Knobs apply instantly over the live endpoint — no flash write, no reboot.</p>

        <h4 style={{ marginTop: 12 }}>Room mapping (spatial waves)</h4>
        <div className="row">
          <label>Map event focus across the room</label>
          <input type="checkbox" checked={!!c.roomMapping}
            onChange={(e) => toggle('roomMapping', e.target.checked)} />
        </div>
        <div className="row">
          <label>Wave count (ring depth)</label>
          <input type="number" min={4} max={12} value={c.maxWaves}
            onChange={(e) => setInt('maxWaves', e.target.value)} />
        </div>
        <p className="muted">
          With mapping on, every strip samples a wave field at its room position (
          <code>roomX</code>/<code>roomY</code> 0–1 in Configuration, default 0.5 = centre). Companion
          focus points become waves that travel outward — booms bloom, cuts sweep, chases lock on. When
          off, strips react identically (no change).
        </p>
      </div>

      <div className="card">
        <h3>Live status</h3>
        <div className="status-line">
          <span className={`dot ${data.active ? 'beat' : ''}`} />
          <span>{data.active ? 'reacting to content' : 'off'}</span>
          <span style={{ marginLeft: 'auto' }}>{data.companionAlive ? 'companion feed live' : 'device audio only'}</span>
          {!!s.spatialActive && <span style={{ marginLeft: 8 }}>· mapping</span>}
        </div>
        <div className="stat-row">
          <div className="stat"><div className="val">{s.sceneId}</div><div className="lbl">Scene</div></div>
          <div className="stat"><div className="val">{s.eventId}</div><div className="lbl">Event</div></div>
          <div className="stat"><div className="val">{s.eventConfidence}%</div><div className="lbl">Confidence</div></div>
          <div className="stat"><div className="val">{s.lastFrameMs} ms</div><div className="lbl">Frame lag</div></div>
        </div>
        <div className="stat-row">
          <div className="stat"><div className="val">{s.luminance}</div><div className="lbl">Luminance</div></div>
          <div className="stat"><div className="val">{s.motion}</div><div className="lbl">Motion</div></div>
          <div className="stat"><div className="val">{s.progAudio}</div><div className="lbl">Prog audio</div></div>
          <div className="stat"><div className="val">{(s.audioLevel ?? 0).toFixed(2)}</div><div className="lbl">Amp</div></div>
        </div>
        <div className="stat-row">
          <div className="stat"><div className="val">{s.moodLabel ?? '—'}</div><div className="lbl">Mood</div></div>
          <div className="stat"><div className="val">{(s.moodEnergy ?? 0).toFixed(2)}</div><div className="lbl">Mood energy</div></div>
          <div className="stat"><div className="val">{s.recentEvents ?? 0}</div><div className="lbl">Events (3s)</div></div>
          <div className="stat"><div className="val">{(s.boom ?? 0).toFixed(2)}</div><div className="lbl">Boom</div></div>
        </div>
        <div className="stat-row">
          <div className="stat"><div className="val">{(s.tension ?? 0).toFixed(2)}</div><div className="lbl">Tension</div></div>
          <div className="stat"><div className="val">{s.hue}</div><div className="lbl">Dominant hue</div></div>
          <div className="stat"><div className="val">{s.sat}</div><div className="lbl">Sat</div></div>
          <div className="stat"><div className="val">{s.val}</div><div className="lbl">Val</div></div>
        </div>
        <p className="muted">
          Boom/flash envelopes are cooldown-gated so loud music can never strobe; if the companion goes
          silent the controller falls back to its own audio analysis.
        </p>

        <h4>Companion link (SceneFrame UDP)</h4>
        <div className="row">
          <label>Receive companion frames</label>
          <input type="checkbox" checked={c.receiveUdp} onChange={(e) => toggle('receiveUdp', e.target.checked)} />
        </div>
        <div className="row">
          <label>Multicast group</label>
          <input type="text" value={c.group} spellCheck={false}
            onChange={(e) => { setDraft((d) => ({ ...d, group: e.target.value })); debounce('group', e.target.value, 500) }} />
        </div>
        <div className="row">
          <label>Port</label>
          <input type="number" min={1024} max={65535} value={c.port}
            onChange={(e) => setInt('port', e.target.value)} />
        </div>
        <div className="row">
          <label>Stale after (ms)</label>
          <input type="number" min={100} max={10000} step={100} value={c.staleMs}
            onChange={(e) => setInt('staleMs', e.target.value)} />
        </div>
        <div className="row">
          <label>Flash hold (ms)</label>
          <input type="number" min={30} max={1000} value={c.flashDurationMs}
            onChange={(e) => setInt('flashDurationMs', e.target.value)} />
        </div>
        <div className="row">
          <label>Flash min gap (ms)</label>
          <input type="number" min={20} max={1000} value={c.flashMinGapMs}
            onChange={(e) => setInt('flashMinGapMs', e.target.value)} />
        </div>
        <div className="row">
          <label>Boom cooldown (ms)</label>
          <input type="number" min={50} max={2000} value={c.boomCooldownMs}
            onChange={(e) => setInt('boomCooldownMs', e.target.value)} />
        </div>
        <p className="muted">
          The device groups with <code>239.255.42.11:9772</code>. Restart the app on the controller after
          changing the multicast address or port.
        </p>
      </div>
    </div>
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

  const [devicesData, setDevicesData] = useState<DevicesData | null>(null)
  const [devicesErr, setDevicesErr] = useState('')
  const [scanning, setScanning] = useState(false)
  const [identSel, setIdentSel] = useState<Record<string, string>>({})
  const [editingId, setEditingId] = useState<string | null>(null)
  const [editName, setEditName] = useState('')
  const [editNote, setEditNote] = useState('')
  const [detailId, setDetailId] = useState<string | null>(null)
  const [manualId, setManualId] = useState<string | null>(null)
  const [manualConn, setManualConn] = useState('bluetooth')
  const [manualCaps, setManualCaps] = useState<Record<string, boolean>>({})
  const [manualCond, setManualCond] = useState(false)
  const [audio, setAudio] = useState<AudioSourcesData | null>(null)
  const [audioMsg, setAudioMsg] = useState('')
  const [cine, setCine] = useState<CinematicData | null>(null)
  const [cineErr, setCineErr] = useState('')

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

  const loadDevices = useCallback(async () => {
    try {
      const r = await fetch('/api/devices')
      if (!r.ok) throw new Error('HTTP ' + r.status)
      setDevicesData((await r.json()) as DevicesData)
      setDevicesErr('')
    } catch (e) {
      setDevicesErr((e as Error).message)
    }
  }, [])

  const scanDevices = async () => {
    setScanning(true)
    setDevicesErr('')
    try {
      const r = await fetch('/api/devices/scan', { method: 'POST' })
      if (!r.ok) {
        const j = await r.json().catch(() => null)
        throw new Error((j && j.error) || 'HTTP ' + r.status)
      }
      setDevicesData((await r.json()) as DevicesData)
    } catch (e) {
      setDevicesErr((e as Error).message)
    } finally {
      setScanning(false)
    }
  }

  const identifyDevice = async (id: string, profileId: string) => {
    if (!profileId) return
    try {
      const r = await fetch('/api/device?id=' + encodeURIComponent(id), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ profileId }),
      })
      if (!r.ok) {
        const j = await r.json().catch(() => null)
        throw new Error((j && j.error) || 'HTTP ' + r.status)
      }
      setDevicesErr('')
      await loadDevices()
    } catch (e) {
      setDevicesErr((e as Error).message)
    }
  }

  const saveDeviceEdit = async (id: string) => {
    try {
      const r = await fetch('/api/device?id=' + encodeURIComponent(id), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ name: editName, note: editNote }),
      })
      if (!r.ok) {
        const j = await r.json().catch(() => null)
        throw new Error((j && j.error) || 'HTTP ' + r.status)
      }
      setEditingId(null)
      setDevicesErr('')
      await loadDevices()
    } catch (e) {
      setDevicesErr((e as Error).message)
    }
  }

  const deleteDevice = (d: Device) =>
    ask({
      title: `Remove ${d.name}?`,
      message: `"${d.id}" loses its trusted identity on this controller. It may reappear the next time a network scan detects it.`,
      confirmLabel: 'Remove',
      onConfirm: async () => {
        try {
          const r = await fetch('/api/device?id=' + encodeURIComponent(d.id), { method: 'DELETE' })
          if (!r.ok) {
            const j = await r.json().catch(() => null)
            throw new Error((j && j.error) || 'HTTP ' + r.status)
          }
          setDevicesErr('')
          await loadDevices()
        } catch (e) {
          setDevicesErr((e as Error).message)
        }
      },
    })

  const declareManual = async (d: Device) => {
    const catalog = devicesData?.capCatalog ?? []
    const bits = Object.entries(manualCaps)
      .filter(([, on]) => on)
      .map(([id]) => id)
    if (bits.length === 0) {
      setDevicesErr('Select at least one capability.')
      return
    }
    // Capability bit positions match the firmware capability table order
    // (CAP_AUDIO_INPUT = bit 0, ...), which is how the catalog is emitted.
    let mask = 0
    for (const bit of bits) {
      const i = catalog.findIndex((c) => c.id === bit)
      if (i >= 0) mask += 1 << i
    }
    if (mask === 0) {
      setDevicesErr('Unknown capability ids.')
      return
    }
    try {
      const r = await fetch('/api/device?id=' + encodeURIComponent(d.id), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          manual: true,
          name: d.name || d.id,
          connection: manualConn,
          capabilities: mask,
          needsConditioning: manualCond,
          note: 'User-identified device',
        }),
      })
      if (!r.ok) {
        const j = await r.json().catch(() => null)
        throw new Error((j && j.error) || 'HTTP ' + r.status)
      }
      setManualId(null)
      setDevicesErr('')
      await loadDevices()
    } catch (e) {
      setDevicesErr((e as Error).message)
    }
  }

  const loadAudio = useCallback(async () => {
    try {
      const r = await fetch('/api/audio/sources')
      if (!r.ok) throw new Error('HTTP ' + r.status)
      setAudio((await r.json()) as AudioSourcesData)
      setAudioMsg('')
    } catch (e) {
      setAudioMsg((e as Error).message)
    }
  }, [])

  const saveAudio = async (body: Record<string, unknown>) => {
    try {
      const r = await fetch('/api/audio/source', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body),
      })
      if (!r.ok) {
        const j = await r.json().catch(() => null)
        throw new Error((j && j.error) || 'HTTP ' + r.status)
      }
      setAudio((await r.json()) as AudioSourcesData)
      setAudioMsg('saved — applied on next boot')
    } catch (e) {
      setAudioMsg((e as Error).message)
    }
  }

  useEffect(() => {
    if (tab !== 'devices') return
    loadDevices()
    loadAudio()
  }, [tab, loadDevices, loadAudio])

  const loadCinematic = useCallback(async () => {
    try {
      const r = await fetch('/api/cinematic')
      if (!r.ok) throw new Error('HTTP ' + r.status)
      setCine((await r.json()) as CinematicData)
      setCineErr('')
    } catch (e) {
      setCineErr((e as Error).message)
    }
  }, [])

  useEffect(() => {
    if (tab !== 'cinema') return
    loadCinematic()
    const id = setInterval(loadCinematic, 500)
    return () => clearInterval(id)
  }, [tab, loadCinematic])

  const saveCine = async (patch: Record<string, unknown>) => {
    try {
      const r = await fetch('/api/cinematic', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(patch),
      })
      if (!r.ok) {
        const j = await r.json().catch(() => null)
        throw new Error((j && j.error) || 'HTTP ' + r.status)
      }
      setCineErr('')
    } catch (e) {
      setCineErr((e as Error).message)
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
        {(['live', 'themes', 'devices', 'config', 'system', 'cinema', 'ota'] as Tab[]).map((t) => (
          <button key={t} className={`tab ${tab === t ? 'active' : ''}`} onClick={() => setTab(t)}>
            {t === 'live' ? 'Live' : t === 'themes' ? 'Themes' : t === 'devices' ? 'Devices & Sources' : t === 'config' ? 'Configuration' : t === 'system' ? 'System' : t === 'cinema' ? 'Cinematic' : 'Firmware Update'}
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

      {tab === 'devices' && (
        <div className="grid">
          <div className="card" style={{ gridColumn: '1 / -1' }}>
            <div className="row">
              <h3 style={{ margin: 0 }}>Detected &amp; configured devices</h3>
              <div className="actions" style={{ margin: 0 }}>
                <button onClick={() => { loadDevices(); loadAudio() }}>Reload</button>
                <button className="primary" onClick={scanDevices} disabled={scanning}>
                  {scanning ? 'Scanning…' : 'Scan network'}
                </button>
              </div>
            </div>
            <p className="muted">
              Discovery is passive and never drives outputs. Scan probes the RESO_DISCOVER multicast group;
              local hardware (this controller, microphone, LED strip) is always listed. Everything starts
              at <em>Detected</em> — bind an identity before any integration is allowed.
            </p>
            {devicesErr && <div className="err">{devicesErr}</div>}
            {!devicesData && !devicesErr && <p className="muted">Loading…</p>}
            {devicesData && devicesData.devices.length === 0 && !devicesErr && (
              <p className="muted">No devices yet — run a scan.</p>
            )}
            {devicesData && devicesData.devices.length > 0 && (
              <div className="device-list">
                {devicesData.devices.map((d) => {
                  const local = d.id.startsWith('local:') || d.id.startsWith('resonux:')
                  const cond = d.needsConditioning
                  return (
                    <div key={d.id} className="device-row">
                      <div className="device-main">
                        <strong>{d.name}</strong>
                        <span className="device-id">{d.id}</span>
                        <div className="caps-cell">
                          {(d.caps ?? []).map((c) => (
                            <span key={c} className="chip">{c}</span>
                          ))}
                        </div>
                      </div>
                      <div className="device-meta">
                        <span className={`badge dev-${d.status ?? 'detected'}`}>{d.statusLabel ?? d.status ?? 'Detected'}</span>
                        <span className="badge">{d.connectionLabel ?? d.connection}</span>
                        {cond && <span className="badge cond">conditioning required</span>}
                        {d.persisted && <span className="badge">trusted</span>}
                        {!local && <span className="badge">transient</span>}
                      </div>
                      <div className="device-actions">
                        {cond && (
                          <span className="warn-hint" title="This device profile carries an electrical risk (e.g. speaker-level output). Do not wire it to a mic/line input without attenuation.">⚠ verify levels</span>
                        )}
                        {d.profileId ? (
                          <span className="badge prof">{d.profileId}</span>
                        ) : (
                          <select
                            value={identSel[d.id] ?? ''}
                            onChange={(e) => {
                              const v = e.target.value
                              setIdentSel((s) => ({ ...s, [d.id]: v }))
                              identifyDevice(d.id, v)
                            }}
                            disabled={!devicesData}
                          >
                            <option value="">Identify as…</option>
                            {devicesData?.profiles.map((p) => (
                              <option key={p.id} value={p.id}>{p.name}{p.needsConditioning ? ' ⚠' : ''}</option>
                            ))}
                          </select>
                        )}
                        {local ? (
                          <span className="badge">builtin</span>
                        ) : (
                          <>
                            <button
                              onClick={() => {
                                setEditingId(d.id)
                                setEditName(d.name)
                                setEditNote(d.note ?? '')
                              }}
                            >
                              Configure
                            </button>
                            {!d.profileId && (
                              <button onClick={() => setManualId(d.id)}>Manual…</button>
                            )}
                            <button className="danger" onClick={() => deleteDevice(d)}>Remove</button>
                          </>
                        )}
                        <button onClick={() => setDetailId(detailId === d.id ? null : d.id)}>
                          {detailId === d.id ? 'Hide details' : 'Details'}
                        </button>
                      </div>
                      {editingId === d.id && (
                        <div className="device-edit">
                          <div className="row">
                            <label>Name</label>
                            <input type="text" value={editName} onChange={(e) => setEditName(e.target.value)} />
                          </div>
                          <div className="row">
                            <label>Note</label>
                            <input type="text" value={editNote} onChange={(e) => setEditNote(e.target.value)} />
                          </div>
                          <div className="actions">
                            <button className="primary" onClick={() => saveDeviceEdit(d.id)}>Save</button>
                            <button onClick={() => setEditingId(null)}>Cancel</button>
                          </div>
                        </div>
                      )}
                      {manualId === d.id && (
                        <div className="device-edit">
                          <h4>Manually identify "{d.name}"</h4>
                          <p className="muted">
                            For devices automatic discovery can't name. You confirm what it is and how it
                            connects — Resonux then treats it as trusted for that capability set.
                          </p>
                          <div className="row">
                            <label>Connection</label>
                            <select value={manualConn} onChange={(e) => setManualConn(e.target.value)}>
                              {(devicesData?.connCatalog ?? []).map((c) => (
                                <option key={c.id} value={c.id}>{c.label}</option>
                              ))}
                            </select>
                          </div>
                          <div className="caps-pick">
                            {(devicesData?.capCatalog ?? []).map((c) => (
                              <label key={c.id} className="pick">
                                <input
                                  type="checkbox"
                                  checked={!!manualCaps[c.id]}
                                  onChange={(e) => setManualCaps((s) => ({ ...s, [c.id]: e.target.checked }))}
                                />
                                {c.label}
                              </label>
                            ))}
                          </div>
                          <div className="row">
                            <label>Conditioning required</label>
                            <input
                              type="checkbox"
                              checked={manualCond}
                              onChange={(e) => setManualCond(e.target.checked)}
                            />
                          </div>
                          <p className="muted">
                            {manualCond
                              ? 'The device will be marked compatible, not safe to connect — verify signal levels before wiring.'
                              : 'A clean declaration makes the device safe to connect for the selected capabilities.'}
                          </p>
                          <div className="actions">
                            <button className="primary" onClick={() => declareManual(d)}>Save manual identity</button>
                            <button onClick={() => setManualId(null)}>Cancel</button>
                          </div>
                        </div>
                      )}
                      {detailId === d.id && (
                        <div className="device-detail">
                          {d.integrations && d.integrations.length > 0 && (
                            <>
                              <h4>Possible integrations</h4>
                              <ul className="integ-list">
                                {d.integrations.map((it) => (
                                  <li key={it.kind}>
                                    {it.safe ? '✓' : '•'} <strong>{it.title}</strong>
                                    <span className="muted"> — {it.route}</span>
                                    {!it.safe && <span className="badge cond">verify first</span>}
                                  </li>
                                ))}
                              </ul>
                            </>
                          )}
                          {d.confidence && (
                            <>
                              <h4>Confidence</h4>
                              <div className="conf-row">
                                <span>Identity <b>{d.confidence.identity}%</b></span>
                                <span>Capabilities <b>{d.confidence.capability}%</b></span>
                                <span>Integration <b>{d.confidence.integration}%</b></span>
                              </div>
                            </>
                          )}
                          {(d.protocols && d.protocols.length > 0) && (
                            <>
                              <h4>Protocols</h4>
                              <div className="caps-cell">
                                {d.protocols.map((p) => <span key={p} className="chip">{p}</span>)}
                              </div>
                            </>
                          )}
                          {d.safetyNotes && d.safetyNotes.length > 0 && (
                            <>
                              <h4>Safety</h4>
                              <ul className="safety-list">
                                {d.safetyNotes.map((s, i) => <li key={i}>{s}</li>)}
                              </ul>
                            </>
                          )}
                          <div className="meta">
                            <span>Last seen {(() => {
                              const age = Math.max(0, (status?.uptimeMs ?? 0) - (d.lastSeen ?? 0))
                              return fmt(age) + ' ago'
                            })()}</span>
                            {d.fw && <span>Firmware {d.fw}</span>}
                            {d.role && <span>Role {d.role}</span>}
                            {d.note && <span>Note: {d.note}</span>}
                          </div>
                        </div>
                      )}
                    </div>
                  )
                })}
              </div>
            )}
          </div>

          <div className="card">
            <h3>Active audio source</h3>
            {!audio && !audioMsg && <p className="muted">Loading…</p>}
            {audioMsg && <div className={audioMsg.startsWith('saved') ? 'ok' : 'err'}>{audioMsg}</div>}
            {audio && (
              <>
                <div className="row">
                  <p className="active-source">
                    <strong>Active source:</strong> {(audio.sources.find((s) => s.id === (audio.active ?? audio.current))?.label) ?? '—'}
                    {audio.reason && (
                      <span className="muted"> — reason: {SOURCE_REASON_LABELS[audio.reason] ?? audio.reason}</span>
                    )}
                  </p>
                </div>
                <div className="row">
                  <label>Input source</label>
                  <select
                    value={audio.current}
                    onChange={(e) => saveAudio({ source: Number(e.target.value) })}
                  >
                    {audio.sources.map((s) => (
                      <option key={s.id} value={s.id} disabled={!s.available}>{s.label} {s.available ? '' : '(reserved)'}</option>
                    ))}
                  </select>
                </div>
                <div className="row">
                  <label>Backup if signal lost</label>
                  <select
                    value={audio.fallback}
                    onChange={(e) => saveAudio({ fallbackSource: Number(e.target.value) })}
                  >
                    {audio.sources.map((s) => (
                      <option key={s.id} value={s.id} disabled={!s.available}>{s.label}</option>
                    ))}
                  </select>
                </div>
                <div className="row">
                  <label>Auto-select back to preferred</label>
                  <input
                    type="checkbox"
                    checked={audio.autoSelect}
                    onChange={(e) => saveAudio({ autoSelect: e.target.checked })}
                  />
                </div>
                <div className="row">
                  <label>Preferred source</label>
                  <select
                    value={audio.preferred}
                    onChange={(e) => saveAudio({ preferredSource: Number(e.target.value) })}
                  >
                    {audio.sources.map((s) => (
                      <option key={s.id} value={s.id} disabled={!s.available}>{s.label}</option>
                    ))}
                  </select>
                </div>
                <p className="muted">
                  With auto-select on, the controller prefers the preferred source and waits 1.5&nbsp;s of
                  hysteresis before committing to the fallback, then returns when the preferred signal is back.
                  Source routing is applied at boot; network/device sources become selectable once a device is
                  trusted with a matching audio capability.
                </p>
              </>
            )}
          </div>
        </div>
      )}

      {tab === 'cinema' && (
        cine ? (
          <CinematicPanel
            data={cine}
            err={cineErr}
            onPatch={saveCine}
            onReload={loadCinematic}
          />
        ) : (
          <div className="card">
            <h3>Cinematic Mode</h3>
            {cineErr ? <div className="err">{cineErr}</div> : <p className="muted">Loading…</p>}
          </div>
        )
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
