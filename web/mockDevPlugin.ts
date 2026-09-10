import type { Plugin, Connect } from 'vite'

// Mock backend for `npm run dev` — simulates the firmware REST API so the
// dashboard can be developed/tested without hardware.

const DEFAULT_CONFIG: Record<string, unknown> = {
  version: 1,
  deviceName: 'Resonux-SIM',
  masterBrightness: 255,
  audioSource: 1,
  autoSelectSource: false,
  preferredSource: 1,
  fallbackSource: 0,
  themeTransitionMs: 500,
  stripCount: 1,
  micSck: 4,
  micWs: 5,
  micData: 6,
  audio: {
    sampleRate: 44100,
    fftSize: 1024,
    hopSize: 512,
    bandCount: 9,
    gain: 1,
    normMode: 1,
    dbFloor: -45,
    dbCeil: -10,
    noiseGate: 0.02,
    smooth: true,
    attack: 0.6,
    release: 0.14,
    beatDetect: true,
    beatSens: 1,
    beatMinGapMs: 250,
    beatLowBands: 3,
    ampGain: 1.5,
    ampDbFloor: -55,
    ampDbCeil: -12,
  },
  strips: [
    {
      name: 'Sim Strip',
      driverType: 0,
      chipset: 0,
      colorOrder: 0,
      dataPin: 48,
      clockPin: -1,
      ledCount: 60,
      reverse: false,
      zoneCount: 1,
      zones: [{ pins: [48, -1, -1], nPins: 1, pos: 0, band: 0 }],
      commonAnode: false,
      pwmFreqHz: 1000,
      effectId: 0,
      maxBrightness: 255,
      minBrightness: 12,
      maxCurrentA: 1,
      maxVolts: 5,
      palette: 0,
      startHue: 0,
      hueSpeed: 8,
      sensitivity: 1,
      decay: 0,
      targetFps: 60,
    },
  ],
  net: {
    enabled: true,
    mode: 0,
    apSsid: 'Resonux',
    apPassword: '',
    staSsid: '',
    staPassword: '',
  },
  artnet: {
    enabled: false,
    ssid: '',
    password: '',
    useDhcp: true,
    staticIp: [0, 0, 0, 0],
    staticMask: [0, 0, 0, 0],
    staticGw: [0, 0, 0, 0],
    universe: 0,
    audioReactive: true,
    panSpeed: 0.5,
    tiltSpeed: 0.5,
    colorSensitivity: 1,
  },
  fixtures: [],
}

function json(res: Connect.ServerResponse, status: number, body: unknown) {
  res.statusCode = status
  res.setHeader('Content-Type', 'application/json')
  res.end(JSON.stringify(body))
}

function seededNoise(): Float32Array {
  // deterministic-ish pseudo noise so the sim looks stable across reloads
  const a = new Float32Array(9)
  const t = Date.now() / 1000
  for (let i = 0; i < a.length; ++i) {
    a[i] =
      0.5 +
      0.5 *
        (Math.sin(t * (1 + i * 0.37) + i * 1.7) * 0.55 +
          Math.sin(t * (2 + i * 0.13) * 1.1 + i * 3.1) * 0.35 +
          0.1 * Math.sin(t * (4 + i * 0.61)))
  }
  return a
}

