import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

export default defineConfig({
  plugins: [react()],
  base: './',
  build: {
    outDir: '../firmware/data/web',
    emptyOutDir: true,
    assetsInlineLimit: 4096,
    chunkSizeWarningLimit: 300,
  },
  server: {
    host: true,
    port: 5173,
  },
})
