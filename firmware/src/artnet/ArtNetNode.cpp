#include "artnet/ArtNetNode.h"
#include "network/WifiManager.h"
#include "util/Log.h"
#include <Arduino.h>
#include <WiFi.h>

ArtNetNode::ArtNetNode(const ArtNetConfig& cfg, const FixtureConfig* fixtures,
                       int fixtureCount)
    : _cfg(cfg), _fixtures(fixtures), _fixtureCount(fixtureCount) {}

ArtNetNode::~ArtNetNode() { stop(); }

bool ArtNetNode::begin() {
  if (!_cfg.enabled) return false;

  if (!_cfg.ssid[0]) {
    logBoot("artnet", "no SSID configured, disabled");
    return false;
  }

  _audioMutex = xSemaphoreCreateMutex();
  if (!_audioMutex) return false;

  memset(_dmxIn, 0, sizeof(_dmxIn));
  memset(_dmxOut, 0, sizeof(_dmxOut));

  if (!connectWifi()) {
    logBoot("artnet", "wifi FAILED, disabled");
    return false;
  }

  if (!_udp.begin(kArtNetPort)) {
    logBoot("artnet", "UDP bind FAILED");
    return false;
  }

  _running = true;
  xTaskCreatePinnedToCore(ArtNetNode::taskEntry, "artnet", kTaskStack, this,
                          kTaskPriority, &_task, 0);

  logBoot("artnet", "node started (universe=%d, fixtures=%d)", _cfg.universe,
          _fixtureCount);
  return true;
}

void ArtNetNode::stop() {
  _running = false;
  if (_task) {
    vTaskDelay(pdMS_TO_TICKS(50));
    _task = nullptr;
  }
  _udp.stop();
  if (_audioMutex) {
    vSemaphoreDelete(_audioMutex);
    _audioMutex = nullptr;
  }
  WiFi.disconnect(true);
  _connected = false;
}

void ArtNetNode::blackout() {
  if (!_running) return;
  memset(_dmxOut, 0, kDmxChannels);
  sendDmx();
}

bool ArtNetNode::connectWifi() {
  WifiManager& wifi = WifiManager::instance();

  if (wifi.managed()) {
    if (!wifi.ensureConnected()) return false;
    _connected = true;
    logBoot("artnet", "using shared wifi ip=%s", wifi.ip().toString().c_str());
    return true;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setHostname("resonux-artnet");

  if (_cfg.useDhcp) {
    WiFi.begin(_cfg.ssid, _cfg.password);
  } else {
    IPAddress ip(_cfg.staticIp[0], _cfg.staticIp[1], _cfg.staticIp[2],
                 _cfg.staticIp[3]);
    IPAddress mask(_cfg.staticMask[0], _cfg.staticMask[1], _cfg.staticMask[2],
                   _cfg.staticMask[3]);
    IPAddress gw(_cfg.staticGw[0], _cfg.staticGw[1], _cfg.staticGw[2],
                 _cfg.staticGw[3]);
    WiFi.config(ip, gw, mask);
    WiFi.begin(_cfg.ssid, _cfg.password);
  }

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > 15000) return false;
    delay(100);
  }

  _connected = true;
  logBoot("artnet", "wifi OK ip=%s", WiFi.localIP().toString().c_str());
  return true;
}

void ArtNetNode::taskEntry(void* arg) {
  ArtNetNode* self = (ArtNetNode*)arg;
  self->taskLoop();
}

void ArtNetNode::taskLoop() {
  uint8_t packet[600];
  uint32_t lastDmxSend = 0;

  while (_running) {
    WifiManager& wifi = WifiManager::instance();
    if (wifi.managed()) {
      if (!wifi.ensureConnected()) {
        delay(1000);
        continue;
      }
    } else if (WiFi.status() != WL_CONNECTED) {
      _connected = false;
      delay(1000);
      connectWifi();
      continue;
    }

    int cb = _udp.parsePacket();
    if (cb > 0 && cb < (int)sizeof(packet)) {
      int len = _udp.read(packet, sizeof(packet));
      IPAddress remoteIp = _udp.remoteIP();
      if (len >= kArtNetHeaderSize) {
        handlePacket(packet, len, remoteIp);
      }
    }

    if (_cfg.audioReactive) {
      applyAudioMapping();
    }

    uint32_t now = millis();
    if (now - lastDmxSend >= 25) {
      lastDmxSend = now;
      sendDmx();
    }

    vTaskDelay(1);
  }
}

