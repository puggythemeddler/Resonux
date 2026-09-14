// UDP RESO_DISCOVER scanner (main-process side). Probes the well-known
// multicast group 239.255.42.10:9770 (firmware kDiscoverGroup/kDiscoverPort),
// joins on every non-internal IPv4 interface, and reports every well-formed
// responder envelope together with the source address the reply came from —
// that address is how the app learns the controller's IP for REST calls.

import dgram from "node:dgram";
import os from "node:os";
import {
  kDiscoverGroup,
  kDiscoverPort,
  kDiscoverProbe,
  kDiscoverProbeIntervalMs,
  parseDiscoverResponse,
  type DiscoverEnvelope,
} from "../domain/discovery";

export interface DiscoveredController extends DiscoverEnvelope {
  address: string;
}

export interface UdpDiscoveryOptions {
  probeIntervalMs?: number;
}

export class UdpDiscovery {
  private socket: dgram.Socket | null = null;
  private timer: NodeJS.Timeout | null = null;
  private readonly probeIntervalMs: number;

  constructor(opts: UdpDiscoveryOptions = {}) {
    this.probeIntervalMs = opts.probeIntervalMs ?? kDiscoverProbeIntervalMs;
  }

  ping(): void {
    if (!this.socket) return;
    try {
      this.socket.send(Buffer.from(kDiscoverProbe), kDiscoverPort, kDiscoverGroup);
    } catch {
      /* socket closing */
    }
  }

  start(onController: (c: DiscoveredController) => void): void {
    if (this.socket) return;
    const socket = dgram.createSocket({ type: "udp4", reuseAddr: true });
    this.socket = socket;

    socket.on("message", (msg, rinfo) => {
      const env = parseDiscoverResponse(msg);
      if (env.valid) {
        onController({ ...env, address: rinfo.address });
      }
    });

    socket.on("error", () => {
      // Non-fatal: an interface may refuse membership; probing stops silently.
    });

    socket.bind(0, () => {
      const ifaces = os.networkInterfaces();
      for (const name of Object.keys(ifaces)) {
        for (const iface of ifaces[name] ?? []) {
          if (iface.family !== "IPv4" || iface.internal) continue;
          try {
            socket.addMembership(kDiscoverGroup, iface.address);
            socket.setMulticastInterface(iface.address);
          } catch {
            // Interface not multicast-capable (VPN, virtual NIC) — skip.
          }
        }
      }
      socket.setMulticastTTL(1);

      const probe = () => this.ping();
      probe();
      this.timer = setInterval(probe, this.probeIntervalMs);
    });
  }

  stop(): void {
    if (this.timer) {
      clearInterval(this.timer);
      this.timer = null;
    }
    if (this.socket) {
      try {
        this.socket.close();
      } catch {
        /* already closed */
      }
      this.socket = null;
    }
  }
}