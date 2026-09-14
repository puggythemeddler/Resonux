import type { ResonuxApi } from "../src/domain/bridge";

declare global {
  interface Window {
    resonux: ResonuxApi;
  }
}

export {};