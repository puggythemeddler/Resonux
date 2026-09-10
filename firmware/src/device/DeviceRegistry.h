#pragma once
#include "device/DeviceTypes.h"
#include <string.h>

// Fixed-slot registry of discovered / configured devices. Pure C++ and
// host-testable. The registry is the device "world view": it holds both
// transient detections (scan results, sync peers) and trusted, persisted
// bindings. Deduplication is by device id (e.g. "wifi:resonux-01",
// "usb:1acf:2001", "profile:am006").
//
// Not thread-safe by design — the DeviceManager serializes access from the
// single WebServer task. Host tests exercise it directly.

namespace dev {

constexpr int kMaxDevices = 16;

struct RegistryHandle {
  int index;
};

class DeviceRegistry {
public:
  int capacity() const { return kMaxDevices; }
  int count() const { return _count; }

  const DeviceInfo* at(int i) const {
    return (i >= 0 && i < _count) ? &_slots[i] : nullptr;
  }

  const DeviceInfo* find(const char* id) const {
    if (!id) return nullptr;
    for (int i = 0; i < _count; ++i)
      if (strcmp(_slots[i].id, id) == 0) return &_slots[i];
    return nullptr;
  }

  int indexOf(const char* id) const {
    if (!id) return -1;
    for (int i = 0; i < _count; ++i)
      if (strcmp(_slots[i].id, id) == 0) return i;
    return -1;
  }

  int indexOfHandle(const RegistryHandle* h) const {
    return h ? h->index : -1;
  }

  // Upsert: returns the slot index (existing or new) or -1 when full.
  int upsert(const DeviceInfo& d) {
    if (!d.id[0]) return -1;
    int idx = indexOf(d.id);
    if (idx >= 0) {
      bool persist = _slots[idx].persisted;
      _slots[idx] = d;
      _slots[idx].persisted = persist;  // discovery never clears trust
      return idx;
    }
    if (_count >= kMaxDevices) return -1;
    _slots[_count] = d;
    _slots[_count].lastSeenMs = d.lastSeenMs;
    return _count++;
  }

  bool remove(const char* id) {
    int idx = indexOf(id);
    if (idx < 0) return false;
    for (int i = idx; i < _count - 1; ++i) _slots[i] = _slots[i + 1];
    _count--;
    return true;
  }

  void clearTransient() {
    int w = 0;
    for (int r = 0; r < _count; ++r) {
      if (_slots[r].persisted) {
        if (w != r) _slots[w] = _slots[r];
        w++;
      }
    }
    _count = w;
  }

  DeviceInfo* slot(int i) { return (i >= 0 && i < _count) ? &_slots[i] : nullptr; }

  // True if every slot is used (registry full).
  bool full() const { return _count >= kMaxDevices; }

private:
  DeviceInfo _slots[kMaxDevices];
  int        _count = 0;
};

}  // namespace dev