void ArtNetNode::handlePacket(const uint8_t* data, int len,
                              const IPAddress& fromIp) {
  const char kArtNetId[] = "Art-Net";
  if (memcmp(data, kArtNetId, 7) != 0) return;

  uint16_t opcode = data[8] | (data[9] << 8);

  switch (opcode) {
    case ARTNET_OP_POLL:
      handlePoll(fromIp);
      break;
    case ARTNET_OP_DMX:
      handleDmx(data, len);
      break;
    default:
      break;
  }
}

void ArtNetNode::handlePoll(const IPAddress& fromIp) {
  sendPollReply(fromIp);
}

void ArtNetNode::handleDmx(const uint8_t* data, int len) {
  if (len < kArtDmxSize) return;

  const ArtNetDmx* pkt = (const ArtNetDmx*)data;
  uint16_t subUni = pkt->subUni;
  if (subUni != _cfg.universe) return;

  uint16_t dmxLen = ((uint16_t)pkt->lengthHi << 8) | pkt->lengthLo;
  if (dmxLen > kDmxChannels) dmxLen = kDmxChannels;

  memcpy(_dmxIn, pkt->data, dmxLen);
}

void ArtNetNode::sendPollReply(const IPAddress& toIp) {
  ArtNetPollReply reply = {};
  artnetFillHeader(reply.id);
  reply.opcode = ARTNET_OP_POLL_REPLY;

  IPAddress localIp = WiFi.localIP();
  reply.ip[0] = localIp[0];
  reply.ip[1] = localIp[1];
  reply.ip[2] = localIp[2];
  reply.ip[3] = localIp[3];
  reply.port = kArtNetPort;

  reply.versionHi = kArtNetVersionHi;
  reply.versionLo = kArtNetVersionLo;
  reply.net = 0;
  reply.sub = 0;
  reply.oemHi = 0xFF;
  reply.oemLo = 0xFF;
  reply.ubeaVersion = 0;
  reply.status1 = 0xD0;
  reply.estaCode = 0x7FFF;
  reply.style = 0x00;

  strncpy(reply.shortName, "Resonux", sizeof(reply.shortName) - 1);
  strncpy(reply.longName, "Resonux Art-Net Node", sizeof(reply.longName) - 1);
  snprintf(reply.report, sizeof(reply.report), "Resonux v1 [%d fixtures]",
           _fixtureCount);

  reply.numPortsHi = 0;
  reply.numPortsLo = 1;
  reply.portTypes[0] = 0x80;
  reply.goodInput[0] = 0x08;
  reply.goodOutput[0] = 0x80;
  reply.swIn[0] = _cfg.universe;
  reply.swOut[0] = _cfg.universe;

  uint8_t mac[6];
  WiFi.macAddress(mac);
  memcpy(reply.mac, mac, 6);
  memcpy(reply.bindIp, reply.ip, 4);
  reply.bindIndex = 1;
  reply.status2 = 0x08;

  _udp.beginPacket(toIp, kArtNetPort);
  _udp.write((const uint8_t*)&reply, kArtPollReplySize);
  _udp.endPacket();
}

void ArtNetNode::sendDmx() {
  ArtNetDmx pkt = {};
  artnetFillHeader(pkt.id);
  pkt.opcode = ARTNET_OP_DMX;
  pkt.versionHi = kArtNetVersionHi;
  pkt.sequence = ++_sequence;
  if (_sequence == 0) _sequence = 1;
  pkt.physical = 0;
  pkt.subUni = _cfg.universe;
  pkt.lengthHi = 0;
  pkt.lengthLo = kDmxChannels & 0xFF;
  memcpy(pkt.data, _dmxOut, kDmxChannels);

  IPAddress broadcast = ~WiFi.subnetMask() | WiFi.localIP();
  _udp.beginPacket(broadcast, kArtNetPort);
  _udp.write((const uint8_t*)&pkt, kArtDmxSize + kDmxChannels);
  _udp.endPacket();
}

void ArtNetNode::setAudioFrame(const AudioFrame& f) {
  if (xSemaphoreTake(_audioMutex, pdMS_TO_TICKS(2))) {
    _audio = f;
    xSemaphoreGive(_audioMutex);
  }
}

void ArtNetNode::applyAudioMapping() {
  AudioFrame frame;
  if (xSemaphoreTake(_audioMutex, pdMS_TO_TICKS(2))) {
    frame = _audio;
    xSemaphoreGive(_audioMutex);
  } else {
    return;
  }

  for (int i = 0; i < _fixtureCount; ++i) {
    const FixtureConfig& fix = _fixtures[i];
    if (fix.profileId <= 0 || fix.profileId >= kFixtureProfileCount) continue;
    const FixtureProfile& profile = kFixtureProfiles[fix.profileId];
    if (profile.channelCount == 0) continue;

    for (int inst = 0; inst < fix.count; ++inst) {
      mapFixture(frame, i, profile);
    }
  }
}

