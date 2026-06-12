import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { viteSingleFile } from 'vite-plugin-singlefile';
import { VitePWA } from 'vite-plugin-pwa';
import { resolve } from 'path';

export default defineConfig({
  plugins: [
    react(), 
    viteSingleFile(),
    VitePWA({
      registerType: 'autoUpdate',
      includeAssets: ['favicon.ico', 'apple-touch-icon.png', 'mask-icon.svg'],
      manifest: {
        name: 'ESP32 Fan & Sniffer',
        short_name: 'FanSniffer',
        description: 'ESP32 Fan Controller and WiFi Sniffer',
        theme_color: '#0a0c12',
        background_color: '#0a0c12',
        display: 'standalone',
        icons: [
          {
            src: 'pwa-192x192.png',
            sizes: '192x192',
            type: 'image/png'
          },
          {
            src: 'pwa-512x512.png',
            sizes: '512x512',
            type: 'image/png'
          }
        ]
      }
    })
  ],
  build: {
    outDir: resolve(__dirname, '../data'),
    emptyOutDir: false,
  },
  server: {
    host: '0.0.0.0',
    proxy: {
      '/api': {
        target: 'http://esp32-fan.local',
        changeOrigin: true,
        ws: true,
      }
    }
  },
  resolve: {
    alias: {
      '@': resolve(__dirname, './src'),
    },
  },
});
