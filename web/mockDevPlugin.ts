import type { Plugin, Connect } from 'vite'

// Mock backend for `npm run dev` — simulates the firmware REST API so the
// dashboard can be developed/tested without hardware.

const DEFAULT_CONFIG: Record<string, unknown> = {
  version: 1,
  deviceName: 'Resonux-SIM',
  masterBrightness: 255,
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

  const DEFAULT_THEMES = [
    {
      id: 'classic', name: 'Classic', builtin: true, brightness: 100, saturation: 255,
      palette: ['#FF0000', '#FF7700', '#FFEE00'],
      response: { bass: 0.5, mid: 0.5, treble: 0.5, beat: 0.5, amp: 1 },
      animation: { movement: 0.5, pulse: 0.5, sparkle: 0.3, smoothing: 0.3 },
    },
    {
      id: 'edm', name: 'EDM', builtin: true, brightness: 100, saturation: 255,
      palette: ['#FF00FF', '#00FFFF', '#0066FF'],
      response: { bass: 1, mid: 0.6, treble: 0.9, beat: 1, amp: 1 },
      animation: { movement: 0.8, pulse: 0.9, sparkle: 0.7, smoothing: 0.2 },
    },
    {
      id: 'chill', name: 'Chill', builtin: true, brightness: 70, saturation: 220,
      palette: ['#2B5B84', '#4FA3D8', '#E8B8D6'],
      response: { bass: 0.4, mid: 0.7, treble: 0.3, beat: 0.2, amp: 0.6 },
      animation: { movement: 0.2, pulse: 0.15, sparkle: 0.1, smoothing: 0.7 },
    },
    {
      id: 'custom1', name: 'My Theme', brightness: 85, saturation: 255,
      palette: ['#FF6A00', '#22D3EE', '#FFFFFF'],
      response: { bass: 0.9, mid: 0.8, treble: 0.7, beat: 0.8, amp: 1 },
      animation: { movement: 0.6, pulse: 0.6, sparkle: 0.5, smoothing: 0.35 },
    },
  ]

  let themesList: unknown[] = DEFAULT_THEMES.map((t) => ({ ...t }))
  let activeThemeId = 'custom1'

  return {
    name: 'resonux-mock-api',
    configureServer(server) {
      server.middlewares.use((req: Connect.IncomingMessage, res: Connect.ServerResponse, next: Connect.NextFunction) => {
        const url = (req.url ?? '').split('?')[0]
        if (!url.startsWith('/api/')) return next()

        if (url === '/api/status') {
          json(res, 200, {
            ok: true,
            device: 'Resonux (simulator)',
            uptimeMs: Date.now() - startMs,
            heap: 180000 + Math.floor(Math.random() * 20000),
            fps: 60 + Math.floor(Math.random() * 5),
            stripCount: 1,
            wifi: { mode: 'AP+STA', ip: '192.168.4.1', connected: true },
            artnet: { enabled: false, fixtures: 0, status: 'idle' },
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
            json(res, 200, { themes: themesList, active: activeThemeId })
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
            if (activeThemeId === id) activeThemeId = 'classic'
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
          activeThemeId = 'classic'
          json(res, 200, { ok: true })
          return
        }

        json(res, 404, { ok: false, error: 'not found' })
      })
    },
  }
}