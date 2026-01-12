import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// Dev server proxies API calls to the gateway so you can run the React dev server
export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    proxy: {
      '/probe_stream': 'http://localhost:3000',
      '/device_stream': 'http://localhost:3000',
      '/upload': 'http://localhost:3000',
      '/devices': 'http://localhost:3000'
    }
  },
  build: {
    outDir: '../../node-server/public/react',
    emptyOutDir: false
  }
});