void ArtNetNode::mapFixture(const AudioFrame& frame, int fixtureIdx,
                            const FixtureProfile& profile) {
  const FixtureConfig& fix = _fixtures[fixtureIdx];
  int baseAddr = fix.dmxAddress - 1;
  if (baseAddr < 0 || baseAddr + profile.channelCount > kDmxChannels) return;

  float amp = frame.amplitude;
  float bass = frame.bass;
  float treble = frame.treble;
  bool beat = frame.beat;
  float beatStr = frame.beatStrength;

  for (int ch = 0; ch < profile.channelCount; ++ch) {
    int addr = baseAddr + ch;
    if (addr >= kDmxChannels) break;

    switch (profile.channels[ch]) {
      case CH_PAN: {
        float center = 127.5f;
        float swing = center * _cfg.panSpeed;
        float dir = (beat && ch % 2 == 0) ? 1.0f : -0.3f;
        _dmxOut[addr] = clamp255((uint8_t)(center + amp * swing * dir));
        break;
      }
      case CH_PAN_FINE:
        _dmxOut[addr] = 0;
        break;
      case CH_TILT: {
        float center = 127.5f;
        float swing = center * _cfg.tiltSpeed;
        float tilt = center + bass * swing;
        _dmxOut[addr] = clamp255((uint8_t)(tilt + 0.5f));
        break;
      }
      case CH_TILT_FINE:
        _dmxOut[addr] = 0;
        break;
      case CH_DIMMER:
        _dmxOut[addr] = clamp255((uint8_t)scaleU8(amp * _cfg.colorSensitivity));
        break;
      case CH_RED: {
        float h = fmod(frame.bass * 360.0f + beatStr * 60.0f, 360.0f);
        float r, g, b;
        hsvToRgb(h / 360.0f, 1.0f, amp, r, g, b);
        _dmxOut[addr] = scaleU8(r);
        break;
      }
      case CH_GREEN: {
        float h = fmod(frame.bass * 360.0f + beatStr * 60.0f, 360.0f);
        float r, g, b;
        hsvToRgb(h / 360.0f, 1.0f, amp, r, g, b);
        _dmxOut[addr] = scaleU8(g);
        break;
      }
      case CH_BLUE: {
        float h = fmod(frame.bass * 360.0f + beatStr * 60.0f, 360.0f);
        float r, g, b;
        hsvToRgb(h / 360.0f, 1.0f, amp, r, g, b);
        _dmxOut[addr] = scaleU8(b);
        break;
      }
      case CH_WHITE:
        _dmxOut[addr] = scaleU8(treble * 0.6f);
        break;
      case CH_STROBE:
        _dmxOut[addr] = beat ? 200 : 0;
        break;
      case CH_GOBO:
        _dmxOut[addr] = (uint8_t)(amp * 200.0f + 28);
        break;
      case CH_PRISM:
        _dmxOut[addr] = beat ? 180 : 0;
        break;
      case CH_FOCUS:
        _dmxOut[addr] = 128;
        break;
      case CH_SPEED:
        _dmxOut[addr] = (uint8_t)(_cfg.panSpeed * 255.0f);
        break;
      case CH_NONE:
      default:
        _dmxOut[addr] = 0;
        break;
    }
  }
}

uint8_t ArtNetNode::clamp255(int v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return (uint8_t)v;
}

uint8_t ArtNetNode::scaleU8(float v) {
  if (v <= 0.0f) return 0;
  if (v >= 1.0f) return 255;
  return (uint8_t)(v * 255.0f + 0.5f);
}

void ArtNetNode::hsvToRgb(float h, float s, float v, float& r, float& g,
                          float& b) {
  if (s <= 0.0f) { r = g = b = v; return; }
  h = fmod(h, 1.0f);
  if (h < 0.0f) h += 1.0f;
  float i = h * 6.0f;
  float f = h * 6.0f - floorf(i);
  float p = v * (1.0f - s);
  float q = v * (1.0f - s * f);
  float t = v * (1.0f - s * (1.0f - f));
  int ii = (int)i % 6;
  switch (ii) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    case 5: r = v; g = p; b = q; break;
  }
}

const char* ArtNetNode::statusString() const {
  if (!_cfg.enabled) return "disabled";
  if (!_connected) return "disconnected";
  return "active";
}
