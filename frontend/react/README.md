NutriCycle React Viewer

Development:
1. cd frontend/react
2. npm install
3. npm run dev (opens http://localhost:5173)

The dev server proxies `/probe_stream` and `/device_stream` to the gateway at http://localhost:3000 (see vite.config.js).

Production build:
- npm run build
- Output is written to `node-server/public/react` so the gateway will serve it at `http://<gateway>:3000/react/`.