export default function mockDevPlugin(): Plugin {
  let configText = JSON.stringify(DEFAULT_CONFIG, null, 2)
  let startMs = Date.now()

  // Effect ids mirror firmware EffectId.
  const E = {
    spectrum: 0, bassPulse: 1, beatFlash: 2, freqWave: 3, freqColor: 4,
    rainbow: 5, vuMeter: 6, energyPulse: 7, runningWave: 8, spark: 9,
    gradient: 10, beatRipple: 11, musicWave: 12, colorEnergy: 13, customMapping: 14,
  }

  const DEFAULT_THEMES = [
    {
      id: 'classic', name: 'Classic', description: 'The original warm hue-sweep signature look.', builtin: true,
      brightness: { base: 100, min: 30 }, saturation: 255,
      palette: ['#FF0000', '#FF7700', '#FFEE00'],
      response: { bass: 1, lowMid: 1, mid: 1, highMid: 1, treble: 1, beat: 1, amp: 1 },
      animation: { movement: 0.5, pulse: 0.5, flash: 0.25, sparkle: 0.3, smoothing: 0.3, contrast: 0.5, density: 0.5 },
      effects: [],
    },
    {
      id: 'party', name: 'Party', description: 'High-energy multi-colour for fast music.', builtin: true,
      brightness: { base: 100, min: 20 }, saturation: 255,
      palette: ['#FF0000', '#FF00FF', '#00FF00', '#00FFFF', '#FFFF00'],
      response: { bass: 1, lowMid: 1.1, mid: 1.2, highMid: 1.1, treble: 1, beat: 1.2, amp: 1 },
      animation: { movement: 0.7, pulse: 0.8, flash: 0.5, sparkle: 0.4, smoothing: 0.25, contrast: 0.6, density: 0.7 },
      effects: [E.freqColor, E.rainbow],
    },
    {
      id: 'bass_heavy', name: 'Bass Heavy', description: 'Deep, weighty response that wallops the low end.', builtin: true,
      brightness: { base: 100, min: 25 }, saturation: 255,
      palette: ['#FF0000', '#990000', '#550000', '#FF4400'],
      response: { bass: 1.5, lowMid: 1.3, mid: 0.8, highMid: 0.7, treble: 0.6, beat: 1.3, amp: 1 },
      animation: { movement: 0.6, pulse: 0.9, flash: 0.3, sparkle: 0.2, smoothing: 0.3, contrast: 0.6, density: 0.75 },
      effects: [E.bassPulse, E.energyPulse],
    },
    {
      id: 'spectrum', name: 'Spectrum', description: 'Band-by-band hue spread across the strip.', builtin: true,
      brightness: { base: 100, min: 25 }, saturation: 255,
      palette: ['#FF0000', '#FFFF00', '#00FF00', '#00FFFF', '#0000FF', '#FF00FF', '#FF0000'],
      response: { bass: 1, lowMid: 1, mid: 1, highMid: 1, treble: 1, beat: 1, amp: 1 },
      animation: { movement: 0.5, pulse: 0.5, flash: 0.2, sparkle: 0.2, smoothing: 0.4, contrast: 0.55, density: 0.6 },
      effects: [E.spectrum, E.freqWave],
    },
    {
      id: 'rainbow', name: 'Rainbow', description: 'Classic full-spectrum colour cycling.', builtin: true,
      brightness: { base: 100, min: 25 }, saturation: 255,
      palette: ['#FF0000', '#FF8800', '#FFFF00', '#00FF00', '#0088FF', '#8800FF', '#FF0088', '#FF0000'],
      response: { bass: 1, lowMid: 1, mid: 1, highMid: 1, treble: 1, beat: 1, amp: 1 },
      animation: { movement: 0.5, pulse: 0.4, flash: 0.25, sparkle: 0.2, smoothing: 0.35, contrast: 0.5, density: 0.5 },
      effects: [E.rainbow, E.vuMeter],
    },
    {
      id: 'club', name: 'Club', description: 'Strobe-friendly pulse for DJ sets.', builtin: true,
      brightness: { base: 95, min: 15 }, saturation: 255,
      palette: ['#FF00FF', '#00FFFF', '#FFFFFF', '#0066FF'],
      response: { bass: 1.2, lowMid: 1, mid: 1, highMid: 0.9, treble: 1, beat: 1.4, amp: 1 },
      animation: { movement: 0.5, pulse: 1, flash: 0.7, sparkle: 0.3, smoothing: 0.15, contrast: 0.7, density: 0.6 },
      effects: [E.beatFlash, E.energyPulse],
    },
    {
      id: 'chill', name: 'Chill', description: 'Slow, moody and smooth for relaxed listening.', builtin: true,
      brightness: { base: 70, min: 12 }, saturation: 220,
      palette: ['#2B5B84', '#4FA3D8', '#E8B8D6'],
      response: { bass: 0.4, lowMid: 0.6, mid: 0.7, highMid: 0.5, treble: 0.3, beat: 0.2, amp: 0.6 },
      animation: { movement: 0.2, pulse: 0.15, flash: 0.05, sparkle: 0.1, smoothing: 0.7, contrast: 0.35, density: 0.3 },
      effects: [E.gradient, E.musicWave],
    },
    {
      id: 'rock', name: 'Rock', description: 'Aggressive mid-forward energy for live guitars.', builtin: true,
      brightness: { base: 100, min: 20 }, saturation: 255,
      palette: ['#FF2200', '#FF8800', '#FFFFFF'],
      response: { bass: 1.1, lowMid: 1.1, mid: 1.4, highMid: 1.2, treble: 1, beat: 1.2, amp: 1 },
      animation: { movement: 0.7, pulse: 0.8, flash: 0.4, sparkle: 0.5, smoothing: 0.2, contrast: 0.6, density: 0.6 },
      effects: [E.spark, E.spectrum],
    },
    {
      id: 'edm', name: 'EDM', description: 'Bright cyan/magenta with aggressive beat pumping.', builtin: true,
      brightness: { base: 100, min: 20 }, saturation: 255,
      palette: ['#FF00FF', '#00FFFF', '#0066FF'],
      response: { bass: 1, lowMid: 0.8, mid: 0.6, highMid: 0.8, treble: 0.9, beat: 1.3, amp: 1 },
      animation: { movement: 0.8, pulse: 0.9, flash: 0.6, sparkle: 0.7, smoothing: 0.2, contrast: 0.65, density: 0.7 },
      effects: [E.beatPulse, E.energyPulse],
    },
    {
      id: 'vocal', name: 'Vocal', description: 'Mid-forward palette tuned for singers and leads.', builtin: true,
      brightness: { base: 90, min: 20 }, saturation: 245,
      palette: ['#FF88CC', '#EEEEFF', '#8800FF', '#FFFFFF'],
      response: { bass: 0.7, lowMid: 1, mid: 1.3, highMid: 1.4, treble: 1.1, beat: 0.6, amp: 1 },
      animation: { movement: 0.4, pulse: 0.3, flash: 0.15, sparkle: 0.3, smoothing: 0.55, contrast: 0.5, density: 0.5 },
      effects: [E.musicWave, E.freqWave],
    },
    {
      id: 'beat', name: 'Beat', description: 'Ripple-and-flash motion that locks to the beat.', builtin: true,
      brightness: { base: 90, min: 15 }, saturation: 255,
      palette: ['#FFFFFF', '#FF6600', '#0066FF'],
      response: { bass: 1.2, lowMid: 1, mid: 0.9, highMid: 0.8, treble: 0.9, beat: 1.5, amp: 1 },
      animation: { movement: 0.6, pulse: 1, flash: 0.6, sparkle: 0.3, smoothing: 0.15, contrast: 0.7, density: 0.6 },
      effects: [E.beatRipple, E.beatFlash],
    },
    {
      id: 'ambient', name: 'Ambient', description: 'Shimmering, gentle gradients for background scenes.', builtin: true,
      brightness: { base: 60, min: 8 }, saturation: 200,
      palette: ['#3A5F8A', '#7FBFE8', '#C8E6F5', '#FFD9E8'],
      response: { bass: 0.5, lowMid: 0.6, mid: 0.5, highMid: 0.5, treble: 0.4, beat: 0.15, amp: 0.5 },
      animation: { movement: 0.25, pulse: 0.1, flash: 0.05, sparkle: 0.15, smoothing: 0.8, contrast: 0.3, density: 0.25 },
      effects: [E.gradient, E.musicWave],
    },
    {
      id: 'fire', name: 'Fire', description: 'Burning embers: deep reds and hot oranges.', builtin: true,
      brightness: { base: 100, min: 15 }, saturation: 255,
      palette: ['#5A0000', '#FF2A00', '#FF8800', '#FFDD00'],
      response: { bass: 1.1, lowMid: 1.2, mid: 1.1, highMid: 0.9, treble: 0.5, beat: 0.9, amp: 1 },
      animation: { movement: 0.65, pulse: 0.7, flash: 0.35, sparkle: 0.55, smoothing: 0.3, contrast: 0.6, density: 0.65 },
      effects: [E.bassPulse, E.spark],
    },
    {
      id: 'ocean', name: 'Ocean', description: 'Deep blues and aqua waves, relaxed motion.', builtin: true,
      brightness: { base: 80, min: 12 }, saturation: 235,
      palette: ['#001133', '#004488', '#00AACC', '#66DDFF'],
      response: { bass: 0.9, lowMid: 0.8, mid: 0.7, highMid: 0.6, treble: 0.4, beat: 0.6, amp: 0.8 },
      animation: { movement: 0.45, pulse: 0.35, flash: 0.1, sparkle: 0.2, smoothing: 0.6, contrast: 0.45, density: 0.45 },
      effects: [E.freqWave, E.musicWave],
    },
    {
      id: 'cyberpunk', name: 'Cyberpunk', description: 'Neon pink/cyan with electrifying contrast.', builtin: true,
      brightness: { base: 100, min: 20 }, saturation: 255,
      palette: ['#FF00C8', '#00F0FF', '#7A00FF', '#202040'],
      response: { bass: 1.1, lowMid: 0.9, mid: 1, highMid: 1.1, treble: 1.2, beat: 1.2, amp: 1 },
      animation: { movement: 0.75, pulse: 0.85, flash: 0.5, sparkle: 0.6, smoothing: 0.2, contrast: 0.75, density: 0.7 },
      effects: [E.energyPulse, E.freqColor],
    },
    {
      id: 'classical', name: 'Classical', description: 'Refined, spacious staging for orchestras.', builtin: true,
      brightness: { base: 70, min: 10 }, saturation: 210,
      palette: ['#C9A227', '#E8DDC0', '#403010', '#FFFFFF'],
      response: { bass: 0.6, lowMid: 0.8, mid: 0.9, highMid: 0.7, treble: 0.7, beat: 0.2, amp: 0.6 },
      animation: { movement: 0.25, pulse: 0.15, flash: 0.05, sparkle: 0.15, smoothing: 0.75, contrast: 0.4, density: 0.35 },
      effects: [E.gradient, E.musicWave],
    },
    {
      id: 'afro_house', name: 'Afro House', description: 'Warm, organic, groove-led South African rhythms.', builtin: true,
      brightness: { base: 50, min: 12 }, saturation: 235,
      palette: ['#FF7A18', '#8B3A16', '#1FA8A0', '#FFD166', '#4A1E6E'],
      response: { bass: 0.85, lowMid: 0.95, mid: 0.65, highMid: 0.45, treble: 0.35, beat: 0.75, amp: 0.85 },
      animation: { movement: 0.55, pulse: 0.55, flash: 0.15, sparkle: 0.2, smoothing: 0.65, contrast: 0.55, density: 0.7 },
      effects: [E.runningWave, E.energyPulse, E.freqWave, E.bassPulse, E.colorEnergy],
    },
    {
      id: 'auto', name: 'Auto', description: 'Reads the music and picks the best mood automatically.', builtin: true,
      brightness: { base: 85, min: 15 }, saturation: 255,
      palette: ['#FF7A18', '#FF00C8', '#00F0FF', '#00CC66'],
      response: { bass: 1, lowMid: 1, mid: 1, highMid: 1, treble: 1, beat: 1, amp: 1 },
      animation: { movement: 0.5, pulse: 0.5, flash: 0.25, sparkle: 0.3, smoothing: 0.4, contrast: 0.5, density: 0.5 },
      effects: [],
    },
    {
      id: 'custom1', name: 'My Theme', description: 'A custom user theme — try deleting or editing me.',
      brightness: { base: 85, min: 15 }, saturation: 255,
      palette: ['#FF6A00', '#22D3EE', '#FFFFFF'],
      response: { bass: 0.9, lowMid: 0.8, mid: 0.8, highMid: 0.8, treble: 0.7, beat: 0.8, amp: 1 },
      animation: { movement: 0.6, pulse: 0.6, flash: 0.3, sparkle: 0.5, smoothing: 0.35, contrast: 0.5, density: 0.6 },
      effects: [],
    },
  ]

  let themesList: unknown[] = DEFAULT_THEMES.map((t) => ({ ...t }))
  let activeThemeId = 'auto'
  let masterBrightness = (DEFAULT_CONFIG.masterBrightness as number) ?? 255
  let stripEffects = [0]
  let stripThemes: string[] = []
  let syncSeq = 0
  let mockSysState = 'running'
  let mockSysAction = 'none'
  let mockOfflineUntil = 0
  let mockBacklight = 70
  let mockTimeout = 60
  let mockSensitivity = 0.5

  // ---- device registry + audio source sim ----------------------------------
  const SOURCES = [
    { id: 0, ident: 'none', label: 'No input' },
    { id: 1, ident: 'mic', label: 'Microphone (INMP441)' },
    { id: 2, ident: 'line_in', label: 'Line input' },
    { id: 3, ident: 'usb_audio', label: 'USB audio' },
    { id: 4, ident: 'bluetooth', label: 'Bluetooth audio' },
    { id: 5, ident: 'network', label: 'Network audio' },
    { id: 6, ident: 'device', label: 'Device audio' },
    { id: 7, ident: 'test', label: 'Test tone' },
  ]

  const PROFILES = [
    { id: 'resonux', name: 'Universal Controller', manufacturer: 'Resonux', deviceType: 'lighting-controller', needsConditioning: false },
    { id: 'am006', name: 'AM-006', manufacturer: '(validation unit)', deviceType: 'multimedia-speaker', needsConditioning: true },
    { id: 'inmp441_mic', name: 'Omnidirectional I2S Microphone', manufacturer: 'INMP441', deviceType: 'microphone', needsConditioning: false },
    { id: 'led_onboard', name: 'LED Output', manufacturer: '(integrated)', deviceType: 'led-driver', needsConditioning: false },
    { id: 'usb_audio', name: 'USB Audio Interface', manufacturer: 'Generic', deviceType: 'audio-interface', needsConditioning: false },
    { id: 'bluetooth_speaker', name: 'Bluetooth Speaker', manufacturer: 'Generic', deviceType: 'speaker', needsConditioning: false },
    { id: 'network_audio', name: 'Network Streamer', manufacturer: 'Generic', deviceType: 'network-audio', needsConditioning: false },
    { id: 'touch_display', name: 'Touch Display', manufacturer: '(integrated)', deviceType: 'display', needsConditioning: false },
    { id: 'dmx_fixture', name: 'DMX Fixture', manufacturer: 'Generic', deviceType: 'lighting-fixture', needsConditioning: false },
  ]

  let devices = [
    { id: 'resonux:self', name: 'Resonux-SIM', profileId: 'resonux', connection: 'network', connectionLabel: 'Network', status: 'configured', statusLabel: 'Configured', source: 5, persisted: true, needsConditioning: false, note: '', lastSeen: Date.now(), caps: ['network', 'artnet', 'led_output', 'addressable_led', 'rgb_pwm', 'microphone', 'audio_input'] },
    { id: 'local:mic', name: 'INMP441 microphone', profileId: 'inmp441_mic', connection: 'gpio', connectionLabel: 'GPIO', status: 'configured', statusLabel: 'Configured', source: 1, persisted: true, needsConditioning: false, note: '', lastSeen: Date.now(), caps: ['microphone', 'audio_input'] },
    { id: 'local:led', name: 'Onboard LED strip', profileId: '', connection: 'gpio', connectionLabel: 'GPIO', status: 'configured', statusLabel: 'Configured', source: 6, persisted: true, needsConditioning: false, note: '', lastSeen: Date.now(), caps: ['led_output', 'addressable_led', 'rgb_pwm'] },
  ]

  let audioState = { source: 1, autoSelect: false, preferred: 1, fallback: 0 }

  return {
    name: 'resonux-mock-api',
    configureServer(server) {
      server.middlewares.use((req: Connect.IncomingMessage, res: Connect.ServerResponse, next: Connect.NextFunction) => {
        const url = (req.url ?? '').split('?')[0]
        if (!url.startsWith('/api/')) return next()

        if (url === '/api/status') {
          if (Date.now() < mockOfflineUntil) {
            res.statusCode = 503
            res.end()
            return
          }
          json(res, 200, {
            ok: true,
            device: 'Resonux (simulator)',
            uptimeMs: Date.now() - startMs,
            heap: 180000 + Math.floor(Math.random() * 20000),
            fps: 60 + Math.floor(Math.random() * 5),
            stripCount: 1,
            wifi: { mode: 'AP+STA', ip: '192.168.4.1', connected: true },
            artnet: { enabled: false, fixtures: 0, status: 'idle' },
            sync: {
              enabled: true,
              role: 'slave',
              seq: ++syncSeq,
              offsetMs: 50 + Math.floor(Math.random() * 300),
              masterAlive: true,
            },
            system: { state: mockSysState, action: mockSysAction },
            display: {
              enabled: true,
              backlightPct: mockBacklight,
              timeoutS: mockTimeout,
              awake: mockSysState === 'running',
              wakePin: -1,
            },
          })
          return
        }

        if (url === '/api/frame') {
          const a = seededNoise()
          const beat = Math.random() < 0.06
          const beatStrength = beat ? 0.5 + Math.random() * 0.5 : 0
          const amp = a.reduce((s, v) => s + v, 0) / a.length
          json(res, 200, {
            ok: true,
            fps: 60,
            amp,
            bass: a[0] * 0.9 + (beat ? beatStrength * 0.3 : 0),
            lowMid: a[1],
            mid: a[2] * 0.8 + a[3] * 0.3,
            highMid: a[4],
            treble: a[5] + a[6] * 0.5,
            beat,
            beatStrength,
            bandCount: 9,
            bands: Array.from(a),
            peaks: a.map((v) => Math.min(1, v + Math.random() * 0.1)),
          })
          return
        }

        if (url === '/api/config' && req.method === 'GET') {
          json(res, 200, JSON.parse(configText))
          return
        }

        if (url === '/api/config' && (req.method === 'PUT' || req.method === 'POST')) {
          let raw = ''
          req.on('data', (c) => (raw += c))
          req.on('end', () => {
            try {
              const parsed = JSON.parse(raw)
              if (parsed && typeof parsed === 'object') {
                configText = JSON.stringify(parsed, null, 2)
                json(res, 200, { ok: true, rebooting: true })
              } else {
                json(res, 400, { ok: false, error: 'config must be an object' })
              }
            } catch (e) {
              json(res, 400, { ok: false, error: `invalid JSON: ${(e as Error).message}` })
            }
          })
          return
        }

        if (url === '/api/reboot') {
          startMs = Date.now()
          json(res, 200, { ok: true })
          return
        }

        if (url === '/api/system/restart') {
          mockSysState = 'restarting'
          mockSysAction = 'restart'
          mockOfflineUntil = Date.now() + 2500
          json(res, 200, { ok: true, state: 'restarting' })
          setTimeout(() => {
            mockSysState = 'running'
            mockSysAction = 'none'
          }, 3000)
          return
        }

        if (url === '/api/system/power-off') {
          mockSysState = 'sleeping'
          mockSysAction = 'power_off'
          json(res, 200, { ok: true, state: 'sleeping' })
          return
        }

        if (url === '/api/system/status') {
          json(res, 200, { ok: true, state: mockSysState, action: mockSysAction })
          return
        }

        if (url === '/api/ota') {
          // Drop the body, simulate success after a short while.
          let size = 0
          req.on('data', (c) => (size += c.length))
          req.on('end', () => {
            setTimeout(() => {
              json(res, 200, { ok: true, size })
            }, 800)
          })
          return
        }

        if (url === '/api/themes') {
          if (req.method === 'GET') {
            json(res, 200, { themes: themesList, active: activeThemeId, strips: stripThemes })
            return
          }
          if (req.method === 'PUT') {
            let raw = ''
            req.on('data', (c) => (raw += c))
            req.on('end', () => {
              try {
                const parsed = JSON.parse(raw) as { id?: string }
                if (!parsed || typeof parsed !== 'object' || !parsed.id) {
                  json(res, 400, { ok: false, error: 'theme requires an id' })
                  return
                }
                const i = themesList.findIndex((t) => (t as { id: string }).id === parsed.id)
                if (i >= 0) {
                  themesList[i] = { ...(themesList[i] as object), ...parsed }
                } else {
                  themesList.push(parsed)
                }
                json(res, 200, { ok: true })
              } catch (e) {
                json(res, 400, { ok: false, error: `invalid JSON: ${(e as Error).message}` })
              }
            })
            return
          }
          if (req.method === 'DELETE') {
            const id = new URL(req.url ?? '', 'http://x').searchParams.get('id') ?? ''
            const i = themesList.findIndex((t) => (t as { id: string }).id === id)
            if (i < 0 || (themesList[i] as { builtin?: boolean }).builtin) {
              json(res, 400, { ok: false, error: 'cannot delete builtin theme' })
              return
            }
            themesList.splice(i, 1)
            if (activeThemeId === id) activeThemeId = 'auto'
            json(res, 200, { ok: true })
            return
          }
          json(res, 405, { ok: false, error: 'method not allowed' })
          return
        }

        if (url === '/api/themes/select') {
          let raw = ''
          req.on('data', (c) => (raw += c))
          req.on('end', () => {
            try {
              const body = JSON.parse(raw) as { id?: string }
              if (!body.id || !themesList.some((t) => (t as { id: string }).id === body.id)) {
                json(res, 400, { ok: false, error: 'unknown theme' })
                return
              }
              activeThemeId = body.id
              json(res, 200, { ok: true })
            } catch {
              json(res, 400, { ok: false, error: 'bad json' })
            }
          })
          return
        }

        if (url === '/api/themes/reset') {
          themesList = DEFAULT_THEMES.map((t) => ({ ...t }))
          activeThemeId = 'auto'
          stripThemes = []
          json(res, 200, { ok: true })
          return
        }

        // ---- devices + audio source (mirrors firmware /api/devices* + /api/audio/*)
        const devicesReply = (extra: Record<string, unknown> = {}) =>
          json(res, 200, {
            ok: true,
            devices: devices.map((d) => ({ ...d })),
            profiles: PROFILES.map((p) => ({ ...p })),
            ...extra,
          })

        if (url === '/api/devices' && req.method === 'GET') {
          devicesReply()
          return
        }

        if (url === '/api/devices/scan') {
          const found = devices.find((d) => d.id === 'wifi:resonux-sim2')
          if (!found && devices.length < 16) {
            devices.push({
              id: 'wifi:resonux-sim2',
              name: 'Resonux-SIM2',
              profileId: '',
              connection: 'network',
              connectionLabel: 'Network',
              status: 'detected',
              statusLabel: 'Detected',
              source: 5,
              persisted: false,
              needsConditioning: false,
              note: '',
              lastSeen: Date.now(),
              caps: ['network', 'audio_input'],
            })
          } else if (found) {
            found.lastSeen = Date.now()
          }
          devicesReply({ found: found ? 1 : 1, registered: devices.length })
          return
        }

        if (url === '/api/device') {
          const id = new URL(req.url ?? '', 'http://x').searchParams.get('id') ?? ''
          if (req.method === 'GET' || req.method === 'POST' || req.method === 'DELETE') {
            const i = devices.findIndex((d) => d.id === id)
            if (req.method === 'GET') {
              if (i < 0) {
                json(res, 404, { ok: false, error: 'not_found' })
                return
              }
              devicesReply()
              return
            }
            if (req.method === 'DELETE') {
              if (id.startsWith('local:') || id.startsWith('resonux:')) {
                json(res, 400, { ok: false, error: 'builtin_row' })
                return
              }
              if (i < 0) {
                json(res, 404, { ok: false, error: 'not_found' })
                return
              }
              devices.splice(i, 1)
              json(res, 200, { ok: true, deleted: true })
              return
            }
            // POST: identify / configure
            if (i < 0) {
              json(res, 404, { ok: false, error: 'not_found' })
              return
            }
            let raw = ''
            req.on('data', (c) => (raw += c))
            req.on('end', () => {
              try {
                const body = JSON.parse(raw) as Record<string, unknown>
                const profileId = String(body.profileId ?? '')
                const name = String(body.name ?? '')
                const note = String(body.note ?? '')
                const d = devices[i]
                if (profileId) {
                  const p = PROFILES.find((x) => x.id === profileId)
                  if (!p) {
                    json(res, 400, { ok: false, error: 'bad_request' })
                    return
                  }
                  d.profileId = profileId
                  d.needsConditioning = p.needsConditioning
                  d.status = p.needsConditioning ? 'compatible' : 'safe_to_connect'
                  d.statusLabel = p.needsConditioning ? 'Compatible' : 'Safe to connect'
                  d.persisted = true
                }
                if (name) d.name = name
                if (note) d.note = note
                json(res, 200, { ok: true })
              } catch {
                json(res, 400, { ok: false, error: 'bad_json' })
              }
            })
            return
          }
          json(res, 405, { ok: false, error: 'method not allowed' })
          return
        }

        if (url === '/api/audio/sources' && req.method === 'GET') {
          json(res, 200, {
            ok: true,
            current: audioState.source,
            autoSelect: audioState.autoSelect,
            preferred: audioState.preferred,
            fallback: audioState.fallback,
            sources: SOURCES.map((s) => ({
              ...s,
              available:
                s.id === 0 || s.id === 7 || s.id === 1 || s.id === 5,
              current: s.id === audioState.source,
            })),
          })
          return
        }

        if (url === '/api/audio/source') {
          let raw = ''
          req.on('data', (c) => (raw += c))
          req.on('end', () => {
            try {
              const body = JSON.parse(raw) as Record<string, unknown>
              if (body.source !== undefined) {
                const v = Number(body.source)
                if (!Number.isInteger(v) || v < 0 || v > 7) {
                  json(res, 400, { ok: false, error: 'bad_value' })
                  return
                }
                audioState.source = v
              }
              if (body.autoSelect !== undefined) audioState.autoSelect = Boolean(body.autoSelect)
              if (body.preferredSource !== undefined) {
                const v = Number(body.preferredSource)
                if (!Number.isInteger(v) || v < 0 || v > 7) {
                  json(res, 400, { ok: false, error: 'bad_value' })
                  return
                }
                audioState.preferred = v
              }
              if (body.fallbackSource !== undefined) {
                const v = Number(body.fallbackSource)
                if (!Number.isInteger(v) || v < 0 || v > 7) {
                  json(res, 400, { ok: false, error: 'bad_value' })
                  return
                }
                audioState.fallback = v
              }
              json(res, 200, { ok: true, ...audioState })
            } catch {
              json(res, 400, { ok: false, error: 'bad_json' })
            }
          })
          return
        }

        const readJson = (cb: (body: Record<string, unknown>) => void) => {
          let raw = ''
          req.on('data', (c) => (raw += c))
          req.on('end', () => {
            try {
              cb(JSON.parse(raw))
            } catch {
              json(res, 400, { ok: false, error: 'bad json' })
            }
          })
        }

        if (url === '/api/state') {
          json(res, 200, {
            ok: true,
            theme: { global: activeThemeId, strips: stripThemes },
            effect: stripEffects,
            brightness: masterBrightness,
            stripCount: 1,
            themeCount: themesList.length,
          })
          return
        }

        if (url === '/api/state/brightness') {
          readJson((body) => {
            const v = Number(body.value)
            if (!Number.isFinite(v) || v < 0 || v > 255) {
              json(res, 400, { ok: false, error: 'value must be 0..255' })
              return
            }
            masterBrightness = Math.round(v)
            json(res, 200, { ok: true, brightness: masterBrightness })
          })
          return
        }

        if (url === '/api/state/effect') {
          readJson((body) => {
            const strip = Number(body.strip ?? 0)
            const id = Number(body.effectId)
            if (!Number.isInteger(strip) || strip < 0 || strip >= 1) {
              json(res, 400, { ok: false, error: 'strip out of range' })
              return
            }
            if (!Number.isInteger(id) || id < 0 || id >= 15) {
              json(res, 400, { ok: false, error: 'effectId out of range' })
              return
            }
            while (stripEffects.length <= strip) stripEffects.push(0)
            stripEffects[strip] = id
            json(res, 200, { ok: true, effect: stripEffects })
          })
          return
        }

        if (url === '/api/state/sensitivity') {
          readJson((body) => {
            const v = Number(body.value)
            if (!Number.isFinite(v) || v < 0 || v > 5) {
              json(res, 400, { ok: false, error: 'value must be 0..5' })
              return
            }
            mockSensitivity = v
            json(res, 200, { ok: true, sensitivity: mockSensitivity })
          })
          return
        }

        if (url === '/api/state/backlight') {
          readJson((body) => {
            const v = Number(body.value)
            if (!Number.isFinite(v) || v < 0 || v > 100) {
              json(res, 400, { ok: false, error: 'value must be 0..100' })
              return
            }
            mockBacklight = Math.round(v)
            json(res, 200, { ok: true, backlightPct: mockBacklight })
          })
          return
        }

        if (url === '/api/state/timeout') {
          readJson((body) => {
            const v = Number(body.value)
            if (!Number.isFinite(v) || v < 0 || v > 86400) {
              json(res, 400, { ok: false, error: 'value must be 0..86400' })
              return
            }
            mockTimeout = Math.round(v)
            json(res, 200, { ok: true, timeoutS: mockTimeout })
          })
          return
        }

        json(res, 404, { ok: false, error: 'not found' })
      })
    },
  }
}