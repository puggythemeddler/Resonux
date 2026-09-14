import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

export default defineConfig({
  root: "ui",
  base: "./",
  plugins: [react()],
  build: {
    outDir: "../dist/renderer",
    emptyOutDir: true,
    sourcemap: true,
  },
  server: {
    port: 5174,
    strictPort: false,
  },
});