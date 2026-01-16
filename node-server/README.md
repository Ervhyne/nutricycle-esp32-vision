NutriCycle Node Gateway

This Node.js gateway acts as the public entry point for ESP32 devices and the frontend. It forwards image payloads to the local Python AI `/detect` endpoint and broadcasts detection JSON to connected frontend clients via Socket.IO.

Quick start:

1. Copy `.env.example` to `.env` and adjust values
2. npm install
3. npm run dev (or npm start)

Endpoints:
- POST /upload -> Accepts an image (multipart/form-data `image`, raw image bytes, or JSON with base64 `image`). For reliability the gateway now acknowledges uploads quickly and runs detection in the background; responses are `{ ok: true, queued: true }`. Use `X-API-KEY` header if `API_KEY` set. If you need synchronous detection results use `POST /detect`.
- POST /detect -> Proxy to Python `/detect` and optionally broadcast detections to clients.
- GET  /video_feed -> Proxy to Python `/video_feed` (MJPEG stream)
- GET  /health -> Basic health check

Example cURL (raw bytes):

curl -X POST http://localhost:3000/upload --data-binary @frame.jpg -H "Content-Type: application/octet-stream" -H "X-API-KEY: changeme"

Example cURL (multipart):

curl -X POST http://localhost:3000/upload -F "image=@frame.jpg" -H "X-API-KEY: changeme"

Socket.IO: clients can connect to the gateway socket and listen for `detections` events and live frames.

// Browser sample (frontend)
const socket = io('http://localhost:3000');
// Detections (bounding boxes)
socket.on('detections', data => console.log('Detections', data));
// Live frames (binary with metadata)
socket.on('frame', (meta, buf) => {
  // buf is a binary Buffer/ArrayBuffer containing JPEG bytes
  // Convert to base64 and set an <img> src to 'data:image/jpeg;base64,' + base64
  console.log('Frame meta', meta)
});

The gateway now broadcasts recent frames as they arrive (event `frame`) along with `meta` (contains `deviceId`, `size`, `ts`). Frontends can subscribe and display live images without needing direct device IP access. For compatibility we also continue to expose `/snapshot` for polling clients and `/device_stream` for MJPEG proxying.

Video (MJPEG): open `http://localhost:3000/video_feed` in your browser to view the proxied MJPEG stream from the private Python server.

Mobile snapshot & ngrok testing:
- The gateway exposes `GET /snapshot` which returns the latest resized (320×320) snapshot uploaded by the ESP32 (useful for mobile apps to poll/refresh cheaply).
- To test over the internet with ngrok, start ngrok to forward port 3000: `ngrok http 3000`. With a single public ngrok link you can expose the gateway once and the gateway will expose per-device public streams at `https://<your-ngrok-id>.ngrok.io/device_stream?device_id=<id>` (no additional tunnels required). Set your ESP32 `UPLOAD_URL` to `https://<your-ngrok-id>.ngrok.io/upload`. Ensure `API_KEY` matches the device header.

- If you want a stable public URL, either reserve a static ngrok subdomain (paid) or set `PUBLIC_BASE_URL` in the Node server `.env` to your ngrok URL (or leave it unset if you will start ngrok manually). When `PUBLIC_BASE_URL` is set, the gateway will return a `publicStreamUrl` in `/upload` responses for devices that include `X-DEVICE-ID`.

HLS (low-bandwidth multi-view) support:
- The gateway can run `ffmpeg` on-demand to transcode device MJPEG into HLS (H.264 `.m3u8` playlists) for efficient multi-view streaming.
- Endpoints:
  - `GET /hls?device_id=<id>` or `GET /hls?url=<encoded_url>` — starts an HLS transcode and returns `{ ok: true, id, playlist }` where `playlist` is e.g. `/hls/<id>/index.m3u8`.
  - `GET /hls/<id>/index.m3u8` and `/hls/<id>/<segment>` — serves the playlist and segments.
- Requirements: `ffmpeg` in PATH with libx264 support. Check on your server with `ffmpeg -version`.

Installing ffmpeg (server only):
- Ubuntu/Debian (apt):
  ```bash
  sudo apt update && sudo apt install -y ffmpeg
  ```
- macOS (Homebrew):
  ```bash
  brew install ffmpeg
  ```
- Windows (Chocolatey):
  ```powershell
  choco install ffmpeg
  ```
  or download from https://ffmpeg.org/download.html and add to PATH.
- Verify installation:
  ```bash
  ffmpeg -version
  ```

Quick tunneling & remote testing (server only):
- Cloudflare Tunnel (recommended, free quick tunnels):
  1. Install cloudflared: https://developers.cloudflare.com/cloudflare-one/connections/connect-apps/install-and-setup/
  2. Run a quick tunnel (for testing only):
     ```bash
     cloudflared tunnel --url http://localhost:3000
     ```
     - Cloudflared prints a temporary URL like `https://<random>.trycloudflare.com` to use as your public gateway.
  3. Point your ESP `UPLOAD_URL` to `https://<random>.trycloudflare.com/upload` and the server will receive uploads.

- ngrok (alternative):
  - Start: `ngrok http 3000` (note: TCP tunnels require an authenticated/paid account)
  - If you want a stable domain consider reserving a subdomain or setting `PUBLIC_BASE_URL` in `.env`.

- LocalTunnel (very quick test):
  ```bash
  npx localtunnel --port 3000
  ```
  - Returns a `https://<name>.loca.lt` URL for quick testing (ephemeral).

Notes & best practices:
- These tunneling tools are for testing and debugging. For production, use a named Cloudflare Tunnel or a proper reverse proxy with a stable domain and TLS certs.
- All commands above are intended to be run on the SERVER machine (where Node/Python run), not on the ESP or in Node itself.

Detection queue and tuning (throttling):
- The gateway includes a bounded detection worker pool to avoid overwhelming the Python server.
- Environment variables:
  - `DETECT_WORKERS` (default 2) — number of concurrent Python jobs
  - `DETECT_QUEUE_SIZE` (default 50) — max queued jobs before new uploads get rejected with 503
- Debug endpoint: `GET /debug/detect_queue` — returns current queue stats for monitoring.



React Native playback notes:
- iOS: HLS is supported natively by AVFoundation; use `react-native-video` (default) to play `.m3u8`.
- Android: use `react-native-video` with the ExoPlayer backend (recommended) for HLS support.
- For internet viewers, serve over HTTPS or configure your RN build to permit HTTP (Android network config / iOS ATS); using HTTPS is recommended for production.

If you'd like, I can add a small UI control to toggle HLS mode in the web UI and provide a sample React Native snippet to play the returned `playlist` URL.

Security notes:
- Set a strong `API_KEY` before exposing the gateway.
- Keep `PYTHON_DETECT_URL` pointing at `localhost` so Python is not exposed publicly.
- Use `MAX_BODY_SIZE` and rate limiting to protect from abuse.