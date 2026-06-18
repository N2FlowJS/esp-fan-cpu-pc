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
      injectRegister: 'script',
      devOptions: {
        enabled: false  // Disable PWA in dev to prevent service worker from blocking API
      },
      workbox: {
        navigateFallbackDenylist: [/^\/api\//],  // Don't precache API routes
        runtimeCaching: [
          {
            urlPattern: /^https?:\/\/(esp32-fan|192\.168|172\.18|localhost).*\/api\/.*/,
            handler: 'NetworkFirst',
            options: {
              cacheName: 'api-cache',
              networkTimeoutSeconds: 5
            }
          }
        ]
      },
      includeAssets: ['favicon.ico', 'apple-touch-icon.png', 'pwa-192x192.png', 'pwa-512x512.png'],
      manifest: {
        name: 'ESP32 Fan & Sniffer',
        short_name: 'FanSniffer',
        description: 'ESP32 Fan Controller and WiFi Sniffer',
        theme_color: '#0a0c12',
        background_color: '#0a0c12',
        display: 'standalone',
        scope: './',
        start_url: './',
        id: './',
        icons: [
          {
            src: 'pwa-192x192.png',
            sizes: '192x192',
            type: 'image/png',
            purpose: 'any'
          },
          {
            src: 'pwa-512x512.png',
            sizes: '512x512',
            type: 'image/png',
            purpose: 'any'
          },
          {
            src: 'pwa-512x512.png',
            sizes: '512x512',
            type: 'image/png',
            purpose: 'maskable'
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
  },
  resolve: {
    alias: {
      '@': resolve(__dirname, './src'),
    },
  },
});
