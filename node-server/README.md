NutriCycle Node Gateway

This Node.js gateway acts as the public entry point for ESP32 devices and the frontend. It forwards image payloads to the local Python AI `/detect` endpoint and broadcasts detection JSON to connected frontend clients via Socket.IO.

Quick start:

1. Copy `.env.example` to `.env` and adjust values
2. npm install
3. npm run dev (or npm start)

Endpoints:
- POST /upload -> Accepts an image (multipart/form-data `image`, raw image bytes, or JSON with base64 `image`), forwards to Python, returns detection JSON. Use `X-API-KEY` header if `API_KEY` set.
- POST /detect -> Proxy to Python `/detect` and optionally broadcast detections to clients.
- GET  /video_feed -> Proxy to Python `/video_feed` (MJPEG stream)
- GET  /health -> Basic health check

Example cURL (raw bytes):

curl -X POST http://localhost:3000/upload --data-binary @frame.jpg -H "Content-Type: application/octet-stream" -H "X-API-KEY: changeme"

Example cURL (multipart):

curl -X POST http://localhost:3000/upload -F "image=@frame.jpg" -H "X-API-KEY: changeme"

Socket.IO: clients can connect to the gateway socket and listen for `detections` events:

// Browser sample (frontend)
const socket = io('http://localhost:3000');
socket.on('detections', data => console.log('Detections', data));

Video (MJPEG): open `http://localhost:3000/video_feed` in your browser to view the proxied MJPEG stream from the private Python server.

Mobile snapshot & ngrok testing:
- The gateway exposes `GET /snapshot` which returns the latest resized (320×320) snapshot uploaded by the ESP32 (useful for mobile apps to poll/refresh cheaply).
- To test over the internet with ngrok, start ngrok to forward port 3000: `ngrok http 3000` and set your ESP32 `UPLOAD_URL` to `https://<your-ngrok-id>.ngrok.io/upload`. Ensure `API_KEY` matches the device header.

HLS (low-bandwidth multi-view) support:
- The gateway can run `ffmpeg` on-demand to transcode device MJPEG into HLS (H.264 `.m3u8` playlists) for efficient multi-view streaming.
- Endpoints:
  - `GET /hls?device_id=<id>` or `GET /hls?url=<encoded_url>` — starts an HLS transcode and returns `{ ok: true, id, playlist }` where `playlist` is e.g. `/hls/<id>/index.m3u8`.
  - `GET /hls/<id>/index.m3u8` and `/hls/<id>/<segment>` — serves the playlist and segments.
- Requirements: `ffmpeg` in PATH with libx264 support. Check on your server with `ffmpeg -version`.

Installing ffmpeg:
- Ubuntu/Debian: `sudo apt update && sudo apt install -y ffmpeg`
- macOS (Homebrew): `brew install ffmpeg`
- Windows (choco): `choco install ffmpeg` or download from https://ffmpeg.org/download.html and add to PATH.
- Confirm with: `ffmpeg -version` (should print version and configuration).

React Native playback notes:
- iOS: HLS is supported natively by AVFoundation; use `react-native-video` (default) to play `.m3u8`.
- Android: use `react-native-video` with the ExoPlayer backend (recommended) for HLS support.
- For internet viewers, serve over HTTPS or configure your RN build to permit HTTP (Android network config / iOS ATS); using HTTPS is recommended for production.

If you'd like, I can add a small UI control to toggle HLS mode in the web UI and provide a sample React Native snippet to play the returned `playlist` URL.

Security notes:
- Set a strong `API_KEY` before exposing the gateway.
- Keep `PYTHON_DETECT_URL` pointing at `localhost` so Python is not exposed publicly.
- Use `MAX_BODY_SIZE` and rate limiting to protect from abuse.